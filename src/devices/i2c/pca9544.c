/*
 *************************************************************************************************
 *
 * Emulation of PCA9544 4-channel I2C-Multiplexer
 *
 * State: Working, Interrupt-Handling is missing 
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "i2c.h"
#include "signode.h"
#include "sgstring.h"
#include "pca9544.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif


struct PCA9544 {
        I2C_Slave i2c_slave;
	uint8_t ctrl_reg;
	uint8_t new_ctrl_reg;
	SigNode *sda;
	SigNode *scl;
        SigNode *sd[4];
        SigNode *sc[4];
};

/*
 * -----------------------------------------------------
 * Write to the one register of the pca9544
 * Store the value to become valid after stop
 * -----------------------------------------------------
 */
static int
pca9544_write(void *dev,uint8_t data) {
        PCA9544 *pca = dev;
	pca->new_ctrl_reg = data;
        return I2C_ACK;
};

static int 
pca9544_read(void *dev,uint8_t *data)
{
        PCA9544 *pca = dev;
	*data = pca->ctrl_reg;
        return I2C_DONE;
};

static int
pca9544_start(void *dev,int i2c_addr,int operation) {
        dbgprintf("pca9544 start\n");
        return I2C_ACK;
}

/*
 * -----------------------------------------------
 * Switching is done after the stop condition
 * remove links of incoming signals to old out port 
 * and set a new one
 * -----------------------------------------------
 */
static void
pca9544_stop(void *dev) {
        PCA9544 *pca = dev;
	switch(pca->ctrl_reg & 0x7) {
		case 0:
		case 1:
		case 2:
		case 3:
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			SigNode_RemoveLink(pca->sda,pca->sd[pca->ctrl_reg & 3]);
			SigNode_RemoveLink(pca->scl,pca->sc[pca->ctrl_reg & 3]);
			break;
	}	
	switch(pca->new_ctrl_reg & 0x7) {
		case 0:
		case 1:
		case 2:
		case 3:
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			SigNode_Link(pca->sda,pca->sd[pca->new_ctrl_reg & 3]);
			SigNode_Link(pca->scl,pca->sc[pca->new_ctrl_reg & 3]);
			break;
	}
	pca->ctrl_reg = pca->new_ctrl_reg & 0xf;
        dbgprintf("pca9544 stop\n");
}

static I2C_SlaveOps pca9544_ops = {
        .start = pca9544_start,
        .stop =  pca9544_stop,
        .read =  pca9544_read,
        .write = pca9544_write
};

I2C_Slave *
PCA9544_New(char *name) {
        PCA9544 *pca = sg_new(PCA9544);
        I2C_Slave *i2c_slave;
        int i;
        i2c_slave = &pca->i2c_slave;
        i2c_slave->devops = &pca9544_ops;
        i2c_slave->dev = pca;
	i2c_slave->speed = I2C_SPEED_FAST;
        for(i=0;i<4;i++) {
                pca->sd[i]=SigNode_New("%s.sd%d",name,i);
                if(!pca->sd[i]) {
			exit(1);
                }
                pca->sc[i]=SigNode_New("%s.sc%d",name,i);
                if(!pca->sc[i]) {
			exit(1);
                }
        }
        pca->scl=SigNode_New("%s.scl",name);
        if(!pca->scl) {
		exit(1);
        }
        pca->sda=SigNode_New("%s.sda",name);
        if(!pca->sda) {
		exit(1);
        }
        fprintf(stderr,"PCA9544 I2C-Multiplexer \"%s\" created\n",name);
        return i2c_slave;
}

