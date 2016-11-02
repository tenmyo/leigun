/*
 *************************************************************************************************
 * 
 * Emulation of ATMegaXX4 Timer 0 and 2
 *
 * state: Some modes are working 
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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
#include <inttypes.h>
#include <string.h>
#include "avr8_io.h" 
#include "avr8_cpu.h" 
#include "sgstring.h"
#include "sgtypes.h"
#include "signode.h"
#include "cycletimer.h"
#include "clock.h"
#include "atm644_timer02.h"
#include "compiler_extensions.h"

#define GTCCR(base)   (0x23 + 0x20)
#define 	GTCCR_TSM     (1 << 7)
#define 	GTCCR_PSRASY  (1 << 1)
#define 	GTCCR_PSRSYNC (1 << 0)

#define TCCRA(base) ((base) + 0x0)
#define 	TCCRA_COMA1  	(1 << 7)
#define 	TCCRA_COMA0  	(1 << 6)
#define 	TCCRA_COMB1  	(1 << 5)
#define 	TCCRA_COMB0  	(1 << 4)
#define 	TCCRA_WGM1   	(1 << 1)
#define 	TCCRA_WGM0   	(1 << 0)

#define TCCRB(base)  ((base) + 0x01)
#define 	TCCRB_FOCA   (1 << 7)
#define 	TCCRB_FOCB   (1 << 6)
#define 	TCCRB_WGM2   (1 << 3)
#define 	TCCRB_CS2    (1 << 2)
#define 	TCCRB_CS1    (1 << 1)
#define 	TCCRB_CS0    (1 << 0)

#define WGM_NORMAL	(0)
#define WGM_PWM_PC_FF	(1)
#define WGM_CTC		(2)
#define WGM_FPWM_FF	(3)
#define WGM_PWM_PC_OCRA	(5)
#define WGM_FPWM_OCRA	(7)

#define TCNT(base)   ((base) + 0x02)
#define OCRA(base)   ((base) + 0x03)
#define OCRB(base)   ((base) + 0x04)

#define TIMSK(ofs)  (0x6e + (ofs))
#define		TIMSK_OCIEB	(1 << 2)
#define		TIMSK_OCIEA	(1 << 1)
#define 	TIMSK_TOIE	(1 << 0)
#define TIFR(ofs)   (0x35 + (ofs))
#define		TIFR_OCFB	(1 << 2)
#define		TIFR_OCFA	(1 << 1)
#define		TIFR_TOV	(1 << 0)


typedef struct ATM644_Timer0 {
	CycleCounter_t last_counter_actualize;
	CycleCounter_t remainder;
	CycleTimer event_timer;
	CycleTimer set_ocfa_timer;
        CycleTimer set_ocfb_timer;
        CycleTimer set_tov_timer;
	CycleCounter_t timeout;
	Clock_t *clk_t0;
	Clock_t *clk_in;
	SigNode *compaIrq;
	SigNode *compaAckIrq;
	SigNode *compbIrq;
	SigNode *compbAckIrq;
	SigNode *ovfIrq;
	SigNode *ovfAckIrq;
	uint8_t gtccr;
	uint8_t tccra;
	uint8_t tccrb;
	uint8_t wg_mode;
	uint8_t tcnt;
	uint8_t ocra;
	uint8_t ocrb;
	uint8_t timsk;
	uint8_t tifr;
	uint8_t ints_old;
} ATM644_Timer0;

static void
update_interrupts(ATM644_Timer0 *tm) 
{
	uint8_t ints = tm->tifr & tm->timsk;
	uint8_t diff = ints ^ tm->ints_old;
	if(!diff) {
		return;
	}
	if(diff & TIMSK_OCIEB) {
		if(ints & TIMSK_OCIEB) {
			SigNode_Set(tm->compbIrq,SIG_LOW);
		} else {
			SigNode_Set(tm->compbIrq,SIG_OPEN);
		}
	}
	if(diff & TIMSK_OCIEA) {
		if(ints & TIMSK_OCIEA) {
			SigNode_Set(tm->compaIrq,SIG_LOW);
		} else {
			SigNode_Set(tm->compaIrq,SIG_OPEN);
		}
	}
	if(diff & TIMSK_TOIE) {
		if(ints & TIMSK_TOIE) {
			SigNode_Set(tm->ovfIrq,SIG_LOW);
		} else {
			SigNode_Set(tm->ovfIrq,SIG_OPEN);
		}
	}
	tm->ints_old = ints;
}

static void
update_clocks(ATM644_Timer0 *tm)
{
	int cs = tm->tccrb & 7;
	uint32_t multiplier = 1;
	uint32_t divider = 1;
	switch(cs) {
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
			divider = 64;
			break;
		case 4:
			divider = 256;
			break;
		case 5: divider = 1024;
			break;
		default:
			fprintf(stderr,"Clock source %d not implemented\n",cs);
			divider = 1024;
			break;
	}
	Clock_MakeDerived(tm->clk_t0,tm->clk_in,multiplier,divider);	
}

/*
 ********************************************************************************************
 * GTTCR register
 * TSM     Bit 7: Holds PSRASY and PSRSYNC in reset. Allows to start them at the same time
 * PSRASY  Bit 1: Reset the prescaler of timer 2, stays one until reset is done (Async mode)
 * PSRSYNC Bit 0: Reset the prescaler of timer 0/1
 ********************************************************************************************
 */
