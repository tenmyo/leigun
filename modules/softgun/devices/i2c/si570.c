/*
 *************************************************************************************************
 *
 * Emulation of Silabs SI570 clock chip 
 *
 * Copyright 2013 Jochen Karrer. All rights reserved.
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
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "i2c.h"
#include "si570.h"
#include "sgstring.h"
#include "configfile.h"
#include "clock.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define SI_STATE_ADDR  	(0)
#define SI_STATE_DATA  	(1)

/* The Freeze/Reset/Mem Register */
#define	RST_REG		(1 << 7)
#define NEW_FREQ	(1 << 6)
#define	FREEZE_M	(1 << 5)
#define FREEZE_VCADC	(1 << 4)
#define RECALL		(1 << 0)

/* The Freeze DCO register */
#define FREEZE_DCO	(1 << 4)

typedef struct Si570 {
	I2C_Slave i2c_slave;
	const char *name;
	const char *strPartNumber;
	bool is7ppm;
	uint8_t byteAddr;
	int state;
	bool updateScheduled; 
	uint8_t regN1;
	uint8_t regHS_DIV;
	uint64_t regRFREQ;
	uint8_t regRESFREEZEMEM;
	uint8_t regFREEZEDCO;

	uint32_t maxFreq;
	uint32_t minFreq;

	Clock_t *clkXtal;
	Clock_t *clkDco;
	Clock_t *clkOut;
} Si570;

static void
update_clocks(Si570 *si)
{
	bool checkDCO = true;
	uint32_t hsdiv;
	uint32_t n1;
	uint64_t freqOut;
	uint64_t freqDCO;
	n1 = si->regN1 + 1;
	switch(si->regHS_DIV) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 5:
		case 7:
			hsdiv = si->regHS_DIV + 4;
			break;
		default:
			fprintf(stderr,"%s: Illegal hsdiv\n",si->name);
			hsdiv = 0xffffffff;
			break;
	}
	if((si->regRESFREEZEMEM & FREEZE_M) == 0) {
		Clock_MakeDerived(si->clkDco,si->clkXtal,si->regRFREQ / 16,UINT32_C(1) << 24);
	} else {
		checkDCO = false;
	}
	if((si->regFREEZEDCO & FREEZE_DCO) == 0) {
		Clock_MakeDerived(si->clkOut,si->clkDco,1,hsdiv * n1);
	} else {
		Clock_MakeDerived(si->clkOut,si->clkDco,0,1);
		checkDCO = false;
	}
	freqOut = Clock_Freq(si->clkOut);
	freqDCO = Clock_Freq(si->clkDco);
	if(checkDCO) {
		if((freqDCO < UINT64_C(4850000000)) || (freqDCO > UINT64_C(5670000000))) {
	//		fprintf(stderr,"%s Warning: DCO freq out of range (%" PRIu64 " Hz)\n",si->name,freqDCO);
		} else {
	//		fprintf(stderr,"%s DCO Freq GOOD (%" PRIu64 " Hz)\n",si->name,freqDCO);
		}
	}
#if 0
	fprintf(stderr,"RFREQ %" PRIx64 "\n",si->regRFREQ);
	fprintf(stderr,"%s DCO freq %" PRIu64 " Hz\n",si->name,freqOut);
	fprintf(stderr,"xtal %" PRIu64 " Hz\n",Clock_Freq(si->clkXtal));
	fprintf(stderr,"dco %" PRIu64 " Hz\n",Clock_Freq(si->clkDco));
	fprintf(stderr,"out %" PRIu64 " Hz\n",Clock_Freq(si->clkOut));
	fprintf(stderr,"n1 %d, hsdiv %d\n",n1,hsdiv);
	sleep(1);
//	exit(1);
#endif
}


/**
 *************************************************************************
 * the list of possible hsdiv dividers (not the register values) .
 *************************************************************************
 */
static const uint8_t hsdiv_tab[] = {
        4, 5, 6, 7, 9, 11
};

