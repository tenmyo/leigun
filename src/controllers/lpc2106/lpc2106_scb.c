/*
 *************************************************************************************************
 * LPC2106 SCB 
 *      Emulation of the system control block of the Philips LPC2106
 *
 * Status: partially implemented 
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
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

#define FOSC	(14745600) 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "sgstring.h"
#include "clock.h"
#include "bus.h"
#include "signode.h"
#include "cycletimer.h"
#include "configfile.h"

/* Memory accellerator module */
#define MAMCR		(0xe01fc000)
#define	MAMTIM		(0xe01fc004)

/* Pin connect block */
#define PINSEL0		(0xe002c000)
#define PINSEL1		(0xe002c004)


#define SCB_EXTINT	0xe01fc140
#define SCB_EXTWAKE	0xe01fc144
#define SCB_MEMMAP	0xe01fc040
#define SCB_PLLCON	0xe01fc080
#define		PLLCON_PLLE	(1)
#define		PLLCON_PLLC	(1<<1)
#define SCB_PLLCFG	0xe01fc084
#define		PLLCFG_MSEL_SHIFT 	(0)
#define		PLLCFG_MSEL_MASK 	(0x1f)
#define		PLLCFG_PSEL_SHIFT 	(5)
#define		PLLCFG_PSEL_MASK 	(3<<5)
#define	SCB_PLLSTAT	0xe01fc088
#define		PLLSTAT_MSEL_SHIFT	(0)
#define		PLLSTAT_MSEL_MASK	(0x1f)
#define		PLLSTAT_PSEL_SHIFT	(5)
#define		PLLSTAT_PSEL_MASK	(3<<5)
#define		PLLSTAT_PLLE		(1<<8)
#define		PLLSTAT_PLLC		(1<<9)
#define		PLLSTAT_PLOCK		(1<<10)

#define SCB_PLLFEED	0xe01fc08c
#define SCB_PCON	0xe01fc0c0
#define SCB_PCONP	0xe01fc0c4
#define	SCB_VPBDIV	0xe01fc100

#define PLLFEED_IDLE 	(0)
#define PLLFEED_55	(1)
#define PLLFEED_AA	(2)

typedef struct SCB {
	BusDevice bdev;
	Clock_t *clk_osc;   /* connected to the  CPU oscillator input */
	Clock_t *clk_cclk;  /* The CPU clock output 		    */
	Clock_t *clk_fcco;  /* The internal oscillator between 156MHz and 320MHz */
	Clock_t *clk_pclk;  /* The peripheral clock */
	CycleTimer dumptimer;

	/* Memory accellerator */
	uint32_t mamcr;
	uint32_t mamtim;

	uint32_t pinsel0;
	uint32_t pinsel1;

	int pllfeed_state;
	uint32_t pllcon;
	uint32_t pllcfg;
	uint32_t pllstat;
	uint32_t vpbdiv;
	CycleCounter_t pllLockTime;
	SigNode *extIntNode[3];
} SCB;

/* Memory accellerator module */
static uint32_t 
mamcr_read(void *clientData,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	return scb->mamcr;
}

static void
mamcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	scb->mamcr = value & 3;
	return;
}

static uint32_t 
mamtim_read(void *clientData,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	return scb->mamtim;
}

static void
mamtim_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	scb->mamtim = value & 7; 	
}

/*
 * -----------------------------------------------------------
 * Pinsel 0 for IO-Ports 0-15
 * -----------------------------------------------------------
 */
static uint32_t 
pinsel0_read(void *clientData,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	return scb->pinsel0;
}

static void
pinsel0_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	/* Currently no effect */
	scb->pinsel0 = value;
}

/*
 * --------------------------------------------------------------------
 * Pinsel 1 for IO-Ports 16-31
 * --------------------------------------------------------------------
 */
static uint32_t 
pinsel1_read(void *clientData,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	return scb->pinsel1;
}

static void
pinsel1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	/* currently no effect */
	scb->pinsel1 = value;
}

static uint32_t
scb_extint_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"reg %08x not implemented\n",address);
        return 0;
}

static void
scb_extint_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	fprintf(stderr,"reg %08x not implemented\n",address);
}

static uint32_t
scb_extwake_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"reg %08x not implemented\n",address);
        return 0;
}

static void
scb_extwake_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	fprintf(stderr,"reg %08x not implemented\n",address);
}

