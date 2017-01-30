/*
 *************************************************************************************************
 *
 * Emulation of AT91RM9200 Timer Counter (TC) 
 *
 * state: very incomplete, only working with u-boot
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <bus.h>
#include <signode.h>
#include <cycletimer.h>
#include <clock.h>
#include <at91_tc.h>
#include <senseless.h>
#include "sgstring.h"

#define TC_CCR(base,chan) 	((base)+(chan)*0x40+0x00)
#define		CCR_CLKEN	(1<<0)
#define		CCR_CLKDIS	(1<<1)
#define		CCR_SWTRG	(1<<2)

#define TC_CMR(base,chan) 	((base)+(chan)*0x40+0x04)
/* Capture Mode */
#define		CMR_CM_TCCLKS_MASK	(7)
#define		CMR_CM_TCCLKS_SHIFT	(0)
#define		CMR_CM_CLKI		(1<<3)
#define		CMR_CM_BURST_MASK	(3<<4)
#define		CMR_CM_BURST_SHIFT	(4)
#define		CMR_CM_LDBSTOP		(1<<6)
#define		CMR_CM_LDBDIS		(1<<7)
#define		CMR_CM_ETRGEDG_MASK	(3<<8)
#define		CMR_CM_ETRGEDG_SHIFT	(8)
#define		CMR_CM_ABETRG		(1<<10)
#define		CMR_CM_CPCTRG		(1<<14)
#define		CMR_CM_WAVE		(1<<15)
#define		CMR_CM_LDRA_MASK	(3<<16)
#define		CMR_CM_LDRA_SHIFT	(16)
#define		CMR_CM_LDRB_MASK	(3<<18)
#define		CMR_CM_LDRB_SHIFT	(18)

/* Waveform Mode */
#define		CMR_WM_TCCLKS_MASK	(7)
#define		CMR_WM_TCCLKS_SHIFT	(0)
#define		CMR_WM_CLKI		(1<<3)
#define		CMR_WM_BURST_MASK	(3<<4)
#define		CMR_WM_BURST_SHIFT	(4)
#define		CMR_WM_CPCSTOP		(1<<6)
#define		CMR_WM_CPCDIS		(1<<7)
#define		CMR_WM_EEVTEDG_MASK	(3<<8)
#define		CMR_WM_EEVTEDG_SHIFT	(8)
#define		CMR_WM_EEVT_MASK	(3<<10)
#define		CMR_WM_EEVT_SHIFT	(10)
#define		CMR_WM_ENETRG		(1<<12)
#define		CMR_WM_WAVSEL_MASK	(3<<13)
#define		CMR_WM_WAVSEL_SHIFT	(13)
#define		CMR_WM_WAVE		(1<<15)
#define		CMR_WM_ACPA_MASK	(3<<16)
#define		CMR_WM_ACPA_SHIFT	(16)
#define		CMR_WM_ACPC_MASK	(3<<18)
#define		CMR_WM_ACPC_SHIFT	(18)
#define		CMR_WM_AEEVT_MASK	(3<<20)
#define		CMR_WM_AEEVT_SHIFT	(20)
#define		CMR_WM_ASWTRG_MASK	(3<<22)
#define		CMR_WM_ASWTRG_SHIFT	(22)
#define		CMR_WM_BCPB_MASK	(3<<24)
#define		CMR_WM_BCPB_SHIFT	(24)
#define		CMR_WM_BCPC_MASK	(3<<26)
#define		CMR_WM_BCPC_SHIFT	(26)
#define		CMR_WM_BEEVT_MASK	(3<<28)
#define		CMR_WM_BEEVT_SHIFT	(28)
#define		CMR_WM_BSWTRG_MASK	(3<<30)
#define		CMR_WM_BSWTRG_SHIFT	(30)

#define TC_CV(base,chan)	((base)+(chan)*0x40+0x10)
#define TC_RA(base,chan)	((base)+(chan)*0x40+0x14)
#define TC_RB(base,chan)	((base)+(chan)*0x40+0x18)
#define TC_RC(base,chan)	((base)+(chan)*0x40+0x1C)
#define TC_SR(base,chan)	((base)+(chan)*0x40+0x20)
#define		SR_COVFS	(1<<0)
#define		SR_LOVRS	(1<<1)
#define		SR_CPAS		(1<<2)
#define		SR_CPBS		(1<<3)
#define		SR_CPCS		(1<<4)
#define		SR_LDRAS	(1<<5)
#define		SR_LDRBS	(1<<6)
#define		SR_ETRGS	(1<<7)
#define		SR_CLKSTA	(1<<16)
#define		SR_MTIOA	(1<<17)
#define		SR_MTIOB	(1<<18)