static bool
set_frequency(Si570 *si,uint32_t freq)
{
        int i;
        uint32_t n1,hs_div;
        uint32_t best_n1,best_hs_div;
        uint64_t rfreq;
        uint32_t div;
        uint32_t fxtal = Clock_Freq(si->clkXtal);
        uint64_t maxdco = UINT64_C(5670000000);
        uint64_t mindco = UINT64_C(4850000000);
        uint64_t optdco = (maxdco + mindco) >> 1;
        uint32_t optDividers = optdco / freq;
        float dev_factor,best_factor;

        if(freq > si->maxFreq || freq < si->minFreq) {
                return false;
        }
        //Con_Printf("Optimal dividers %lu\n",(uint32_t)optDividers);
        best_factor = 1000;
        best_n1 = best_hs_div =  0;
        for(i = 0; i < array_size(hsdiv_tab); i++) {
                hs_div = hsdiv_tab[i];
                n1 = optDividers / hs_div;
                if((n1 < 1) || (n1 > 129)) {
                        continue;
                }
                dev_factor = 1.0 * optDividers / (n1 * hs_div);
                if(dev_factor < 1) {
                        dev_factor = 1 / dev_factor;
                }
                if(dev_factor < best_factor) {
                        best_factor = dev_factor;
                        best_n1 = n1;
                        best_hs_div = hs_div;
                        //Con_Printf("opt %f, bestn1 %d, best_hs_div %d\n",best_factor,best_n1,hs_div);
                }
        }
        n1 = best_n1;
        hs_div = best_hs_div;
        div = fxtal * hs_div * n1;
        rfreq = ((1 << 28) * (uint64_t)freq + (fxtal >> 1)) / fxtal * hs_div * n1;

	si->regN1 = n1 - 1;
	si->regHS_DIV = hs_div - 4;
	si->regRFREQ = rfreq;
	update_clocks(si);
	return true;
}

static void
read_nvm(Si570 *si) 
{
	set_frequency(si,644531250);
}

static uint8_t 
reg_read(Si570 *si,uint8_t regAddr) 
{
	uint8_t value;
	if(si->is7ppm) {
		if((regAddr >= 7) && (regAddr <= 12)) {
			fprintf(stderr,"%s: Error: Access to non 7PPM registers\n",si->name);
		}
	} else {
		if((regAddr >= 13) && (regAddr <= 18)) {
			fprintf(stderr,"%s: Error: Access to 7PPM registers\n",si->name);
		}
	}
	switch(regAddr) {
		case 7:
		case 13:
			value = ((si->regN1 >> 2) & 0x1f) |
				((si->regHS_DIV & 7) << 5);
			break;
		case 8:
		case 14:
			value = ((si->regN1 & 3) << 6) 
				| ((si->regRFREQ >> 32) & 0x3f);
			break;
		case 9:
		case 15:
			value = (si->regRFREQ >> 24) & 0xff;
			break;
		case 10:
		case 16:
			value = (si->regRFREQ >> 16) & 0xff;
			break;
		case 11:
		case 17:
			value = (si->regRFREQ >> 8) & 0xff;
			break;
		case 12:
		case 18:
			value = (si->regRFREQ >> 0) & 0xff;
			break;
		case 135:
			value = si->regRESFREEZEMEM;
			break;	
		case 137:
			value = si->regFREEZEDCO;
			break;	
		default:
			value = 0;
			fprintf(stderr,"%s warning: read from nonexistent Reg. 0x%02x\n",
				si->name,regAddr);
			break;
	}
	return value;
}

static void
reg_write(Si570 *si,uint8_t regAddr,uint8_t value) 
{
	if(si->is7ppm) {
		if((regAddr >= 7) && (regAddr <= 12)) {
			fprintf(stderr,"%s: Error: Access to non 7PPM registers\n",si->name);
		}
	} else {
		if((regAddr >= 13) && (regAddr <= 18)) {
			fprintf(stderr,"%s: Error: Access to 7PPM registers\n",si->name);
		}
	}
	switch(regAddr) {
		case 7:
		case 13:
			si->regN1 = (si->regN1 & 3)  | ((value & 0x1f) << 2);
			si->regHS_DIV = (value >> 5) & 7;
			si->updateScheduled = true;
			break;
		case 8:
		case 14:
			si->regN1 = (si->regN1 & 0xfc) | ((value >> 6) & 3);
			si->regRFREQ = (si->regRFREQ & ~UINT32_C(0)) |
					(((uint64_t)(value & 0x3f)) << 32);
			si->updateScheduled = true;
			break;
		case 9:
		case 15:
			si->regRFREQ = (si->regRFREQ & UINT64_C(0x3F00FFFFFF)) |
					(((uint64_t)value) << 24);
			si->updateScheduled = true;
			break;
		case 10:
		case 16:
			si->regRFREQ = (si->regRFREQ & UINT64_C(0x3FFF00FFFF)) |
					(((uint64_t)value) << 16);
			si->updateScheduled = true;
			break;
		case 11:
		case 17:
			si->regRFREQ = (si->regRFREQ & UINT64_C(0x3FFFFF00FF)) |
					(((uint64_t)value) << 8);
			si->updateScheduled = true;
			break;
		case 12:
		case 18:
			si->regRFREQ = (si->regRFREQ & UINT64_C(0x3FFFFFFF00)) |
					(((uint64_t)value) << 0);
			si->updateScheduled = true;
			break;
		case 135:
			si->regRESFREEZEMEM = value & 0xb0;
			if(value & RECALL) {
				read_nvm(si);	
			}
			si->updateScheduled = true;
			//fprintf(stderr,"Reset/Freeze/Memctrl register not implemented\n");
			break;	
		case 137:
			si->regFREEZEDCO = value & (1 << 4);
			si->updateScheduled = true;
			//fprintf(stderr,"Freeze DCO register not implemented\n");
			break;	
		default:
			fprintf(stderr,"%s warning: write to nonexistent Reg %u\n",si->name,regAddr);
			break;
	}
}
/*
 * ------------------------------------
 * Si570 Write state machine 
 * ------------------------------------
 */
