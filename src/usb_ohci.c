/*
 *************************************************************************************************
 *
 * Platform Independent Emulation of OHCI USB Controller 
 *
 * Status:
 *	Correctly detected by the Linux Kernel
 *	but no functionality (List Processing)
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bus.h"
#include "usb_ohci.h"
#include "cycletimer.h"
#include "signode.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define MAX_NDP 15

struct OhciHC {
	BusDevice bdev;		// Own function
	Bus *bus; 		// Bus	

	SigNode *irqNode;
	SigNode *endianNode;
        SigTrace *endianTrace;
        int endian;

	//UsbDevice roothub;
	int hold_reset; 	// if true all registers have default values and write is ignored 

	uint32_t HcControl;
	uint32_t HcCommandStatus;
	uint32_t HcInterruptStatus;
	uint32_t HcInterruptEnable;
	uint32_t HcHCCA;
	uint32_t HcPeriodCurrentED;
	uint32_t HcControlHeadED;
	uint32_t HcControlCurrentED;
	uint32_t HcBulkHeadED;
	uint32_t HcBulkCurrentED;
	uint32_t HcDoneHead;
	uint32_t HcFmInterval;	
	uint32_t HcFmRemaining;
	uint32_t HcFmNumber;
	uint32_t HcPeriodicStart; 
	uint32_t HcLSThreshold; 

	uint32_t HcRhDescriptorA; 
	uint32_t HcRhDescriptorB;
	uint32_t HcRhStatus; 
	uint32_t HcRhPortStatus[MAX_NDP];

	uint32_t cbs_count; // cross frame bulk/control ration counter
	uint32_t LargestPacketCounter;
	int interrupt_posted;
	CycleCounter_t UsbEnterResetTime; // store time when controller entered reset state

	int timer_is_active;
	CycleTimer timer;
}; 


/*
 * ---------------------------------------------------------
 * Descriptors are stored in Host Byteorder !!!
 * The OHCI device driver in the target system 
 * stores them in Little Endian. Conversion
 * to little endian is done when reading from the host 
 * memory. 
 * The names of the fields are stolen from the Linux Kernel
 * ---------------------------------------------------------
 */

typedef struct EndPointDescriptor {
	/* First for fields are read from host memory */
        uint32_t    	hwControl;         /* endpoint config bitmap */
        uint32_t    	hwTailP;        /* tail of TD list */
        uint32_t   	hwHeadP;        /* head of TD list (hc r/w) */
        uint32_t 	hwNextED;       /* next ED in list */
} EndPointDescriptor;

typedef struct GeneralTD {
	/* First for fields are read from host memory */
        uint32_t    	hwControl;   
        uint32_t    	hwCBP;   
        uint32_t   	hwNextTD;
        uint32_t 	hwBE; 
} GeneralTD;

typedef struct IsoTD {
	/* First for fields are read from host memory */
        uint32_t    	hwControl;   
        uint32_t    	hwBP0;   
        uint32_t   	hwNextTD;
        uint32_t 	hwBE; 
	uint32_t 	hwOffset01;
	uint32_t 	hwOffset23;
	uint32_t	hwOffset45;
	uint32_t	hwOffset67;
} IsoTD;

//define ED_DEQUEUE      (1 << 27)
//define ED_ISO          (1 << 15)
//define ED_SKIP         (1 << 14)
//define ED_LOWSPEED     (1 << 13)
//define ED_OUT          (0x01 << 11)
//define ED_IN           (0x02 << 11)

/* Lowest Bits in TailP */

typedef struct TransferDescriptor {
        uint32_t	hwControl;         /* transfer info bitmask */
        uint32_t 	hwCBP;          /* Current Buffer Pointer (or 0) */
        uint32_t	hwNextTD;       /* Next TD Pointer */
        uint32_t	hwBE;           /* Memory Buffer End Pointer */
} TransferDescriptor;

typedef struct HCCA {
	uint32_t InterruptTable[32];
	uint16_t FrameNumber;
	uint16_t Pad1;
	uint32_t HccaDoneHead;
} HCCA;

static void
update_interrupts(OhciHC *hc) {
	uint32_t ien=hc->HcInterruptEnable;
	uint32_t ist=hc->HcInterruptStatus;
	if((ist & ien) && (ien & HCInterrupt_MIE)) {
		if(!hc->interrupt_posted) {
			SigNode_Set(hc->irqNode,SIG_LOW);
			hc->interrupt_posted=1;
		}
	} else {
		if(hc->interrupt_posted) {
			SigNode_Set(hc->irqNode,SIG_PULLUP);
			hc->interrupt_posted=0;
		}
	}
}

/*
 * -------------------------------------------------
 * Helpers for reading/writing the DMA descriptors
 * -------------------------------------------------
 */
static inline void
HcMasterWrite32(OhciHC *hc,uint32_t value,uint32_t addr) 
{
	if(hc->endian == TARGET_BYTEORDER) {
		return hc->bus->write32(value,addr);	
	} else {
		return hc->bus->write32(swap32(value),addr);	
	}
}

