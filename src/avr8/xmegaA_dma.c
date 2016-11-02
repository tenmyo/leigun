/*
 *************************************************************************************************
 *
 * Emulation of ATXMega A DMA controller 
 *
 * State: nothing implemented
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
#include <stdio.h>
#include <string.h>
#include "sgstring.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "fio.h"
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "xmegaA_dma.h"

/* Base is 0x100 in xmega32a4 */
#define DMA_CTRL(base)		((base) + 0x00)
#define 	CTRL_DMA_ENABLE		0x80  
#define 	CTRL_DMA_RESET		0x40  
#define 	CTRL_DMA_DBUFMODE_MSK  	0x0C  
#define 	CTRL_DMA_PRIMODE_MSK	0x03 

#define DMA_INTFLAGS(base) 	((base) + 0x03)
#define 	INTFLAGS_CH3ERRIF	0x80  
#define 	INTFLAGS_CH2ERRIF	0x40
#define 	INTFLAGS_CH1ERRIF	0x20 
#define 	INTFLAGS_CH0ERRIF	0x10
#define		INTFLAGS_CH3TRNIF	0x08 
#define 	INTFLAGS_CH2TRNIF	0x04
#define 	INTFLAGS_CH1TRNIF	0x02 
#define 	INTFLAGS_CH0TRNIF	0x01 

#define DMA_STATUS(base)  	((base) + 0x04)
#define 	STATUS_CH3BUSY_bm  0x80
#define 	STATUS_CH2BUSY_bm  0x40
#define 	STATUS_CH1BUSY_bm  0x20
#define 	STATUS_CH0BUSY_bm  0x10
#define 	STATUS_CH3PEND_bm  0x08
#define 	STATUS_CH2PEND_bm  0x04
#define 	STATUS_CH1PEND_bm  0x02
#define 	STATUS_CH0PEND_bm  0x01

#define DMA_TEMPL(base)  	((base) + 0x06)
#define DMA_TEMPH(base)  	((base) + 0x07)

#define DMA_CH_CTRLA(base,ch)  		((base) + (ch) * 0x10 + 0x10)
#define DMA_CH_CTRLB(base,ch)  		((base) + (ch) * 0x10 + 0x11)
#define DMA_CH_ADDRCTRL(base,ch)  	((base) + (ch) * 0x10 + 0x12)
#define DMA_CH_TRIGSRC(base,ch)  	((base) + (ch) * 0x10 + 0x13)
#define DMA_CH_TRFCNTL(base,ch)  	((base) + (ch) * 0x10 + 0x14)
#define DMA_CH_TRFCNTH(base,ch)  	((base) + (ch) * 0x10 + 0x15)
#define DMA_CH_REPCNT(base,ch)  	((base) + (ch) * 0x10 + 0x16)
#define DMA_CH_SRCADDR0(base,ch)  	((base) + (ch) * 0x10 + 0x18)
#define DMA_CH_SRCADDR1(base,ch)  	((base) + (ch) * 0x10 + 0x19)
#define DMA_CH_SRCADDR2(base,ch)  	((base) + (ch) * 0x10 + 0x1a)
#define DMA_CH_DSTADDR0(base,ch)  	((base) + (ch) * 0x10 + 0x1c)
#define DMA_CH_DSTADDR1(base,ch)  	((base) + (ch) * 0x10 + 0x1d)
#define DMA_CH_DSTADDR2(base,ch)  	((base) + (ch) * 0x10 + 0x1e)

typedef struct XMegaDma XMegaDma; 
typedef struct DmaChannel {
	XMegaDma *dma;
	uint8_t reg_CtrlA;
	uint8_t reg_CtrlB;
	uint8_t reg_AddrCtrl;
	uint8_t reg_TrigSrc;
	uint8_t reg_TrfCntL;
	uint8_t reg_TrfCntH;
	uint8_t reg_RepCnt;	
	uint8_t reg_SrcAddr0;
	uint8_t reg_SrcAddr1;
	uint8_t reg_SrcAddr2;
	uint8_t reg_DstAddr0;
	uint8_t reg_DstAddr1;
	uint8_t reg_DstAddr2;
} DmaChannel;

