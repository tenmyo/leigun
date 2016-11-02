/*
 ****************************************************************************************
 * M32C IO Ports
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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
 ****************************************************************************************
 */

#include "bus.h"
#include "pio_m32c.h"
#include "cpu_m32c.h"

#define REG_P0	(0x3e0)
#define REG_P1	(0x3e1)
#define REG_P2	(0x3e4)
#define REG_P3	(0x3e5)
#define REG_P4	(0x3e8)
#define REG_P5	(0x3e9)
#define REG_P6	(0x3c0)
#define REG_P7	(0x3c1)
#define REG_P8	(0x3c4)
#define REG_P9	(0x3c5)
#define REG_P10 (0x3c8)
#define REG_P11	(0x3c9)
#define REG_P12 (0x3cc)
#define REG_P13 (0x3cd)
#define REG_P14	(0x3d0)
#define REG_P15	(0x3d1)

#define REG_PD0		(0x3e2)
#define REG_PD1		(0x3e3)
#define REG_PD2		(0x3e6)
#define REG_PD3		(0x3e7)
#define REG_PD4		(0x3ea)
#define REG_PD5		(0x3eb)
#define REG_PD6		(0x3c2)
#define REG_PD7		(0x3c3)
#define REG_PD8		(0x3c6)
#define REG_PD9		(0x3c7)
#define REG_PD10	(0x3ca)
#define REG_PD11	(0x3cb)
#define REG_PD12	(0x3ce)
#define REG_PD13	(0x3cf)
#define REG_PD14	(0x3d2)
#define REG_PD15	(0x3d3)

#define REG_PUR0	(0x3f0)
#define REG_PUR1	(0x3f1)
#define REG_PUR2	(0x3da)
#define REG_PUR3	(0x3db)
#define REG_PUR4	(0x3dc)
#define REG_PCR		(0x3ff)

/* Function select registers */
#define REG_PS0		(0x3b0)
#define REG_PS1		(0x3b1)
#define REG_PS2		(0x3b4)
#define REG_PS3		(0x3b5)
#define REG_PS4		(0x3b8)
#define REG_PS5		(0x3b9)
#define REG_PS6		(0x3bc)
#define REG_PS7		(0x3bd)
#define REG_PS8		(0x3a0)
#define REG_PS9		(0x3a1)

#define REG_PSL0	(0x3b2)
#define REG_PSL1	(0x3b3)
#define REG_PSL2	(0x3b6)
#define REG_PSL3	(0x3b7)
#define REG_PSL5	(0x3bb)
#define REG_PSL6	(0x3be)
#define REG_PSL7	(0x3bf)
#define REG_PSL9	(0x3a3)

#define	REG_PSC		(0x3af)
#define REG_PSC2	(0x3ac)
#define REG_PSC3	(0x3ad)
#define REG_PSC6	(0x3aa)

#define REG_PSD1	(0x3a7)
#define REG_PSD2	(0x3a8)
#define REG_PSE1	(0x3ab)

#include "bus.h"
#include "signode.h"
#include "sgstring.h"

typedef struct M32C_Pio M32C_Pio; 

typedef struct Port {
	M32C_Pio *pio;
	int port_nr;
	SigNode  *ioPin;
	SigTrace *ioPinTrace;
	SigNode  *puEn;
	SigTrace *puEnTrace;
	SigNode  *puSel;
	SigTrace *puSelTrace;
	SigNode  *portLatch;
	SigTrace *portLatchTrace;
	SigNode  *portDir;
	SigTrace *portDirTrace;
	SigNode  *pcr;
	SigTrace *pcrTrace;
} Port;

struct M32C_Pio {
	BusDevice bdev;	
	char *name;
	uint16_t addrP[16];
	uint16_t addrPD[16];
	int addrPS[16]; 
	uint8_t regP[16];	
	uint8_t regPD[16];
	SigNode *sigPRC2;
	Port *port[128];
	uint8_t reg_PSL0;
	uint8_t reg_PSL1;
	uint8_t reg_PSL2;
	uint8_t reg_PSL3;
	uint8_t reg_PSL4;
	uint8_t reg_PSL5;
	uint8_t reg_PSL6;
	uint8_t reg_PSL7;
	uint8_t reg_PSL9;
	uint8_t reg_PSC;
};

static void
update_puen(Port *port)
{
	int pusel = SigNode_Val(port->puSel);
	int pdir = SigNode_Val(port->portDir);
	if((pusel == SIG_HIGH) && (pdir == SIG_LOW)) {
		SigNode_Set(port->puEn, SIG_LOW);
	} else {
		SigNode_Set(port->puEn, SIG_HIGH);
	}
}

