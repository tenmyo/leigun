/*
 *****************************************************************************************************
 *
 * Dummy sound backend for softgun used when no other sound backend
 * is used. It eats the sound samples and does nothing
 *
 * State: Working 
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


#include <stdio.h>
#include "sound.h"
#include "sgstring.h"
#include "signode.h"

static int 
NullDev_SetSoundFormat(SoundDevice *dev,SoundFormat *format) 
{
	return 0;
}

static int 
NullDev_PlaySamples(SoundDevice *dev,void *data,uint32_t len)
{
	return len;
}

SoundDevice *
NullSound_New(const char *name) 
{
	SoundDevice *sdev = sg_new(SoundDevice);	
	sdev->setSoundFormat = NullDev_SetSoundFormat;
	sdev->playSamples = NullDev_PlaySamples;
	sdev->speedUp = SigNode_New("%s.speed_up",name);
	sdev->speedDown = SigNode_New("%s.speed_down",name);
	if(!sdev->speedUp || ! sdev->speedDown) {
		fprintf(stderr,"Can not create sound speed control lines\n");
		exit(1);
	}
	fprintf(stderr,"Created Null sound device \"%s\" for eating up sound\n",name);
	return sdev;
}
