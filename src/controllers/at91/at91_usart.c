/*
 **************************************************************************************************
 * Emulation of AT91RM9200 USART
 *
 * state: minimal implementation, working 
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "fio.h"
#include "signode.h"
#include "serial.h"
#include "configfile.h"
#include "at91_usart.h"
#include "clock.h"
#include "senseless.h"
#include "sgstring.h"

#define US_CR(base) 	((base)+0)
#define		CR_RSTRX	(1<<2)
#define		CR_RSTTX	(1<<3)
#define		CR_RXEN		(1<<4)
#define		CR_RXDIS	(1<<5)
#define		CR_TXEN		(1<<6)
#define		CR_TXDIS	(1<<7)
#define		CR_RSTSTA	(1<<8)
#define		CR_STTBRK	(1<<9)
#define		CR_STPBRK	(1<<10)
#define		CR_STTTO	(1<<11)
#define		CR_SENDA	(1<<12)
#define		CR_RSTIT	(1<<13)
#define		CR_RSTNACK	(1<<14)
#define		CR_RETTO	(1<<15)
#define		CR_DTREN	(1<<16)
#define		CR_DTRDIS	(1<<17)
#define		CR_RTSEN	(1<<18)
#define		CR_RTSDIS	(1<<19)
#define US_MR(base) 	((base)+0x4)
#define		MR_FILTER			(1<<28)
#define		MR_MAX_ITERATION_MASK		(7<<24)
#define		MR_MAX_ITERATION_SHIFT		(24)
#define		MR_DSNACK			(1<<21)	
#define		MR_INACK			(1<<20)
#define		MR_OVER				(1<<19)
#define		MR_CLKO				(1<<18)
#define		MR_MODE9			(1<<17)
#define		MR_MSBF				(1<<16)
#define		MR_CHMODE_MASK			(3<<14)
#define		MR_CHMODE_SHIFT			(14)
#define		MR_NBSTOP_MASK			(3<<12)
#define		MR_NBSTOP_SHIFT			(12)
#define		MR_PAR_MASK			(7<<9)
#define		MR_PAR_SHIFT			(9)
#define		MR_SYNC				(1<<8)
#define		MR_CHRL_MASK			(3<<6)
#define		MR_CHRL_SHIFT			(6)
#define		MR_USCLKS_MASK			(3<<4)
#define		MR_USCLKS_SHIFT			(4)
#define		   USCLKS_MCK		(0)
#define		   USCLKS_MCK_DIV	(1<<4)
#define		   USCLKS_SCK		(3<<4)
#define		MR_USART_MODE_MASK		(0xf<<0)
#define		MR_USART_MODE_SHIFT		(0)
#define	 	   USART_MODE_NORMAL		(0)	
#define		   USART_MODE_RS485		(1)
#define		   USART_MODE_HWHS		(2)
#define		   USART_MODE_MODEM		(3)
#define		   USART_MODE_ISO7816_T0	(4)
#define		   USART_MODE_ISO7816_T1	(6)
#define		   USART_MODE_IRDA		(8)

#define US_IER(base) 	((base)+0x8)
#define US_IDR(base) 	((base)+0xc)
#define US_IMR(base)	((base)+0x10)
#define US_CSR(base)	((base)+0x14)
#define		CSR_RXRDY	(1<<0)
#define		CSR_TXRDY	(1<<1)
#define		CSR_RXBRK	(1<<2)
#define		CSR_ENDRX	(1<<3)
#define		CSR_ENDTX	(1<<4)
#define		CSR_OVRE	(1<<5)
#define		CSR_FRAME	(1<<6)
#define		CSR_PARE	(1<<7)
#define		CSR_TIMEOUT	(1<<8)
#define		CSR_TXEMPTY	(1<<9)
#define		CSR_ITERATION	(1<<10)
#define		CSR_TXBUFE	(1<<11)
#define		CSR_RXBUFF	(1<<12)
#define		CSR_NACK	(1<<13)

#define US_RHR(base)	((base)+0x18)
#define US_THR(base)	((base)+0x1c)
#define US_BRGR(base)	((base)+0x20)
#define US_RTOR(base)	((base)+0x24)
#define US_TTGR(base)	((base)+0x28)
#define US_FIDI(base)	((base)+0x40)
#define US_NER(base)	((base)+0x44)
#define	US_IF(base)	((base)+0x4c)

typedef struct AT91Usart {
	BusDevice bdev;
	const char *name;
	UartPort *port;
	SigNode *irqNode;
	Clock_t *mck;
	Clock_t *mck_div;
	Clock_t *sck;
	Clock_t *sel_clk;
	Clock_t *div_clk;
	Clock_t *samp_clk;
	Clock_t *br_clk;
	/* Interface to pdc */
	SigNode *sigRxReady;
	SigNode *sigTxReady;
	
	uint32_t cr;
	uint32_t mr;
	uint32_t imr;
	uint32_t csr;
	uint32_t rhr;
	uint32_t thr;
	uint32_t tx_shiftreg;
	uint32_t brgr;
	uint32_t rtor;
	uint32_t ttgr;
	uint32_t fidi;
	uint32_t ner;
	uint32_t ifr;
} AT91Usart;

