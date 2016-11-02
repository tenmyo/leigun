/*
 ***************************************************************************************************
 *
 * Emulation of Hilscher NetX GPIO and timer module 
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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "bus.h"
#include "sgstring.h"
#include "fio.h"
#include "signode.h"
#include "clock.h"
#include "cycletimer.h"
#include "netx_gpio.h"
#include "senseless.h"
#include "sgtypes.h"

/* Register definitions taken from linux kernel */
#define NETX_GPIO_CFG(base,index)      		((base) + 0x0  + ((index) << 2))
#define NETX_GPIO_THRESH_CAPT(base,index)	((base) + 0x40 + ((index) << 2))
#define NETX_GPIO_CTR_CTRL(base,counter)	((base) + 0x80 + ((counter) << 2))
#define NETX_GPIO_CTR_MAX(base,counter) 	((base) + 0x94 + ((counter) << 2))
#define NETX_GPIO_CTR_CURR(base,counter)	((base) + 0xa8 + ((counter) << 2))
#define NETX_GPIO_IRQ_ENABLE(base)              ((base) + 0xbc)
#define NETX_GPIO_IRQ_DISABLE(base)            	((base) + 0xc0)
#define NETX_GPIO_SYSTIME_NS_CMP(base)         	((base) + 0xc4)
#define NETX_GPIO_IO(base)                   	((base) + 0xc8)
#define NETX_GPIO_IRQ(base)                   	((base) + 0xd0)

/* Bits */
#define CFG_IOCFG_GP_INPUT                 (0x0)
#define CFG_IOCFG_GP_OUTPUT                (0x1)
#define CFG_IOCFG_GP_UART                  (0x2)
#define CFG_INV                            (1<<2)
#define CFG_MODE_INPUT_READ                (0<<3)
#define CFG_MODE_INPUT_CAPTURE_CONT_RISING (1<<3)
#define CFG_MODE_INPUT_CAPTURE_ONCE_RISING (2<<3)
#define CFG_MODE_INPUT_CAPTURE_HIGH_LEVEL  (3<<3)
#define CFG_COUNT_REF_COUNTER0             (0<<5)
#define CFG_COUNT_REF_COUNTER1             (1<<5)
#define CFG_COUNT_REF_COUNTER2             (2<<5)
#define CFG_COUNT_REF_COUNTER3             (3<<5)
#define CFG_COUNT_REF_COUNTER4             (4<<5)
#define CFG_COUNT_REF_SYSTIME              (7<<5)

#define CTRL_RUN                   (1<<0)
#define CTRL_SYM                   (1<<1)
#define CTRL_ONCE                  (1<<2)
#define CTRL_IRQ_EN                (1<<3)
#define CTRL_CNT_EVENT             (1<<4)
#define CTRL_RST_EN                (1<<5)
#define CTRL_SEL_EVENT             (1<<6)
#define CTRL_GPIO_REF /* FIXME */

typedef struct NetXGpio NetXGpio; 

typedef struct NetXTimer {
	NetXGpio *gpio;
	CycleTimer event_timer;
	SigNode *sigIrq;

	int index;
	uint32_t reg_ctr_ctrl;
	uint32_t reg_ctr_max;

	/* This is not the value read from the ctr register ! */
	uint64_t count;

	CycleCounter_t last_actualized;
	CycleCounter_t accumulated_cycles;
} NetXTimer;

struct NetXGpio {
	BusDevice bdev;
	NetXTimer timer[5];

	uint32_t reg_cfg[16];	
	uint32_t reg_thresh_capt[16];

	uint32_t reg_inten;
	uint32_t reg_systime;
	uint32_t reg_io;
	uint32_t reg_irq;

	SigNode *sigIrq;
	int interrupt_posted;
	SigNode *sigGpio15Irq;
	Clock_t *clk_in;
};