#if 0
static uint8_t
gtccr_read(void *clientData,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        return tm->gtccr;
}

static void
gtccr_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	tm->gtccr = value;	
	fprintf(stderr,"Register not implemented\n");
}
#endif

static void
set_ocfa(void *clientData)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        if(!(tm->tifr & TIFR_OCFA)) {
                tm->tifr |= TIFR_OCFA;
                update_interrupts(tm);
        }
}

static void
set_ocfb(void *clientData)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        if(!(tm->tifr & TIFR_OCFB)) {
                tm->tifr |= TIFR_OCFB;
                update_interrupts(tm);
        }
}

static void
set_tov(void *clientData)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        if(!(tm->tifr & TIFR_TOV)) {
                tm->tifr |= TIFR_TOV;
                update_interrupts(tm);
        }

}

static void
act_counter(ATM644_Timer0 *tm,uint32_t top,uint32_t tov_set_at)
{
	FractionU64_t frac;
	uint64_t timer_steps;
	int carry;
	tm->remainder += CycleCounter_Get() - tm->last_counter_actualize;	
	tm->last_counter_actualize = CycleCounter_Get();
	frac = Clock_MasterRatio(tm->clk_t0);
	if(frac.nom && frac.denom) {
		timer_steps = tm->remainder * frac.nom / frac.denom;
		tm->remainder -= timer_steps * frac.denom / frac.nom;
	} else {
		tm->remainder = 0;
		timer_steps = 0;
	}
	if(timer_steps == 0) {
		return;
	}
	carry = (tm->tcnt + timer_steps) > top; 
	if(unlikely(tm->tcnt > top)) {
		if(tm->ocra <= top) {
			if((tm->tcnt + timer_steps) > ((0x100 - tm->tcnt) + tm->ocra)) {
				tm->tifr |= TIFR_OCFA;
			}
		} else if(tm->tcnt <= tm->ocra) {
			if((tm->tcnt + timer_steps) > tm->ocra) {
				tm->tifr |= TIFR_OCFA;
			}
		}
	} else if(tm->ocra <= top) {
		if((tm->tcnt <= tm->ocra) && ((tm->tcnt + timer_steps) > tm->ocra)) {
			tm->tifr |= TIFR_OCFA;
		} else if((tm->tcnt > tm->ocra) && carry && 
			((tm->tcnt + timer_steps) % (top + 1) > tm->ocra)) {
			tm->tifr |= TIFR_OCFA;
		}
	}
	if(unlikely(tm->tcnt > top)) {
		if(tm->ocrb <= top) {
			if((tm->tcnt + timer_steps) > ((0x100 - tm->tcnt) + tm->ocrb)) {
				tm->tifr |= TIFR_OCFB;
			}
		} else if(tm->tcnt <= tm->ocrb) {
			if((tm->tcnt + timer_steps) > tm->ocrb) {
				tm->tifr |= TIFR_OCFB;
			}
		}
	} else if(tm->ocrb <= top) {
		if((tm->tcnt <= tm->ocrb) && ((tm->tcnt + timer_steps) > tm->ocrb)) {
			tm->tifr |= TIFR_OCFB;
		} else if((tm->tcnt > tm->ocrb) && carry && 
			((tm->tcnt + timer_steps) % (top + 1) > tm->ocrb)) {
			tm->tifr |= TIFR_OCFB;
		}
	}
	if(unlikely(tm->tcnt > top)) {
		if((tm->tcnt + timer_steps) > (tov_set_at + 0x100)) {
			tm->tifr |= TIFR_TOV;
		}
	} else if(top >= tov_set_at) {
		if(((tm->tcnt <= tov_set_at) || carry) 
		&& ((tm->tcnt + timer_steps) > tov_set_at)) {
			tm->tifr |= TIFR_TOV;
		}
	}
	update_interrupts(tm);
	if(unlikely(tm->tcnt > top)) {
		uint32_t ovf_steps;
		ovf_steps = 0x100 - tm->tcnt;
		if(timer_steps <= ovf_steps) {
			tm->tcnt += timer_steps;
		} else {
			timer_steps-= ovf_steps;
			tm->tcnt = timer_steps % (top + 1);
		}
	} else {
		tm->tcnt = ((uint64_t)tm->tcnt + timer_steps) % (top + 1);
	}
	/* Special case of updating timer when exactlie hiting ocfa, ocfb or tov */
	if(unlikely(tm->tcnt == tm->ocra)) {
                if(!CycleTimer_IsActive(&tm->set_ocfa_timer)) {
                        CycleTimer_Mod(&tm->set_ocfa_timer,1);
                }
        }
        if(unlikely(tm->tcnt == tm->ocrb)) {
                if(!CycleTimer_IsActive(&tm->set_ocfb_timer)) {
                        CycleTimer_Mod(&tm->set_ocfb_timer,1);
                }
        }
        if(unlikely(tm->tcnt == tov_set_at)) {
                if(!CycleTimer_IsActive(&tm->set_tov_timer)) {
                        CycleTimer_Mod(&tm->set_tov_timer,1);
                }
        }
}

