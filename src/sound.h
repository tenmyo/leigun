/*
 **********************************************************************************
 * SoundDevice: Abstract base class definition 
 * used by sound backends 
 *
 * (C) 2009 Jochen Karrer
 *
 * State:
 *      Working 
 **********************************************************************************
 */

#ifndef _SOUND_H_
#define _SOUND_H_
#include <stdint.h>
#include "signode.h"

#define SG_SND_PCM_FORMAT_S16_LE  (1)
#define SG_SND_PCM_FORMAT_U16_LE  (2)
#define SG_SND_PCM_FORMAT_S8	  (3)
#define SG_SND_PCM_FORMAT_U8	  (4)

typedef struct SoundFormat {
	int channels;
	int sg_snd_format;
	uint32_t samplerate;
} SoundFormat;

/*
 *****************************************************************
 * The Abstract base class SoundDevice is implemented by the
 * backends
 *****************************************************************
 */
typedef struct SoundDevice {
	void *owner;
	int (*setSoundFormat)(struct SoundDevice *,SoundFormat *);
        int (*playSamples)(struct SoundDevice *,void *data,uint32_t len);
	SoundFormat soundFormat;
	SigNode *speedUp;
	SigNode *speedDown;
} SoundDevice; 

static inline int 
Sound_PlaySamples(SoundDevice *dev,void *data,uint32_t len)
{
	return dev->playSamples(dev,data,len);
}

static inline int 
Sound_SetFormat(SoundDevice *dev,SoundFormat *sf)
{
	return dev->setSoundFormat(dev,sf);
}

/*
 ******************************************************
 * SoundDevice_New
 *	The interface for the board creator
 ******************************************************
 */
SoundDevice *
SoundDevice_New(const char *name);

/*
 ******************************************************
 * The interface for registering new backends
 ******************************************************
 */
typedef SoundDevice * SoundDevice_NewProc(const char *name);
void Sound_BackendRegister(const char *name,SoundDevice_NewProc *);

#endif