static uint32_t
scb_memmap_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"reg %08x not implemented\n",address);
        return 0;
}

static void
scb_memmap_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	fprintf(stderr,"reg %08x not implemented\n",address);
}

static uint32_t
scb_pllcon_read(void *clientData,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
        return scb->pllcon;
}

static void
scb_pllcon_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	scb->pllcon = value;
}

static uint32_t
scb_pllcfg_read(void *clientData,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	return scb->pllcfg;
}

static void
scb_pllcfg_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	scb->pllcfg = value;
}

static uint32_t
scb_pllstat_read(void *clientData,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	uint32_t val=scb->pllstat;
	if((scb->pllstat & PLLCON_PLLE) && (CycleCounter_Get() >= scb->pllLockTime)) {
		val |= (1<<10);
	}
	return val;
}

static void
scb_pllstat_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	fprintf(stderr,"PLLSTAT reg %08x is not writable\n",address);
}

static uint32_t
scb_pllfeed_read(void *clientData,uint32_t address,int rqlen) 
{
	/* register is write only */ 
        return 0;
}

static void
update_pll_clocks(SCB *scb) 
{
	uint32_t msel,psel,m,p;
	uint32_t fcco,cclk;
	msel = (scb->pllcfg & PLLCFG_MSEL_MASK) >> PLLCFG_MSEL_SHIFT;
	psel = (scb->pllcfg & PLLCFG_PSEL_MASK) >> PLLCFG_PSEL_SHIFT;
	if((scb->pllcon & PLLCON_PLLC) && (scb->pllcon & PLLCON_PLLE)) {
		p = (1 << psel);
		m = msel + 1;
		Clock_MakeDerived(scb->clk_fcco,scb->clk_osc,m * 2 * p,1);
		Clock_MakeDerived(scb->clk_cclk,scb->clk_fcco, 1 , 2 * p);

		cclk = Clock_Freq(scb->clk_cclk);
		fcco = Clock_Freq(scb->clk_fcco); 
		if((cclk > 60000000) || (cclk < 10000000)) {
			fprintf(stderr,"PLL: Illegal CPU clock %d Hz, m %d, p %d\n",cclk,m,p);
		}
		if((fcco < 156000000) || (fcco > 320000000)) {
			fprintf(stderr,"PLL: Illegal Fcco frequency %d\n",fcco);
		}
//		fprintf(stderr,"**************************************CCLK: %d\n",cclk);
	} else {
		Clock_MakeDerived(scb->clk_cclk,scb->clk_osc,1,1);
	}
}


static void
scb_pllfeed_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB*) clientData;		
	if(value == 0xaa) {
		scb->pllfeed_state = PLLFEED_AA;
		return;
	}
	switch(scb->pllfeed_state) {
		case PLLFEED_IDLE:
			if(value == 0xaa) {
				scb->pllfeed_state = PLLFEED_AA;
			}
			break;
		case PLLFEED_AA:
			if(value == 0x55) {
				uint8_t msel,psel;
				scb->pllstat = 0;
				if(scb->pllcon & PLLCON_PLLE) {
					if(!(scb->pllstat & PLLSTAT_PLLE)) {
						scb->pllstat |= PLLSTAT_PLLE;
						scb->pllLockTime =  CycleCounter_Get() + MicrosecondsToCycles(500);
					}
				}
				msel = scb->pllcfg & PLLCFG_MSEL_MASK;
				psel = scb->pllcfg & PLLCFG_PSEL_MASK;
				scb->pllstat |= msel | psel;
				update_pll_clocks(scb);
			} else {
				scb->pllfeed_state = PLLFEED_IDLE;
			}
			break;
		default:
			scb->pllfeed_state = PLLFEED_IDLE;
			break;
	}
}

static uint32_t
scb_pcon_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"reg %08x not implemented\n",address);
        return 0;
}

static void
scb_pcon_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	fprintf(stderr,"reg %08x not implemented\n",address);
}

static uint32_t
scb_pconp_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"reg %08x not implemented\n",address);
        return 0;
}

static void
scb_pconp_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	fprintf(stderr,"reg %08x not implemented\n",address);
}

static uint32_t
scb_vpbdiv_read(void *clientData,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
        return scb->vpbdiv;
}

