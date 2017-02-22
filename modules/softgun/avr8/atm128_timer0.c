#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "sgstring.h"
#include "sgtypes.h"
#include "signode.h"
#include "cycletimer.h"
#include "clock.h"
#include "atm128_timer0.h"
#include "compiler_extensions.h"

#define REG_TCCR0(base)		(0x53)
#define		TCCR0_FOC0		(1 << 7)
#define 	TCCR0_WGM00		(1 << 6)
#define 	TCCR0_COM01		(1 << 5)
#define		TCCR0_COM00		(1 << 4)
#define 	TCCR0_WGM01		(1 << 3)
#define		TCCR0_CS02		(1 << 2)
#define		TCCR0_CS01		(1 << 1)
#define		TCCR0_CS00		(1 << 0)

#define WGM_NORMAL			(0)
#define	WGM_PWM_PC_FF 		(1)
#define WGM_CTC				(2)
#define WGM_FPWM_FF			(3)

#define REG_OCR0(base)		(0x51)
#define REG_TCNT0(base)		(0x52)
#define REG_ASSR(base)		(0x50)
#define REG_TIMSK(base)		(0x57)
#define		TIMSK_OCIE2		(1 << 7)
#define		TIMSK_TOIE2		(1 << 6)
#define		TIMSK_TCIE1		(1 << 5)
#define		TIMSK_OCIE1A	(1 << 4)
#define		TIMSK_OCIE1B	(1 << 3)
#define		TIMSK_TOIE1		(1 << 2)
#define		TIMSK_OCIE0		(1 << 1)
#define		TIMSK_TOIE0		(1 << 0)

#define REG_TIFR(base)		(0x56)		
#define		TIFR_OCF2		(1 << 7)
#define		TIFR_TOV2		(1 << 6)
#define		TIFR_ICF1		(1 << 5)
#define		TIFR_OCF1A		(1 << 4)
#define		TIFR_OCF1B		(1 << 3)
#define		TIFR_TOV1		(1 << 2)
#define		TIFR_OCF0		(1 << 1)
#define		TIFR_TOV0		(1 << 0)

#define REG_SFIOR(base)		(0x40)
#define		SFIOR_TSM		(1 << 7)
#define		SFIOR_ACME		(1 << 3)
#define		SFIOR_PUD		(1 << 2)
#define 	SFIOR_PSR0		(1 << 1)
#define	 	SFIOR_PSR321	(1 << 0)

typedef struct ATM128_Timer0 {
    CycleCounter_t last_counter_actualize;
    CycleCounter_t remainder;
    CycleTimer event_timer;
    CycleTimer set_ocf0_timer;
    CycleTimer set_tov0_timer;
    CycleCounter_t timeout;
    Clock_t *clk_t0;
    Clock_t *clk_in;
    SigNode *tov0Irq;
    SigNode *tov0AckIrq;
    SigNode *ocf0Irq;
    SigNode *ocf0AckIrq;
	uint8_t regTCCR0;
	uint8_t regOCR0;
	uint8_t regTCNT0;
	uint8_t regASSR;
	uint8_t regTIMSK;
	uint8_t regTIFR;
	uint8_t regSFIOR;
    uint8_t wg_mode;
    uint8_t ints_old;
} ATM128_Timer0;

static void
update_interrupts(ATM128_Timer0 * tm)
{
    uint8_t ints = tm->regTIFR & tm->regTIMSK;
    uint8_t diff = ints ^ tm->ints_old;
    if (!diff) {
        return;
    }
    if (diff & TIMSK_OCIE0) {
        if (ints & TIMSK_OCIE0) {
            SigNode_Set(tm->ocf0Irq, SIG_LOW);
        } else {
            SigNode_Set(tm->ocf0Irq, SIG_OPEN);
        }
    }
    if (diff & TIMSK_TOIE0) {
        if (ints & TIMSK_TOIE0) {
            SigNode_Set(tm->tov0Irq, SIG_LOW);
        } else {
            SigNode_Set(tm->tov0Irq, SIG_OPEN);
        }
    }
    tm->ints_old = ints;
}