static inline int 
HcMasterRead32(OhciHC *hc,uint32_t addr) 
{
	if(hc->endian == TARGET_BYTEORDER) {
		return hc->bus->read32(addr);	
	} else {
		return swap32(hc->bus->read32(addr));	
	}
}

#define RET_SUCCESS        (0)
#define RET_NONE_AVAILABLE (1)
#define RET_END_OF_FRAME   (2)
#define RET_RETIRE 	   (4)
#define RET_ERROR 	   (32)
#define RET_BUG 	   (64)
/*
 * ---------------------------------------
 * Read Transfer Descriptors
 * ---------------------------------------
 */
static void
HcReadGTD(OhciHC *hc,GeneralTD *gtd,uint32_t addr) {
	gtd->hwControl=HcMasterRead32(hc,addr);
        gtd->hwCBP=HcMasterRead32(hc,addr+4);
        gtd->hwNextTD=HcMasterRead32(hc,addr+8);
        gtd->hwBE=HcMasterRead32(hc,addr+12);
}

static void
HcReadITD(OhciHC *hc,IsoTD *itd,uint32_t addr) {
	itd->hwControl=HcMasterRead32(hc,addr);
        itd->hwBP0=HcMasterRead32(hc,addr+4);
        itd->hwNextTD=HcMasterRead32(hc,addr+8);
        itd->hwBE=HcMasterRead32(hc,addr+12);
        itd->hwOffset01=HcMasterRead32(hc,addr+16);
        itd->hwOffset23=HcMasterRead32(hc,addr+20);
        itd->hwOffset45=HcMasterRead32(hc,addr+24);
        itd->hwOffset67=HcMasterRead32(hc,addr+28);
}

/*
 * -----------------------------------------
 * Retire Transfer Descriptors
 * -----------------------------------------
 */
static void
HcRetireGTD(OhciHC *hc,GeneralTD *gtd,uint32_t gtd_address) {
	uint32_t HccaDoneHead;
	HccaDoneHead=HcMasterRead32(hc,hc->HcHCCA+0x84);
	gtd->hwNextTD=HccaDoneHead;
	HcMasterWrite32(hc,gtd_address,hc->HcHCCA+0x84);
		
}
static void
HcRetireITD(OhciHC *hc,IsoTD *itd,uint32_t itd_address) {
	uint32_t HccaDoneHead;
	HccaDoneHead=HcMasterRead32(hc,hc->HcHCCA+0x84);
	itd->hwNextTD=HccaDoneHead;
	HcMasterWrite32(hc,itd_address,hc->HcHCCA+0x84);
		
}
/*
 * -------------------------------------
 * Read Enpoint descriptors
 * -------------------------------------
 */
static void
HcReadED(OhciHC *hc,EndPointDescriptor *ed,uint32_t addr) {
        ed->hwControl=HcMasterRead32(hc,addr);
        ed->hwTailP=HcMasterRead32(hc,addr+4);
        ed->hwHeadP=HcMasterRead32(hc,addr+8); 
        ed->hwNextED=HcMasterRead32(hc,addr+12); 
}

/*
 * -----------------------------------------------------------
 * Implementation of the state diagramm in Figure 6-7 p. 100
 * -----------------------------------------------------------
 */

static int
HcServiceGTD(OhciHC *hc,GeneralTD *gtd,EndPointDescriptor *ed) 
{
	uint32_t dir = ED_D(ed);
	uint32_t mps = ED_MPS(ed); 
	uint32_t cbp,be;
	int remaining,ps;
	if((dir==0) || (dir==3)) {
		dir=GTD_DP(gtd);
	}
	cbp=GTD_CBP(gtd);
	be=gtd->hwBE;
	if((cbp & 0xfff)==(be&0xfff)) {
		remaining= be-cbp;
	} else {
		remaining=0x1000-(cbp&0xfff) + (be & 0xfff);
	}
	if(remaining<0) {
		fprintf(stderr,"Transfer Descriptor Bug: less than 0 bytes remaining in buffer\n");
		return RET_RETIRE;
	}
	if(remaining > mps) {
		ps=mps;
	} else {
		ps=remaining;
	}
	
	if(dir==TD_PID_OUT) {
		fprintf(stderr,"out paket size %d\n",ps);
	} else if (dir==TD_PID_IN) {
		fprintf(stderr,"in paket size %d\n",ps);
	} else if (dir==TD_PID_SETUP) {
		fprintf(stderr,"setup paket size %d\n",ps);
	}	
	cbp += ps;
	if((cbp&0xfff) != (GTD_CBP(gtd)&0xfff)) {
		cbp=(cbp&0xfff) | (gtd->hwBE &~ 0xfffU);
	}
	gtd->hwCBP=cbp;
	if(gtd->hwCBP==gtd->hwBE) {
		return RET_RETIRE;
	} else {
		return RET_SUCCESS;
	}
}

