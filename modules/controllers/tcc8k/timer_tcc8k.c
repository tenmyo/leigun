#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "timer_tcc8k.h"
#include "serial.h"
#include "cycletimer.h"
#include "clock.h"

#define REG_TCFG16(base,idx)	((base) + 0x00 + ((idx) << 4))
#define REG_TCNT16(base,idx) 	((base) + 0x04 + ((idx) << 4))
#define REG_TREF16(base,idx) 	((base) + 0x08 + ((idx) << 4))
#define REG_TMREF16(base,idx) 	((base) + 0x0C + ((idx) << 4))

#define REG_TCFG20(base,idx) 	((base) + 0x40 + ((idx) << 4))
#define REG_TCNT20(base,idx) 	((base) + 0x44 + ((idx) << 4))
#define REG_TREF20(base,idx) 	((base) + 0x48 + ((idx) << 4))

#define REG_TIREQ(base) 	((base) + 0x60)
#define REG_TWDCFG(base) 	((base) + 0x70)
#define REG_TWDCLR(base) 	((base) + 0x74)
#define REG_TWDCNT(base) 	((base) + 0x78)

#define REG_TC32EN(base) 	((base) + 0x80)
#define 	TC32EN_LDM1		(1 << 29)
#define		TC32EN_LDM0		(1 << 28)
#define		TC32EN_STOPMODE		(1 << 26)
#define		TC32EN_LOADZERO		(1 << 25)
#define		TC32EN_ENABLE		(1 << 24)
#define		TC32EN_PRESCALE_MSK	(0xffffff)

#define REG_TC32LDV(base) 	((base) + 0x84)
#define REG_TC32CMP0(base) 	((base) + 0x88)
#define REG_TC32CMP1(base) 	((base) + 0x8C)
#define REG_TC32PCNT(base) 	((base) + 0x90)
#define REG_TC32MCNT(base) 	((base) + 0x94)
#define REG_TC32IRQ(base) 	((base) + 0x98)
#define		TC32IRQ_IRQCLR		(1 << 31)
#define		TC32IRQ_RSYNC		(1 << 30)
#define		TC32IRQ_BITSEL_MSK	(0x3f << 24)
#define		TC32IRQ_IRQEN4		(1 << 20)
#define		TC32IRQ_IRQEN3		(1 << 19)
#define		TC32IRQ_PEIRQEN		(1 << 19)
#define		TC32IRQ_IRQEN2		(1 << 18)
#define		TC32IRQ_IRQEN1		(1 << 17)
#define		TC32IRQ_CMP1IRQEN	(1 << 17)
#define		TC32IRQ_IRQEN0		(1 << 16)
#define		TC32IRQ_CMP0IRQEN	(1 << 16)
#define		TC32IRQ_IRQSTAT4	(1 << 12)
#define		TC32IRQ_IRQSTAT3	(1 << 11)
#define		TC32IRQ_IRQSTAT2	(1 << 10)
#define		TC32IRQ_IRQSTAT1	(1 << 9)
#define		TC32IRQ_CMP1IRQSTAT	(1 << 9)
#define		TC32IRQ_IRQSTAT0	(1 << 8)
#define		TC32IRQ_CMP0IRQSTAT	(1 << 8)
#define		TC32IRQ_IRQMSTAT4	(1 << 4)
#define		TC32IRQ_IRQMSTAT3	(1 << 3)
#define		TC32IRQ_IRQMSTAT2	(1 << 2)
#define		TC32IRQ_IRQMSTAT1	(1 << 1)
#define		TC32IRQ_IRQMSTAT0	(1 << 0)

#define REG_DEBUG(base) 	((base) + 0xA8)

typedef struct TCC_TMod TCC_TMod;

typedef struct TC16 {
	TCC_TMod *tmod;
	uint32_t regTcfg;
	uint32_t regTcnt;
	uint32_t regTref;
	uint32_t regTmref0;
} TC16;

typedef struct TC20 {
	TCC_TMod *tmod;
	uint32_t regTcfg;
	uint32_t regTcnt;
	uint32_t regTref;
} TC20;

