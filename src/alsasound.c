/*
 **************************************************************************************************
 *
 * Alsa sound backend for softgun 
 *
 * State:
 * 	Working with Uzebox emulator. Problems with the rare samplerate
 *	of 15700 Hz on some PC's. Has a feedback loop for controlling
 *	the speed of the CPU. 
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */

#ifdef __linux__
#include <alsa/asoundlib.h>
#include <poll.h>
#include <pthread.h>
#include "fio.h"
#include "sound.h"
#include "sgstring.h"
#include "signode.h"
#include "cycletimer.h"

#if 0
#define dbgprintf(...) fprintf(stderr,__VA_ARGS__)
#else
#define dbgprintf(...)
#endif

#define BUFFERSIZE (6*1024)
#define BUF_WP(as)	((as)->outBuffer_wp % BUFFERSIZE)
#define BUF_RP(as)	((as)->outBuffer_rp % BUFFERSIZE)
#define BUF_CNT(as) 	((as)->outBuffer_wp - (as)->outBuffer_rp)

typedef struct AlsaSound {
	SoundDevice sdev;
	CycleTimer buffer_check_timer;
	int bytes_per_frame;
	int alsa_sound_fmt;
	unsigned int samplerate;
	snd_pcm_t *outHandle;
	snd_pcm_hw_params_t *hwpar;
	snd_pcm_sw_params_t *swpar;
	uint8_t outBuffer[BUFFERSIZE];
	uint64_t outBuffer_wp;
	uint64_t outBuffer_rp;
	unsigned int outBufferSize;
	int poll_fd;
	int outFh_active;
	FIO_FileHandler outFh;
	pthread_t write_thread;
	pthread_mutex_t write_mutex;
	int speed_up;
	int speed_down;
} AlsaSound;

/*
 *************************************************************************
 * SetSoundFormat
 * 	Set samplerate, sample format and number of channels
 *************************************************************************
 */
static int
AlsaSound_SetSoundFormat(SoundDevice * sdev, SoundFormat * fmt)
{
	AlsaSound *asdev = sdev->owner;
	int dir;
	int rc;
	int periods;
	int periodsize;
	switch (fmt->sg_snd_format) {
	    case SG_SND_PCM_FORMAT_S16_LE:
		    asdev->alsa_sound_fmt = SND_PCM_FORMAT_S16_LE;
		    asdev->bytes_per_frame = fmt->channels * 2;
		    break;

	    case SG_SND_PCM_FORMAT_U16_LE:
		    asdev->alsa_sound_fmt = SND_PCM_FORMAT_U16_LE;
		    asdev->bytes_per_frame = fmt->channels * 2;
		    break;

	    case SG_SND_PCM_FORMAT_S8:
		    asdev->alsa_sound_fmt = SND_PCM_FORMAT_S8;
		    asdev->bytes_per_frame = fmt->channels;
		    break;

	    case SG_SND_PCM_FORMAT_U8:
		    asdev->alsa_sound_fmt = SND_PCM_FORMAT_U8;
		    asdev->bytes_per_frame = fmt->channels;
		    break;

	    default:
		    fprintf(stderr, "Unknown sound format %d\n", fmt->sg_snd_format);
		    return -1;
	}
	snd_pcm_hw_params_alloca(&asdev->hwpar);
	/* Fill in default values */
	snd_pcm_hw_params_any(asdev->outHandle, asdev->hwpar);

	/* Interleaved mode */
	snd_pcm_hw_params_set_access(asdev->outHandle, asdev->hwpar, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(asdev->outHandle, asdev->hwpar, asdev->alsa_sound_fmt);
	rc = snd_pcm_hw_params_set_channels(asdev->outHandle, asdev->hwpar, fmt->channels);
	asdev->samplerate = fmt->samplerate;
	rc = snd_pcm_hw_params_set_rate_near(asdev->outHandle, asdev->hwpar, &asdev->samplerate, &dir);
	periods = 4;
	if (snd_pcm_hw_params_set_periods(asdev->outHandle, asdev->hwpar, periods, 0) < 0) {
		fprintf(stderr, "Error setting periods.\n");
		exit(1);
	}
	periodsize = 512;
#if 1
	/* latency = periodsize * periods / (rate * bytes_per_frame)     */
	if (snd_pcm_hw_params_set_buffer_size
	    (asdev->outHandle, asdev->hwpar, (periodsize * periods) / asdev->bytes_per_frame)) {
		fprintf(stderr, "Error setting buffersize.\n");
		return (-1);
	}
#endif
	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(asdev->outHandle, asdev->hwpar);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		exit(1);
	}
	{
		snd_pcm_uframes_t frames;
		int dir;
		snd_pcm_hw_params_get_period_size(asdev->hwpar, &frames, &dir);
		fprintf(stderr, "frames per period is %lu\n", frames);
	}

	/* Reset the buffers */
	asdev->outBuffer_rp = asdev->outBuffer_wp = 0;

	snd_pcm_sw_params_alloca(&asdev->swpar);
	snd_pcm_sw_params_current(asdev->outHandle, asdev->swpar);
	rc = snd_pcm_sw_params_set_avail_min(asdev->outHandle, asdev->swpar, 512);
	snd_pcm_sw_params_set_start_threshold(asdev->outHandle, asdev->swpar, 2048);
	snd_pcm_sw_params(asdev->outHandle, asdev->swpar);
	return 0;
}

/*
 ****************************************************************
 * write_samples
 * 	The thread writing samples to the sound device
 ****************************************************************
 */