static inline uint64_t 
ctr_period(NetXTimer *tm)
{
	uint32_t ctrl = tm->reg_ctr_ctrl;	
	uint64_t period;
	if(ctrl & CTRL_SYM) {
		/* triangle */
		period = 2 * (uint64_t)tm->reg_ctr_max;  
	} else {
		period = (uint64_t)tm->reg_ctr_max + 1;  
	}
	return period;
}

static void
update_timer_interrupt(NetXTimer *tm) 
{
	NetXGpio *gpio = tm->gpio;
	if((tm->reg_ctr_ctrl & CTRL_IRQ_EN) && 
	    (gpio->reg_inten & gpio->reg_irq & (0x10000 << tm->index))) {
		SigNode_Set(tm->sigIrq,SIG_LOW);
	} else {
		SigNode_Set(tm->sigIrq,SIG_HIGH);
	}
}

static void
update_gpio_interrupt(NetXGpio *gpio) 
{
	if(gpio->reg_inten & gpio->reg_irq & 0xffff) {
		//fprintf(stderr,"Poste timer interrupt, val %d\n",SigNode_Val(gpio->sigIrq));
                if(!gpio->interrupt_posted) {
                        SigNode_Set(gpio->sigIrq,SIG_LOW);
                        gpio->interrupt_posted=1;
                }
        } else {
		//fprintf(stderr,"unPoste timer interrupt inten %08x irq %08x\n",gpio->reg_inten,gpio->reg_irq);
                if(gpio->interrupt_posted) {
                        SigNode_Set(gpio->sigIrq,SIG_HIGH);
                        gpio->interrupt_posted=0;
                }
        }
}


static void
actualize_counter(NetXTimer *tm) 
{
	FractionU64_t frac;
	uint32_t ctrl;
	uint64_t elapsed_cycles,acc;
	uint64_t counter_cycles;
	uint64_t count;
	uint64_t period;
	NetXGpio *gpio = tm->gpio;

	ctrl = tm->reg_ctr_ctrl;	
	elapsed_cycles = CycleCounter_Get() - tm->last_actualized; 
	tm->last_actualized = CycleCounter_Get(); 
	acc = tm->accumulated_cycles + elapsed_cycles;
	frac = Clock_MasterRatio(gpio->clk_in);
	if((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr,"Warning: gpio module has no clock frequency\n");
		return;
	}
	counter_cycles = acc * frac.nom / frac.denom;
	acc -= counter_cycles * frac.denom / frac.nom;
	tm->accumulated_cycles = acc;
	if(!(ctrl & CTRL_RUN)) {
		return;
	} 
	period = ctr_period(tm);
	//fprintf(stderr,"elapsed %lld, acc %lld, hz %d, adv %lld, period %lld\n",elapsed_cycles,acc,hz,counter_cycles,period);
	if(!period) {
		tm->count = 0;
		gpio->reg_irq |= (0x10000 << tm->index);
		update_timer_interrupt(tm);
	} else {
		count = tm->count;
		count += counter_cycles;
		if(count >= period) {
			tm->count = count = count % period;
			gpio->reg_irq |= (0x10000 << tm->index);
			update_timer_interrupt(tm);
			//fprintf(stderr,"Count %lld, period %lld\n",count,period);
		} else {
			//fprintf(stderr,"count %lld, period %lld\n",count,period);
			tm->count = count;
		}
	}
	
}

static void
update_timeout(NetXTimer *tm) 
{
	uint64_t period;
	int64_t remaining;
	CycleCounter_t master_cycles;
	uint32_t ctrl = tm->reg_ctr_ctrl;	
	uint64_t count = tm->count;
	FractionU64_t frac;
	if(!(ctrl & CTRL_RUN)) {
		/* Should delete old timer here */
		return;
	}
	period = ctr_period(tm);
	if(!period) {	
		return;
	}
	remaining = period - count;	
	if(remaining < 0) {
		fprintf(stderr,"Bug: update timeout with non actualized counter\n");
		return;
	}
	frac = Clock_MasterRatio(tm->gpio->clk_in);
	if(frac.nom) {
		master_cycles = remaining * frac.denom / frac.nom;
		master_cycles -= tm->accumulated_cycles;
		CycleTimer_Mod(&tm->event_timer,master_cycles);
	} else {
		fprintf(stderr,"Warning, Bad clock for timer module\n");
	}
	//fprintf(stderr,"Mod timer%d, cpu %lld, remaining %lld\n",tm->index,cpu_cycles,remaining);
}

