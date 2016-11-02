/*
 *************************************************************************************************
 *
 * Emulation of the Intel LXT971a Ethernet PHY 
 *
 * State:
 *	Registers readable but not yet writable	
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "phy.h"
#include "lxt971a.h"
#include "sgstring.h"

#define LXT_CONTROL		(0)
#define LXT_STATUS		(1)
#define LXT_PHY_ID1		(2)
#define LXT_PHY_ID2		(3)
#define LXT_AN_ADVERTISE	(4)
#define LXT_AN_LP_ABILITY	(5)
#define LXT_AN_EXPANSION	(6)
#define LXT_AN_NEXT_PAGE_TXMIT	(7)
#define LXT_AN_LP_NEXT_PAGE	(8)
#define LXT_PORT_CONFIG		(16)
#define LXT_STATUS2		(17)
#define LXT_IRQ_ENABLE		(18)
#define LXT_IRQ_STATUS		(19)
#define LXT_LED_CONFIG		(20)
#define LXT_DIGITAL_CONFIG	(26)
#define LXT_TRANSMIT_CONTROL	(30)

typedef struct Lxt971a {
	PHY_Device phy;
	uint16_t control;	
	uint16_t status;
	uint16_t phy_id1;
	uint16_t phy_id2;
	uint16_t an_advertise;
	uint16_t an_lp_ability; // partner
	uint16_t an_expansion;
	uint16_t an_next_page_txmit;
	uint16_t an_lp_next_page;
	uint16_t port_config;
	uint16_t status2;
	uint16_t irq_enable;
	uint16_t irq_status;
	uint16_t led_config;
	uint16_t digital_config;
	uint16_t transmit_control;
} Lxt971a;


static int
lxt971a_write(PHY_Device *phy,uint16_t value,int reg) {
	Lxt971a *lxt = phy->owner;
	switch(reg) {
		case LXT_CONTROL:
			lxt->control = value;
			break;
		case LXT_STATUS:
			break;
		case LXT_PHY_ID1:
			break;
		case LXT_PHY_ID2:
			break;
		case LXT_AN_ADVERTISE:
			break;
		case LXT_AN_LP_ABILITY:
			break;
		case LXT_AN_EXPANSION:
			break;
		case LXT_AN_NEXT_PAGE_TXMIT:
			break;
		case LXT_AN_LP_NEXT_PAGE:
			break;
		case LXT_PORT_CONFIG:
			break;
		case LXT_STATUS2:
			break;
		case LXT_IRQ_ENABLE:
			break;
		case LXT_IRQ_STATUS:
			break;
		case LXT_LED_CONFIG:
			lxt->led_config=value;
			break;
		case LXT_DIGITAL_CONFIG:
			break;
		case LXT_TRANSMIT_CONTROL:
			break;
		default:
			fprintf(stderr,"LXT971A: Write to non existing register %d\n",reg);
			return -1;
	}
	return 0;
}
static int
lxt971a_read(PHY_Device *phy,uint16_t *value,int reg) {
	Lxt971a *lxt = phy->owner;
	switch(reg) {
		case LXT_CONTROL:
			*value = lxt->control;
			break;
		case LXT_STATUS:
			*value = lxt->status;
			break;
		case LXT_PHY_ID1:
			*value = lxt->phy_id1;
			break;
		case LXT_PHY_ID2:
			*value = lxt->phy_id2;
			break;
		case LXT_AN_ADVERTISE:
			*value = lxt->an_advertise;
			break;
		case LXT_AN_LP_ABILITY:
			*value = lxt->an_lp_ability;
			break;
		case LXT_AN_EXPANSION:
			*value = lxt->an_expansion;
			break;
		case LXT_AN_NEXT_PAGE_TXMIT:
			*value = lxt->an_next_page_txmit;
			break;
		case LXT_AN_LP_NEXT_PAGE:
			*value = lxt->an_lp_next_page;
			break;
		case LXT_PORT_CONFIG:
			*value = lxt->port_config;
			break;
		case LXT_STATUS2:
			*value = lxt->status2;
			break;
		case LXT_IRQ_ENABLE:
			*value = lxt->irq_enable;
			break;
		case LXT_IRQ_STATUS:
			*value = lxt->irq_status;
			break;
		case LXT_LED_CONFIG:
			*value = lxt->led_config;
			break;
		case LXT_DIGITAL_CONFIG:
			*value = lxt->digital_config;
			break;
		case LXT_TRANSMIT_CONTROL:
			*value = lxt->transmit_control;
			break;
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 28:
			*value = 0xffff;
			break;
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 27:
		case 29:
			*value = 0x0;
			break;
		case 31: /* Don't know where this is described */
			*value=0x3578;
			break;
		
		default:
			fprintf(stderr,"LXT971A: Read from nonexisting register %d\n",reg);
			return -1;
	}
	return 0;
}

PHY_Device *
Lxt971a_New() {
	Lxt971a *lxt = sg_new(Lxt971a);
	lxt->control = 0x3100;
	lxt->status = 0x782d; // Link up, negotiation complete
	lxt->phy_id1 = 0x0013;
	lxt->phy_id2 = 0x78e2;
	lxt->an_advertise = 0x01e1;
	lxt->an_lp_ability = 0x45e1;
	lxt->an_expansion = 0;	
	lxt->an_next_page_txmit = 0x2001;
	lxt->an_lp_next_page=0;
	lxt->port_config = 0x0084;
	lxt->status2=0;	
	lxt->irq_enable=0;	
	lxt->irq_status=0;	
	lxt->led_config=0x422;
	lxt->digital_config=0x88;
	lxt->transmit_control=0;
	lxt->phy.readreg = lxt971a_read;
	lxt->phy.writereg = lxt971a_write;
	lxt->phy.owner = lxt;
	return &lxt->phy;
}