static void
scb_vpbdiv_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	SCB *scb = (SCB *) clientData;
	uint32_t divider;
	switch (value) {
		case 0:
			divider = 4;
			break;
		case 1:
			divider = 1;
			break;
		case 2:
			divider = 2;
			break;
		default:
			return;
	}
	scb->vpbdiv = value;
	Clock_MakeDerived(scb->clk_pclk,scb->clk_cclk,1,divider);
}

static void
SCB_UnMap(void *owner,uint32_t base,uint32_t mask) {
	IOH_Delete32(MAMCR);
	IOH_Delete32(MAMTIM);
	IOH_Delete32(PINSEL0);
	IOH_Delete32(PINSEL1);
	IOH_Delete32(SCB_EXTINT);
	IOH_Delete32(SCB_EXTWAKE);
	IOH_Delete32(SCB_MEMMAP);
	IOH_Delete32(SCB_PLLCON);
	IOH_Delete32(SCB_PLLCFG);
	IOH_Delete32(SCB_PLLSTAT);
	IOH_Delete32(SCB_PLLFEED);
	IOH_Delete32(SCB_PCON);
	IOH_Delete32(SCB_PCONP);
	IOH_Delete32(SCB_VPBDIV);
}

static void
SCB_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags) 
{

	SCB *scb = owner;
	/* Memory accellerator module */
	IOH_New32(MAMCR,mamcr_read,mamcr_write,scb);
	IOH_New32(MAMTIM,mamtim_read,mamtim_write,scb);

	/* Pin connect block */
	IOH_New32(PINSEL0,pinsel0_read,pinsel0_write,scb);
	IOH_New32(PINSEL1,pinsel1_read,pinsel1_write,scb);

	/* System control block */
	IOH_New32(SCB_EXTINT,scb_extint_read,scb_extint_write,scb);
	IOH_New32(SCB_EXTWAKE,scb_extwake_read,scb_extwake_write,scb);
	IOH_New32(SCB_MEMMAP,scb_memmap_read,scb_memmap_write,scb);
	IOH_New32(SCB_PLLCON,scb_pllcon_read,scb_pllcon_write,scb);
	IOH_New32(SCB_PLLCFG,scb_pllcfg_read,scb_pllcfg_write,scb);
	IOH_New32(SCB_PLLSTAT,scb_pllstat_read,scb_pllstat_write,scb);
	IOH_New32(SCB_PLLFEED,scb_pllfeed_read,scb_pllfeed_write,scb);
	IOH_New32(SCB_PCON,scb_pcon_read,scb_pcon_write,scb);
	IOH_New32(SCB_PCONP,scb_pconp_read,scb_pconp_write,scb);
	IOH_New32(SCB_VPBDIV,scb_vpbdiv_read,scb_vpbdiv_write,scb);
}

static void
dump_proc(void *cd)
{
	SCB *scb = (SCB *)cd;
        Clock_DumpTree(scb->clk_osc);
}

BusDevice *
LPC2106_ScbNew(const char *devname) 
{
	int i;
	uint32_t osc_freq;
	SCB *scb = sg_new(SCB);
	if(Config_ReadUInt32(&osc_freq,"global","oscillator") < 0) {
                osc_freq = 14745600;
        }
	for(i=0;i<3;i++) {
		scb->extIntNode[i] = SigNode_New("%s.extIrq%d",devname,i);
	}
	scb->mamtim = 7;
	scb->pllfeed_state = PLLFEED_IDLE;
	scb->bdev.first_mapping=NULL;
        scb->bdev.Map=SCB_Map;
        scb->bdev.UnMap=SCB_UnMap;
        scb->bdev.owner=scb;
        scb->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	scb->clk_osc = Clock_New("%s.clk",devname);
	scb->clk_cclk = Clock_New("%s.cclk",devname); 
	scb->clk_fcco = Clock_New("%s.fcco",devname); 
	scb->clk_pclk = Clock_New("%s.pclk",devname); 
	if(!scb->clk_osc || !scb->clk_cclk || !scb->clk_fcco || !scb->clk_pclk) {
		fprintf(stderr,"Can not create clocks for System control block\n");
		exit(1);
	}
	Clock_SetFreq(scb->clk_osc,osc_freq);	
	update_pll_clocks(scb);
	Clock_MakeDerived(scb->clk_pclk,scb->clk_cclk,1,4);
	fprintf(stderr,"LPC2106 System Control block created\n");
	CycleTimer_Add(&scb->dumptimer,(uint64_t)MillisecondsToCycles(1000),dump_proc,scb);

	return &scb->bdev;
}