static void
update_clocks(ATM128_Timer0 * tm)
{
    int cs = tm->regTCCR0 & 7;
    uint32_t multiplier = 1;
    uint32_t divider = 1;
    switch (cs) {
        case 0:
            multiplier = 0;
            break;
        case 1:
            divider = 1;
            break;
        case 2:
            divider = 8;
            break;
        case 3:
            divider = 32;
            break;
        case 4:
            divider = 64;
            break;
        case 5:
            divider = 128;
            break;
		case 6:
			divider = 256;
			break;
		case 7:
			divider = 1024;
			break;
        default:
            fprintf(stderr, "Clock source %d not implemented\n", cs);
            divider = 1024;
            break;
    }
    Clock_MakeDerived(tm->clk_t0, tm->clk_in, multiplier, divider);
}

static void
set_ocf0(void *clientData)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    if (!(tm->regTIFR & TIFR_OCF0)) {
        tm->regTIFR |= TIFR_OCF0;
        update_interrupts(tm);
    }
}

static void
set_tov0(void *clientData)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    if (!(tm->regTIFR & TIFR_TOV0)) {
        tm->regTIFR |= TIFR_TOV0;
        update_interrupts(tm);
    }
}

static void
act_counter(ATM128_Timer0 * tm, uint32_t top, uint32_t tov_set_at)
{
    FractionU64_t frac;
    uint64_t timer_steps;
    int carry;
    tm->remainder += CycleCounter_Get() - tm->last_counter_actualize;
    tm->last_counter_actualize = CycleCounter_Get();
    frac = Clock_MasterRatio(tm->clk_t0);
    if (frac.nom && frac.denom) {
        timer_steps = tm->remainder * frac.nom / frac.denom;
        tm->remainder -= timer_steps * frac.denom / frac.nom;
    } else {
        tm->remainder = 0;
        timer_steps = 0;
    }
    if (timer_steps == 0) {
        return;
    }
    carry = (tm->regTCNT0 + timer_steps) > top;
    if (unlikely(tm->regTCNT0 > top)) {
        if (tm->regOCR0 <= top) {
            if ((tm->regTCNT0 + timer_steps) > ((0x100 - tm->regTCNT0) + tm->regOCR0)) {
                tm->regTIFR |= TIFR_OCF0;
            }
        } else if (tm->regTCNT0 <= tm->regOCR0) {
            if ((tm->regTCNT0 + timer_steps) > tm->regOCR0) {
                tm->regTIFR |= TIFR_OCF0;
            }
        }
    } else if (tm->regOCR0 <= top) {
        if ((tm->regTCNT0 <= tm->regOCR0) && ((tm->regTCNT0 + timer_steps) > tm->regOCR0)) {
            tm->regTIFR |= TIFR_OCF0;
        } else if ((tm->regTCNT0 > tm->regOCR0) && carry &&
               ((tm->regTCNT0 + timer_steps) % (top + 1) > tm->regOCR0)) {
            tm->regTIFR |= TIFR_OCF0;
        }
    }
    if (unlikely(tm->regTCNT0 > top)) {
        if ((tm->regTCNT0 + timer_steps) > (tov_set_at + 0x100)) {
            tm->regTIFR |= TIFR_TOV0;
        }
    } else if (top >= tov_set_at) {
        if (((tm->regTCNT0 <= tov_set_at) || carry)
            && ((tm->regTCNT0 + timer_steps) > tov_set_at)) {
            tm->regTIFR |= TIFR_TOV0;
        }
    }
    update_interrupts(tm);
    if (unlikely(tm->regTCNT0 > top)) {
        uint32_t ovf_steps;
        ovf_steps = 0x100 - tm->regTCNT0;
        if (timer_steps <= ovf_steps) {
            tm->regTCNT0 += timer_steps;
        } else {
            timer_steps -= ovf_steps;
            tm->regTCNT0 = timer_steps % (top + 1);
        }
    } else {
        tm->regTCNT0 = ((uint64_t) tm->regTCNT0 + timer_steps) % (top + 1);
    }
	/* Special case of updating timer when exactlie hiting ocfa, ocfb or tov */
    if (unlikely(tm->regTCNT0 == tm->regOCR0)) {
        if (!CycleTimer_IsActive(&tm->set_ocf0_timer)) {
            CycleTimer_Mod(&tm->set_ocf0_timer, 1);
        }
    }
    if (unlikely(tm->regTCNT0 == tov_set_at)) {
        if (!CycleTimer_IsActive(&tm->set_tov0_timer)) {
            CycleTimer_Mod(&tm->set_tov0_timer, 1);
        }
    }
}