static void
actualize_counter(ATM644_Timer0 *tm) {
	switch(tm->wg_mode) {
		case WGM_NORMAL:
			act_counter(tm,0xff,0xff);
			break;

		case WGM_PWM_PC_FF:
			act_counter(tm,0xff,0);
			break;

		case WGM_CTC:
			act_counter(tm,tm->ocra,0xff);
			break;

		case WGM_FPWM_FF:
			act_counter(tm,0xff,0xff);
			break;
		case WGM_PWM_PC_OCRA:
			act_counter(tm,tm->ocra,0);
			break;

		case WGM_FPWM_OCRA:
			act_counter(tm,tm->ocra,tm->ocra);
			break;
	}
}

static void
update_tout(ATM644_Timer0 *tm,uint32_t top, uint32_t tov_set_at) 
{
	int32_t timer_steps_ocra = -1;
        int32_t timer_steps_ocrb = -1;
        int32_t timer_steps_tov = -1;
        int32_t min = 0x10000000;
        uint8_t required_mask = tm->timsk & ~tm->tifr;
	if(required_mask & TIMSK_OCIEA) {
                if(unlikely(tm->tcnt > top)) {
                        timer_steps_ocra = (0x100 - tm->tcnt) + top + 1;
                } else {
                        if(tm->ocra <= top) {
                                timer_steps_ocra = (tm->ocra + 1 - tm->tcnt);
                        }
                }
        }
        if(required_mask & TIMSK_OCIEB) {
                if(unlikely(tm->tcnt > top)) {
                        timer_steps_ocrb = (0x100 - tm->tcnt) + top + 1;
                } else  {
                        if(tm->ocrb <= top) {
                                timer_steps_ocrb = (tm->ocrb + 1 - tm->tcnt);
                        }
                }
        }
        if(required_mask & TIMSK_TOIE) {
                if(top >= tov_set_at) {
                        timer_steps_tov = (top + 1 - tm->tcnt);
                }
        }
        if((timer_steps_ocra >= 0)) {
                min = timer_steps_ocra;
        }
        if((timer_steps_ocrb >= 0)) {
                min = (min < timer_steps_ocrb) ? min : timer_steps_ocrb;
        }
        if((timer_steps_tov >= 0)) {
                min = (min < timer_steps_tov) ? min : timer_steps_tov;
        }
	if(min != 0x10000000) {
		CycleCounter_t timeout;
		FractionU64_t frac;
                uint64_t cycles = 1000;
                frac = Clock_MasterRatio(tm->clk_t0);
                if(frac.nom && frac.denom) {
                        cycles = min * frac.denom / frac.nom;
                }
		if(unlikely(cycles < tm->remainder)) {
			fprintf(stderr,"Bug in %s %d, cycles %"PRId64", remainder %"PRId64"\n",__FILE__,__LINE__
				,cycles,tm->remainder);
			return;
		}
		cycles -= tm->remainder;
                timeout = CycleCounter_Get() + cycles;
                if(timeout != tm->timeout) {
                        CycleTimer_Mod(&tm->event_timer,cycles);
                        tm->timeout = timeout;
                }
        } else {
                tm->timeout = 0;
                CycleTimer_Remove(&tm->event_timer);
        }

}
static void
update_timeout(ATM644_Timer0 *tm) 
{
	switch(tm->wg_mode) {
		case WGM_NORMAL:
			update_tout(tm,0xff,0xff);
			break;

		case WGM_PWM_PC_FF:
			update_tout(tm,0xff,0);
			break;

		case WGM_CTC:
			update_tout(tm,tm->ocra,0xff);
			break;

		case WGM_FPWM_FF:
			update_tout(tm,0xff,0xff);
			break;
		case WGM_PWM_PC_OCRA:
			update_tout(tm,tm->ocra,0);
			break;

		case WGM_FPWM_OCRA:
			update_tout(tm,tm->ocra,tm->ocra);
			break;
	}
}