static int 
HcServiceITD(OhciHC *hc,IsoTD *itd,EndPointDescriptor *ed) {
	//uint32_t dir = ED_D(ed);
	uint16_t fc=ITD_FC(itd);
	uint16_t startframe = ITD_SF(itd);
	int relframe=hc->HcFmNumber-startframe;
	uint32_t offset,page;
	uint32_t start_addr,end_addr;
	if(relframe > fc) {
		// error
		return RET_RETIRE;
	}
	if(relframe<0) {
		// do nothing, too early
		return RET_SUCCESS;
	}
	offset=*(&itd->hwOffset01+(fc>>1));
	offset=offset>>(16*(fc&1)) & 0x1fff;
	page=offset&0x1000;
	offset=offset&0xfff;
	if(page) {
		start_addr=(ITD_BE(itd)&~0xfff)+offset;
	} else {
		start_addr=ITD_BP(itd)+offset;
	}
	if(relframe==fc)  {
		end_addr=ITD_BE(itd);
	} else {
		offset=*(&itd->hwOffset01+((fc+1)>>1));
		offset=offset>>(16*((fc+1)&1)) & 0x1fff;
		page=offset&0x1000;
		offset=offset&0xfff;
		if(page) {
			end_addr=(ITD_BE(itd)&~0xfff)+offset-1;
		} else {
			end_addr=ITD_BP(itd)+offset-1;
		}
	}
	// SendISOPaket 
	if(relframe==fc) {
		return RET_RETIRE;
	}
	return RET_SUCCESS;
}

/*
 * ---------------------------------
 * Endpoint descriptor Servicing
 * ---------------------------------
 */

static void
HcServicePeriodicED(OhciHC *hc,EndPointDescriptor *ed)  
{
	int skip,halted,is_iso;
	int result;
	skip=ED_K(ed);
	halted=ED_H(ed);
	is_iso=ED_F(ed);
	if(halted || skip) {
		return;
	}
	if(ED_HEADP(ed)==ED_TAILP(ed)) {
		return;
	} 
	if(is_iso) {
		IsoTD itd;
		uint32_t itd_addr = ED_HEADP(ed);
		HcReadITD(hc,&itd,itd_addr);
		result=HcServiceITD(hc,&itd,ed); 
		if(result & RET_RETIRE) {
			uint32_t flags=ed->hwHeadP&0xf;
			ed->hwHeadP=(itd.hwNextTD&~0xf) | flags;
			HcRetireITD(hc,&itd,itd_addr);
		}
	} else {
		GeneralTD gtd;
		uint32_t gtd_addr = ED_HEADP(ed);
		HcReadGTD(hc,&gtd,gtd_addr);
		result=HcServiceGTD(hc,&gtd,ed);
		if(result & RET_RETIRE) {
			uint32_t flags=ed->hwHeadP&0xf;
			ed->hwHeadP=(gtd.hwNextTD&~0xf) | flags;
			HcRetireGTD(hc,&gtd,gtd_addr);
		}
	}
}
static void
HcServiceBulkED(OhciHC *hc,EndPointDescriptor *ed)  
{
	GeneralTD gtd;
	uint32_t gtd_addr;
	int skip,halted;
	int result;
	skip=ED_K(ed);
	halted=ED_H(ed);
	if(halted || skip) {
		return;
	}
	if(ED_HEADP(ed)==ED_TAILP(ed)) {
		return;
	} 
	hc->HcCommandStatus |= HcCommandStatus_BLF;
	gtd_addr = ED_HEADP(ed);
	HcReadGTD(hc,&gtd,gtd_addr);
	result=HcServiceGTD(hc,&gtd,ed);
	if(result & RET_RETIRE) {
		uint32_t flags=ed->hwHeadP&0xf;
		ed->hwHeadP=(gtd.hwNextTD&~0xf) | flags;
		HcRetireGTD(hc,&gtd,gtd_addr);
	}
}
static void
HcServiceControlED(OhciHC *hc,EndPointDescriptor *ed)  
{
	GeneralTD gtd;
	uint32_t gtd_addr;
	int skip,halted;
	int result;
	skip=ED_K(ed);
	halted=ED_H(ed);
	if(halted || skip) {
		return;
	}
	if(ED_HEADP(ed)==ED_TAILP(ed)) {
		return;
	} 
	hc->HcCommandStatus |= HcCommandStatus_CLF;
	gtd_addr = ED_HEADP(ed);
	result=HcServiceGTD(hc,&gtd,ed);
	if(result & RET_RETIRE) {
		uint32_t flags=ed->hwHeadP&0xf;
		ed->hwHeadP=(gtd.hwNextTD&~0xf) | flags;
		HcRetireGTD(hc,&gtd,gtd_addr);
	}
}
/*
 * -----------------------------------------------------------
 * Implementation of the state diagramm in Figure 6-5 p. 96 
 * -----------------------------------------------------------
 */


