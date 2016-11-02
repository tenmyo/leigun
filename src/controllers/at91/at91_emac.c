/*
 *************************************************************************************************
 *
 * Emulation of the AT91RM9200 Ethernet Controller
 *
 * State: working
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

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "linux-tap.h"
#include "fio.h"
#include "bus.h"
#include "phy.h"
#include "signode.h"
#include "cycletimer.h"
#include "sgstring.h"

#if 1
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif


#define ETH_CTL(base)	((base)+0)
#define		CTL_BP		(1<<8)
#define		CTL_WES		(1<<7)
#define		CTL_ISR		(1<<6)
#define		CTL_CSR		(1<<5)
#define		CTL_MPE		(1<<4)
#define		CTL_TE		(1<<3)
#define		CTL_RE		(1<<2)
#define		CTL_LBL		(1<<1)
#define		CTL_LB		(1<<0)
#define ETH_CFG(base)	((base)+0x4)
#define		CFG_RMII	(1<<13)
#define		CFG_RTY		(1<<12)
#define		CFG_CLK_MASK	(3<<10)
#define		CFG_CLK_SHIFT	(10)
#define 	CFG_EAE		(1<<9)
#define		CFG_BIG		(1<<8)
#define		CFG_UNI		(1<<7)
#define		CFG_MTI		(1<<6)
#define		CFG_NBC		(1<<5)
#define		CFG_CAF		(1<<4)
#define		CFG_BR		(1<<2)
#define		CFG_FD		(1<<1)
#define		CFG_SPD		(1<<0)
#define ETH_SR(base)	((base)+0x8)
#define		SR_IDLE		(1<<2) /* possible documentation bug ! */
#define		SR_MDIO		(1<<1)
#define		SR_LINK		(1<<0)
#define ETH_TAR(base)	((base)+0xc)
#define ETH_TCR(base)	((base)+0x10)
#define		TCR_NCRC	(1<<15)
#define		TCR_LEN_MASK = 0x7ff;

#define ETH_TSR(base)	((base)+0x14)
#define		TSR_UND		(1<<6)
#define		TSR_COMP	(1<<5)
#define 	TSR_BNQ		(1<<4)
#define		TSR_IDLE	(1<<3)
#define		TSR_RLE		(1<<2)
#define		TSR_COL		(1<<1)
#define		TSR_OVR		(1<<0)

#define ETH_RBQP(base)	((base)+0x18)
#define ETH_RSR(base)	((base)+0x20)
#define		RSR_OVR		(1<<2)
#define		RSR_REC		(1<<1)
#define		RSR_BNA		(1<<0)

