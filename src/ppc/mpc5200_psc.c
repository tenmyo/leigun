/*
 *************************************************************************************************
 *
 * Emulation of MPC5200B Programmable serial controller
 *
 * state:  Not implemented
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
#include <serial.h>

#define PSC_MR1(base) 		((base) + 0x00) 
#define PSC_MR2(base) 		((base) + 0x00)
#define PSC_SR(base)		((base) + 0x04)
#define PSC_CSR(base)		((base) + 0x04) 
#define PSC_CR(base)		((base) + 0x08)
#define PSC_RB(base)		((base) + 0x0c)  
#define PSC_TB(base)		((base) + 0x0c)  
#define PSC_IPCR(base)		((base) + 0x10)
#define	PSC_ACR(base)		((base) + 0x10)
#define PSC_ISR(base)		((base) + 0x14)
#define PSC_IMR(base)		((base) + 0x14)
#define PSC_CTUR(base)		((base) + 0x18)
#define PSC_CTLR(base)		((base) + 0x1c)
#define PSC_CCR(base)		((base) + 0x20)
#define PSC_AC97Slots(base)	((base) + 0x24)
#define PSC_AC97CMD(base)	((base) + 0x28)
#define PSC_AC97Data(base)	((base) + 0x2c)
#define PSC_IVR(base)		((base) + 0x30)
#define PSC_IP(base)		((base) + 0x34)
#define PSC_OP1(base)		((base) + 0x38)
#define PSC_OP0(base)		((base) + 0x3c)
#define PSC_SICR(base)		((base) + 0x40)
#define PSC_IRCR1(base)		((base) + 0x44)
#define PSC_IRCR2(base)		((base) + 0x48)
#define PSC_IRSDR(base)		((base) + 0x4c)
#define PSC_IRMDR(base)		((base) + 0x50)
#define PSC_IRFDR(base)		((base) + 0x54)
#define PSC_RFNUM(base)		((base) + 0x58)
#define PSC_TFNUM(base)		((base) + 0x5c)
#define PSC_RFDATA(base)	((base) + 0x60)
#define PSC_RFSTAT(base)	((base) + 0x64)
#define PSC_RFCNTL(base)	((base) + 0x68)
#define PSC_RFALARM(base)	((base) + 0x6e)
#define PSC_RFRPTR(base)	((base) + 0x72)
#define PSC_RFWPTR(base)	((base) + 0x76)
#define PSC_RFLRFPTR(base)	((base) + 0x7a)
#define PSC_RFLWFPTR(base)	((base) + 0x7c)
#define PSC_TFDATA(base)	((base) + 0x80)
#define PSC_TFSTAT(base)	((base) + 0x84)
#define PSC_TFCNTL(base)	((base) + 0x88)
#define PSC_TFALARM(base)	((base) + 0x8e)
#define PSC_TFRPTR(base)	((base) + 0x92)
#define PSC_TFWPTR(base)	((base) + 0x96)
#define PSC_TFLRFPTR(base)	((base) + 0x9a)
#define PSC_TFLWFPTR(base)	((base) + 0x9c)

typedef struct Psc {
	BusDevice bdev;
	uint8_t rxfifo[512];
	uint8_t txfifo[512];
} Psc;

static uint32_t
mr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
mr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}

static uint32_t
sr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
csr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
cr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
cr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
rb_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
tb_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
ipcr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
acr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
isr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
isr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static void
ctur_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static void
ctlr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
ccr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
ccr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static void
ac97slots_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
ac97cmd_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
ac97cmd_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
ac97data_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static uint32_t
ivr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
ivr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
ip_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
op1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static void
op0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
sicr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
sicr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
ircr1_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
ircr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
ircr2_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
ircr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
irsdr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
irsdr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
irmdr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
irmdr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
irfdr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
irfdr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
rfnum_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static uint32_t
tfnum_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static uint32_t
rfdata_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
rfdata_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
rfstat_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
rfstat_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
rfcntl_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
rfcntl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
rfalarm_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
rfalarm_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
rfrptr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
rfrptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
rfwptr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
rfwptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
rflrfptr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
rflrfptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
rflwfptr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
rflwfptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
tfdata_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
tfdata_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
tfstat_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
tfstat_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
tfcntl_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
tfcntl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
tfalarm_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
tfalarm_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
tfrptr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
tfrptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
tfwptr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
tfwptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
tflrfptr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
tflrfptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}


static uint32_t
tflwfptr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
        return 0;
}

static void
tflwfptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"PSC: register not implemented\n");
}




static void
Psc_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete8(PSC_MR1(base));
	IOH_Delete16(PSC_SR(base));
	IOH_Delete8(PSC_CR(base));
	IOH_Delete32(PSC_RB(base));
	IOH_Delete8(PSC_IPCR(base));
	IOH_Delete16(PSC_ISR(base));
	IOH_Delete8(PSC_CTUR(base));
	IOH_Delete8(PSC_CTLR(base));
	IOH_Delete32(PSC_CCR(base));
	IOH_Delete32(PSC_AC97Slots(base));
	IOH_Delete32(PSC_AC97CMD(base));
	IOH_Delete32(PSC_AC97Data(base));
	IOH_Delete8(PSC_IVR(base));
	IOH_Delete8(PSC_IP(base));
	IOH_Delete8(PSC_OP1(base));
	IOH_Delete8(PSC_OP0(base));
	IOH_Delete32(PSC_SICR(base));
	IOH_Delete8(PSC_IRCR1(base));
	IOH_Delete8(PSC_IRCR2(base));
	IOH_Delete8(PSC_IRSDR(base));
	IOH_Delete8(PSC_IRMDR(base));
	IOH_Delete8(PSC_IRFDR(base));
	IOH_Delete16(PSC_RFNUM(base));
	IOH_Delete16(PSC_TFNUM(base));
	IOH_Delete32(PSC_RFDATA(base));
	IOH_Delete16(PSC_RFSTAT(base));
	IOH_Delete16(PSC_RFCNTL(base));
	IOH_Delete16(PSC_RFALARM(base));
	IOH_Delete16(PSC_RFRPTR(base));
	IOH_Delete16(PSC_RFWPTR(base));
	IOH_Delete16(PSC_RFLRFPTR(base));
	IOH_Delete16(PSC_RFLWFPTR(base));
	IOH_Delete32(PSC_TFDATA(base));
	IOH_Delete16(PSC_TFSTAT(base));
	IOH_Delete8(PSC_TFCNTL(base));
	IOH_Delete16(PSC_TFALARM(base));
	IOH_Delete16(PSC_TFRPTR(base));
	IOH_Delete16(PSC_TFWPTR(base));
	IOH_Delete16(PSC_TFLRFPTR(base));
	IOH_Delete16(PSC_TFLWFPTR(base));

}

static void
Psc_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	Psc *psc = (Psc *)owner;
	IOH_New8(PSC_MR1(base),mr_read,mr_write,psc); 
	IOH_New16(PSC_SR(base),sr_read,csr_write,psc);
	IOH_New8(PSC_CR(base),cr_read,cr_write,psc);
	IOH_New32(PSC_RB(base),rb_read,tb_write,psc);
	IOH_New8(PSC_IPCR(base),ipcr_read,acr_write,psc);
	IOH_New16(PSC_ISR(base),isr_read,isr_write,psc);
	IOH_New8(PSC_CTUR(base),NULL,ctur_write,psc);
	IOH_New8(PSC_CTLR(base),NULL,ctlr_write,psc);	
	IOH_New32(PSC_CCR(base),ccr_read,ccr_write,psc);
	IOH_New32(PSC_AC97Slots(base),NULL,ac97slots_write,psc);	
	IOH_New32(PSC_AC97CMD(base),ac97cmd_read,ac97cmd_write,psc);
	IOH_New32(PSC_AC97Data(base),ac97data_read,NULL,psc);
	IOH_New8(PSC_IVR(base),ivr_read,ivr_write,psc);	
	IOH_New8(PSC_IP(base),ip_read,NULL,psc);	
	IOH_New8(PSC_OP1(base),NULL,op1_write,psc);	
	IOH_New8(PSC_OP0(base),NULL,op0_write,psc);	
	IOH_New32(PSC_SICR(base),sicr_read,sicr_write,psc);
	IOH_New8(PSC_IRCR1(base),ircr1_read,ircr1_write,psc);	
	IOH_New8(PSC_IRCR2(base),ircr2_read,ircr2_write,psc);
	IOH_New8(PSC_IRSDR(base),irsdr_read,irsdr_write,psc);
	IOH_New8(PSC_IRMDR(base),irmdr_read,irmdr_write,psc);
	IOH_New8(PSC_IRFDR(base),irfdr_read,irfdr_write,psc);
	IOH_New16(PSC_RFNUM(base),rfnum_read,NULL,psc);
	IOH_New16(PSC_TFNUM(base),tfnum_read,NULL,psc);
	IOH_New32(PSC_RFDATA(base),rfdata_read,rfdata_write,psc);	
	IOH_New16(PSC_RFSTAT(base),rfstat_read,rfstat_write,psc);
	IOH_New16(PSC_RFCNTL(base),rfcntl_read,rfcntl_write,psc);
	IOH_New16(PSC_RFALARM(base),rfalarm_read,rfalarm_write,psc);
	IOH_New16(PSC_RFRPTR(base),rfrptr_read,rfrptr_write,psc);
	IOH_New16(PSC_RFWPTR(base),rfwptr_read,rfwptr_write,psc);
	IOH_New16(PSC_RFLRFPTR(base),rflrfptr_read,rflrfptr_write,psc);
	IOH_New16(PSC_RFLWFPTR(base),rflwfptr_read,rflwfptr_write,psc);
	IOH_New32(PSC_TFDATA(base),tfdata_read,tfdata_write,psc);	
	IOH_New16(PSC_TFSTAT(base),tfstat_read,tfstat_write,psc);
	IOH_New8(PSC_TFCNTL(base),tfcntl_read,tfcntl_write,psc);
	IOH_New16(PSC_TFALARM(base),tfalarm_read,tfalarm_write,psc);	
	IOH_New16(PSC_TFRPTR(base),tfrptr_read,tfrptr_write,psc);
	IOH_New16(PSC_TFWPTR(base),tfwptr_read,tfwptr_write,psc);
	IOH_New16(PSC_TFLRFPTR(base),tflrfptr_read,tflrfptr_write,psc);
	IOH_New16(PSC_TFLWFPTR(base),tflwfptr_read,tflwfptr_write,psc);
}

BusDevice *
MPC5200_PSCNew(BusDevice *dpram,const char *name)
{    
	Psc *psc = sg_calloc(sizeof(Psc));
	psc->bdev.first_mapping=NULL;
        psc->bdev.Map=Psc_Map;
        psc->bdev.UnMap=Psc_Unmap;
        psc->bdev.owner=psc;
        psc->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	return &psc->bdev;
}
