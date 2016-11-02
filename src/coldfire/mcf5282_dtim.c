/*
 *************************************************************************************************
 *
 * Emulation of Coldfire DMA Timers 
 *
 * State: not implemented
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <bus.h>
#include <fio.h>
#include <signode.h>
#include <configfile.h>
#include <clock.h>
#include <cycletimer.h>
#include <sgstring.h>
#include <mcf5282_dtim.h>

typedef struct DTim {
	BusDevice bdev;	
} DTim;

static void
DTim_Unmap(void *owner,uint32_t base,uint32_t mask)
{
//        IOH_Delete16(CFU_UMR1(base));
}

static void
DTim_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{

}
BusDevice *
MCF5282_DTimNew(const char *name)
{
        DTim *dtim = sg_calloc(sizeof(DTim));
        dtim->bdev.first_mapping=NULL;
        dtim->bdev.Map=DTim_Map;
        dtim->bdev.UnMap=DTim_Unmap;
        dtim->bdev.owner=dtim;
        dtim->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        return &dtim->bdev;
}