#define ETH_ISR(base)	((base)+0x24)
#define		ISR_ABT		(1<<11)
#define		ISR_ROVR	(1<<10)
#define		ISR_LINK	(1<<9)
#define		ISR_TIDLE	(1<<8)
#define		ISR_TCOM	(1<<7)
#define		ISR_TBRE	(1<<6)
#define		ISR_RTRY	(1<<5)
#define		ISR_TUND	(1<<4)
#define		ISR_TOVR	(1<<3)
#define		ISR_RBNA	(1<<2)
#define 	ISR_RCOM	(1<<1)
#define		ISR_DONE	(1<<0)
#define ETH_IER(base)	((base)+0x28)
#define		IER_ABT		(1<<11)
#define		IER_ROVR	(1<<10)
#define		IER_LINK	(1<<9)
#define		IER_TIDLE	(1<<8)
#define		IER_TCOM	(1<<7)
#define		IER_TBRE	(1<<6)
#define		IER_RTRY	(1<<5)
#define		IER_TUND	(1<<4)
#define		IER_TOVR	(1<<3)
#define		IER_RBNA	(1<<2)
#define 	IER_RCOM	(1<<1)
#define		IER_DONE	(1<<0)
#define ETH_IDR(base)	((base)+0x2c)
#define		IDR_ABT		(1<<11)
#define		IDR_ROVR	(1<<10)
#define		IDR_LINK	(1<<9)
#define		IDR_TIDLE	(1<<8)
#define		IDR_TCOM	(1<<7)
#define		IDR_TBRE	(1<<6)
#define		IDR_RTRY	(1<<5)
#define		IDR_TUND	(1<<4)
#define		IDR_TOVR	(1<<3)
#define		IDR_RBNA	(1<<2)
#define 	IDR_RCOM	(1<<1)
#define		IDR_DONE	(1<<0)
#define ETH_IMR(base)	((base)+0x30)
#define		IMR_ABT		(1<<11)
#define		IMR_ROVR	(1<<10)
#define		IMR_LINK	(1<<9)
#define		IMR_TIDLE	(1<<8)
#define		IMR_TCOM	(1<<7)
#define		IMR_TBRE	(1<<6)
#define		IMR_RTRY	(1<<5)
#define		IMR_TUND	(1<<4)
#define		IMR_TOVR	(1<<3)
#define		IMR_RBNA	(1<<2)
#define 	IMR_RCOM	(1<<1)
#define		IMR_DONE	(1<<0)
#define ETH_MAN(base)	((base)+0x34)
#define		MAN_LOW		(1<<31)
#define		MAN_HIGH	(1<<30)
#define		MAN_RW_MASK	(3<<28)
#define		MAN_RW_SHIFT	(28)
#define		MAN_READ	(2<<28)
#define		MAN_WRITE	(1<<28)
#define		MAN_PHYA_MASK	(0x1f<<23)
#define		MAN_PHYA_SHIFT	(23)
#define		MAN_REGA_MASK	(0x1f<<18)
#define		MAN_REGA_SHIFT	(18)
#define		MAN_CODE_MASK	(3<<16)
#define		MAN_CODE_SHIFT	(16)
#define		MAN_DATA_MASK	(0xffff)
#define ETH_FRA(base)	((base)+0x40)
#define ETH_SCOL(base)	((base)+0x44)
#define ETH_MCOL(base)	((base)+0x48)
#define ETH_OK(base)	((base)+0x4c)
#define ETH_SEQE(base)	((base)+0x50)
#define ETH_ALE(base)	((base)+0x54)
#define ETH_DTE(base)	((base)+0x58)
#define ETH_LCOL(base)	((base)+0x5c)
#define ETH_ECOL(base)	((base)+0x60)
#define ETH_TUE(base)	((base)+0x64)
#define ETH_CSE(base)	((base)+0x68)
#define ETH_DRFC(base)	((base)+0x6c)
#define ETH_ROV(base)	((base)+0x70)
#define ETH_CDE(base)	((base)+0x74)
#define ETH_ELR(base)	((base)+0x78)
#define ETH_RJB(base)	((base)+0x7c)
#define ETH_USF(base)	((base)+0x80)
#define ETH_SQEE(base)	((base)+0x84)
#define	ETH_HSL(base)	((base)+0x90)
#define	ETH_HSH(base)	((base)+0x94)
#define	ETH_SA1L(base)	((base)+0x98)
#define ETH_SA1H(base)	((base)+0x9c)
#define	ETH_SA2L(base)	((base)+0xa0)
#define	ETH_SA2H(base)	((base)+0xa4)
#define	ETH_SA3L(base)	((base)+0xa8)
#define	ETH_SA3H(base)	((base)+0xac)
#define	ETH_SA4L(base)	((base)+0xb0)
#define	ETH_SA4H(base)	((base)+0xb4)

/* Address match bitfield definition for Receive buffer descriptor */
#define	AM_BROADCAST	(1<<31) /* is this all ones or only broadcast bit ?  */
#define AM_MULTICAST	(1<<30)
#define AM_UNICAST	(1<<29)	/* Hash match, not local address match */ 
#define	AM_EXTADDR	(1<<28)
#define AM_USA		(1<<27)
#define	AM_SA1		(1<<26)
#define	AM_SA2		(1<<25)
#define	AM_SA3		(1<<24)
#define	AM_SA4		(1<<23)