typedef struct TC32 {
	TCC_TMod *tmod;
	SigNode *sigIrq;
	Clock_t *clk_in;
	CycleCounter_t last_actualized;
	CycleCounter_t accumulated_cycles;
	CycleTimer event_timer;

	uint32_t regTc32en;
	uint32_t regTc32ldv;
	uint32_t regTc32cmp0;
	uint32_t regTc32cmp1;
	uint32_t regTc32pcnt;
	uint32_t regTc32mcnt;
	uint32_t regTc32irq;
} TC32;

struct TCC_TMod {
	BusDevice bdev;
	TC16 tc16[4];
	TC20 tc20[2];
	TC32 tc32[1];
	uint32_t regTireq;
	uint32_t regTwdcfg;
	uint32_t regTwdclr;
	uint32_t regTwdcnt;
};

/**
 * TCC8K interrupts are active high
 */
static void
update_interrupt_tc32(TC32 * tc32)
{
	uint32_t tc32irq = tc32->regTc32irq;
	uint32_t irqstat = (tc32irq >> 8) & 0x3f;
	uint32_t irqen = (tc32irq >> 16) & 0x3f;
	if (irqstat & irqen) {
		SigNode_Set(tc32->sigIrq, SIG_HIGH);
	} else {
		SigNode_Set(tc32->sigIrq, SIG_LOW);
	}
}

static void
reload_tc32(TC32 * tc32)
{
	tc32->regTc32mcnt = 0;
}

#include "arm9cpu.h"
static void
actualize_counter_tc32(TC32 * tc32)
{
	FractionU64_t frac;
	uint64_t elapsed_cycles;
	uint64_t acc;
	uint64_t counter_cycles;
	uint64_t pcnt;
	uint32_t mcnt;
	uint32_t prescaler_period = (tc32->regTc32en & 0xffffff) + 1;

	/* Check if timer is enabled is missing here */

	elapsed_cycles = CycleCounter_Get() - tc32->last_actualized;
	tc32->last_actualized = CycleCounter_Get();
	acc = tc32->accumulated_cycles + elapsed_cycles;
	frac = Clock_MasterRatio(tc32->clk_in);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Warning, No clock for TC32 module at 0x%08x\n", ARM_GET_CIA);
		usleep(10000);
		return;
	}
	counter_cycles = acc * frac.nom / frac.denom;
	acc -= counter_cycles * frac.denom / frac.nom;
	tc32->accumulated_cycles = acc;
	pcnt = tc32->regTc32pcnt & 0xffffff;
	mcnt = tc32->regTc32mcnt;
	pcnt = pcnt + counter_cycles;
	if (pcnt >= prescaler_period) {
		uint64_t delta_mcnt;
		uint64_t mcnt_timeout;
		uint32_t new_mcnt;
		if (tc32->regTc32irq & TC32IRQ_IRQEN3) {
			tc32->regTc32irq |= TC32IRQ_IRQSTAT3;
			update_interrupt_tc32(tc32);
		}
		delta_mcnt = pcnt / prescaler_period;
		new_mcnt = (uint64_t) mcnt + delta_mcnt;
		if (tc32->regTc32irq & TC32IRQ_CMP0IRQEN) {
			mcnt_timeout = (uint32_t) (tc32->regTc32cmp0 - mcnt);
			if ((new_mcnt == tc32->regTc32cmp0) || (delta_mcnt >= mcnt_timeout)) {
				tc32->regTc32irq |= TC32IRQ_CMP0IRQSTAT;
				update_interrupt_tc32(tc32);
				//      fprintf(stderr,"TC32_Event\n");
			} else {
				//      fprintf(stderr,"TC32_Noevent 0x%x 0x%llx\n",mcnt,new_mcnt);
			}
		}
		if (tc32->regTc32irq & TC32IRQ_CMP1IRQEN) {
			if ((new_mcnt == tc32->regTc32cmp1) ||
			    ((mcnt < tc32->regTc32cmp1)
			     && (new_mcnt > (uint64_t) tc32->regTc32cmp1))) {
				tc32->regTc32irq |= TC32IRQ_CMP1IRQSTAT;
				update_interrupt_tc32(tc32);
			}
		}
		mcnt = new_mcnt;
		pcnt = pcnt % prescaler_period;
		tc32->regTc32mcnt = mcnt;
	}
	tc32->regTc32pcnt = (tc32->regTc32pcnt & 0xff000000) | (pcnt & 0xffffff);
	//fprintf(stderr,"Actualized Cnt 0x%08x PS 0x%08x, prescaler period %08x\n",tc32->regTc32mcnt,tc32->regTc32pcnt,prescaler_period);
}