static void
timer_event(void *clientData)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *)clientData;
        actualize_counter(tm);
	update_timeout(tm);
}

static void
update_waveform_generation_mode(ATM644_Timer0 *tm) 
{
	int wgm;
	wgm = tm->tccra & (TCCRA_WGM1 | TCCRA_WGM0);
	wgm |= (!!(tm->tccrb & TCCRB_WGM2)) << 2;
	tm->wg_mode = wgm;
}
/*
 *************************************************************************************
 * TCCRA register
 *	COMA1 	(Bit 7)
 *	COMA0 	(Bit 6) Determines behaviour of OC0A output
 *	COMB1 	(Bit 5)
 * 	COMB0 	(Bit 4) Determines behaviour of OC0B output
 * 	WGM1  	(Bit 1) Waveform generation Mode
 * 	WGM0  	(Bit 0) Waveform generation Mode
 *************************************************************************************
 */
static uint8_t
tccra_read(void *clientData,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        return tm->tccra; 
}

static void
tccra_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	actualize_counter(tm);
	tm->tccra = value;	
	update_waveform_generation_mode(tm);
	update_timeout(tm);
}

/*
 ******************************************************
 * TCCRB Register
 *	FOCA   (Bit 7)
 * 	FOCB   (Bit 6)
 *	WGM2   (Bit 3) Time Waveform generation mode
 *	CS2    (Bit 2)
 *	CS1    (Bit 1)
 *	CS0    (Bit 0)
 ******************************************************
 */
static uint8_t
tccrb_read(void *clientData,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        return tm->tccrb; 
}

static void
tccrb_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	uint8_t diff = tm->tccrb ^ value; 
	actualize_counter(tm);
	tm->tccrb = value;
	if(diff & 7) {
		update_clocks(tm);
	}
	update_waveform_generation_mode(tm);
	update_timeout(tm);
}

static uint8_t
tcnt_read(void *clientData,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	actualize_counter(tm);
        return tm->tcnt; 
}

static void
tcnt_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	actualize_counter(tm);
	tm->tcnt = value;
	update_timeout(tm);
	fprintf(stderr,"Register behaviout not known for forbidden regions\n");
}

static uint8_t
ocra_read(void *clientData,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        return tm->ocra; 
}

static void
ocra_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	actualize_counter(tm);
	tm->ocra = value;
	update_timeout(tm);
}

static uint8_t
ocrb_read(void *clientData,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        return tm->ocrb; 
}

static void
ocrb_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	actualize_counter(tm);
	tm->ocrb = value;
	update_timeout(tm);
}

static uint8_t
timsk_read(void *clientData,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	return tm->timsk;
}

static void
timsk_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	actualize_counter(tm);
	tm->timsk = value;
	update_interrupts(tm);
	update_timeout(tm);
}

static uint8_t
tifr_read(void *clientData,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	return tm->tifr;
}

static void
tifr_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
	uint8_t clear = tm->tifr & value;
	fprintf(stderr,"TIFR write %02x\n",value);
	actualize_counter(tm);
	tm->tifr &= ~clear;
	update_interrupts(tm);
	update_timeout(tm);
}