#define MAX_PHYS	(32)
typedef struct AT91Emac {
        BusDevice bdev;
	int ether_fd;
	FIO_FileHandler input_fh;
        int receiver_is_enabled;
	PHY_Device *phy[MAX_PHYS];
	CycleTimer rcvDelayTimer;
	SigNode *irqNode;
	uint32_t ctl;
	uint32_t cfg;
	uint32_t sr;
	uint32_t tar;
	uint32_t tcr;
	uint32_t tsr;
	uint32_t rbqp;
	uint32_t rbdscr_offs;
	uint32_t rsr;
	uint32_t isr;
	uint32_t imr;
	uint32_t man;
	uint64_t fra;
	uint64_t scol;
	uint64_t mcol;
	uint64_t ok;
	uint64_t seqe;
	uint64_t ale;
	uint64_t dte;
	uint64_t lcol;
	uint64_t ecol;
	uint64_t tue;
	uint64_t cse;
	uint64_t drfc;
	uint64_t rov;
	uint64_t cde;
	uint64_t elr;
	uint64_t rjb;
	uint64_t usf;
	uint64_t sqee;
	uint32_t hsl;
	uint32_t hsh;
	uint8_t sa1[6];
	uint8_t sa2[6];
	uint8_t sa3[6];
	uint8_t sa4[6];
} AT91Emac;

static void enable_receiver(AT91Emac *emac); 
static void disable_receiver(AT91Emac *emac); 

static void
update_interrupt(AT91Emac *emac) 
{
	if(emac->isr & (~emac->imr) & 0xfff) {
		SigNode_Set(emac->irqNode,SIG_HIGH);
	} else {
		SigNode_Set(emac->irqNode,SIG_PULLDOWN);
	}
}

static void
update_receiver_status(AT91Emac *emac) 
{
	if(!(emac->ctl & CTL_RE)) {
		disable_receiver(emac);
		return;
	} else {
		/* should do some overrun check here */
		if(!CycleTimer_IsActive(&emac->rcvDelayTimer)) {
			enable_receiver(emac);
		}
	}
}

static void
rcv_delay_done(void *clientData) 
{
	AT91Emac *emac = (AT91Emac *)clientData;
	update_receiver_status(emac);
}

static inline int
is_broadcast(uint8_t *mac) {
        if(mac[0] & 1) {
                return 1;
        }
        return 0;
}

static uint32_t
match_address(AT91Emac *emac,uint8_t *packet) 
{
	uint32_t matchflags = 0;
	if(is_broadcast(packet)) {
		matchflags |= AM_BROADCAST;
	}
	if(memcmp(packet,emac->sa1,6)==0) {
		matchflags |= AM_SA1;
	}
	if(memcmp(packet,emac->sa2,6)==0) {
		matchflags |= AM_SA2;
	}
	if(memcmp(packet,emac->sa3,6)==0) {
		matchflags |= AM_SA3;
	}
	if(memcmp(packet,emac->sa4,6)==0) {
		matchflags |= AM_SA4;
	}
	return matchflags;	
}
/*
 * Write a packet to memory
 */
#define RBF_OWNER	(1<<0)	/* 1 = software owned */
#define RBF_WRAP	(1<<1)
#define RBF_ADDR	(0xfffffffc)
static void
dma_write_packet(AT91Emac *emac,uint8_t *buf,int count,uint32_t matchflags)
{
	uint32_t rba,rbstat;
	/* Fetch receive buffer descriptor, manual says the counter is ored ! */
	uint32_t descr_addr = emac->rbqp | emac->rbdscr_offs;
	//fprintf(stderr,"desc_addr %08x\n",descr_addr);
	rba = Bus_Read32(descr_addr);
	rbstat = Bus_Read32(descr_addr | 4);
	if(rba & RBF_OWNER) {
		fprintf(stderr,"AT91Emac: Receive buffer not available\n");
                emac->isr |= ISR_RBNA;
		emac->rsr |= RSR_BNA;
		update_interrupt(emac);
		return;
	}
	Bus_Write((rba & RBF_ADDR),buf,count);
	Bus_Write32(rba | RBF_OWNER,descr_addr);
	Bus_Write32(count | matchflags,descr_addr + 4);
	if(rba & RBF_WRAP) {
		/* WRAP */
		//fprintf(stderr,"receive buffer WRAP\n"); // jk
		emac->rbdscr_offs = 0;
	} else {
		emac->rbdscr_offs += 8;
	}
}


