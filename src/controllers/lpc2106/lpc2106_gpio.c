/*
 *************************************************************************************************
 *
 * Philips LPC2106 GPIO module emulation 
 *
 * State: untested 
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "signode.h"
#include "bus.h"
#include "sgstring.h"

#define REG_IOPIN 	(0xe0028000)
#define REG_IOSET	(0xe0028004)
#define REG_IODIR	(0xe0028008)
#define REG_IOCLR	(0xe002800c)

typedef struct LPC2106_Gpio {
	uint32_t gpio_outstate;
	uint32_t gpiodir;   
	SigNode *gpioNode[32];	
} LPC2106_Gpio;

void
update_gpios(LPC2106_Gpio *gpio) 
{
	int i;
	for(i=0;i<32;i++) {
		if(!(gpio->gpiodir & (1<<i))) 
		{
			SigNode_Set(gpio->gpioNode[i],SIG_OPEN);
			continue;
		}
		if(gpio->gpio_outstate & (1<<i)) {
			SigNode_Set(gpio->gpioNode[i],SIG_HIGH);
		} else {
			SigNode_Set(gpio->gpioNode[i],SIG_LOW);
		}
	}
}

static uint32_t 
iopin_read(void *clientData,uint32_t address,int rqlen) 
{
	LPC2106_Gpio *gpio = clientData;
	uint32_t pinval=0;
	int i;
	for(i=0;i<32;i++) {
		int val = SigNode_Val(gpio->gpioNode[i]); 
		if((val == SIG_HIGH) || (val == SIG_PULLUP)) {
			pinval |= (1<<i);	
		}
	}
	return pinval;
}

static void 
iopin_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	LPC2106_Gpio *gpio = clientData;
	gpio->gpio_outstate = value;
	update_gpios(gpio);
}

static uint32_t 
ioset_read(void *clientData,uint32_t address,int rqlen) 
{
	LPC2106_Gpio *gpio = clientData;
	return gpio->gpio_outstate;
}

static void 
ioset_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	LPC2106_Gpio *gpio = clientData;
	gpio->gpio_outstate |= value;
	update_gpios(gpio);
}

static uint32_t 
iodir_read(void *clientData,uint32_t address,int rqlen) 
{
	LPC2106_Gpio *gpio = clientData;
	return gpio->gpiodir;
}

static void 
iodir_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	LPC2106_Gpio *gpio = clientData;
	gpio->gpiodir = value;
	update_gpios(gpio);
}

static uint32_t 
ioclr_read(void *clientData,uint32_t address,int rqlen) 
{
	return 0;
}

static void 
ioclr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	LPC2106_Gpio *gpio = clientData;
	gpio->gpio_outstate &= ~value;
}

static void
LPCGpio_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags) 
{
	LPC2106_Gpio *gpio = owner;
	IOH_New32(REG_IOPIN,iopin_read,iopin_write,gpio);
	IOH_New32(REG_IOSET,ioset_read,ioset_write,gpio);
	IOH_New32(REG_IODIR,iodir_read,iodir_write,gpio);
	IOH_New32(REG_IOCLR,ioclr_read,ioclr_write,gpio);
}

void
LPC2106_GpioNew(const char *name) 
{
	LPC2106_Gpio *gpio;
	int i;
	gpio = sg_new(LPC2106_Gpio);
	for(i=0;i<32;i++) {
		gpio->gpioNode[i] = SigNode_New("%s.%d",name,i);	
		if(!gpio->gpioNode[i]) {
			fprintf(stderr,"Can not create gpio node %d\n",i);
			exit(4);
		}
	}	
	LPCGpio_Map(gpio,0xe0028000,0,0);
}
