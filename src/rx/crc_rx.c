/*
 **********************************************************************************************
 * Renesas RX62N CRC module  
 *
 * State: working, MSTP not implemented 
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 **********************************************************************************************
 */

#include <stdint.h>
#include "bus.h"
#include "crc16.h"
#include "sgstring.h"
#include "crc_rx.h"
#include "crc8.h"

#define REG_CRCCR(base)		((base) + 0x00)
#define REG_CRCDIR(base)	((base) + 0x01)
#define REG_CRCDOR(base)	((base) + 0x02)

typedef struct CRCMod {
	BusDevice bdev;
	uint8_t regCRCCR;
	uint16_t regCRCDOR;
	//SigNode *sigMstp;
} CRCMod;

static uint32_t
crccr_read(void *clientData,uint32_t address,int rqlen)
{
	CRCMod *cm = clientData;
	return cm->regCRCCR;
}

static void
crccr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	CRCMod *cm = clientData;
	cm->regCRCCR = value & 0x7;
	if(value & 0x80) {
		cm->regCRCDOR = 0;
	}
}

static uint32_t
crcdir_read(void *clientData,uint32_t address,int rqlen)
{
	return 0;
}

static void
crcdir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	CRCMod *cm = clientData;
	uint8_t data = value;
	switch(cm->regCRCCR & 7) {
		case 0:
			break;
		case 1:
			cm->regCRCDOR = Crc8_Poly7Rev(cm->regCRCDOR,&data,1);
			break;
		case 2:
			cm->regCRCDOR = CRC16_0x8005Rev(cm->regCRCDOR,&data,1);
			break;
		case 3:
			cm->regCRCDOR = CRC16_0x1021Rev(cm->regCRCDOR,&data,1);
			break;
		case 4:
			break;
		case 5:
			cm->regCRCDOR = Crc8_Poly7(cm->regCRCDOR,&data,1);
			break;
		case 6:
			cm->regCRCDOR = CRC16_0x8005(cm->regCRCDOR,&data,1);
			break;
		case 7:
			cm->regCRCDOR = CRC16_0x1021(cm->regCRCDOR,&data,1);
			break;
	}
}

static uint32_t
crcdor_read(void *clientData,uint32_t address,int rqlen)
{
	CRCMod *cm = clientData;
	return cm->regCRCDOR;
}

static void
crcdor_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	CRCMod *cm = clientData;
	cm->regCRCDOR = value;
}

static void
CRCMod_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete8(REG_CRCCR(base));
	IOH_Delete8(REG_CRCDIR(base));
	IOH_Delete16(REG_CRCDOR(base));
}

static void
CRCMod_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	CRCMod *cm = owner;
	IOH_New8(REG_CRCCR(base),crccr_read,crccr_write,cm);
	IOH_New8(REG_CRCDIR(base),crcdir_read,crcdir_write,cm);
	IOH_New16(REG_CRCDOR(base),crcdor_read,crcdor_write,cm);
}

BusDevice *
RxCrc_New(const char *name)
{
	CRCMod *cm = sg_new(CRCMod);
        cm->bdev.first_mapping = NULL;
        cm->bdev.Map = CRCMod_Map;
        cm->bdev.UnMap = CRCMod_Unmap;
        cm->bdev.owner = cm;
        cm->bdev.hw_flags = MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        return &cm->bdev;
}