static void
update_interrupt(AT91Usart *usart) 
{
	if(usart->csr & usart->imr) {
		SigNode_Set(usart->irqNode,SIG_PULLDOWN); // workaround for Interrupt controller bug
		SigNode_Set(usart->irqNode,SIG_HIGH);
	} else {
		SigNode_Set(usart->irqNode,SIG_PULLDOWN);
	}
}

static void
update_br_clk(AT91Usart *usart) 
{
	uint32_t fidi = usart->fidi & 0x7ff;
	uint32_t over = (usart->mr & MR_OVER);
	uint32_t sync = (usart->mr & MR_SYNC); 
	uint32_t mode = (usart->mr & MR_USART_MODE_MASK);
	uint32_t usclks = usart->mr & MR_USCLKS_MASK;
        UartCmd cmd;
	switch(usclks) {
		case USCLKS_MCK:
			Clock_MakeDerived(usart->sel_clk,usart->mck,1,1);
			break;

		case USCLKS_MCK_DIV:
			Clock_MakeDerived(usart->sel_clk,usart->mck_div,1,1);
			break;

		case USCLKS_SCK:
			Clock_MakeDerived(usart->sel_clk,usart->sck,1,1);
			break;
		default:
			fprintf(stderr,"Illegal clock selection %08x\n",usart->mr);
	}
	switch(mode) {
		case USART_MODE_NORMAL:
		case USART_MODE_RS485:
		case USART_MODE_HWHS:	
		case USART_MODE_MODEM:
		case USART_MODE_IRDA:
			if(sync) {
				Clock_MakeDerived(usart->br_clk,usart->samp_clk,1,1);
			} else if(over) {
				Clock_MakeDerived(usart->br_clk,usart->samp_clk,1,8);
			} else {
				Clock_MakeDerived(usart->br_clk,usart->samp_clk,1,16);
			}
		break;
		case USART_MODE_ISO7816_T0:
		case USART_MODE_ISO7816_T1:
			if(sync) {
				Clock_MakeDerived(usart->br_clk,usart->samp_clk,1,1);
			} else if(fidi) {
				Clock_MakeDerived(usart->br_clk,usart->samp_clk,1,fidi);
			} else {
				Clock_MakeDerived(usart->br_clk,usart->samp_clk,0,1);
			}
			break;
		default:
			break;
	}
	if(sync && (usclks == USCLKS_SCK)) {
		Clock_MakeDerived(usart->samp_clk,usart->sck,1,1);
	} else {
		Clock_MakeDerived(usart->samp_clk,usart->div_clk,1,1);
	}
#if 0
	fprintf(stderr,"Baudrate is now %f\n",Clock_Freq(usart->br_clk));
#endif
        cmd.opcode = UART_OPC_SET_BAUDRATE;
        cmd.arg = Clock_Freq(usart->br_clk);
        SerialDevice_Cmd(usart->port,&cmd);
#if 0
	if(Clock_Find("pmc.main_clk")) {
		Clock_DumpTree(Clock_Find("pmc.main_clk"));
	}
#endif
}
static void
update_receiver(AT91Usart *usart)
{

	if(usart->cr & CR_RXDIS) {
		SerialDevice_StopRx(usart->port);
		usart->csr &= ~(CSR_RXRDY);
	} else if(usart->cr & CR_RXEN) {
		if(!(usart->csr & CSR_RXRDY)) {
			//fprintf(stderr,"Start rx\n");
			SerialDevice_StartRx(usart->port);
		}
	}
}

static void
serial_output(void *cd) {
        AT91Usart *usart=cd;
	int count;
        while(!(usart->csr & CSR_TXEMPTY)) {
		UartChar data = usart->tx_shiftreg;
                count=SerialDevice_Write(usart->port,&data,1);
		if(count == 0) {
        		update_interrupt(usart);
			return;
		}
		if(!(usart->csr & CSR_TXRDY)) {
			usart->tx_shiftreg = usart->thr;
			usart->csr |= CSR_TXRDY;
		} else {
			usart->csr |= CSR_TXEMPTY;
		}
        }
        SerialDevice_StopTx(usart->port);
        update_interrupt(usart);
        return;
}