static void
actualize_counter(ATM128_Timer0 * tm)
{
    switch (tm->wg_mode) {
        case WGM_NORMAL:
            act_counter(tm, 0xff, 0xff);
            break;

        case WGM_PWM_PC_FF:
            act_counter(tm, 0xff, 0);
            break;

        case WGM_CTC:
            act_counter(tm, tm->regOCR0, 0xff);
            break;

        case WGM_FPWM_FF:
            act_counter(tm, 0xff, 0xff);
            break;
    }
}

static void
update_tout(ATM128_Timer0 * tm, uint32_t top, uint32_t tov_set_at)
{
    int32_t timer_steps_ocr0 = -1;
    int32_t timer_steps_tov0 = -1;
    int32_t min = 0x10000000;
    uint8_t required_mask = tm->regTIMSK & ~tm->regTIFR;
    if (required_mask & TIMSK_OCIE0) {
        if (unlikely(tm->regTCNT0 > top)) {
            timer_steps_ocr0 = (0x100 - tm->regTCNT0) + top + 1;
        } else {
            if (tm->regOCR0 <= top) {
                timer_steps_ocr0 = (tm->regOCR0 + 1 - tm->regTCNT0);
            }
        }
    }
    if (required_mask & TIMSK_TOIE0) {
        if (top >= tov_set_at) {
            timer_steps_tov0 = (top + 1 - tm->regTCNT0);
        }
    }
    if ((timer_steps_ocr0 >= 0)) {
        min = timer_steps_ocr0;
    }
    if ((timer_steps_tov0 >= 0)) {
        min = (min < timer_steps_tov0) ? min : timer_steps_tov0;
    }
	if (min != 0x10000000) {
        CycleCounter_t timeout;
        FractionU64_t frac;
        uint64_t cycles = 1000;
        frac = Clock_MasterRatio(tm->clk_t0);
        if (frac.nom && frac.denom) {
            cycles = min * frac.denom / frac.nom;
        }
        if (unlikely(cycles < tm->remainder)) {
            fprintf(stderr, "Bug in %s %d, cycles %" PRId64 ", remainder %" PRId64 "\n",
                __FILE__, __LINE__, cycles, tm->remainder);
            return;
        }
        cycles -= tm->remainder;
        timeout = CycleCounter_Get() + cycles;
        if (timeout != tm->timeout) {
            CycleTimer_Mod(&tm->event_timer, cycles);
            tm->timeout = timeout;
        }
    } else {
        tm->timeout = 0;
        CycleTimer_Remove(&tm->event_timer);
    }
}

static void
update_timeout(ATM128_Timer0 * tm)
{
    switch (tm->wg_mode) {
        case WGM_NORMAL:
            update_tout(tm, 0xff, 0xff);
            break;

        case WGM_PWM_PC_FF:
            update_tout(tm, 0xff, 0);
            break;

        case WGM_CTC:
            update_tout(tm, tm->regOCR0, 0xff);
            break;

        case WGM_FPWM_FF:
            update_tout(tm, 0xff, 0xff);
            break;
    }
}

static void
timer_event(void *clientData)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    actualize_counter(tm);
    update_timeout(tm);
}

static void
update_waveform_generation_mode(ATM128_Timer0 * tm)
{
    int wgm;
    wgm = (tm->regTCCR0 >> 6) & 1; 
    wgm |= (tm->regTCCR0 >> 2) & 2;
    tm->wg_mode = wgm;
}

/*
 *************************************************************************************
 * TCCR0 register
 *************************************************************************************
 */
static uint8_t
tccr0_read(void *clientData, uint32_t address)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    return tm->regTCCR0;
}

static void
tccr0_write(void *clientData, uint8_t value, uint32_t address)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
	uint8_t diff = tm->regTCCR0 ^ value;
    actualize_counter(tm);
    tm->regTCCR0 = value;
    if (diff & 7) {
        update_clocks(tm);
    }
    update_waveform_generation_mode(tm);
    update_timeout(tm);
	//fprintf(stderr,"TCCR0 %02x\n",value);
	//exit(1);
}