#define TC_IER(base,chan)	((base)+(chan)*0x40+0x24)
#define		IER_COVFS	(1<<0)
#define		IER_LOVRS	(1<<1)
#define		IER_CPAS	(1<<2)
#define		IER_CPBS	(1<<3)
#define		IER_CPCS	(1<<4)
#define		IER_LDRAS	(1<<5)
#define		IER_LDRBS	(1<<6)
#define		IER_ETRGS	(1<<7)

#define TC_IDR(base,chan)	((base)+(chan)*0x40+0x28)
#define		IDR_COVFS	(1<<0)
#define		IDR_LOVRS	(1<<1)
#define		IDR_CPAS	(1<<2)
#define		IDR_CPBS	(1<<3)
#define		IDR_CPCS	(1<<4)
#define		IDR_LDRAS	(1<<5)
#define		IDR_LDRBS	(1<<6)
#define		IDR_ETRGS	(1<<7)

#define TC_IMR(base,chan)	((base)+(chan)*0x40+0x2C)
#define		IMR_COVFS	(1<<0)
#define		IMR_LOVRS	(1<<1)
#define		IMR_CPAS	(1<<2)
#define		IMR_CPBS	(1<<3)
#define		IMR_CPCS	(1<<4)
#define		IMR_LDRAS	(1<<5)
#define		IMR_LDRBS	(1<<6)
#define		IMR_ETRGS	(1<<7)

#define TC_BCR(base)		((base)+0xc0)
#define		BCR_SYNC	(1<<0)

#define TC_BMR(base)		((base)+0xc4)
#define		TC2XC2S_MASK	(3<<4)
#define		TC2XC2S_SHIFT	(4)
#define		TC1XC1S_MASK	(3<<2)
#define		TC1XC1S_SHIFT	(2)
#define		TC0XC0S_MASK	(3)
#define		TC0XC0S_SHIFT	(0)

typedef struct AT91Tc AT91Tc;

/* One channel in the timer Controller */
typedef struct AT91TcChannel {
	AT91Tc *tc;
	CycleCounter_t last_timer_update;
	CycleCounter_t remainder;
	uint32_t ccr;
	uint32_t cmr;
	uint16_t cv;
	uint16_t ra;
	uint16_t rb;
	uint16_t rc;
	uint32_t sr;
	uint8_t imr;
	SigNode *irqNode;
	Clock_t *xc0;
	Clock_t *xc1;
	Clock_t *xc2;

	Clock_t *mck;
	Clock_t *slck;
	Clock_t *timer_clock1;
	Clock_t *timer_clock2;
	Clock_t *timer_clock3;
	Clock_t *timer_clock4;
	Clock_t *timer_clock5;

	Clock_t *clk;		/* The real input to the counter */
} AT91TcChannel;

/* The timer Controller */
struct AT91Tc {
	BusDevice bdev;
	uint32_t bmr;
	AT91TcChannel chan[3];
};

static void
update_interrupt(AT91TcChannel * tcchan)
{
	if (tcchan->sr & tcchan->imr) {
		SigNode_Set(tcchan->irqNode, SIG_HIGH);
	} else {
		SigNode_Set(tcchan->irqNode, SIG_PULLDOWN);
	}
}

static void
actualize_counter(AT91TcChannel * tcchan)
{
	uint64_t timer_cycles;
	uint32_t cmr = tcchan->cmr;
	uint32_t period;
	if (cmr & CMR_CM_CPCTRG) {
		period = tcchan->rc;
	} else {
		period = 65536;
	}
	if (!period) {
		tcchan->cv = 0;
		return;
	}
	if (!Clock_Freq(tcchan->clk)) {
		return;
	}
	tcchan->remainder += CycleCounter_Get() - tcchan->last_timer_update;
	tcchan->last_timer_update = CycleCounter_Get();
	if (tcchan->remainder > (uint64_t) 0x80000000) {
		timer_cycles = tcchan->remainder / (CycleTimerRate_Get() / Clock_Freq(tcchan->clk));
		tcchan->remainder -=
		    timer_cycles * (CycleTimerRate_Get() / Clock_Freq(tcchan->clk));
	} else {
		timer_cycles = Clock_Freq(tcchan->clk) * tcchan->remainder / CycleTimerRate_Get();
		tcchan->remainder -= timer_cycles * CycleTimerRate_Get() / Clock_Freq(tcchan->clk);
	}
	tcchan->cv = (tcchan->cv + timer_cycles) % period;

}