static int
HcServicePeriodicList(OhciHC *hc) {
	int iso;
	uint32_t nextED;
	EndPointDescriptor ed;
	unsigned int list_nr; 
	list_nr=hc->HcFmNumber&31;
	/* Check Periodic List enable */
	if(!(hc->HcControl & HcControl_PLE)) {
		return RET_NONE_AVAILABLE;
	}
	hc->HcPeriodCurrentED = nextED = HcMasterRead32(hc,hc->HcHCCA+(list_nr<<2));
	if(!nextED) {
		return RET_NONE_AVAILABLE;
	}
	HcReadED(hc,&ed,nextED);	
	while(1) {
		iso=ED_F(&ed);	
		if(iso) {
			/* Check ISO List enable */
			if(!(hc->HcControl & HcControl_IE)) {
				return RET_SUCCESS;
			}
		}
		HcServicePeriodicED(hc,&ed);	
		nextED=ED_NEXTED(&ed);
		if(!nextED)
			return RET_SUCCESS;
		HcReadED(hc,&ed,nextED);	
	}
	
}
static int
HcServiceListBulk(OhciHC *hc) {
	uint32_t nextED;
	EndPointDescriptor ed;
	if(!(hc->HcControl & HcControl_BLE)) {
		printf("NBLE\n");
		return RET_NONE_AVAILABLE;
	}
	printf("Bulk\n");
	nextED=hc->HcBulkCurrentED;
	if(nextED==0) {
		if(!(hc->HcCommandStatus & HcCommandStatus_BLF)) {
			return RET_NONE_AVAILABLE;
		}      
		nextED=hc->HcBulkCurrentED=hc->HcBulkHeadED;
		hc->HcCommandStatus &= ~HcCommandStatus_BLF;
		if(!nextED) {
			return RET_NONE_AVAILABLE;
		}
	}
	HcReadED(hc,&ed,nextED);	
	HcServiceBulkED(hc,&ed);	
	return RET_SUCCESS;
}

static int
HcServiceListControl(OhciHC *hc) {
	uint32_t nextED;
	EndPointDescriptor ed;
	if(!(hc->HcControl & HcControl_CLE)) {
		printf("NCLE\n");
		return RET_NONE_AVAILABLE;
	}
	printf("Control\n");
	nextED=hc->HcControlCurrentED;
	if(nextED==0) {
		if(!(hc->HcCommandStatus & HcCommandStatus_CLF)) {
			return RET_NONE_AVAILABLE;
		}      
		nextED=hc->HcControlCurrentED=hc->HcControlHeadED;
		hc->HcCommandStatus &= ~HcCommandStatus_CLF;
		if(!nextED) {
			return RET_NONE_AVAILABLE;
		}
	}
	HcReadED(hc,&ed,nextED);	
	HcServiceControlED(hc,&ed);	
	return RET_SUCCESS;
}

static int
HcServiceNonPeriodicList(OhciHC *hc) {
	int i;
	for(i=0;i<2;i++) {
		if(hc->cbs_count==0) {
			hc->cbs_count=HcControl_CBSR(hc->HcControl)+1;
			if(hc->HcControl & HcControl_BLE) {
				if(HcServiceListBulk(hc)!=RET_NONE_AVAILABLE) {
					return RET_SUCCESS;
				}	
			}
		} else {
			hc->cbs_count--;
			if(hc->HcControl & HcControl_CLE) {
				if(HcServiceListControl(hc)!=RET_NONE_AVAILABLE) {
					return RET_SUCCESS;
				}	
			}
			hc->cbs_count=0; // skip unnecessary retries
		}
	}
	return RET_NONE_AVAILABLE;
}

/*
 * -------------------------------------
 * Do one Frame (1ms)
 * -------------------------------------
 */

static inline void
HcOneFrame(OhciHC *hc) {
	int count=0;
	int result;
	hc->HcFmRemaining = hc->HcFmInterval & (HcFmInterval_FI_MASK | HcFmInterval_FIT);
	hc->LargestPacketCounter= HcFmInterval_FSMPS(hc->HcFmInterval);

	hc->HcCommandStatus  &= ~(HcCommandStatus_CLF | HcCommandStatus_BLF);

	while(hc->HcFmRemaining > hc->HcPeriodicStart) {
		result=HcServiceNonPeriodicList(hc);
		if(result==RET_NONE_AVAILABLE) {
			break;
		} else if(result==RET_END_OF_FRAME) {
			// HCDriver bug
			fprintf(stderr,"End of frame before Periodic start Should never be\n");
		}
		count++;
		if(count>1000)
			goto bug;
	}	
	HcServicePeriodicList(hc);
	while(1) {
		result=HcServiceNonPeriodicList(hc);
		if(result==RET_NONE_AVAILABLE) {
			break;
		} else if(result==RET_END_OF_FRAME) {
			// This is no bug for periodic	
			break;
		}
		count++;
		if(count>1000)
			goto bug;
	}	
	return;
bug:	
	fprintf(stderr,"BUG To many transferdescriptors \n");
}

#if 0
static void 
HcFrameTimerProc(void *cd) {
	OhciHC *hc=cd;
	CycleTimer_Mod(&hc->timer,CycleTimerRate/1000,HcFrameTimerProc,hc);
	return HcOneFrame(hc);
}
#endif