static void
update_iopin(Port *port)
{
	int pdir = SigNode_Val(port->portDir);
	int platch = SigNode_Val(port->portLatch);
	int puen = SigNode_Val(port->puEn);
	if(pdir == SIG_HIGH) {
		if(platch == SIG_HIGH) {
			SigNode_Set(port->ioPin,SIG_HIGH);
		} else {
			//fprintf(stderr,"Set low %s\n",SigName(port->ioPin));
			SigNode_Set(port->ioPin,SIG_LOW);
		}
	} else {
		if(puen == SIG_HIGH) {
			//fprintf(stderr,"Set pullup %s\n",SigName(port->ioPin));
			SigNode_Set(port->ioPin,SIG_WEAK_PULLUP);
		} else {
			SigNode_Set(port->ioPin,SIG_OPEN);
		}
	}
}

static void 
iopin_trace(SigNode *node,int value,void *clientData) 
{
//	Port *port = clientData;
//	update_puen(port);
}

static void 
puen_trace(SigNode *node,int value,void *clientData) 
{
	Port *port = clientData;
	update_iopin(port);
}

static void 
pusel_trace(SigNode *node,int value,void *clientData) 
{
	Port *port = clientData;
	update_puen(port);
}

static void 
port_latch_trace(SigNode *node,int value,void *clientData) 
{
	Port *port = clientData;
	update_iopin(port);
}

static void 
port_dir_trace(SigNode *node,int value,void *clientData) 
{
	Port *port = clientData;
	update_puen(port);
	update_iopin(port);
}

static void 
pcr_trace(SigNode *node,int value,void *clientData) 
{
#if 0
	Port *port = clientData;
	update_puen(port);
#endif
}

#if 0
static uint32_t
debug_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"0x%04x At %06x\n",address,M32C_REG_PC);
        return 0;
}

static void
debug_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"0x%04x At %06x\n",address,M32C_REG_PC);
}
#endif

static uint32_t
port_read(void *clientData,uint32_t address,int rqlen)
{
	Port *firstport = clientData;
	M32C_Pio *pio = firstport->pio;
	int port_base = firstport->port_nr;
	int i;
	uint8_t value;
	int idx = port_base >> 3;
	value = 0;
	for(i = 0;i < 8;i++) {
		Port *port = pio->port[port_base + i];
		if(pio->regPD[idx] & (1 << i)) {
			if(pio->regP[idx] & (1 << i)) {
				value |= (1 << i);
			}
		} else {
			if(SigNode_Val(port->ioPin) == SIG_HIGH) {
				value |= (1 << i);
			}
		}
	}
#if 0
	if((idx == 6) || (idx == 7)) {
		fprintf(stderr,"Read Port%d: %02x\n",idx,value);
	}	
#endif
	//value = value & port->regPD[i]
	return value;
}

static void
port_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Port *firstport = clientData;
	M32C_Pio *pio = firstport->pio;
	int port_base = firstport->port_nr;
	int idx = port_base >> 3;
	int i;
	pio->regP[idx] = value;	
	for(i=0;i<8;i++) {
		Port *port = pio->port[i+port_base];
		if(value & (1 << i)) {
			SigNode_Set(port->portLatch,SIG_HIGH);
		} else {
			SigNode_Set(port->portLatch,SIG_LOW);
		}	
	}
}

static uint32_t
port_dir_read(void *clientData,uint32_t address,int rqlen)
{
	Port *firstport = clientData;
	M32C_Pio *pio = firstport->pio;
	int port_base = firstport->port_nr;
	int idx = port_base >> 3;
	return pio->regPD[idx];

}

static void
port_dir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Port *firstport = clientData;
	M32C_Pio *pio = firstport->pio;
	int port_base = firstport->port_nr;
	int idx = port_base >> 3;
	int i;
	if(idx == 9) {
		if(SigNode_Val(pio->sigPRC2)  == SIG_LOW) {
			fprintf(stderr,"Write to PD%u disabled by PRC2\n",idx);
			return;
		}
	}
	pio->regPD[idx] = value;	
	for(i=0;i<8;i++) {
		Port *port = pio->port[i+port_base];
		if(value & (1 << i)) {
			SigNode_Set(port->portDir,SIG_HIGH);
		} else {
			SigNode_Set(port->portDir,SIG_LOW);
		}	
	}
#if 0
	if((idx == 6) || (idx == 7)) {
		fprintf(stderr,"Write Dir: %d: %02x\n",idx,value);
	}	