static void
serial_input(void *cd) 
{
	AT91Usart *usart = cd;
	UartChar b;
	int count=SerialDevice_Read(usart->port,&b,1);
	if(count > 0) {
		usart->rhr = b;
		usart->csr |= CSR_RXRDY;
		//fprintf(stderr,"Stop rx\n");
		SerialDevice_StopRx(usart->port);
		update_interrupt(usart);
	}
	
}

static uint32_t
cr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Usart: read location 0x%08x not implemented\n",address);
        return 0;
}


static void
cr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *)clientData;
	uint32_t oldcr = usart->cr;
	uint32_t diff;
	uint32_t ena = CR_RXEN | CR_TXEN | CR_STTBRK | CR_DTREN | CR_RTSEN;
	uint32_t disa = CR_RXDIS | CR_TXDIS | CR_STPBRK | CR_DTRDIS | CR_RTSDIS;
	uint32_t action_mask = (CR_RSTRX | CR_RSTTX | CR_RSTSTA | CR_STTTO | CR_SENDA
			  | CR_RSTIT | CR_RSTNACK | CR_RETTO);  
	uint32_t action = value & action_mask;

	ena = ena & value;
	disa = disa & value;
	usart->cr = (usart->cr | disa)	& ~(disa>>1); 
	usart->cr = (usart->cr | ena) & ~(ena<<1);
	usart->cr = (usart->cr & ~action_mask) | action; /* don't know if info is required */
	diff = usart->cr ^ oldcr;
//	fprintf(stderr,"AT91Usart: CR write %08x\n",value);
	if(diff & (CR_TXEN | CR_TXDIS)) {
		if(usart->cr & CR_TXDIS) {
        		SerialDevice_StopTx(usart->port);
			usart->csr |= CSR_TXEMPTY | CSR_TXRDY;
		} else if(usart->cr & CR_TXEN) {
			usart->csr |= CSR_TXEMPTY | CSR_TXRDY;
		}
		update_interrupt(usart);
	}
	if(diff & (CR_RXEN | CR_RXDIS)) {
		update_receiver(usart);
		update_interrupt(usart);
	}
}

static uint32_t
mr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
        return 0;
        return usart->mr;
}

static void
mr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
	usart->mr = value & 0x173fffff;
	update_br_clk(usart);
}

static uint32_t
ier_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Usart: IER register is writeonly\n");
        return 0;
}

static void
ier_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart*) clientData;
	usart->imr |= value & 0xf3fff;
	update_interrupt(usart);
}

static uint32_t
idr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Usart: IDR register is writeonly\n");
        return 0;
}

static void
idr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart*) clientData;
	usart->imr &= ~(value & 0xf3fff);
	update_interrupt(usart);
}

static uint32_t
imr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart*) clientData;
        return usart->imr;
}

static void
imr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Usart: Detected write to readonly IMR\n");
}

static uint32_t
csr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
	uint32_t sensefull_high = CSR_RXRDY | usart->imr;
	uint32_t sensefull_low = CSR_TXRDY;
	if(!(usart->csr & sensefull_high) && !(~usart->csr & sensefull_low)) {
		Senseless_Report(100);
	}
        return usart->csr;
}

static void
csr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Usart: CSR is a read only register\n");
}

static uint32_t
rhr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *)clientData;
	uint32_t retval = usart->rhr;
	if(usart->csr & CSR_RXRDY) {
		usart->csr &= ~CSR_RXRDY;
		update_receiver(usart);
		update_interrupt(usart);
	} else {
		Senseless_Report(100);
	}
        return retval;
}

static void
rhr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Usart: write location 0x%08x not implemented\n",address);
}

static uint32_t
thr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Usart: THR is a writeonly register\n");
        return 0;
}

static void
thr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart*) clientData;
	if(!(usart->cr & CR_TXEN) ) {
		fprintf(stderr,"Warning: tx not enabled %s\n",usart->name);
		return;
	}
	if(usart->csr & CSR_TXEMPTY) {
		usart->tx_shiftreg = value; 
		usart->csr &= ~CSR_TXEMPTY;
		SerialDevice_StartTx(usart->port);
		update_interrupt(usart);
	} else if(usart->csr & CSR_TXRDY) {
		usart->thr = value;
		usart->csr &= ~CSR_TXRDY;
		update_interrupt(usart);
	} 
}

static uint32_t
brgr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Usart: read location 0x%08x not implemented\n",address);
        return 0;
}