static void 
input_event(void *cd,int mask)
{
        AT91Emac *emac = (AT91Emac*)cd;
        int result;
        uint8_t buf[1522];
	uint32_t matchflags;
	while(emac->receiver_is_enabled) {
		result=read(emac->ether_fd,buf,1522);
		if(result>0) {
			matchflags = match_address(emac,buf);
			if(!matchflags && !(emac->cfg & CFG_CAF)) {
				fprintf(stderr,"no mac match, continue\n");
				continue;
			}
			if((result > (1522-4)) && !(emac->cfg & CFG_BIG)) {
				fprintf(stderr,"BIG PACKET\n"); // jk
				continue;
			}
			if(result<60) {
				dma_write_packet(emac,buf,60,matchflags);
			} else {
				dma_write_packet(emac,buf,result,matchflags);
			}
			emac->isr |= ISR_RCOM;
			emac->rsr |= RSR_REC;
			emac->ok++;
			update_interrupt(emac);
			disable_receiver(emac);
			CycleTimer_Mod(&emac->rcvDelayTimer,NanosecondsToCycles(result*100));
			break;
		} else {
			return;
		}
	}
        return;
}

static void
enable_receiver(AT91Emac *emac) {
        if(!emac->receiver_is_enabled && (emac->ether_fd >=0)) {
                dbgprintf("AT91Emac: enable receiver\n");
                //fprintf(stderr,"AT91Emac: enable receiver\n");
                FIO_AddFileHandler(&emac->input_fh,emac->ether_fd,FIO_READABLE,input_event,emac);
                emac->receiver_is_enabled=1;
        }
}

static void
disable_receiver(AT91Emac *emac)
{
        if(emac->receiver_is_enabled) {
        	dbgprintf("AT91Emac: disable receiver\n");
        	//fprintf(stderr,"AT91Emac: disable receiver\n");
                FIO_RemoveFileHandler(&emac->input_fh);
                emac->receiver_is_enabled=0;
        }
}

static void
transmit_packet(AT91Emac *emac)
{
	int len = emac->tcr & 0x7ff;
	uint8_t buf[2048];
	if((emac->tcr & TCR_NCRC) && (len >= 4)) {
		len-=4;
	}
	Bus_Read(buf,emac->tar,len);
	if(len<60) {
		memset(buf+len,0x00,60-len);
		len = 60;
	}
	if(write(emac->ether_fd,buf,len) == len) {
		emac->fra++;
	}
	emac->isr |= ISR_TCOM | ISR_TIDLE;
	emac->tsr |= TSR_IDLE | TSR_COMP | TSR_BNQ;
	/* Manual contradicts u-boot code: clear tcr when transmit done */
	emac->tcr = emac->tcr & ~0x7ff; 
	update_interrupt(emac);
}

static uint32_t
ctl_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return emac->ctl;
}

static void
ctl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->ctl = value &  0x1ff;
	update_receiver_status(emac);
}

/*
 * -------------------------------------------------------------------------------------
 * CFG register
 * Bit 13: CFG_RMII enables RMII mode
 * Bit 12: CFG_RTY Retry test		
 * Bit 10-11: CFG_CLK  MDC clock (<=2.5 MHz) 
 * Bit 9: CFG_EAE External Address match enable ???
 * Bit 8: CFG_BIG Receive Big packets (up to 1522 bytes)
 * Bit 7: CFG_UNI Unicast hash enable
 * Bit 6: CFG_MTI Multicast hash enable
 * Bit 5: CFG_NBC No broadcast
 * Bit 4: CFG_CAF Copy all frames 
 * Bit 3: CFG_BR  Bit rate (optional)
 * Bit 2: CFG_FD  1 = Full duplex
 * Bit 1: CFG_SPD 1=100Mbit, 0 = 10Mbit
 * -------------------------------------------------------------------------------------
 */

static uint32_t
cfg_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return emac->cfg;
}

static void
cfg_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->cfg = value & 0x3ff7;	
}

static uint32_t
sr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return emac->sr;
}

static void
sr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"emac: Status register is readonly\n");
}

static uint32_t
tar_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return emac->tar;
}

static void
tar_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->tar = value;
}

static uint32_t
tcr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return emac->tcr;
}

static void
tcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->tcr = value;	
	if(emac->ctl & CTL_TE) {
		transmit_packet(emac);
	}
}

static uint32_t
tsr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	return emac->tsr;
}

static void
tsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t clearmask = value & (TSR_OVR | TSR_COL | TSR_RLE | TSR_COMP | TSR_UND);
	emac->tsr &= ~clearmask;
}

static uint32_t
rbqp_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	return emac->rbqp;
}

static void
rbqp_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	emac->rbqp = value;
	emac->rbdscr_offs = 0; /* ?????? When is this really reset */
}

