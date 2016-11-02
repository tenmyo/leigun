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
#include <string.h>
#include "avr8_io.h" 
#include "avr8_cpu.h" 
#include "sgstring.h"
#include "sgtypes.h"
#include "sound.h"
#include "signode.h"
#include "cycletimer.h"
#include "clock.h"
#include "atm644_timer02.h"

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
	SoundDevice *sdev;
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
			fprintf(stderr,"Post ocieb\n");
			SigNode_Set(tm->compbIrq,SIG_LOW);
		} else {
			SigNode_Set(tm->compbIrq,SIG_OPEN);
		}
	}
	if(diff & TIMSK_OCIEA) {
		if(ints & TIMSK_OCIEA) {
			//fprintf(stderr,"Post ociea, raw %04x, sig %04x\n",gavr8.cpu_signals_raw,gavr8.cpu_signals);
			SigNode_Set(tm->compaIrq,SIG_LOW);
		} else {
			//fprintf(stderr,"UnPost ociea\n");
			SigNode_Set(tm->compaIrq,SIG_OPEN);
		}
	}
	if(diff & TIMSK_TOIE) {
		if(ints & TIMSK_TOIE) {
			fprintf(stderr,"Post TOIE\n");
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
update_timeout(ATM644_Timer0 *tm) 
{
	int16_t timer_steps_ocra = -1;
	int16_t timer_steps_ocrb = -1;
	int16_t timer_steps_tov = -1;
	int16_t min = 32767;
	int ocra_meassured,ocrb_meassured,tov_meassured;
	switch(tm->wg_mode) {
		case WGM_NORMAL:
			timer_steps_ocra = (tm->ocra - tm->tcnt);
			if(timer_steps_ocra <= 0) {
				timer_steps_ocra += 0x100;
			}
			timer_steps_ocrb = (tm->ocrb - tm->tcnt);
			if(timer_steps_ocrb <= 0) {
				timer_steps_ocrb += 0x100;
			}
			timer_steps_tov = (0xff - tm->tcnt);
			if(timer_steps_tov == 0) {
				timer_steps_tov += 0x100;
			}
			break;
		case WGM_PWM_PC_FF:
			timer_steps_ocra = (tm->ocra - tm->tcnt);
			if(timer_steps_ocra <= 0) {
				timer_steps_ocra += 0x100;
			}
			timer_steps_ocrb = (tm->ocrb - tm->tcnt);
			if(timer_steps_ocrb <= 0) {
				timer_steps_ocrb += 0x100;
			}
			timer_steps_tov = (0x100 - tm->tcnt);
			break;
		case WGM_CTC:
			if(tm->tcnt > tm->ocra) {
				timer_steps_ocra = (0x100 - tm->tcnt) + tm->ocra + 1;
			} else {
				timer_steps_ocra = (tm->ocra + 1 - tm->tcnt);
			}
			if(tm->ocrb <= tm->ocra) {
				timer_steps_ocrb = (tm->ocrb + 1 - tm->tcnt);
				if(timer_steps_ocrb <= 0) {
					timer_steps_ocrb += tm->ocra + 1;
				}
			}
			if(tm->ocra == 0xff) {
				timer_steps_tov = (0x100 - tm->tcnt);
			}
			break;
		case WGM_FPWM_FF:
			timer_steps_ocra = (tm->ocra - tm->tcnt);	
			if(timer_steps_ocra <= 0) {
				timer_steps_ocra += 0x100;
			}
			timer_steps_ocrb = (tm->ocrb - tm->tcnt);
			if(timer_steps_ocrb <= 0) {
				timer_steps_ocrb += 0x100;
			}
			timer_steps_tov = (0xff - tm->tcnt);
			if(timer_steps_tov == 0) {
				timer_steps_tov = 0x100;
			}
			break;

		case WGM_PWM_PC_OCRA:
			timer_steps_ocra = (tm->ocra - tm->tcnt);	
			if(timer_steps_ocra == 0) {
				timer_steps_ocra = tm->ocra + 1;
			}
			if(tm->ocrb <= tm->ocra) {
				timer_steps_ocrb = (tm->ocrb - tm->tcnt);
				if(timer_steps_ocrb <= 0) {
					timer_steps_ocrb += tm->ocra + 1;
				}
			}
			timer_steps_tov = (tm->ocra + 1 - tm->tcnt);
			/* Should not happen ? */
			if(timer_steps_tov <= 0) {
				timer_steps_tov += 0x100;
			}
			break;

		case WGM_FPWM_OCRA:
			timer_steps_ocra = (tm->ocra - tm->tcnt);	
			if(timer_steps_ocra == 0) {
				timer_steps_ocra = tm->ocra + 1;
			}
			if(tm->ocrb <= tm->ocra) {
				timer_steps_ocrb = (tm->ocrb - tm->tcnt);
				if(timer_steps_ocrb <= 0) {
					timer_steps_ocrb += tm->ocra + 1;
				}
			}
			timer_steps_tov = tm->ocra - tm->tcnt;
			if(timer_steps_tov <= 0) {
				timer_steps_tov =  tm->ocra + 1;
			}
			break;
	}
	if(tm->timsk & TIMSK_OCIEB) {
		ocrb_meassured = 1;
	} else {
		ocrb_meassured = 0;
	}
	if(tm->timsk & TIMSK_OCIEA) {
		ocra_meassured = 1;
	} else {
		ocra_meassured = 0;
	}
	if(tm->timsk & TIMSK_TOIE) {
		tov_meassured = 1;
	} else {
		tov_meassured = 0;
	}


	if((timer_steps_ocra >= 0) && (ocra_meassured)) {
		min = timer_steps_ocra;
	} 
	if((timer_steps_ocrb >= 0) && (ocrb_meassured)) {
		min = (min < timer_steps_ocrb) ? min : timer_steps_ocrb;
	}
	if((timer_steps_tov >= 0) && (tov_meassured)) {
		min = (min < timer_steps_tov) ? min : timer_steps_tov;
	}
	if(min != 32767) {
		FractionU64_t frac;
		uint64_t cycles = 1000;
		frac = Clock_MasterRatio(tm->clk_t0);
		if(frac.nom && frac.denom) {
			cycles = min * frac.denom / frac.nom; 
		}
		if(unlikely(cycles < tm->remainder)) {
			fprintf(stderr,"Bug in %s %d, clk freq %u\n",__FILE__,__LINE__,(uint32_t)Clock_Freq(tm->clk_t0));
			return;
		}
		cycles -= tm->remainder;
		CycleTimer_Mod(&tm->event_timer,cycles);
	} else {
		CycleTimer_Remove(&tm->event_timer);
	}
}

static void
actualize_counter(ATM644_Timer0 *tm)
{
	FractionU64_t frac;
	uint64_t timer_steps;

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
	switch(tm->wg_mode) {
		case WGM_NORMAL:
			if((tm->tcnt + timer_steps) >= tm->ocra) {
				tm->tifr |= TIFR_OCFA;
			}
			if(((tm->tcnt + timer_steps) >= tm->ocrb)) {
				tm->tifr |= TIFR_OCFB;
			}
			if(((tm->tcnt + timer_steps) >= 0xff)) {
				tm->tifr |= TIFR_TOV;
			}
			update_interrupts(tm);
			tm->tcnt += timer_steps;
			break;

		case WGM_PWM_PC_FF:
			if((tm->tcnt + timer_steps) >= tm->ocra) {
				tm->tifr |= TIFR_OCFA;
			}
			if(((tm->tcnt + timer_steps) >= tm->ocrb)) {
				tm->tifr |= TIFR_OCFB;
			}
			if(((tm->tcnt + timer_steps) >= 0x100) || ((tm->tcnt + timer_steps) == 0)) {
				tm->tifr |= TIFR_TOV;
			}
			update_interrupts(tm);
			tm->tcnt += timer_steps;
			break;

		case WGM_CTC:
			if((tm->tcnt + timer_steps) > tm->ocra) {
				tm->tifr |= TIFR_OCFA;
			}
			if(((tm->tcnt + timer_steps) > tm->ocrb) && (tm->ocrb <= tm->ocra)) {
				tm->tifr |= TIFR_OCFB;
			}
			if(((tm->tcnt + timer_steps) > 0xff) && (tm->ocra == 0xff)) {
				tm->tifr |= TIFR_TOV;
			}
			update_interrupts(tm);
			tm->tcnt = ((uint64_t)tm->tcnt + timer_steps) % (tm->ocra + 1); ;
			break;

		case WGM_FPWM_FF:
			if((tm->tcnt + timer_steps) >= tm->ocra) {
				tm->tifr |= TIFR_OCFA;
			}
			if(((tm->tcnt + timer_steps) >= tm->ocrb)) {
				tm->tifr |= TIFR_OCFB;
			}
			if((tm->tcnt + timer_steps) >= 0xff) {
				tm->tifr |= TIFR_TOV;
			}
			update_interrupts(tm);
			tm->tcnt += timer_steps;
			break;
		case WGM_PWM_PC_OCRA:
			if((tm->tcnt + timer_steps) >= tm->ocra) {
				tm->tifr |= TIFR_OCFA;
			}
			if(((tm->tcnt + timer_steps) >= tm->ocrb) && (tm->ocrb <= tm->ocra)) {
				tm->tifr |= TIFR_OCFB;
			}
			if(((tm->tcnt + timer_steps) > tm->ocra) || ((tm->tcnt + timer_steps) == 0)) {
				tm->tifr |= TIFR_TOV;
			}
			update_interrupts(tm);
			tm->tcnt = ((uint64_t)tm->tcnt + timer_steps) % (tm->ocra + 1); ;
			break;

		case WGM_FPWM_OCRA:
			if((tm->tcnt + timer_steps) >= tm->ocra) {
				tm->tifr |= TIFR_OCFA;
			}
			if(((tm->tcnt + timer_steps) >= tm->ocrb) && (tm->ocrb <= tm->ocra)) {
				tm->tifr |= TIFR_OCFB;
			}
			if((tm->tcnt + timer_steps) >= tm->ocra) {
				tm->tifr |= TIFR_TOV;
			}
			update_interrupts(tm);
			tm->tcnt = ((uint64_t)tm->tcnt + timer_steps) % (tm->ocra + 1); ;
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
	Sound_PlaySamples(tm->sdev,&value,1);
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
Uze_Timer2New(const char *name,SoundDevice *sdev)
{
	SoundFormat sf;
	uint32_t base = 0xb0;
	uint32_t timsk_offset = 2;
	ATM644_Timer0 *tm = sg_new(ATM644_Timer0);
	tm->sdev = sdev;
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
	SigNode_Trace(tm->compbAckIrq,compbAckIrq,tm);
	SigNode_Trace(tm->compaAckIrq,compaAckIrq,tm);
	SigNode_Trace(tm->ovfAckIrq,ovfAckIrq,tm);
	update_interrupts(tm);
	actualize_counter(tm);
	sf.channels = 1;
	sf.sg_snd_format = SG_SND_PCM_FORMAT_U8;
	sf.samplerate = 15724;
	Sound_SetFormat(sdev,&sf);
	fprintf(stderr,"Created Uzebox timer 2 \"%s\"\n",name);
}