static int
si570_write(void *dev, uint8_t data)
{
	Si570 *si = dev;
	if (si->state == SI_STATE_ADDR) {
		dbgprintf("Si570 Addr 0x%02x\n", data);
		si->byteAddr = data; 
		si->state = SI_STATE_DATA;
	} else if (si->state == SI_STATE_DATA) {
		dbgprintf("Si570 Write 0x%02x to %04x\n", data, si->byteAddr);
		reg_write(si,si->byteAddr,data);
		si->byteAddr++;
	}
	return I2C_ACK;
};

static int
si570_read(void *dev, uint8_t * data)
{
	Si570 *si = dev;
	*data = reg_read(si,si->byteAddr);
	dbgprintf("Si570 read 0x%02x from %04x\n", *data, si->byteAddr);
	si->byteAddr++;
	return I2C_DONE;
};

static int
si570_start(void *dev, int i2c_addr, int operation)
{
	Si570 *si = dev;
	dbgprintf("Si570 start\n");
	if (operation == I2C_WRITE) {
		si->state = SI_STATE_ADDR;
	}
	return I2C_ACK;
}

static void
si570_stop(void *dev)
{
	Si570 *si = dev;
	if(si->updateScheduled) {
		update_clocks(si);
	}
	dbgprintf("Si570 stop\n");
}

static I2C_SlaveOps si570_ops = {
	.start = si570_start,
	.stop = si570_stop,
	.read = si570_read,
	.write = si570_write
};

I2C_Slave *
Si570_New(char *name)
{
	Si570 *si = sg_new(Si570);
	char *strPartNumber;
	I2C_Slave *i2c_slave;
	i2c_slave = &si->i2c_slave;
	i2c_slave->devops = &si570_ops;
	i2c_slave->dev = si;
	i2c_slave->speed = I2C_SPEED_FAST;
	strPartNumber = Config_ReadVar(name,"part_number");
	if(!strPartNumber || (strlen(strPartNumber) < 14)) {
		fprintf(stderr, "Missing/Short part number for Si570 X0 \"%s\"\n", name);
		exit(1);
	}
	si->clkXtal = Clock_New("%s.xtal",name);
	si->clkDco = Clock_New("%s.dco",name);
	si->clkOut = Clock_New("%s.out",name);
	si->maxFreq = 810 * 1000000;
        si->minFreq = 10 * 1000000;
	if(!si->clkXtal || !si->clkDco || !si->clkOut) {
		fprintf(stderr,"Can not create clocks for \"%s\"\n",name);
		exit(1);
	}	      
	//Clock_SetFreq(si->clkXtal,114285000);
	Clock_SetFreq(si->clkXtal, 114242378); /* The fXTAL is 2000ppm ! */
	si->strPartNumber = sg_strdup(strPartNumber);
	si->name = sg_strdup(name);
	switch(strPartNumber[4]) {
		case 'A':
		case 'B':
			si->is7ppm = false;
			break;
		case 'C':
			si->is7ppm = true;
			break;
		default:
			fprintf(stderr,"Unknown 2nd Option code for Si570 X0 \"%s\"\n",name);
			exit(1);
	}
	set_frequency(si,644531250);
	fprintf(stderr, "Si570 clock generator \"%s\" created\n", name);
	return i2c_slave;
}