static uint32_t
rsr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	return emac->rsr;
}

static void
rsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	emac->rsr &= ~(value & 7);
	
}

static uint32_t
isr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	uint32_t isr = emac->isr;
	emac->isr = 0;
	update_interrupt(emac);
	return isr;
}

static void
isr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"emac: what does write isr ? \n");
}

static uint32_t
ier_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"emac: IER register is write only\n");
	return 0;
}

static void
ier_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->imr &= ~(value & 0xfff);
	update_interrupt(emac);
}

static uint32_t
idr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"emac: IDR register is write only\n");
	return 0;
}

static void
idr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->imr |= (value & 0xfff);
	update_interrupt(emac);
}

static uint32_t
imr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	fprintf(stderr,"emac: register %08x not implemented\n",address);
	return emac->imr;
}

static void
imr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"IMR is not writable\n");
}

static uint32_t
man_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return emac->man;
}

static void
man_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	int read;
	int phya;
	int rega; 
	PHY_Device *phy;
	uint16_t result;
	
	if(!(emac->ctl & CTL_MPE)) {
		fprintf(stderr,"AT91Emac: Phy management port is disabled\n");
		return;
	}
	if(value & MAN_LOW) {
		fprintf(stderr,"AT91Emac: MAN low error\n");
		return;
	}	
	if(!(value & MAN_HIGH)) {
		fprintf(stderr,"AT91Emac: MAN high error\n");
		return;
	}
	if((value & MAN_CODE_MASK) != (2<<MAN_CODE_SHIFT)) {
		fprintf(stderr,"AT91Emac: MAN code error\n");
		return;
	}
	if((value & MAN_RW_MASK) == MAN_READ) {
		read = 1;
	} else if((value & MAN_RW_MASK) == MAN_WRITE) {
		read = 0;
	} else {
		fprintf(stderr,"AT91Emac: MAN neither read nor write\n");
		return;	
	}
	phya = (value & MAN_PHYA_MASK) >> MAN_PHYA_SHIFT;
	rega = (value & MAN_REGA_MASK) >> MAN_REGA_SHIFT;
	phy = emac->phy[phya];
	if(!phy) {
		return;
	}
	if(read) {
		PHY_ReadRegister(phy,&result,rega);	
		emac->man = result | (value & 0xffff0000);
		//fprintf(stderr,"Phy read %08x\n",emac->man);
	} else {
		PHY_WriteRegister(phy,value & 0xffff,rega);	
		emac->man = value; /* does this shift out and read 0 of 0xffff ? */
		//fprintf(stderr,"Phy write %04x\n",emac->man &0xffff);
	}
}

static uint32_t
fra_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t fra = (emac->fra > 0xffffff) ? 0xffffff : emac->fra;
	emac->fra = 0;
	return fra;
}

static void
fra_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: FRA register is not writable\n");
}

static uint32_t
scol_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t scol = (emac->scol > 0xffff) ? 0xffff : emac->scol;
	emac->scol = 0;
	return scol;
}

static void
scol_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: SCOL register is not writable\n");
}

static uint32_t
mcol_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t mcol = (emac->mcol > 0xffff) ? 0xffff : emac->mcol;
	emac->mcol = 0;
	return mcol;
}

static void
mcol_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: MCOL register is not writable\n");
}

static uint32_t
ok_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t ok = (emac->ok > 0xffffff) ? 0xffffff : emac->ok;
	emac->ok = 0;
	return ok;
}

static void
ok_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: OK register is not writable\n");
}

static uint32_t
seqe_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t seqe = (emac->seqe > 0xff) ? 0xff : emac->seqe;
	emac->seqe = 0;
	return seqe;
}

static void
seqe_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: SEQE register is not writable\n");
}

static uint32_t
ale_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t ale = (emac->ale > 0xff) ? 0xff : emac->ale;
	emac->ale = 0;
	return ale;
}

static void
ale_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: ALE register is not writable\n");
}

static uint32_t
dte_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t dte = (emac->dte > 0xffff) ? 0xffff : 0;
	emac->dte = 0;
	return dte;
}

static void
dte_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: DTE register is not writable\n");
}

static uint32_t
lcol_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t lcol = (emac->lcol > 0xff) ? 0xff : emac->lcol; 	
	emac->lcol = 0;
	return lcol;
}