static void
timer_event(void *clientData)
{
        NetXTimer *tm = (NetXTimer *)clientData;
        actualize_counter(tm);
        update_timeout(tm);
}

/*
 ***************************************************************
 * Calculate counter shown to the outside from the internal 
 * 64 Bit counter which runs from 0 to max in sawtooth mode
 * and from 0 to 2 * max in symetric mode 
 ***************************************************************
 */
static inline uint32_t 
reg_counter(NetXGpio *gpio,unsigned int ctr)
{
	uint64_t count = gpio->timer[ctr].count;
	if(count > gpio->timer[ctr].reg_ctr_max) {
		return count - gpio->timer[ctr].reg_ctr_max;
	} else {
		return count;
	}	
}

static inline unsigned int get_gpio_index(uint32_t addr) 
{
	 return ((addr & 0x3f) >> 2) % 16;	
}

/*
 ***************************************************************
 * Bit 0-1: IOCFG
 *	    0: GP_INPUT
 *	    1: GP_OUTPUT
 *	    2: GP_UART
 * Bit 2: IVN
 * Bit 3-4: MODE
 *	    Input:
 *	    0: Input Read
 *	    1: Capture rising
 *	    2: Capture once at rising
 *	    3: Capture high
 *	    Output:
 *	    0: Output set to 0
 *	    1: Output set to 1
 *	    2: set to gpio_out[i]
 *	    3: pwm mode
 * Bit 5-7: Counter Reference
 *	    0 - 4: Counter 0 - 4
 *          7: Reference is Systime
 ***************************************************************
 */

static uint32_t
gpio_cfg_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"Gpio register %08x not implemented\n",address);
        return 0; 
}

static void
gpio_cfg_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Gpio register %08x not implemented\n",address);

}

static uint32_t
thresh_capt_read(void *clientData,uint32_t address,int rqlen)
{
	NetXGpio *gpio = (NetXGpio *) clientData;
	uint32_t index = get_gpio_index(address); 
	return gpio->reg_thresh_capt[index];
}

static void
thresh_capt_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	NetXGpio *gpio = (NetXGpio *) clientData;
	uint32_t index = get_gpio_index(address); 
	gpio->reg_thresh_capt[index] = value;
	//update_timeout of the counter belonging to this threshold
}

/*
 *****************************************************************************
 * Bit 0: RUN Start/Stop counter.
 * Bit 1: SYM Symetric mode or sawtooth mode.
 * Bit 2: ONCE Count once or count continuous.
 * Bit 3: IRQ_EN Enable interrupt request
 * Bit 4: CNT_EVENT Count external events/Count every clock cycle
 * Bit 5: RST_EN Enable automatic reset ????????
 * Bit 6: SEL_EVENT Select external event: 0 = (high) level, 1 = pos edge
 * Bit 7-10: GPIO_REF
 *****************************************************************************
 */
static uint32_t
ctr_ctrl_read(void *clientData,uint32_t address,int rqlen)
{
	NetXTimer *tm = (NetXTimer *) clientData;
	return tm->reg_ctr_ctrl;
}

static void
ctr_ctrl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	NetXTimer *tm = (NetXTimer *) clientData;
	uint32_t diff = value ^ tm->reg_ctr_ctrl;
	if(diff & (CTRL_RUN | CTRL_SYM)) {
		actualize_counter(tm);
	}
	tm->reg_ctr_ctrl = value;
	if(diff & (CTRL_SEL_EVENT | CTRL_RUN | CTRL_SYM | CTRL_IRQ_EN | CTRL_CNT_EVENT)) {
		update_timeout(tm);
		update_timer_interrupt(tm);
	}
	fprintf(stderr,"Counter%d Control write value: %08x\n",tm->index,value);
}