static void
HcSoftReset(OhciHC *hc) {
	if(hc->timer_is_active) {
		hc->timer_is_active=0;
		CycleTimer_Remove(&hc->timer);
	}
	hc->HcControl=(hc->HcControl & (HcControl_RWC | HcControl_IR)) 
			| HcFunctionalStateUsbSuspend<<HcControl_HCFS_SHIFT;
	hc->HcCommandStatus=0;
	hc->HcInterruptStatus=0;
	hc->HcInterruptEnable=0;
	hc->HcHCCA=0;
	hc->HcPeriodCurrentED=0;
	hc->HcControlHeadED=0;
	hc->HcControlCurrentED=0;
	hc->HcBulkHeadED=0;
	hc->HcBulkCurrentED=0;
	hc->HcDoneHead=0;
	hc->HcFmInterval = 0x2edf;
	hc->HcFmRemaining=0;
	hc->HcFmNumber=0;
	hc->HcPeriodicStart=0; 
	hc->HcLSThreshold = 0x0628;

	hc->cbs_count=0;

	//hc->HcRhPortStatus[0]=HcRhPortStatus_CCS | HcRhPortStatus_CSC;
	hc->HcRhPortStatus[0]= HcRhPortStatus_CSC;
	update_interrupts(hc);
}

static void
HcHardReset(OhciHC *hc) {	
	if(hc->timer_is_active) {
		hc->timer_is_active=0;
		CycleTimer_Remove(&hc->timer);
	}
	hc->HcControl=0;
	hc->HcCommandStatus=0;
	hc->HcInterruptStatus=0;
	hc->HcInterruptEnable=0;
	hc->HcHCCA=0;
	hc->HcPeriodCurrentED=0;
	hc->HcControlHeadED=0;
	hc->HcControlCurrentED=0;
	hc->HcBulkHeadED=0;
	hc->HcBulkCurrentED=0;
	hc->HcDoneHead=0;
	hc->HcFmInterval = 0x2edf;
	hc->HcRhDescriptorA=0x80000201;
	hc->HcFmRemaining=0;
	hc->HcFmNumber=0;
	hc->HcPeriodicStart=0; 
	hc->HcLSThreshold = 0x0628;

	hc->cbs_count=0;
	//hc->HcRhPortStatus[0]=HcRhPortStatus_CCS | HcRhPortStatus_CSC;
	hc->HcRhPortStatus[0]= HcRhPortStatus_CSC;
	update_interrupts(hc);
}

/*
 * ---------------------------
 * Registers
 * ---------------------------
 */
static uint32_t 
HcRevision_read(void *clientData,uint32_t address,int rqlen) {
        dbgprintf("HcRevision read\n");
        return 0x10;
}
static void 
HcRevision_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
        dbgprintf("HcRevision not writable\n");
        return;
}
static uint32_t
HcControl_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *ohci=clientData;
        dbgprintf("HcControl: unhandled read of addr %08x\n",address);
        return ohci->HcControl;
}

/*
 * ---------------------------------------------------
 * Control Register
 * USB-States: Chapter 6 p. 87
 * ---------------------------------------------------
 */

static void 
HcControl_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	uint32_t fState,oldFState;
	//uint32_t diff=hc->HcControl^value;;
        dbgprintf("HcControl unhandled write of addr %08x, value %08x\n",address,value);
	fState=(value & HcControl_HCFS_MASK)>>HcControl_HCFS_SHIFT;
	oldFState=(hc->HcControl & HcControl_HCFS_MASK)>>HcControl_HCFS_SHIFT;

	if(oldFState==HcFunctionalStateUsbReset && ((oldFState!=fState))) {
		int ms = CyclesToMilliseconds(CycleCounter_Get() - hc->UsbEnterResetTime);
		if(ms<10) {
			fprintf(stderr,"OHCI Warning: UsbResetState was not kept 10ms CPU-Cycles: %lld\n",(unsigned long long)(CycleCounter_Get() - hc->UsbEnterResetTime));
		}
	}
	/* 
	   On Resume we switch to Operational immediately, should
	   be delayed by ???? 
	*/
	if((fState==HcFunctionalStateUsbReset) && (oldFState != HcFunctionalStateUsbReset)) {
		hc->UsbEnterResetTime=CycleCounter_Get();
	}
	if((fState==HcFunctionalStateUsbResume) && (oldFState!=fState)) {
		fState=HcFunctionalStateUsbOperational;
	} 
	if((fState==HcFunctionalStateUsbOperational) && (oldFState!=fState)) {
		hc->HcFmRemaining = hc->HcFmInterval & (HcFmInterval_FI_MASK | HcFmInterval_FIT);
	} 
	if(fState==HcFunctionalStateUsbOperational) {
		if(!hc->timer_is_active) {
#if 0
			hc->timer_is_active=1;
			CycleTimer_Mod(&hc->timer,CycleTimerRate/1000,HcFrameTimerProc,hc);	
#endif
		}
	} else {
		if(hc->timer_is_active) {
			hc->timer_is_active=0;
			CycleTimer_Remove(&hc->timer);
		}
	}
	//fprintf(stderr,"New fstate %d, value %08x\n",fState,value);
	hc->HcControl=value;	
        return;
}
/*
 * ----------------------------------------------------------------------------------
 *  Command Status Register
 * ----------------------------------------------------------------------------------
 */
