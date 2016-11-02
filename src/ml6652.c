/*
 *************************************************************************************************
 *
 * Emulation of the Microlinear ML6652 Mediaconverter MII Interface
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
#include "ml6652.h"
#include "sgstring.h"

#define LXT_CONTROL		(0)
#define LXT_STATUS		(1)
#define ML6652_PHY_ID1		(2)
#define ML6652_PHY_ID2		(3)
#define ML6652_CTRL31		(31)
#define ML6652_CTRL30		(30)
#define ML6652_CTRL29		(29)
#define ML6652_CTRL28		(28)
#define ML6652_CTRL27		(27)
#define ML6652_CTRL26		(26)
#define ML6652_CTRL25		(25)

typedef struct ML6652 {
	PHY_Device phy;
	uint16_t phy_id1;
	uint16_t phy_id2;
	uint16_t ctrl25;
	uint16_t ctrl26;
	uint16_t ctrl27;
	uint16_t ctrl28;
	uint16_t ctrl29;
	uint16_t ctrl30;
	uint16_t ctrl31;
} ML6652;


static int
ml6652_write(PHY_Device *phy,uint16_t value,int reg) {
	//ML6652 *ml = phy->owner;
	switch(reg) {
		default:
			fprintf(stderr,"ML66562: Write ignored %d\n",reg);
			return -1;
	}
	return 0;
}
static int
ml6652_read(PHY_Device *phy,uint16_t *value,int reg) {
	ML6652 *ml = phy->owner;
	switch(reg) {
		case ML6652_PHY_ID1:
			*value = ml->phy_id1;
			break;
		case ML6652_PHY_ID2:
			*value = ml->phy_id2;
			break;
		case ML6652_CTRL25:
			*value = 0;
			break;
		case ML6652_CTRL26:
			*value = 0x4000;
			break;
		case ML6652_CTRL27:
			*value = 0xd810;
			break;
		case ML6652_CTRL28:
			*value = 0x0002;
			break;
		case ML6652_CTRL29:
			*value = 0x007f;
			break;
		case ML6652_CTRL30:
			*value = 0x0082;
			break;
		case ML6652_CTRL31:
			*value = 0x0000;
			break;
		default:
			*value = 0xffff;
			//fprintf(stderr,"ML6652: Read from nonexisting register %d\n",reg);
			return -1;
	}
	return 0;
}

PHY_Device *
ML6652_New() {
	ML6652 *ml = sg_new(ML6652);
	ml->phy_id1 = 0x0004;
	ml->phy_id2 = 0x3820;
	ml->phy.readreg = ml6652_read;
	ml->phy.writereg = ml6652_write;
	ml->phy.owner = ml;
	return &ml->phy;
}