static uint32_t
ctr_max_read(void *clientData,uint32_t address,int rqlen)
{
	NetXTimer *tm = (NetXTimer *) clientData;
	return tm->reg_ctr_max;
}

static void
ctr_max_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	NetXTimer *tm = (NetXTimer *) clientData;

	actualize_counter(tm);
	tm->reg_ctr_max = value;
	update_timeout(tm);
	fprintf(stderr,"Ctr%d max is now %08x\n",tm->index,value);
}

static uint32_t
ctr_curr_read(void *clientData,uint32_t address,int rqlen)
{
	NetXTimer *tm = (NetXTimer *) clientData;
 	NetXGpio *gpio = tm->gpio; 

	actualize_counter(tm);
	//fprintf(stderr,"Gpio Ctr %d read: %08x\n",index,reg_counter(gpio,index));
	Senseless_Report(150);
        return reg_counter(gpio,tm->index); 
}

static void
ctr_curr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	NetXTimer *tm = (NetXTimer *) clientData;
	actualize_counter(tm);
	/* I Do not know in which half of the triangle i should start */
	fprintf(stderr,"Gpio ctr write\n");
	tm->count = (uint64_t)value;
}

static uint32_t
irq_enable_read(void *clientData,uint32_t address,int rqlen)
{
 	NetXGpio *gpio = (NetXGpio *) clientData;
	return gpio->reg_inten;
}

static void
irq_enable_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
 	NetXGpio *gpio = (NetXGpio *) clientData;
	int i;
	uint32_t diff =  ~gpio->reg_inten & value;
	gpio->reg_inten |= value;
	if(diff & 0xffff) {
		update_gpio_interrupt(gpio);
	}	
	for(i = 0;i < 5; i++) {
		if(diff & (0x10000 << i)) {
			update_timer_interrupt(&gpio->timer[i]);
		} 
	}
}

static uint32_t
irq_disable_read(void *clientData,uint32_t address,int rqlen)
{
 	NetXGpio *gpio = (NetXGpio *) clientData;
	fprintf(stderr,"Reading from write only register GPIO IRQ-Disable\n");
	return gpio->reg_inten;
}

static void
irq_disable_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
 	NetXGpio *gpio = (NetXGpio *) clientData;
	uint32_t diff = gpio->reg_inten & value;
	int i;
	gpio->reg_inten &= ~value;
	if(diff & 0xffff) {
		update_gpio_interrupt(gpio);
	}
	for(i = 0;i < 5; i++) {
		if(diff & (0x10000 << i)) {
			update_timer_interrupt(&gpio->timer[i]);
		} 
	}
}

static uint32_t
systime_ns_cmp_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"Gpio register %08x not implemented\n",address);
        return 0; 
}

static void
systime_ns_cmp_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Gpio register %08x not implemented\n",address);
}

static uint32_t
io_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"Gpio register %08x not implemented\n",address);
        return 0; 
}

static void
io_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Gpio register %08x not implemented\n",address);
}

static uint32_t
irq_read(void *clientData,uint32_t address,int rqlen)
{
	NetXGpio *gpio = (NetXGpio *) clientData;
	return gpio->reg_irq;
}

static void
irq_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	NetXGpio *gpio = (NetXGpio *) clientData;
	int i;
	uint32_t diff = gpio->reg_irq & value;
	gpio->reg_irq &= ~value;
	if(diff & 0xffff) {
		update_gpio_interrupt(gpio);
	}
	for(i = 0;i < 5; i++) {
		if(diff & (0x10000 << i)) {
			update_timer_interrupt(&gpio->timer[i]);
		} 
	}
}