/* Update the frequency of the input to the 16 Bit counter */
static void
update_clk(AT91TcChannel * tcchan)
{
	uint32_t tcclks = (tcchan->cmr & CMR_WM_TCCLKS_MASK);
	int clken = !!(tcchan->ccr & CCR_CLKEN);
	switch (tcclks) {
	    case 0:
		    Clock_MakeDerived(tcchan->clk, tcchan->timer_clock1, clken, 1);
		    break;
	    case 1:
		    Clock_MakeDerived(tcchan->clk, tcchan->timer_clock2, clken, 1);
		    break;
	    case 2:
		    Clock_MakeDerived(tcchan->clk, tcchan->timer_clock3, clken, 1);
		    break;
	    case 3:
		    Clock_MakeDerived(tcchan->clk, tcchan->timer_clock4, clken, 1);
		    break;
	    case 4:
		    Clock_MakeDerived(tcchan->clk, tcchan->timer_clock5, clken, 1);
		    break;
	    case 5:
		    Clock_MakeDerived(tcchan->clk, tcchan->xc0, clken, 1);
		    break;
	    case 6:
		    Clock_MakeDerived(tcchan->clk, tcchan->xc1, clken, 1);
		    break;
	    case 7:
		    Clock_MakeDerived(tcchan->clk, tcchan->xc2, clken, 1);
		    break;
	    default:
		    break;
	}
}

static uint32_t
ccr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Tc: CCR is writeonly\n");
	return 0;
}

static void
ccr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	/* CLKDIS has precedence according to manual */
	if (value & CCR_CLKDIS) {
		tcchan->ccr &= ~CCR_CLKEN;
		tcchan->ccr |= ~CCR_CLKDIS;
	} else if (value & CCR_CLKEN) {
		tcchan->ccr &= ~CCR_CLKDIS;
		tcchan->ccr |= ~CCR_CLKEN;
	}
	update_clk(tcchan);
	return;
}

static uint32_t
cmr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	return tcchan->cmr;
}

static void
cmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	actualize_counter(tcchan);
	tcchan->cmr = value & 0x000fc7ff;
	update_clk(tcchan);
	return;
}

static uint32_t
cv_read(void *clientData, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	actualize_counter(tcchan);
	Senseless_Report(250);
	return tcchan->cv;
}

static void
cv_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Tc: CV is a read-only register\n");
	return;
}

static uint32_t
ra_read(void *clientData, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	return tcchan->ra;
}

static void
ra_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	tcchan->ra = value & 0xffff;
}

static uint32_t
rb_read(void *clientData, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	return tcchan->rb;
}

static void
rb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	tcchan->rb = value & 0xffff;
}

static uint32_t
rc_read(void *clientData, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	return tcchan->rc;
}

static void
rc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	tcchan->rc = value & 0xffff;
	return;
}

static uint32_t
sr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	return tcchan->sr;
}

static void
sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Tc: writing to readonly register SR\n");
	return;
}

static uint32_t
ier_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Tc: reading writeonly register IER\n");
	return 0;
}

static void
ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	tcchan->imr |= value;
	update_interrupt(tcchan);
	return;
}

static uint32_t
idr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Tc: reading from writeonly register IDR\n");
	return 0;
}

static void
idr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	tcchan->imr &= ~value;
	update_interrupt(tcchan);
	return;
}

static uint32_t
imr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91TcChannel *tcchan = (AT91TcChannel *) clientData;
	return tcchan->imr;
}

static void
imr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Tc: writing to readonly register IMR\n");
	return;
}

static uint32_t
bcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Tc: reading from writeonly register BCR\n");
	return 0;
}

static void
bcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Tc *tc = (AT91Tc *) clientData;
	AT91TcChannel *tcchan;
	int i;
	if (value & BCR_SYNC) {
		for (i = 0; i < 3; i++) {
			tcchan = &tc->chan[i];
			actualize_counter(tcchan);
			tcchan->cv = 0;
		}
	}
	return;
}

static uint32_t
bmr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Tc *tc = (AT91Tc *) clientData;
	return tc->bmr;
}

static void
bmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Tc *tc = (AT91Tc *) clientData;
	tc->bmr = value & 0x3f;
	if (value != 0x15) {
		fprintf(stderr, "AT91Tc: BMR not implemented\n");
	}
	return;
}