struct XMegaDma {
	uint8_t reg_DmaCtrl;
	uint8_t reg_IntFlags;
	uint8_t reg_Status;
	uint8_t reg_TempL;
	uint8_t reg_TempH;
	DmaChannel dmaCh[4];
};

static uint8_t
dma_ctrl_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
dma_ctrl_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t
dma_intflags_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
dma_intflags_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t
dma_status_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
dma_status_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t
dma_templ_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
dma_templ_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t
dma_temph_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
dma_temph_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t 
ch_ctrla_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_ctrla_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t 
ch_ctrlb_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_ctrlb_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t 
ch_addrctrl_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_addrctrl_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t 
ch_trigsrc_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_trigsrc_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t 
ch_trfcntl_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_trfcntl_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t 
ch_trfcnth_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_trfcnth_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t 
ch_repcnt_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_repcnt_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t 
ch_srcaddr0_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_srcaddr0_write(void *clientData,uint8_t value,uint32_t address)
{

}
static uint8_t 
ch_srcaddr1_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_srcaddr1_write(void *clientData,uint8_t value,uint32_t address)
{

}
static uint8_t 
ch_srcaddr2_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_srcaddr2_write(void *clientData,uint8_t value,uint32_t address)
{

}

static uint8_t 
ch_dstaddr0_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_dstaddr0_write(void *clientData,uint8_t value,uint32_t address)
{

}
static uint8_t 
ch_dstaddr1_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_dstaddr1_write(void *clientData,uint8_t value,uint32_t address)
{

}
static uint8_t 
ch_dstaddr2_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ch_dstaddr2_write(void *clientData,uint8_t value,uint32_t address)
{

}

void
XMegaA_DmaNew(const char *name,uint32_t base)
{
        XMegaDma *dma = sg_new(XMegaDma);
	int i;
        AVR8_RegisterIOHandler(DMA_CTRL(base),dma_ctrl_read,dma_ctrl_write,dma);
        AVR8_RegisterIOHandler(DMA_INTFLAGS(base),dma_intflags_read,dma_intflags_write,dma);
        AVR8_RegisterIOHandler(DMA_STATUS(base),dma_status_read,dma_status_write,dma);
        AVR8_RegisterIOHandler(DMA_TEMPL(base),dma_templ_read,dma_templ_write,dma);
        AVR8_RegisterIOHandler(DMA_TEMPH(base),dma_temph_read,dma_temph_write,dma);
	for(i = 0; i < 4; i++) {
		DmaChannel *ch = &dma->dmaCh[i];
		ch->dma = dma;
		AVR8_RegisterIOHandler(DMA_CH_CTRLA(base,i),ch_ctrla_read,ch_ctrla_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_CTRLB(base,i),ch_ctrlb_read,ch_ctrlb_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_ADDRCTRL(base,i),ch_addrctrl_read,ch_addrctrl_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_TRIGSRC(base,i),ch_trigsrc_read,ch_trigsrc_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_TRFCNTL(base,i),ch_trfcntl_read,ch_trfcntl_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_TRFCNTH(base,i),ch_trfcnth_read,ch_trfcnth_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_REPCNT(base,i),ch_repcnt_read,ch_repcnt_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_SRCADDR0(base,i),ch_srcaddr0_read,ch_srcaddr0_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_SRCADDR1(base,i),ch_srcaddr1_read,ch_srcaddr1_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_SRCADDR2(base,i),ch_srcaddr2_read,ch_srcaddr2_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_DSTADDR0(base,i),ch_dstaddr0_read,ch_dstaddr0_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_DSTADDR1(base,i),ch_dstaddr1_read,ch_dstaddr1_write,ch);
		AVR8_RegisterIOHandler(DMA_CH_DSTADDR2(base,i),ch_dstaddr2_read,ch_dstaddr2_write,ch);
	}	
        fprintf(stderr,"Created ATMegaA4 DMA Controller \"%s\"\n",name);
}