static uint32_t 
HcCommandStatus_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcCommandStatus: unhandled read of addr %08x\n",address);
        return hc->HcCommandStatus;
}

static void 
HcCommandStatus_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcCommandStatus unhandled write of addr %08x, value %08x\n",address,value);
	hc->HcCommandStatus=value;
	if(value & HcCommandStatus_HCR) {
		HcSoftReset(hc); 
	} 
        return;
}

static uint32_t 
HcInterruptStatus_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcInterruptStatus: read of value %08x\n",hc->HcInterruptStatus);
        return hc->HcInterruptStatus;
}

static void 
HcInterruptStatus_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcInterruptStatus write value %08x\n",value);
	hc->HcInterruptStatus &=  ~value;
	update_interrupts(hc);
        return;
}

static uint32_t 
HcInterruptEnable_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        return hc->HcInterruptEnable;
}
/*
 * ----------------------------------------------
 * Enable and Disable Interrupts
 * 	Using enable/disable masks allows 
 *      atomic modification
 * ----------------------------------------------
 */
static void 
HcInterruptEnable_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcInterruptEnable unhandled write of addr %08x, value %08x\n",address,value);
	hc->HcInterruptEnable |= value;
	update_interrupts(hc);
        return;
}
static uint32_t 
HcInterruptDisable_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        return hc->HcInterruptEnable;
}
static void 
HcInterruptDisable_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcInterruptEnable unhandled write of addr %08x, value %08x\n",address,value);
	hc->HcInterruptEnable &= ~value;
	update_interrupts(hc);
        return;
}
/*
 * ---------------------------------------------------------
 * Host Controller Communication Area
 *	defined in Chapter 4.4 p. 33 
 * ---------------------------------------------------------
 */
static uint32_t 
HcHCCA_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcHCCA: read value %08x\n",*value);
        return hc->HcHCCA;
}

static void 
HcHCCA_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	if((value&0xff)!=0) {
		fprintf(stderr,"Driver Bug: OHCI-HCCA not 256 Byte aligned\n");
	} 
	hc->HcHCCA=value;
        return;
}

static uint32_t 
HcPeriodCurrentED_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        return hc->HcPeriodCurrentED;
}

static void 
HcPeriodCurrentED_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	hc->HcPeriodCurrentED=value&~0xfU;
        return;
}

static uint32_t 
HcControlHeadED_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        return hc->HcControlHeadED;
}

static void 
HcControlHeadED_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	hc->HcControlHeadED=value&~0xfU;
        return;
}

static uint32_t 
HcControlCurrentED_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	return hc->HcControlCurrentED;
}
static void 
HcControlCurrentED_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	hc->HcControlCurrentED=value&~0xfU;
        return;
}
static uint32_t 
HcBulkHeadED_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        return hc->HcBulkHeadED;
}
static void 
HcBulkHeadED_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	hc->HcBulkHeadED=value&~0xfU;
        return;
}

static uint32_t 
HcBulkCurrentED_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        return hc->HcBulkCurrentED;
}
static void 
HcBulkCurrentED_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	hc->HcBulkCurrentED=value&~0xfU;
        return;
}

static uint32_t 
HcDoneHead_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	return hc->HcDoneHead;
}

static void 
HcDoneHead_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
        fprintf(stderr,"Driver Bug: HcDoneHead is not writable\n");
        return;
}

static uint32_t 
HcFmInterval_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        return hc->HcFmInterval;
}
static void 
HcFmInterval_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	hc->HcFmInterval=value;
	dbgprintf("FSLargestDataPacket: %d\n",HcFmInterval_FSMPS(value));
        return;
}

static uint32_t 
HcFmRemaining_read(void *clientData,uint32_t address,int rqlen) {
	// calculate remaining from CPU-cycles
        dbgprintf("HcFmRemaining: unhandled read of addr %08x\n",address);
        return 0;
}

static void 
HcFmRemaining_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
        fprintf(stderr,"Driver Bug: HcFmRemaining is not writable");
        return;
}

static uint32_t 
HcFmNumber_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcFmNumber: read value %08x\n",*value);
        return hc->HcFmNumber;
}

static void
HcFmNumber_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
        dbgprintf("HcFmNumber: Not writable by HCD\n");
        return;
}

static uint32_t 
HcPeriodicStart_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcPeriodicStart: read value %08x\n",*value);
        return hc->HcPeriodicStart;
}

static void 
HcPeriodicStart_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	hc->HcPeriodicStart=value;
        dbgprintf("HcPeriodicStart: unhandled write of addr %08x, value %08x\n",address,value);
        //printf("HcPeriodicStart:  write of addr %08x, value %08x\n",address,value);
        return;
}
static uint32_t 
HcLSThreshold_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcLSThreshold: unhandled read of addr %08x\n",address);
        return hc->HcLSThreshold;
}
static void 
HcLSThreshold_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	hc->HcLSThreshold=value;
        dbgprintf("HcLSThreshold:  write of value %08x\n",value);
        return;
}

/*
 * -------------------------------------------------------
 * Root Hub with one Port
 * -------------------------------------------------------
 */