static void
update_timeout_tc32(TC32 * tc32)
{
	FractionU64_t frac;
	uint64_t timeout;
	uint64_t smallest_timeout = ~UINT64_C(0);
	uint64_t cpu_cycles;
	//tmr_mode = tm->reg_tbimr & 3;
	frac = Clock_MasterRatio(tc32->clk_in);
	if (!frac.nom) {
		return;
	}
	if (tc32->regTc32irq & TC32IRQ_IRQEN3) {
		/* End of prescale count */
		uint64_t prescale = tc32->regTc32en & 0xffffff;
		uint64_t current = tc32->regTc32pcnt & 0xffffff;
		timeout = prescale - current + 1;
		smallest_timeout = timeout;
	}
	if (tc32->regTc32irq & TC32IRQ_CMP0IRQEN) {
		uint32_t cnt = tc32->regTc32mcnt;
		uint64_t presc_period = (tc32->regTc32en & 0xffffff) + 1;
		uint64_t current_ps = tc32->regTc32pcnt & 0xffffff;
		timeout = presc_period - current_ps;
		//fprintf(stderr,"Pres per %lld, current_ps %lld\n",presc_period,current_ps);
		timeout += presc_period * (uint32_t) (tc32->regTc32cmp0 - cnt - 1);
		if (timeout < smallest_timeout) {
			smallest_timeout = timeout;
		}
		//fprintf(stderr,"Cmp0 is enabled timeout %lld, now %u, cmp0 %u\n",timeout,cnt,tc32->regTc32cmp0);
	}
	if (tc32->regTc32irq & TC32IRQ_CMP1IRQEN) {
		uint32_t cnt = tc32->regTc32mcnt;
		uint64_t presc_period = (tc32->regTc32en & 0xffffff) + 1;
		uint64_t current_ps = tc32->regTc32pcnt & 0xffffff;
		timeout = presc_period - current_ps;
		timeout += presc_period * (uint32_t) (tc32->regTc32cmp1 - cnt - 1);
		if (timeout < smallest_timeout) {
			smallest_timeout = timeout;
		}
	}
	//fprintf(stderr,"Smallest timeout %lld\n",smallest_timeout);
	if (smallest_timeout != ~UINT64_C(0)) {
		cpu_cycles = (smallest_timeout * frac.denom) / frac.nom;
		CycleTimer_Mod(&tc32->event_timer, cpu_cycles);
	}
}

static void
tc32_event(void *clientData)
{
	TC32 *tc32 = clientData;
	actualize_counter_tc32(tc32);
	update_timeout_tc32(tc32);
}

static uint32_t
tcfg16_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
tcfg16_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
tcnt16_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
tcnt16_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
tref16_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
tref16_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
tmref16_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
tmref16_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
tcfg20_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
tcfg20_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
tcnt20_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
tcnt20_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
tref20_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
tref20_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
tc32en_read(void *clientData, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	return tc32->regTc32en;
}

static void
tc32en_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	actualize_counter_tc32(tc32);
	tc32->regTc32en = value;
	reload_tc32(tc32);
	fprintf(stderr, "TC32EN: 0x%08x\n", value);
	if (tc32->regTc32en & (TC32EN_LOADZERO | TC32EN_LDM1 | TC32EN_LDM0)) {
		fprintf(stderr, "Timer mode not implemented: 0x%08x\n", value);
		exit(1);
	}
	update_timeout_tc32(tc32);
}

static uint32_t
tc32ldv_read(void *clientData, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	return tc32->regTc32ldv;
}