static void
AT91Tc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Tc *tc = (AT91Tc *) owner;
	int chan;
	AT91TcChannel *tcchan;
	for (chan = 0; chan < 3; chan++) {
		tcchan = &tc->chan[chan];
		IOH_New32(TC_CCR(base, chan), ccr_read, ccr_write, tcchan);
		IOH_New32(TC_CMR(base, chan), cmr_read, cmr_write, tcchan);
		IOH_New32(TC_CV(base, chan), cv_read, cv_write, tcchan);
		IOH_New32(TC_RA(base, chan), ra_read, ra_write, tcchan);
		IOH_New32(TC_RB(base, chan), rb_read, rb_write, tcchan);
		IOH_New32(TC_RC(base, chan), rc_read, rc_write, tcchan);
		IOH_New32(TC_SR(base, chan), sr_read, sr_write, tcchan);
		IOH_New32(TC_IER(base, chan), ier_read, ier_write, tcchan);
		IOH_New32(TC_IDR(base, chan), idr_read, idr_write, tcchan);
		IOH_New32(TC_IMR(base, chan), imr_read, imr_write, tcchan);
	}

	IOH_New32(TC_BCR(base), bcr_read, bcr_write, tc);
	IOH_New32(TC_BMR(base), bmr_read, bmr_write, tc);
}

static void
AT91Tc_UnMap(void *owner, uint32_t base, uint32_t mask)
{
//        IOH_Delete32(US_CR(base));
}

BusDevice *
AT91Tc_New(const char *name)
{
	int i;
	AT91Tc *tc = sg_new(AT91Tc);
#if 0
	tc->mck = Clock_New("%s.mck", name);
	tc->slck = Clock_New("%s.slck", name);
	tc->timer_clock1 = Clock_New("%s.timer_clock1", name);
	tc->timer_clock2 = Clock_New("%s.timer_clock2", name);
	tc->timer_clock3 = Clock_New("%s.timer_clock3", name);
	tc->timer_clock4 = Clock_New("%s.timer_clock4", name);
	tc->timer_clock5 = Clock_New("%s.timer_clock5", name);
	Clock_MakeDerived(tc->timer_clock1, tc->mck, 1, 2);
	Clock_MakeDerived(tc->timer_clock2, tc->mck, 1, 8);
	Clock_MakeDerived(tc->timer_clock3, tc->mck, 1, 32);
	Clock_MakeDerived(tc->timer_clock4, tc->mck, 1, 128);
	Clock_MakeDerived(tc->timer_clock5, tc->slck, 1, 1);
#endif

	for (i = 0; i < 3; i++) {
		AT91TcChannel *tcchan = &tc->chan[i];
		tcchan->tc = tc;
		tcchan->irqNode = SigNode_New("%s.ch%d.irq", name, i);
		if (!tcchan->irqNode) {
			fprintf(stderr, "AT91Tc: Can not create interrupt signal line\n");
		}
		SigNode_Set(tcchan->irqNode, SIG_PULLDOWN);

		tcchan->mck = Clock_New("%s.ch%d.mck", name, i);
		tcchan->slck = Clock_New("%s.ch%d.slck", name, i);
		tcchan->timer_clock1 = Clock_New("%s.ch%d.timer_clock1", name, i);
		tcchan->timer_clock2 = Clock_New("%s.ch%d.timer_clock2", name, i);
		tcchan->timer_clock3 = Clock_New("%s.ch%d.timer_clock3", name, i);
		tcchan->timer_clock4 = Clock_New("%s.ch%d.timer_clock4", name, i);
		tcchan->timer_clock5 = Clock_New("%s.ch%d.timer_clock5", name, i);
		Clock_MakeDerived(tcchan->timer_clock1, tcchan->mck, 1, 2);
		Clock_MakeDerived(tcchan->timer_clock2, tcchan->mck, 1, 8);
		Clock_MakeDerived(tcchan->timer_clock3, tcchan->mck, 1, 32);
		Clock_MakeDerived(tcchan->timer_clock4, tcchan->mck, 1, 128);
		Clock_MakeDerived(tcchan->timer_clock5, tcchan->slck, 1, 1);

		tcchan->xc0 = Clock_New("%s.ch%d.xc0", name, i);
		tcchan->xc1 = Clock_New("%s.ch%d.xc1", name, i);
		tcchan->xc2 = Clock_New("%s.ch%d.xc2", name, i);
		tcchan->clk = Clock_New("%s.ch%d.clk", name, i);
		Clock_SetFreq(tcchan->xc0, 0);
		Clock_SetFreq(tcchan->xc1, 0);
		Clock_SetFreq(tcchan->xc2, 0);
		update_interrupt(tcchan);
		update_clk(tcchan);
	}

	tc->bdev.first_mapping = NULL;
	tc->bdev.Map = AT91Tc_Map;
	tc->bdev.UnMap = AT91Tc_UnMap;
	tc->bdev.owner = tc;
	tc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "AT91RM9200 TimerCounter TC \"%s\" created\n", name);
	return &tc->bdev;
}
