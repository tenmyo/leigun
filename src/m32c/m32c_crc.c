/*
 *************************************************************************************************
 *
 * M32C / M16C CRC generator 
 *
 * State: working
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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

#include <stdint.h>
#include "bus.h"
#include "crc16.h"
#include "sgstring.h"

typedef struct RegSet {
	uint32_t aCrcd;
	uint32_t aCrcIn;
} RegSet;

typedef struct CRCGen {
	BusDevice bdev;
	RegSet *regSet;
	uint16_t regCrcd;
	uint8_t regCrcIn;
} CRCGen;

RegSet crc_regs[] = {
	{
		/* M32C87 */
		.aCrcd = 0x37c,
		.aCrcIn = 0x37e,
	},
	{
		/* M16C65 */
		.aCrcd = 0x3bc,
		.aCrcIn = 0x3be,
	}
};

static uint32_t
crcd_read(void *clientData,uint32_t address,int rqlen)
{
	CRCGen *cg = clientData;	
        return cg->regCrcd;
}

static void
crcd_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	CRCGen *cg = clientData;	
        cg->regCrcd = value;
}

static uint32_t
crcin_read(void *clientData,uint32_t address,int rqlen)
{
	CRCGen *cg = clientData;	
	return cg->regCrcIn;
}

static void
crcin_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	CRCGen *cg = clientData;	
	uint8_t data = value;
        cg->regCrcIn = value;
	cg->regCrcd = CRC16_0x1021Rev(cg->regCrcd,&data,1);	
}

static void
M32C_CRCGenUnmap(void *owner,uint32_t base,uint32_t mask)
{
	CRCGen *cg = owner;
	RegSet *rs = cg->regSet;
	IOH_Delete16(rs->aCrcd);
	IOH_Delete8(rs->aCrcIn);
}

static void
M32C_CRCGenMap(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	CRCGen *cg = owner;
	RegSet *rs = cg->regSet;
	uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_PRD_RARP | IOH_FLG_PA_CBSE | IOH_FLG_PWR_RMW;
	IOH_New16f(rs->aCrcd,crcd_read,crcd_write,cg,flags);
	IOH_New8(rs->aCrcIn,crcin_read,crcin_write,cg);
}

BusDevice *
M32C_CRCNew(const char *name,unsigned int register_set)
{
	CRCGen *cg = sg_new(CRCGen);
	if(register_set >= array_size(crc_regs)) {
		fprintf(stderr,"CRC Generator: illegal register set %d\n",register_set);
		exit(1);
	}
	cg->regSet = &crc_regs[register_set];
        cg->bdev.first_mapping = NULL;
        cg->bdev.Map = M32C_CRCGenMap;
        cg->bdev.UnMap = M32C_CRCGenUnmap;
        cg->bdev.owner = cg;
        cg->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &cg->bdev;
}