static void
tc32ldv_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	tc32->regTc32ldv = value;
	actualize_counter_tc32(tc32);
	reload_tc32(tc32);
	update_timeout_tc32(tc32), fprintf(stderr, "Reload value 0x%08x\n", value);
}

static uint32_t
tc32cmp0_read(void *clientData, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	return tc32->regTc32cmp0;
}

static void
tc32cmp0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	actualize_counter_tc32(tc32);
	tc32->regTc32cmp0 = value;
	update_timeout_tc32(tc32);
}

static uint32_t
tc32cmp1_read(void *clientData, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	return tc32->regTc32cmp1;
}

static void
tc32cmp1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	actualize_counter_tc32(tc32);
	tc32->regTc32cmp1 = value;
	update_timeout_tc32(tc32);
}

static uint32_t
tc32pcnt_read(void *clientData, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	actualize_counter_tc32(tc32);
	//fprintf(stderr,"PCNT %08x\n",tc32->regTc32pcnt);
	return tc32->regTc32pcnt;
}

static void
tc32pcnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Prescaler Counter Register is not writable\n", __func__);
}

static uint32_t
tc32mcnt_read(void *clientData, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	actualize_counter_tc32(tc32);
	/* Warn if Clock rate is to high to read this register reliably */
	//fprintf(stderr,"MCNT %08x\n",tc32->regTc32mcnt);
	return tc32->regTc32mcnt;
}

static void
tc32mcnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: MCNT register is not writable\n", __func__);
}

static uint32_t
tc32irq_read(void *clientData, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	uint32_t value = tc32->regTc32irq;
	uint32_t irqclr = value & TC32IRQ_IRQCLR;
	if (!irqclr) {
		/* Clear irq status by reading */
		tc32->regTc32irq = value & ~(0x3f << 8);
		update_interrupt_tc32(tc32);
	}
	//    fprintf(stderr,"TCC8K Timer: %s: Register not implemented\n",__func__);
	return 0;
}

static void
tc32irq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TC32 *tc32 = clientData;
	uint32_t clearmask = ~(value & (0x3f << 8));
	uint32_t keepmask = (0x3f << 8);
	actualize_counter_tc32(tc32);
	tc32->regTc32irq &= clearmask;
	tc32->regTc32irq = (tc32->regTc32irq & keepmask) | (value & ~keepmask);
	update_interrupt_tc32(tc32);
	update_timeout_tc32(tc32), fprintf(stderr, "IRQ write 0x%08x\n", value);
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
tireq_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
tireq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
twdcfg_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
twdcfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	if (value == 0x49) {
		fprintf(stderr, "Watchdog timer reset\n");
		exit(0);
	}
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
twdclr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
twdclr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
}

static uint32_t
twdcnt_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 0;
}

static void
twdcnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	static int first = 1;
	if (first) {
		first = 0;
		fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	}
}

#if 0
static uint32_t
debug_read(void *clientData, uint32_t address, int rqlen)
{
	//static int i = 0;
	fprintf(stderr, "TCC8K Timer: %s: Register not implemented\n", __func__);
	return 123;
}

static void
debug_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Timer: %s: %08x\n", __func__, value);
}
#endif