static uint8_t
tcnt0_read(void *clientData, uint32_t address)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    actualize_counter(tm);
    return tm->regTCNT0;
}

static void
tcnt0_write(void *clientData, uint8_t value, uint32_t address)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    actualize_counter(tm);
    tm->regTCNT0 = value;
    update_timeout(tm);
}

static uint8_t
ocr0_read(void *clientData, uint32_t address)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    return tm->regOCR0;
}

static void
ocr0_write(void *clientData, uint8_t value, uint32_t address)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    actualize_counter(tm);
    tm->regOCR0 = value;
    update_timeout(tm);
}

static uint8_t
timsk_read(void *clientData, uint32_t address)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    return tm->regTIMSK;
}

static void
timsk_write(void *clientData, uint8_t value, uint32_t address)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    actualize_counter(tm);
    tm->regTIMSK = value;
    update_interrupts(tm);
    update_timeout(tm);
}

static uint8_t
tifr_read(void *clientData, uint32_t address)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    return tm->regTIFR;
}

static void
tifr_write(void *clientData, uint8_t value, uint32_t address)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    uint8_t clear = tm->regTIFR & value;
    fprintf(stderr, "TIFR write %02x\n", value);
    actualize_counter(tm);
    tm->regTIFR &= ~clear;
    update_interrupts(tm);
    update_timeout(tm);
}

static void
ocf0AckIrq(SigNode * node, int value, void *clientData)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    if (value == SIG_LOW) {
        if (!(tm->regTIFR & TIFR_OCF0 & tm->regTIMSK)) {
            fprintf(stderr, "Bug: Ack of nonposted Interrupt compa\n");
        }
        actualize_counter(tm);
        tm->regTIFR &= ~TIFR_OCF0;
        update_interrupts(tm);
        update_timeout(tm);
    }
}

static void
tov0AckIrq(SigNode * node, int value, void *clientData)
{
    ATM128_Timer0 *tm = (ATM128_Timer0 *) clientData;
    if (value == SIG_LOW) {
        if (!(tm->regTIFR & TIFR_TOV0 & tm->regTIMSK)) {
            fprintf(stderr, "Bug: Ack of nonposted Interrupt TOV\n");
        }
        actualize_counter(tm);
        tm->regTIFR &= ~TIFR_TOV0;
        update_interrupts(tm);
        update_timeout(tm);
    }
}

void
ATM128_Timer0New(const char *name)
{
	ATM128_Timer0 *tm = sg_new(ATM128_Timer0);

	AVR8_RegisterIOHandler(REG_TCCR0(base), tccr0_read, tccr0_write, tm);
    AVR8_RegisterIOHandler(REG_TCNT0(base), tcnt0_read, tcnt0_write, tm);
    AVR8_RegisterIOHandler(REG_OCR0(base), ocr0_read, ocr0_write, tm);
    AVR8_RegisterIOHandler(REG_TIMSK(base), timsk_read, timsk_write, tm);
    AVR8_RegisterIOHandler(REG_TIFR(base), tifr_read, tifr_write, tm);

	tm->ocf0Irq = SigNode_New("%s.compIrq", name);
    tm->tov0Irq = SigNode_New("%s.ovfIrq", name);
    tm->ocf0AckIrq = SigNode_New("%s.compAckIrq", name);
    tm->tov0AckIrq = SigNode_New("%s.ovfAckIrq", name);
    SigNode_Set(tm->ocf0Irq, SIG_OPEN);
    SigNode_Set(tm->tov0Irq, SIG_OPEN);
    SigNode_Set(tm->ocf0AckIrq, SIG_OPEN);
    SigNode_Set(tm->tov0AckIrq, SIG_OPEN);
    tm->clk_t0 = Clock_New("%s.clk_t0", name);
    tm->clk_in = Clock_New("%s.clk", name);
	update_clocks(tm);
	CycleTimer_Init(&tm->event_timer, timer_event, tm);
    CycleTimer_Init(&tm->set_ocf0_timer, set_ocf0, tm);
    CycleTimer_Init(&tm->set_tov0_timer, set_tov0, tm);

	SigNode_Trace(tm->tov0AckIrq, tov0AckIrq, tm);
    SigNode_Trace(tm->ocf0AckIrq, ocf0AckIrq, tm);
}