#define MAX_FRAMES 1024
static void *
write_samples(void *clientData)
{
	AlsaSound *asdev = (AlsaSound *) clientData;
	int rc;
	int frames;
	unsigned int rp;
	unsigned int maxframes;
	while (1) {
		usleep(30000);
		while (BUF_CNT(asdev) > 256) {
			rp = BUF_RP(asdev);
			frames = BUF_CNT(asdev) / asdev->bytes_per_frame;
			maxframes = (asdev->outBufferSize - rp) / asdev->bytes_per_frame;
			if (frames > maxframes) {
				frames = maxframes;
			}
			if (frames > MAX_FRAMES) {
				frames = MAX_FRAMES;
			}
			rc = snd_pcm_writei(asdev->outHandle, asdev->outBuffer + rp, frames);
			dbgprintf("%05lu: wp %d - rp %d  frames: %d\n", BUF_CNT(asdev), wp, rp,
				  frames);
			if (rc == -EPIPE) {
				snd_pcm_prepare(asdev->outHandle);
				asdev->outBuffer_rp = asdev->outBuffer_wp = 0;
				fprintf(stderr, "Alsasound EPIPE\n");
			} else if (rc < 0) {
				fprintf(stderr, "error from writei: %s\n", snd_strerror(rc));
				asdev->outBuffer_rp = asdev->outBuffer_wp = 0;
			} else {
				asdev->outBuffer_rp =
				    asdev->outBuffer_rp + frames * asdev->bytes_per_frame;
			}
		}
	}
	return NULL;
}

static void
alsa_check_buffer_fill(void *clientData)
{
	uint32_t count;
	AlsaSound *asdev = (AlsaSound *) clientData;
	SoundDevice *sdev = &asdev->sdev;
	count = BUF_CNT(asdev);
	if ((count > (3 * BUFFERSIZE / 4)) && !asdev->speed_down) {
		asdev->speed_down = 1;
		SigNode_Set(sdev->speedDown, SIG_HIGH);
		dbgprintf("Speed down\n");
	} else if ((count < (BUFFERSIZE / 2)) && asdev->speed_down) {
		SigNode_Set(sdev->speedDown, SIG_LOW);
		dbgprintf("Speed Ok\n");
		asdev->speed_down = 0;
	} else if ((count > (BUFFERSIZE / 2)) && asdev->speed_up) {
		SigNode_Set(sdev->speedUp, SIG_LOW);
		dbgprintf("Speed Ok\n");
		asdev->speed_up = 0;
	} else if ((count < (BUFFERSIZE / 4)) && !asdev->speed_up) {
		asdev->speed_up = 1;
		SigNode_Set(sdev->speedUp, SIG_HIGH);
		dbgprintf("Speed up\n");
	}
	if (count > (BUFFERSIZE / 4)) {
		pthread_mutex_unlock(&asdev->write_mutex);
	}
	CycleTimer_Mod(&asdev->buffer_check_timer, CycleTimerRate_Get() >> 1);
}

/*
 *******************************************************************************
 * PlaySamples
 *	Take the samples and put them to the circular buffer for the
 *	thread witch sends them to the sound device
 *******************************************************************************
 */
static int
AlsaSound_PlaySamples(SoundDevice * sdev, void *data, uint32_t len)
{
	AlsaSound *asdev = sdev->owner;
	uint32_t count;
	int i;
	count = asdev->outBuffer_wp - asdev->outBuffer_rp;
	if ((count + len) >= sizeof(asdev->outBuffer)) {
		return len;
	}
	for (i = 0; i < len; i++) {
		asdev->outBuffer[BUF_WP(asdev)] = ((uint8_t *) data)[i];
		asdev->outBuffer_wp = asdev->outBuffer_wp + 1;
	}
	return len;
}

/*
 *************************************************************************
 * AlsaSound_New
 * 	Constructor for the ALSA implementation of the abstract 
 * 	base class "SoundDevice" 
 *
 **************************************************************************
 */
SoundDevice *
AlsaSound_New(const char *name)
{
	int rc;
	AlsaSound *asdev = sg_new(AlsaSound);
	SoundDevice *sdev = &asdev->sdev;
	sdev->setSoundFormat = AlsaSound_SetSoundFormat;
	sdev->playSamples = AlsaSound_PlaySamples;
	sdev->owner = asdev;
	asdev->outBufferSize = sizeof(asdev->outBuffer);
	asdev->outFh_active = 0;
	/* Open PCM device for playback. */
	rc = snd_pcm_open(&asdev->outHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) {
		fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
		sleep(1);
		sg_free(asdev);
		return NULL;
	}
	pthread_mutex_init(&asdev->write_mutex, NULL);

	pthread_create(&asdev->write_thread, NULL, write_samples, (void *)asdev);
	CycleTimer_Init(&asdev->buffer_check_timer, alsa_check_buffer_fill, asdev);
	CycleTimer_Mod(&asdev->buffer_check_timer, CycleTimerRate_Get() >> 1);
	sdev->speedUp = SigNode_New("%s.speedUp", name);
	sdev->speedDown = SigNode_New("%s.speedDown", name);
	if (!sdev->speedUp || !sdev->speedDown) {
		fprintf(stderr, "Can not create sound speed control lines\n");
		exit(1);
	}
	fprintf(stderr, "Created ALSA sound device \"%s\"\n", name);

	return sdev;
}
#else				/* ifdef __linux */

#include "nullsound.h"
SoundDevice *
AlsaSound_New(const char *name)
{
	return NullSound_New(name);;
}

#endif