static uint32_t 
HcRhDescriptorA_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
    //    dbgprintf("HcRhDescriptorA: read of value %08x\n",*value);
        return hc->HcRhDescriptorA;
}
static void 
HcRhDescriptorA_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	uint32_t writemask = HcRhDescriptorA_NPS | HcRhDescriptorA_PSM | 
		HcRhDescriptorA_OCPM | HcRhDescriptorA_POTPGT_MASK;
	hc->HcRhDescriptorA=(value&writemask) |(hc->HcRhDescriptorA & ~writemask);
        dbgprintf("HcRhDescriptorA: write of value %08x\n",value);
        return;
}
static uint32_t 
HcRhDescriptorB_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcRhDescriptorB: raad value %08x\n",*value);
        return hc->HcRhDescriptorB;
}
static void 
HcRhDescriptorB_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcRhDescriptorB: write value %08x\n",value);
	hc->HcRhDescriptorB=value;	
        return;
}
static uint32_t 
HcRhStatus_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
     //   dbgprintf("HcRhStatus: read value %08x\n",*value);
        return hc->HcRhStatus;
}
static void 
HcRhStatus_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
        dbgprintf("HcRhDescriptorB: write value %08x\n",value);
	hc->HcRhStatus=value;	
        return;
}
static uint32_t 
HcRhPortStatus_read(void *clientData,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	uint32_t port=((address&0xff)-OHCI_HcRhPortStatus(0))>>2;
	if(port>=MAX_NDP) {
		fprintf(stderr,"Error Trying to read Port Status of port %d\n",port); 
		return -1;
	}
        dbgprintf("HcRhPortStatus:  read port %d value %08x\n",port,*value);
        return hc->HcRhPortStatus[port];
}
/*
 * -------------------------------------------------------------------------
 * Some Port Status flags are cleared by write 1 some are set by write 1  
 * -------------------------------------------------------------------------
 */ 
static void 
HcRhPortStatus_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	OhciHC *hc=clientData;
	uint32_t tcmask = 0x000f0000;
	uint32_t tsmask = 0x100;
	uint32_t tc = value & tcmask;
	uint32_t ts = value & tsmask;
	uint32_t port=((address&0xff)-OHCI_HcRhPortStatus(0))>>2;
	if(port>=MAX_NDP) {
		fprintf(stderr,"Device driver Bug: Trying to write Port Status of port %d\n",port); 
		return;
	}
	if(value & HcRhPortStatus_CCS) {
		tc |= HcRhPortStatus_PES;
	} 
	if(value & HcRhPortStatus_PES) {
		if(!(hc->HcRhPortStatus[port] & HcRhPortStatus_CCS)) {
			ts |= HcRhPortStatus_CSC; // status change
		} else {
			ts |= HcRhPortStatus_PES; 
		}
	}

	if(value & HcRhPortStatus_PSS) {
		if(!(hc->HcRhPortStatus[port] & HcRhPortStatus_CCS)) {
			ts|= HcRhPortStatus_CSC; // status change
		} else {
			ts|= HcRhPortStatus_PPS; 
		}
	}
	if(value & HcRhPortStatus_POCI) {
		tc|=HcRhPortStatus_PSS;
	}
	if(value & HcRhPortStatus_PRS) {
		// should be delayed 10ms
		if(hc->HcRhPortStatus[port] & HcRhPortStatus_CCS) {
			ts |= HcRhPortStatus_PRSC;
			ts |= HcRhPortStatus_PES;
			tc |= HcRhPortStatus_PRS;
		}
	} 
	if(value & HcRhPortStatus_PRSC)  {
		tc|= HcRhPortStatus_PRSC; 
	}
	if(value & HcRhPortStatus_CSC) {
//		fprintf(stderr,"Clear connection Status change at %08x\n",GET_REG_PC);
	}
	if(value & HcRhPortStatus_LSDA) {
		tc|= HcRhPortStatus_PPS;
	}
        dbgprintf("HcRhPortStatus: write of value %08x to port %d\n",value,port);
	hc->HcRhPortStatus[port] = (hc->HcRhPortStatus[port] & ~tc) |ts;
//	fprintf(stderr,"new RhPortStatus %08x\n",hc->HcRhPortStatus[port]); 
        return;
}

/*
 * ----------------------------------------------------------------
 * Bus Interface functions
 * ----------------------------------------------------------------
 */

/*
 * ----------------------------------------------------
 * In Reset Mode all Registers are mapped readonly
 * ----------------------------------------------------
 */
static void
MapReadonly(uint32_t address,IOReadProc *readproc,IOWriteProc *writeproc,void *clientData) 
{
	return IOH_New32(address,readproc,NULL,clientData);
}

typedef void MapProc32(uint32_t address,IOReadProc *readproc,IOWriteProc *writeproc,void *clientData);