static void
brgr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
	value = value & 0xffff;
	if(value == 0) {
		Clock_MakeDerived(usart->div_clk,usart->sel_clk,0,1);
	} else {
		Clock_MakeDerived(usart->div_clk,usart->sel_clk,1,value);
	}
}

static uint32_t
rtor_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
        return usart->rtor;
}

static void
rtor_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
	usart->rtor = value & 0xffff;
}

static uint32_t
ttgr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
        return usart->ttgr;
}

static void
ttgr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
	if(value) {
        	fprintf(stderr,"AT91Usart: Transmitter timegard not impelented\n");
	}
	usart->ttgr = value;
}

static uint32_t
fidi_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
        return usart->fidi;
}

static void
fidi_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
	usart->fidi = value & 0x7ff;
	update_br_clk(usart);
}

static uint32_t
ner_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ner_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Usart: NER is a read only register\n");
}

static uint32_t
if_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
        return usart->ifr;
}

static void
if_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Usart *usart = (AT91Usart *) clientData;
	usart->ifr = value & 0xff;
}


static void
AT91Usart_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	AT91Usart *usart = (AT91Usart*) owner;
	IOH_New32(US_CR(base),cr_read,cr_write,usart);
	IOH_New32(US_MR(base),mr_read,mr_write,usart);
	IOH_New32(US_IER(base),ier_read,ier_write,usart);
	IOH_New32(US_IDR(base),idr_read,idr_write,usart);
	IOH_New32(US_IMR(base),imr_read,imr_write,usart);
	IOH_New32(US_CSR(base),csr_read,csr_write,usart);
	IOH_New32(US_RHR(base),rhr_read,rhr_write,usart);
	IOH_New32(US_THR(base),thr_read,thr_write,usart);
	IOH_New32(US_BRGR(base),brgr_read,brgr_write,usart);
	IOH_New32(US_RTOR(base),rtor_read,rtor_write,usart);
	IOH_New32(US_TTGR(base),ttgr_read,ttgr_write,usart);
	IOH_New32(US_FIDI(base),fidi_read,fidi_write,usart);
	IOH_New32(US_NER(base),ner_read,ner_write,usart);
	IOH_New32(US_IF(base),if_read,if_write,usart);
}

static void
AT91Usart_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(US_CR(base));
	IOH_Delete32(US_MR(base));
	IOH_Delete32(US_IER(base));
	IOH_Delete32(US_IDR(base));
	IOH_Delete32(US_IMR(base));
	IOH_Delete32(US_CSR(base));
	IOH_Delete32(US_RHR(base));
	IOH_Delete32(US_THR(base));
	IOH_Delete32(US_BRGR(base));
	IOH_Delete32(US_RTOR(base));
	IOH_Delete32(US_TTGR(base));
	IOH_Delete32(US_FIDI(base));
	IOH_Delete32(US_NER(base));
	IOH_Delete32(US_IF(base));
}

BusDevice *
AT91Usart_New(const char *name)
{
        AT91Usart *usart = sg_new(AT91Usart);
	usart->name = sg_strdup(name);
	usart->mck = Clock_New("%s.mck",name);
	usart->mck_div = Clock_New("%s.mck_div",name);
	usart->sck = Clock_New("%s.sck",name);
	usart->sel_clk = Clock_New("%s.sel_clk",name);
	usart->br_clk = Clock_New("%s.br_clk",name);
	usart->div_clk = Clock_New("%s.div_clk",name);
	usart->samp_clk = Clock_New("%s.samp_clk",name);
	usart->csr = CSR_TXRDY | CSR_TXEMPTY;
	Clock_MakeDerived(usart->mck_div,usart->mck,1,8);
        usart->irqNode = SigNode_New("%s.irq",name);
	usart->sigRxReady = SigNode_New("%s.rxready",name);
	usart->sigTxReady = SigNode_New("%s.txready",name);
        if(!usart->irqNode || !usart->sigRxReady || !usart->sigTxReady) {
                fprintf(stderr,"AT91Usart: Can not create signal lines\n");
		exit(1);
        }
	SigNode_Set(usart->irqNode,SIG_PULLDOWN);
	usart->fidi = 0x174;
        usart->bdev.first_mapping=NULL;
        usart->bdev.Map=AT91Usart_Map;
        usart->bdev.UnMap=AT91Usart_UnMap;
        usart->bdev.owner=usart;
        usart->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        update_interrupt(usart);
        fprintf(stderr,"AT91 Usart \"%s\" created\n",name);
        usart->port = Uart_New(name,serial_input,serial_output,NULL,usart);
        return &usart->bdev;
}