static void
lcol_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: LCOL register is not writable\n");
}

static uint32_t
ecol_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t ecol = (emac->ecol > 0xff) ? 0xff : emac->ecol;
	emac->ecol = 0;
	return ecol;
}

static void
ecol_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: ECOL register is not writable\n");
}

static uint32_t
tue_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t tue = (emac->tue > 0xff) ? 0xff : emac->tue;
	emac->tue = 0;
	return tue;
}

static void
tue_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: TUE register is not writable\n");
}

static uint32_t
cse_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t cse = (emac->cse > 0xff) ? 0xff : emac->cse; 
	emac->cse = 0;
	return cse;
}

static void
cse_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: CSE register is not writable\n");
}

static uint32_t
drfc_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac * emac = (AT91Emac *) clientData;
	uint32_t drfc = (emac->drfc > 0xffff) ? 0xffff : emac->drfc;
	emac->drfc = 0;
	return drfc;
}

static void
drfc_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: DRFC register is not writable\n");
}

static uint32_t
rov_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t rov = (emac->rov > 0xff) ? 0xff : emac->rov;
	emac->rov = 0;
	return rov;
}

static void
rov_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: ROV register is not writable\n");
}

static uint32_t
cde_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t cde = (emac->cde > 0xff) ? 0xff : emac->cde;
	emac->cde = 0;
	return cde;
}

static void
cde_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: CDE register is not writable\n");
}

static uint32_t
elr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t elr = (emac->elr > 0xff) ? 0xff : emac->elr;
	emac->elr = 0;
	return elr;
}

static void
elr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: ELR register is not writable\n");
}

static uint32_t
rjb_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	uint32_t rjb = (emac->rjb > 0xff) ? 0xff : emac->rjb;
	emac->rjb = 0;
	return rjb;
}

static void
rjb_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: RJB register is not writable\n");
}

static uint32_t
usf_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t usf = (emac->usf > 0xff) ? 0xff : emac->usf;
	emac->usf = 0;
	return usf;
}

static void
usf_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: USF register is not writable\n");
}

static uint32_t
sqee_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	uint32_t sqee = (emac->sqee > 0xff) ? 0xff : emac->sqee;
	emac->sqee = 0;
	return sqee;
}

static void
sqee_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"AT91Emac: SQEE register is not writable\n");
}

static uint32_t
hsl_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	return emac->hsl;
}

static void
hsl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	emac->hsl = value;
}

static uint32_t
hsh_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	return emac->hsh;
}

static void
hsh_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *) clientData;
	emac->hsh = value;
}

static uint32_t
sa1l_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return (emac->sa1[3]<<24) | (emac->sa1[2]<<16) 
		| (emac->sa1[1]<<8) | emac->sa1[0];
}

static void
sa1l_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->sa1[3] = (value >> 24) & 0xff;
	emac->sa1[2] = (value >> 16) & 0xff;
	emac->sa1[1] = (value >> 8) & 0xff;
	emac->sa1[0] = (value >> 0) & 0xff;
}

static uint32_t
sa1h_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return  (emac->sa1[5]<<8) | emac->sa1[4];
}

static void
sa1h_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->sa1[5] = (value >> 8) & 0xff;
	emac->sa1[4] = (value >> 0) & 0xff;
}

static uint32_t
sa2l_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return (emac->sa2[3]<<24) | (emac->sa2[2]<<16) 
		| (emac->sa2[1]<<8) | emac->sa2[0];
}

static void
sa2l_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->sa2[3] = (value >> 24) & 0xff;
	emac->sa2[2] = (value >> 16) & 0xff;
	emac->sa2[1] = (value >> 8) & 0xff;
	emac->sa2[0] = (value >> 0) & 0xff;
}

static uint32_t
sa2h_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return  (emac->sa2[5]<<8) | emac->sa2[4];
}

static void
sa2h_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->sa2[5] = (value >> 8) & 0xff;
	emac->sa2[4] = (value >> 0) & 0xff;
}

static uint32_t
sa3l_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return (emac->sa3[3]<<24) | (emac->sa3[2]<<16) 
		| (emac->sa3[1]<<8) | emac->sa3[0];
}