static void
TimerMod_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	int i;
	TCC_TMod *tmod = owner;
	TC32 *tc32;
	for (i = 0; i < array_size(tmod->tc16); i++) {
		TC16 *tc16 = &tmod->tc16[i];
		IOH_New32(REG_TCFG16(base, i), tcfg16_read, tcfg16_write, tc16);
		IOH_New32(REG_TCNT16(base, i), tcnt16_read, tcnt16_write, tc16);
		IOH_New32(REG_TREF16(base, i), tref16_read, tref16_write, tc16);
		IOH_New32(REG_TMREF16(base, i), tmref16_read, tmref16_write, tc16);
	}
	for (i = 0; i < array_size(tmod->tc20); i++) {
		TC20 *tc20 = &tmod->tc20[i];
		IOH_New32(REG_TCFG20(base, i), tcfg20_read, tcfg20_write, tc20);
		IOH_New32(REG_TCNT20(base, i), tcnt20_read, tcnt20_write, tc20);
		IOH_New32(REG_TREF20(base, i), tref20_read, tref20_write, tc20);
	}
	tc32 = &tmod->tc32[0];
	IOH_New32(REG_TC32EN(base), tc32en_read, tc32en_write, tc32);
	IOH_New32(REG_TC32LDV(base), tc32ldv_read, tc32ldv_write, tc32);
	IOH_New32(REG_TC32CMP0(base), tc32cmp0_read, tc32cmp0_write, tc32);
	IOH_New32(REG_TC32CMP1(base), tc32cmp1_read, tc32cmp1_write, tc32);
	IOH_New32(REG_TC32PCNT(base), tc32pcnt_read, tc32pcnt_write, tc32);
	IOH_New32(REG_TC32MCNT(base), tc32mcnt_read, tc32mcnt_write, tc32);
	IOH_New32(REG_TC32IRQ(base), tc32irq_read, tc32irq_write, tc32);

	IOH_New32(REG_TIREQ(base), tireq_read, tireq_write, tmod);
	IOH_New32(REG_TWDCFG(base), twdcfg_read, twdcfg_write, tmod);
	IOH_New32(REG_TWDCLR(base), twdclr_read, twdclr_write, tmod);
	IOH_New32(REG_TWDCNT(base), twdcnt_read, twdcnt_write, tmod);
	//IOH_New32(REG_DEBUG(base),debug_read,debug_write,tmod);
	//IOH_New32(0x9801304c,debug_read,debug_write,tmod);
}

static void
TimerMod_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	TCC_TMod *tmod = owner;
	for (i = 0; i < array_size(tmod->tc16); i++) {
		IOH_Delete32(REG_TCFG16(base, i));
		IOH_Delete32(REG_TCNT16(base, i));
		IOH_Delete32(REG_TREF16(base, i));
		IOH_Delete32(REG_TMREF16(base, i));
	}
	for (i = 0; i < array_size(tmod->tc20); i++) {
		IOH_Delete32(REG_TCFG20(base, i));
		IOH_Delete32(REG_TCNT20(base, i));
		IOH_Delete32(REG_TREF20(base, i));
	}
	IOH_Delete32(REG_TC32EN(base));
	IOH_Delete32(REG_TC32LDV(base));
	IOH_Delete32(REG_TC32CMP0(base));
	IOH_Delete32(REG_TC32CMP1(base));
	IOH_Delete32(REG_TC32PCNT(base));
	IOH_Delete32(REG_TC32MCNT(base));
	IOH_Delete32(REG_TC32IRQ(base));

	IOH_Delete32(REG_TIREQ(base));
	IOH_Delete32(REG_TWDCFG(base));
	IOH_Delete32(REG_TWDCLR(base));
	IOH_Delete32(REG_TWDCNT(base));
}

BusDevice *
TCC8K_TimerModNew(const char *name)
{
	TCC_TMod *tmod = sg_new(TCC_TMod);
	int i;
	tmod->bdev.first_mapping = NULL;
	tmod->bdev.Map = TimerMod_Map;
	tmod->bdev.UnMap = TimerMod_UnMap;
	tmod->bdev.owner = tmod;
	tmod->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	for (i = 0; i < array_size(tmod->tc16); i++) {
		tmod->tc16[i].tmod = tmod;
	}
	for (i = 0; i < array_size(tmod->tc20); i++) {
		tmod->tc20[i].tmod = tmod;
	}
	for (i = 0; i < array_size(tmod->tc32); i++) {
		TC32 *tc32 = &tmod->tc32[i];
		tc32->tmod = tmod;
		tc32->regTc32en = 0x7fff;
		tc32->clk_in = Clock_New("%s.tc32.clk", name);
		tc32->sigIrq = SigNode_New("%s.tc32.irq", name);
		if (!tc32->clk_in || !tc32->sigIrq) {
			fprintf(stderr, "Can not create TC32\n");
			exit(1);
		}
		CycleTimer_Init(&tc32->event_timer, tc32_event, tc32);
	}
	fprintf(stderr, "TCC8000 timer module creaded\n");
	return &tmod->bdev;
}