static void
OhciHC_Map(void *module_owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	OhciHC *hc=module_owner;
	int i;
	MapProc32 *map;
	if(hc->hold_reset) {
		map=MapReadonly;
	} else {
		map=IOH_New32;
	}
	//fprintf(stderr,"map OHCI at %08x\n",base);
	map(base+OHCI_HcRevision,HcRevision_read,HcRevision_write,hc);
	map(base+OHCI_HcControl,HcControl_read,HcControl_write,hc);
	map(base+OHCI_HcCommandStatus,HcCommandStatus_read,HcCommandStatus_write,hc);
	map(base+OHCI_HcInterruptStatus,HcInterruptStatus_read,HcInterruptStatus_write,hc);
	map(base+OHCI_HcInterruptEnable,HcInterruptEnable_read,HcInterruptEnable_write,hc);
	map(base+OHCI_HcInterruptDisable,HcInterruptDisable_read,HcInterruptDisable_write,hc);
	map(base+OHCI_HcHCCA,HcHCCA_read,HcHCCA_write,hc);
	map(base+OHCI_HcPeriodCurrentED,HcPeriodCurrentED_read,HcPeriodCurrentED_write,hc);
	map(base+OHCI_HcControlHeadED,HcControlHeadED_read,HcControlHeadED_write,hc);
	map(base+OHCI_HcControlCurrentED,HcControlCurrentED_read,HcControlCurrentED_write,hc);
	map(base+OHCI_HcBulkHeadED,HcBulkHeadED_read,HcBulkHeadED_write,hc);
	map(base+OHCI_HcBulkCurrentED,HcBulkCurrentED_read,HcBulkCurrentED_write,hc);
	map(base+OHCI_HcDoneHead,HcDoneHead_read,HcDoneHead_write,hc);
	map(base+OHCI_HcFmInterval,HcFmInterval_read,HcFmInterval_write,hc);
	map(base+OHCI_HcFmRemaining,HcFmRemaining_read,HcFmRemaining_write,hc);
	map(base+OHCI_HcFmNumber,HcFmNumber_read,HcFmNumber_write,hc);
	map(base+OHCI_HcPeriodicStart,HcPeriodicStart_read,HcPeriodicStart_write,hc);
	map(base+OHCI_HcLSThreshold,HcLSThreshold_read,HcLSThreshold_write,hc);
	map(base+OHCI_HcRhDescriptorA,HcRhDescriptorA_read,HcRhDescriptorA_write,hc);
	map(base+OHCI_HcRhDescriptorB,HcRhDescriptorB_read,HcRhDescriptorB_write,hc);
	map(base+OHCI_HcRhStatus,HcRhStatus_read,HcRhStatus_write,hc);
	for(i=0;i<MAX_NDP;i++) {
		map(base+OHCI_HcRhPortStatus(i),HcRhPortStatus_read,HcRhPortStatus_write,hc);
	}
}

static void
OhciHC_UnMap(void *module_owner,uint32_t base,uint32_t mask)
{
	int i;
	for(i=0;i<256;i+=4) {
		IOH_Delete32(base+i);	
	}
}

/*
 * HardReset the hc
 */
void
OhciHC_Disable(BusDevice *dev) {
	OhciHC *hc=dev->owner;
	hc->hold_reset=1;
	HcHardReset(hc);
	Mem_AreaUpdateMappings(dev);
}

void
OhciHC_Enable(BusDevice *dev) {
	OhciHC *hc=dev->owner;
	hc->hold_reset=0;
	Mem_AreaUpdateMappings(dev);
}

/*
 * -------------------------------------------------------
 * change_endian_of_dma
 *      Invoked when the Endian Signal Line changes
 *	No clue what the real device does here
 * -------------------------------------------------------
 */
static void 
change_endian(SigNode *node,int value,void *clientData)
{
        OhciHC *hc = clientData;
        if(value == SIG_HIGH) {
                hc->endian = en_BIG_ENDIAN;
		fprintf(stderr,"USB OHCI is now big endian\n");
        } else if(value==SIG_LOW) {
                hc->endian = en_LITTLE_ENDIAN;
        } else {
                fprintf(stderr,"OHCI: Endian is neither Little nor Big\n");
                exit(3424);
        }
}

/*
 *---------------------------------------------------------------
 *
 * Create an OHCI Host Controller
 *	Needs  parent bus and the interrupt number
 *	on the parent bus as arguments
 *
 *---------------------------------------------------------------
 */
BusDevice *
OhciHC_New(char *name,Bus *bus) {
	OhciHC *hc = sg_new(OhciHC);	
	hc->irqNode = SigNode_New("%s.irq",name);
	if(!hc->irqNode) {
		fprintf(stderr,"Can not create irq signal\n");
		exit(1);
	}

	hc->endian = en_LITTLE_ENDIAN;
        hc->endianNode = SigNode_New("%s.endian",name);
        if(!hc->endianNode) {
                exit(3429);
        }
       	hc->endianTrace = SigNode_Trace(hc->endianNode,change_endian,hc);

	hc->hold_reset=1; // Enable the device before usage ! 
	hc->bdev.first_mapping=NULL;
        hc->bdev.Map=OhciHC_Map;
        hc->bdev.UnMap=OhciHC_UnMap;
        hc->bdev.owner=hc;
        hc->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	hc->bus=bus;
	HcHardReset(hc);
	fprintf(stderr,"USB-OHCI module created\n");
	return &hc->bdev;
}