static void
sa3l_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->sa3[3] = (value >> 24) & 0xff;
	emac->sa3[2] = (value >> 16) & 0xff;
	emac->sa3[1] = (value >> 8) & 0xff;
	emac->sa3[0] = (value >> 0) & 0xff;
}

static uint32_t
sa3h_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return  (emac->sa3[5]<<8) | emac->sa3[4];
}

static void
sa3h_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->sa3[5] = (value >> 8) & 0xff;
	emac->sa3[4] = (value >> 0) & 0xff;
}

static uint32_t
sa4l_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return (emac->sa4[3]<<24) | (emac->sa4[2]<<16) 
		| (emac->sa4[1]<<8) | emac->sa4[0];
}

static void
sa4l_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->sa4[3] = (value >> 24) & 0xff;
	emac->sa4[2] = (value >> 16) & 0xff;
	emac->sa4[1] = (value >> 8) & 0xff;
	emac->sa4[0] = (value >> 0) & 0xff;
}

static uint32_t
sa4h_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	return  (emac->sa4[5]<<8) | emac->sa4[4];
}

static void
sa4h_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Emac *emac = (AT91Emac *)clientData;
	emac->sa4[5] = (value >> 8) & 0xff;
	emac->sa4[4] = (value >> 0) & 0xff;
}

static void
AT91Emac_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
        AT91Emac *emac = (AT91Emac*) owner;
	IOH_New32(ETH_CTL(base),ctl_read,ctl_write,emac);
	IOH_New32(ETH_CFG(base),cfg_read,cfg_write,emac);
	IOH_New32(ETH_SR(base),sr_read,sr_write,emac);
	IOH_New32(ETH_TAR(base),tar_read,tar_write,emac);
	IOH_New32(ETH_TCR(base),tcr_read,tcr_write,emac);
	IOH_New32(ETH_TSR(base),tsr_read,tsr_write,emac);
	IOH_New32(ETH_RBQP(base),rbqp_read,rbqp_write,emac);
	IOH_New32(ETH_RSR(base),rsr_read,rsr_write,emac);
	IOH_New32(ETH_ISR(base),isr_read,isr_write,emac);
	IOH_New32(ETH_IER(base),ier_read,ier_write,emac);
	IOH_New32(ETH_IDR(base),idr_read,idr_write,emac);
	IOH_New32(ETH_IMR(base),imr_read,imr_write,emac);
	IOH_New32(ETH_MAN(base),man_read,man_write,emac);
	IOH_New32(ETH_FRA(base),fra_read,fra_write,emac);
	IOH_New32(ETH_SCOL(base),scol_read,scol_write,emac);
	IOH_New32(ETH_MCOL(base),mcol_read,mcol_write,emac);
	IOH_New32(ETH_OK(base),ok_read,ok_write,emac);
	IOH_New32(ETH_SEQE(base),seqe_read,seqe_write,emac);
	IOH_New32(ETH_ALE(base),ale_read,ale_write,emac);
	IOH_New32(ETH_DTE(base),dte_read,dte_write,emac);
	IOH_New32(ETH_LCOL(base),lcol_read,lcol_write,emac);
	IOH_New32(ETH_ECOL(base),ecol_read,ecol_write,emac);
	IOH_New32(ETH_TUE(base),tue_read,tue_write,emac);
	IOH_New32(ETH_CSE(base),cse_read,cse_write,emac);
	IOH_New32(ETH_DRFC(base),drfc_read,drfc_write,emac);
	IOH_New32(ETH_ROV(base),rov_read,rov_write,emac);
	IOH_New32(ETH_CDE(base),cde_read,cde_write,emac);
	IOH_New32(ETH_ELR(base),elr_read,elr_write,emac);
	IOH_New32(ETH_RJB(base),rjb_read,rjb_write,emac);
	IOH_New32(ETH_USF(base),usf_read,usf_write,emac);
	IOH_New32(ETH_SQEE(base),sqee_read,sqee_write,emac);
	IOH_New32(ETH_HSL(base),hsl_read,hsl_write,emac);
	IOH_New32(ETH_HSH(base),hsh_read,hsh_write,emac);
	IOH_New32(ETH_SA1L(base),sa1l_read,sa1l_write,emac);
	IOH_New32(ETH_SA1H(base),sa1h_read,sa1h_write,emac);
	IOH_New32(ETH_SA2L(base),sa2l_read,sa2l_write,emac);
	IOH_New32(ETH_SA2H(base),sa2h_read,sa2h_write,emac);
	IOH_New32(ETH_SA3L(base),sa3l_read,sa3l_write,emac);
	IOH_New32(ETH_SA3H(base),sa3h_read,sa3h_write,emac);
	IOH_New32(ETH_SA4L(base),sa4l_read,sa4l_write,emac);
	IOH_New32(ETH_SA4H(base),sa4h_read,sa4h_write,emac);
}

