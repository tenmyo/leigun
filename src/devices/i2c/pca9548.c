/*
 *************************************************************************************************
 *
 * Emulation of PCA9548 8-channel I2C-Multiplexer
 *
 * State: Working 
 *
 * Copyright 2007 Jochen Karrer. All rights reserved.
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
#include "pca9548.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

struct PCA9548 {
	I2C_Slave i2c_slave;
	uint8_t ctrl_reg;
	uint8_t ctrl_reg_new;
	SigNode *sda;
	SigNode *scl;
	SigNode *sd[8];
	SigNode *sc[8];
	SigNode *reset;
};

/*
 * -----------------------------------------------------
 * Write to the one register of the pca9548
 * Store the value to become valid after stop
 * -----------------------------------------------------
 */
static int
pca9548_write(void *dev, uint8_t data)
{
	PCA9548 *pca = dev;
	pca->ctrl_reg_new = data;
	return I2C_ACK;
};

static int
pca9548_read(void *dev, uint8_t * data)
{
	PCA9548 *pca = dev;
	*data = pca->ctrl_reg;
	return I2C_DONE;
};

static int
pca9548_start(void *dev, int i2c_addr, int operation)
{
	PCA9548 *pca = dev;
	dbgprintf("pca9548 start\n");
	if (SigNode_Val(pca->reset) == SIG_LOW) {
		return I2C_NACK;
	}
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
pca9548_stop(void *dev)
{
	int i;
	PCA9548 *pca = dev;
	uint8_t diff;
	diff = pca->ctrl_reg ^ pca->ctrl_reg_new;
	for (i = 0; i < 8; i++) {
		if (!(diff & (1 << i))) {
			continue;
		}
		if (!(pca->ctrl_reg_new & (1 << i))) {
			dbgprintf("unlink %d\n", i);
			SigNode_RemoveLink(pca->sda, pca->sd[i]);
			SigNode_RemoveLink(pca->scl, pca->sc[i]);
		}
	}
	for (i = 0; i < 8; i++) {
		if (!(diff & (1 << i))) {
			continue;
		}
		if (pca->ctrl_reg_new & (1 << i)) {
			dbgprintf("link %d\n", i);
			if (SigNode_Val(pca->sd[i]) != SIG_HIGH) {
				fprintf(stderr,
					"PCA9548: Warning, linking with nonhigh SDA line\n");
			}
			if (SigNode_Val(pca->sc[i]) != SIG_HIGH) {
				fprintf(stderr,
					"PCA9548: Warning, linking with nonhigh SCL line\n");
			}
			SigNode_Link(pca->sda, pca->sd[i]);
			SigNode_Link(pca->scl, pca->sc[i]);
		}
	}
	pca->ctrl_reg = pca->ctrl_reg_new;
	dbgprintf("pca9548 stop val %02x\n", pca->ctrl_reg);
}

static I2C_SlaveOps pca9548_ops = {
	.start = pca9548_start,
	.stop = pca9548_stop,
	.read = pca9548_read,
	.write = pca9548_write
};

static void
reset_change(struct SigNode *node, int value, void *clientData)
{
	PCA9548 *pca = (PCA9548 *) clientData;
	int i;
	if (value == SIG_LOW) {
		for (i = 0; i < 8; i++) {
			if (pca->ctrl_reg & (1 << i)) {
				//fprintf(stderr,"unlink_r %d\n",i);
				SigNode_RemoveLink(pca->sda, pca->sd[i]);
				SigNode_RemoveLink(pca->scl, pca->sc[i]);
			}
		}
		pca->ctrl_reg = 0;
	}
}

I2C_Slave *
PCA9548_New(char *name)
{
	PCA9548 *pca = sg_new(PCA9548);
	I2C_Slave *i2c_slave;
	int i;
	i2c_slave = &pca->i2c_slave;
	i2c_slave->devops = &pca9548_ops;
	i2c_slave->dev = pca;
	i2c_slave->speed = I2C_SPEED_FAST;
	for (i = 0; i < 8; i++) {
		pca->sd[i] = SigNode_New("%s.sd%d", name, i);
		if (!pca->sd[i]) {
			exit(1);
		}
		pca->sc[i] = SigNode_New("%s.sc%d", name, i);
		if (!pca->sc[i]) {
			exit(1);
		}
	}
	pca->scl = SigNode_New("%s.scl", name);
	if (!pca->scl) {
		exit(1);
	}
	pca->sda = SigNode_New("%s.sda", name);
	if (!pca->sda) {
		exit(1);
	}
	pca->reset = SigNode_New("%s.reset", name);
	if (!pca->reset) {
		exit(1);
	}
	SigNode_Trace(pca->reset, reset_change, pca);
	fprintf(stderr, "PCA9548 I2C-Multiplexer \"%s\" created\n", name);
	return i2c_slave;
}