static void
NetXGpio_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
        NetXGpio *gpio = (NetXGpio *) owner;
	int i;
	for(i = 0; i < 16; i++) {
        	IOH_New32(NETX_GPIO_CFG(base,i),gpio_cfg_read,gpio_cfg_write,gpio);
		IOH_New32(NETX_GPIO_THRESH_CAPT(base,i),thresh_capt_read,thresh_capt_write,gpio);
	}
	for(i = 0; i < 5; i++) {
		NetXTimer *tm = &gpio->timer[i];
		IOH_New32(NETX_GPIO_CTR_CTRL(base,i),ctr_ctrl_read,ctr_ctrl_write,tm);
		IOH_New32(NETX_GPIO_CTR_MAX(base,i),ctr_max_read,ctr_max_write,tm);
		IOH_New32(NETX_GPIO_CTR_CURR(base,i),ctr_curr_read,ctr_curr_write,tm);
	}
	IOH_New32(NETX_GPIO_IRQ_ENABLE(base),irq_enable_read,irq_enable_write,gpio);
	IOH_New32(NETX_GPIO_IRQ_DISABLE(base),irq_disable_read,irq_disable_write,gpio);
	IOH_New32(NETX_GPIO_SYSTIME_NS_CMP(base),systime_ns_cmp_read,systime_ns_cmp_write,gpio);
	IOH_New32(NETX_GPIO_IO(base),io_read,io_write,gpio);	
	IOH_New32(NETX_GPIO_IRQ(base),irq_read,irq_write,gpio);
}

static void
NetXGpio_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	int i;
	for(i = 0; i < 16; i++) {
        	IOH_Delete32(NETX_GPIO_CFG(base,i));
		IOH_Delete32(NETX_GPIO_THRESH_CAPT(base,i));
	}
	for(i = 0; i < 5; i++) {
		IOH_Delete32(NETX_GPIO_CTR_CTRL(base,i));
		IOH_Delete32(NETX_GPIO_CTR_MAX(base,i));
		IOH_Delete32(NETX_GPIO_CTR_CURR(base,i));
	}
	IOH_Delete32(NETX_GPIO_IRQ_ENABLE(base));
	IOH_Delete32(NETX_GPIO_IRQ_DISABLE(base));
	IOH_Delete32(NETX_GPIO_SYSTIME_NS_CMP(base));
	IOH_Delete32(NETX_GPIO_IO(base));	
	IOH_Delete32(NETX_GPIO_IRQ(base));
}


BusDevice *
NetXGpio_New(const char *devname) {
	NetXGpio *gpio = sg_new(NetXGpio);
	int i;
        gpio->bdev.first_mapping = NULL;
        gpio->bdev.Map = NetXGpio_Map;
        gpio->bdev.UnMap = NetXGpio_UnMap;
        gpio->bdev.owner = gpio;
        gpio->bdev.hw_flags = MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;

        gpio->sigIrq = SigNode_New("%s.irq",devname);
        gpio->sigGpio15Irq = SigNode_New("%s.gpio15irq",devname);
	if(!gpio->sigIrq || !gpio->sigGpio15Irq) {
                fprintf(stderr,"Can not create irq signals for \"%s\"\n",devname);
	}
        gpio->clk_in = Clock_New("%s.clk",devname);
        if(!gpio->clk_in) {
                fprintf(stderr,"Can not create clock for \"%s\"\n",devname);
                exit(1);
        }
        SigNode_Set(gpio->sigIrq,SIG_HIGH); /* No request on startup */
        gpio->interrupt_posted=0;
	for(i = 0; i < 5; i++) {
		NetXTimer *timer = &gpio->timer[i];
		timer->gpio = gpio;
		timer->index = i;
		CycleTimer_Init(&timer->event_timer,timer_event,timer);
        	timer->sigIrq = SigNode_New("%s.timer%d.irq",devname,i);
	}
        Clock_SetFreq(gpio->clk_in,100000000);

        fprintf(stderr,"Created NetX GPIO + Counter module \"%s\"\n",devname);
        return &gpio->bdev;
}