static void compbAckIrq(SigNode *node,int value,void *clientData)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        if(value == SIG_LOW) {
		//fprintf(stderr,"compbAck, tifr %02x\n",tm->tifr);
		if(!(tm->tifr & TIFR_OCFB & tm->timsk)) {
			fprintf(stderr,"Bug: Ack of nonposted Interrupt compb\n");
		}
		actualize_counter(tm);
		tm->tifr &= ~TIFR_OCFB;
		update_interrupts(tm);
		update_timeout(tm); 
        }
}
static void compaAckIrq(SigNode *node,int value,void *clientData)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        if(value == SIG_LOW) {
		if(!(tm->tifr & TIFR_OCFA & tm->timsk)) {
			fprintf(stderr,"Bug: Ack of nonposted Interrupt compa\n");
		}
		actualize_counter(tm);
		tm->tifr &= ~TIFR_OCFA;
		update_interrupts(tm);
		update_timeout(tm); 
        }
}

static void ovfAckIrq(SigNode *node,int value,void *clientData)
{
        ATM644_Timer0 *tm = (ATM644_Timer0 *) clientData;
        if(value == SIG_LOW) {
		if(!(tm->tifr & TIFR_TOV & tm->timsk)) {
			fprintf(stderr,"Bug: Ack of nonposted Interrupt TOV\n");
		}
		actualize_counter(tm);
		tm->tifr &= ~TIFR_TOV;
		update_interrupts(tm);
		update_timeout(tm); 
        }
}


void
ATM644_Timer02New(const char *name,uint32_t base,uint32_t timsk_offset)
{
	ATM644_Timer0 *tm = sg_new(ATM644_Timer0);
	//AVR8_RegisterIOHandler(GTCCR(base),gtccr_read,gtccr_write,tm);
	AVR8_RegisterIOHandler(TCCRA(base),tccra_read,tccra_write,tm);
	AVR8_RegisterIOHandler(TCCRB(base),tccrb_read,tccrb_write,tm);
	AVR8_RegisterIOHandler(TCNT(base),tcnt_read,tcnt_write,tm);
	AVR8_RegisterIOHandler(OCRA(base),ocra_read,ocra_write,tm);
	AVR8_RegisterIOHandler(OCRB(base),ocrb_read,ocrb_write,tm);
	AVR8_RegisterIOHandler(TIMSK(timsk_offset),timsk_read,timsk_write,tm);
	AVR8_RegisterIOHandler(TIFR(timsk_offset),tifr_read,tifr_write,tm);
	tm->compaIrq = SigNode_New("%s.compaIrq",name);
        tm->compbIrq = SigNode_New("%s.compbIrq",name);
        tm->ovfIrq = SigNode_New("%s.ovfIrq",name);
	tm->compaAckIrq = SigNode_New("%s.compaAckIrq",name);
        tm->compbAckIrq = SigNode_New("%s.compbAckIrq",name);
        tm->ovfAckIrq = SigNode_New("%s.ovfAckIrq",name);
	SigNode_Set(tm->compaIrq,SIG_OPEN);
	SigNode_Set(tm->compbIrq,SIG_OPEN);
	SigNode_Set(tm->ovfIrq,SIG_OPEN);
	SigNode_Set(tm->compaAckIrq,SIG_OPEN);
	SigNode_Set(tm->compbAckIrq,SIG_OPEN);
	SigNode_Set(tm->ovfAckIrq,SIG_OPEN);
	tm->clk_t0 = Clock_New("%s.clk_t0",name);
	tm->clk_in = Clock_New("%s.clk",name);
	update_clocks(tm);
	CycleTimer_Init(&tm->event_timer,timer_event,tm);
	CycleTimer_Init(&tm->set_ocfa_timer,set_ocfa,tm);
	CycleTimer_Init(&tm->set_ocfb_timer,set_ocfb,tm);
	CycleTimer_Init(&tm->set_tov_timer,set_tov,tm);

	SigNode_Trace(tm->compbAckIrq,compbAckIrq,tm);
	SigNode_Trace(tm->compaAckIrq,compaAckIrq,tm);
	SigNode_Trace(tm->ovfAckIrq,ovfAckIrq,tm);
	update_interrupts(tm);
	actualize_counter(tm);
	fprintf(stderr,"Created ATMegaXX4 Timer 0/2 \"%s\"\n",name);
}