#endif
}

static uint32_t
ps_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ps_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Port *port = clientData;
	M32C_Pio *pio = port->pio;
	int ps_nr = port->port_nr >> 3;	
	if(ps_nr == 9) {
		if(SigNode_Val(pio->sigPRC2) == SIG_LOW) {
			fprintf(stderr,"Write to PS3 disabled by PRC2\n");
		} else {
			//fprintf(stderr,"Write to PS3 not implented\n");
		}
	}
}

static uint32_t
psl0_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
psl0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	static int warn = 0;
	if(!warn) {
		warn++;
		fprintf(stderr,"PSL register 0x%03x not implemented\n",address);
	}
}

static uint32_t
psl1_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
psl1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	static int warn = 0;
	if(!warn) {
		warn++;
		fprintf(stderr,"PSL register 0x%03x not implemented\n",address);
	}
}
static uint32_t
psl2_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
psl2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	static int warn = 0;
	if(!warn) {
		warn++;
		fprintf(stderr,"PSL register 0x%03x not implemented\n",address);
	}
}
static uint32_t
psl3_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
psl3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	static int warn = 0;
	if(!warn) {
		warn++;
		fprintf(stderr,"PSL register 0x%03x not implemented\n",address);
	}
}
static uint32_t
psl5_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
psl5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	static int warn = 0;
	if(!warn) {
		warn++;
		fprintf(stderr,"PSL register 0x%03x not implemented\n",address);
	}
}
static uint32_t
psl6_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
psl6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	static int warn = 0;
	if(!warn) {
		warn++;
		fprintf(stderr,"PSL register 0x%03x not implemented\n",address);
	}
}
static uint32_t
psl7_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
psl7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	static int warn = 0;
	if(!warn) {
		warn++;
		fprintf(stderr,"PSL register 0x%03x not implemented\n",address);
	}
}
static uint32_t
psl9_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
psl9_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	static int warn = 0;
	if(!warn) {
		warn++;
		fprintf(stderr,"PSL register 0x%03x not implemented\n",address);
	}
}

static uint32_t
psc_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Pio *pio = clientData;
	return pio->reg_PSC;
}

static void
psc_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Pio *pio = clientData;
	pio->reg_PSC = value;
//	fprintf(stderr,"PSC %02x\n",value);
}
static void
M32CPio_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	int i;
	M32C_Pio *pio = (M32C_Pio *)owner;
	for(i=0;i<16;i++) {
		IOH_Delete8(pio->addrP[i]);	
		IOH_Delete8(pio->addrPD[i]);	
		if(pio->addrPS[i] >= 0) {
			IOH_Delete8(pio->addrPS[i]);	
		}
	}
	IOH_Delete8(REG_PSL0);
	IOH_Delete8(REG_PSL1);
	IOH_Delete8(REG_PSL2);
	IOH_Delete8(REG_PSL3);
	IOH_Delete8(REG_PSL5);
	IOH_Delete8(REG_PSL6);
	IOH_Delete8(REG_PSL7);
	IOH_Delete8(REG_PSL9);
	IOH_Delete8(REG_PSC);
}

static void
M32CPio_Map(void *owner,uint32_t _base,uint32_t mask,uint32_t mapflags)
{
	int i;
	M32C_Pio *pio = (M32C_Pio *)owner;
	for(i=0;i<16;i++) {
		Port *port = pio->port[i << 3];
		IOH_New8(pio->addrP[i],port_read,port_write,port);	
		IOH_New8(pio->addrPD[i],port_dir_read,port_dir_write,port);	
		if(pio->addrPS[i] >= 0) {
			IOH_New8(pio->addrPS[i],ps_read,ps_write,port);	
		}
	}
	IOH_New8(REG_PSL0,psl0_read,psl0_write,pio);
	IOH_New8(REG_PSL1,psl1_read,psl1_write,pio);
	IOH_New8(REG_PSL2,psl2_read,psl2_write,pio);
	IOH_New8(REG_PSL3,psl3_read,psl3_write,pio);
	IOH_New8(REG_PSL5,psl5_read,psl5_write,pio);
	IOH_New8(REG_PSL6,psl6_read,psl6_write,pio);
	IOH_New8(REG_PSL7,psl7_read,psl7_write,pio);
	IOH_New8(REG_PSL9,psl9_read,psl9_write,pio);
	IOH_New8(REG_PSC,psc_read,psc_write,pio);
}