static void
AT91Emac_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(ETH_CTL(base));
	IOH_Delete32(ETH_CFG(base));
	IOH_Delete32(ETH_SR(base));
	IOH_Delete32(ETH_TAR(base));
	IOH_Delete32(ETH_TCR(base));
	IOH_Delete32(ETH_TSR(base));
	IOH_Delete32(ETH_RBQP(base));
	IOH_Delete32(ETH_RSR(base));
	IOH_Delete32(ETH_ISR(base));
	IOH_Delete32(ETH_IER(base));
	IOH_Delete32(ETH_IDR(base));
	IOH_Delete32(ETH_IMR(base));
	IOH_Delete32(ETH_MAN(base));
	IOH_Delete32(ETH_FRA(base));
	IOH_Delete32(ETH_SCOL(base));
	IOH_Delete32(ETH_MCOL(base));
	IOH_Delete32(ETH_OK(base));
	IOH_Delete32(ETH_SEQE(base));
	IOH_Delete32(ETH_ALE(base));
	IOH_Delete32(ETH_DTE(base));
	IOH_Delete32(ETH_LCOL(base));
	IOH_Delete32(ETH_ECOL(base));
	IOH_Delete32(ETH_TUE(base));
	IOH_Delete32(ETH_CSE(base));
	IOH_Delete32(ETH_DRFC(base));
	IOH_Delete32(ETH_ROV(base));
	IOH_Delete32(ETH_CDE(base));
	IOH_Delete32(ETH_ELR(base));
	IOH_Delete32(ETH_RJB(base));
	IOH_Delete32(ETH_USF(base));
	IOH_Delete32(ETH_SQEE(base));
	IOH_Delete32(ETH_HSL(base));
	IOH_Delete32(ETH_HSH(base));
	IOH_Delete32(ETH_SA1L(base));
	IOH_Delete32(ETH_SA1H(base));
	IOH_Delete32(ETH_SA2L(base));
	IOH_Delete32(ETH_SA2H(base));
	IOH_Delete32(ETH_SA3L(base));
	IOH_Delete32(ETH_SA3H(base));
	IOH_Delete32(ETH_SA4L(base));
	IOH_Delete32(ETH_SA4H(base));
}


int
AT91Emac_RegisterPhy(BusDevice *dev,PHY_Device *phy,unsigned int phy_addr)
{
        AT91Emac *emac=dev->owner;
        if(phy_addr >= MAX_PHYS) {
                fprintf(stderr,"AT91Emac: Illegal PHY address %d\n",phy_addr);
                return -1;
        } else {
                emac->phy[phy_addr]=phy;
                return 0;
        }

}


BusDevice *
AT91Emac_New(const char *name) 
{
	AT91Emac *emac = sg_new(AT91Emac);
	emac->ether_fd = Net_CreateInterface(name);
        fcntl(emac->ether_fd,F_SETFL,O_NONBLOCK);
	emac->irqNode = SigNode_New("%s.irq",name);
	if(!emac->irqNode) {
		fprintf(stderr,"AT91Emac: Can't create interrupt request line\n");
		exit(1);
	}
	SigNode_Set(emac->irqNode,SIG_PULLDOWN);

	/* The reset value initialization is complete ! all other values are 0 */
	emac->cfg = 0x800;
	emac->sr = 0x6;
	emac->tsr = 0x18;
	emac->imr = 0xfff;
	emac->bdev.first_mapping=NULL;
        emac->bdev.Map=AT91Emac_Map;
        emac->bdev.UnMap=AT91Emac_UnMap;
        emac->bdev.owner=emac;
        emac->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	CycleTimer_Init(&emac->rcvDelayTimer,rcv_delay_done,emac);
        fprintf(stderr,"AT91RM9200 Ethernet MAC \"%s\" created\n",name);
        return &emac->bdev;
}