static void
M32C_PortsInit(M32C_Pio *pio) 
{
	int i,j;
	for(i=0;i<128;i++) {
		Port *port;
		port = pio->port[i] = sg_new(Port);
		j = i & 7;
		port->port_nr = i;	
		port->pio = pio;
		port->ioPin = SigNode_New("%s.P%d.%d",pio->name,i >> 3,j);
		port->puEn = SigNode_New("%s.PuEn%d.%d",pio->name,i >> 3,j);
		port->puSel = SigNode_New("%s.PuSel%d.%d",pio->name,i >> 3,j);
		port->portLatch = SigNode_New("%s.PortLatch%d.%d",pio->name,i >> 3,j);
		port->portDir = SigNode_New("%s.PortDir%d.%d",pio->name,i >> 3,j);
		port->pcr = SigNode_New("%s.Pcr%d.%d",pio->name,i >> 3,j);
		if(!port->ioPin || !port->puEn || !port->puSel || 
		   !port->portLatch || !port->portDir || !port->pcr) {
			fprintf(stderr,"Can not create signal line\n");
			exit(1);
		}
		port->ioPinTrace = SigNode_Trace(port->ioPin,iopin_trace,port);
		port->puEnTrace = SigNode_Trace(port->puEn,puen_trace,port);
		port->puSelTrace = SigNode_Trace(port->puSel,pusel_trace,port);
		port->portLatchTrace = SigNode_Trace(port->portLatch,port_latch_trace,port);
		port->portDirTrace = SigNode_Trace(port->portDir,port_dir_trace,port);
		port->pcrTrace = SigNode_Trace(port->pcr,pcr_trace,port);
	}
	pio->addrP[0] = REG_P0;
	pio->addrP[1] = REG_P1;
	pio->addrP[2] = REG_P2;
	pio->addrP[3] = REG_P3;
	pio->addrP[4] = REG_P4;
	pio->addrP[5] = REG_P5;
	pio->addrP[6] = REG_P6;
	pio->addrP[7] = REG_P7;
	pio->addrP[8] = REG_P8;
	pio->addrP[9] = REG_P9;
	pio->addrP[10] = REG_P10;
	pio->addrP[11] = REG_P11;
	pio->addrP[12] = REG_P12;
	pio->addrP[13] = REG_P13;
	pio->addrP[14] = REG_P14;
	pio->addrP[15] = REG_P15;

	pio->addrPD[0] = REG_PD0;
	pio->addrPD[1] = REG_PD1;
	pio->addrPD[2] = REG_PD2;
	pio->addrPD[3] = REG_PD3;
	pio->addrPD[4] = REG_PD4;
	pio->addrPD[5] = REG_PD5;
	pio->addrPD[6] = REG_PD6;
	pio->addrPD[7] = REG_PD7;
	pio->addrPD[8] = REG_PD8;
	pio->addrPD[9] = REG_PD9;
	pio->addrPD[10] = REG_PD10;
	pio->addrPD[11] = REG_PD11;
	pio->addrPD[12] = REG_PD12;
	pio->addrPD[13] = REG_PD13;
	pio->addrPD[14] = REG_PD14;
	pio->addrPD[15] = REG_PD15;

	pio->addrPS[0] = -1; 
	pio->addrPS[1] = -1; 
	pio->addrPS[2] = -1; 
	pio->addrPS[3] = -1; 
	pio->addrPS[4] = -1; 
	pio->addrPS[5] = -1; 
	pio->addrPS[6] = REG_PS0;
	pio->addrPS[7] = REG_PS1;
	pio->addrPS[8] = REG_PS2;
	pio->addrPS[9] = REG_PS3;
	pio->addrPS[10] = REG_PS4;
	pio->addrPS[11] = REG_PS5;
	pio->addrPS[12] = REG_PS6;
	pio->addrPS[13] = REG_PS7;
	pio->addrPS[14] = REG_PS8;
	pio->addrPS[15] = REG_PS9;
}

BusDevice *
M32C_PioNew(const char *name)
{
	M32C_Pio *pio = sg_new(M32C_Pio);
	pio->bdev.first_mapping = NULL;
	pio->bdev.Map = M32CPio_Map;
	pio->bdev.UnMap = M32CPio_Unmap;
	pio->bdev.owner = pio;
	pio->bdev.hw_flags = MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	pio->name = sg_strdup(name);
	pio->sigPRC2 = SigNode_New("%s.prc2",pio->name);
	if(pio->sigPRC2 == NULL) {
		fprintf(stderr,"Can not create sig PRC2\n");
		exit(1);
	}
	M32C_PortsInit(pio);
	return &pio->bdev;
}
