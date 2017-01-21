/*
 *************************************************************************************************
 *
 * Emulation of Philips ISP1301 USB OTG transceiver controlled over I2C bus
 *
 * state:
 *	all registers present, but no functionality implemented	
 *
 * comment:
 *	All "Clear"  registers seem to return 0 on real device.
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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
#include <stddef.h>
#include "i2c.h"
#include "signode.h"
#include "isp1301.h"
#include "sgstring.h"

/* Register definitions */
#define TRANS_VENODR_ID_LOW               (0)
#define TRANS_VENDOR_ID_HIGH              (1)
#define TRANS_PRODUCT_ID_LOW              (2)
#define TRANS_PRODUCT_ID_HIGH             (3)
#define TRANS_MODE_CONTROL_1_SET          (4)
#define TRANS_MODE_CONTROL_1_CLR          (5)
#define		MC1_SPEED_REG		(1<<0)
#define		MC1_SUSPEND_REG		(1<<1)
#define		MC1_DAT_SE0		(1<<2)
#define		MC1_TRANSP_EN		(1<<3)
#define		MC1_BDIS_ACON_EN	(1<<4)
#define		MC1_OE_INT_EN		(1<<5)
#define		MC1_UART_EN		(1<<6)
#define TRANS_OTG_CONTROL_SET             (6)
#define TRANS_OTG_CONTROL_CLR             (7)
#define		OC_DP_PULLUP		(1<<0)
#define		OC_DM_PULLUP		(1<<1)
#define		OC_DP_PULLDOWN		(1<<2)
#define		OC_DM_PULLDOWN		(1<<3)
#define		OC_ID_PULLDOWN		(1<<4)
#define		OC_VBUS_DRV		(1<<5)
#define		OC_VBUS_DISCHRG		(1<<6)
#define		OC_VBUS_CHRG		(1<<7)
#define TRANS_INTERRUPT_SRC               (8)
#define		ISRC_VBUS_VLD		(1<<0)
#define		ISRC_SESS_VLD		(1<<1)
#define		ISRC_DP_HI		(1<<2)
#define		ISRC_ID_GND		(1<<3)
#define		ISRC_DM_HI		(1<<4)
#define		ISRC_ID_FLOAT		(1<<5)
#define		ISRC_BDIS_ACON		(1<<6)
#define		ISRC_CR_INT		(1<<7)
#define TRANS_INTLAT_SET                  (0xa)
#define TRANS_INTLAT_CLR                  (0xb)
#define		ILAT_VBUS_VLD		(1<<0)
#define		ILAT_SESS_VLD		(1<<1)
#define		ILAT_DP_HI		(1<<2)
#define		ILAT_ID_GND		(1<<3)
#define		ILAT_DM_HI		(1<<4)
#define		ILAT_ID_FLOAT		(1<<5)
#define		ILAT_BDIS_ACON		(1<<6)
#define		ILAT_CR_INT		(1<<7)
#define	TRANS_INTEN_LOW_SET		  (0xc)
#define	TRANS_INTEN_LOW_CLR		  (0xd)
#define TRANS_INTEN_HIGH_SET              (0xe)
#define TRANS_INTEN_HIGH_CLR              (0xf)
#define		INTEN_VBUS_VLD		(1<<0)
#define		INTEN_SESS_VLD		(1<<1)
#define		INTEN_DP_HI		(1<<2)
#define		INTEN_ID_GND		(1<<3)
#define		INTEN_DM_HI		(1<<4)
#define		INTEN_ID_FLOAT		(1<<5)
#define		INTEN_BDIS_ACON		(1<<6)
#define		INTEN_CR_INT		(1<<7)
#define TRANS_MODE_CONTROL_2_SET          (0x12)
#define TRANS_MODE_CONTROL_2_CLR          (0x13)
#define		MC2_GLOBAL_PWR_DOWN	(1<<0)
#define		MC2_SPD_SUSP_CTRL	(1<<1)
#define		MC2_BI_DI		(1<<2)
#define		MC2_TRANSP_BDIR0		(1<<3)
#define		MC2_TRANSP_BDIR1		(1<<4)
#define		MC2_AUDIO_EN		(1<<5)
#define		MC2_PSW_OE		(1<<6)
#define		MC2_EN2V7		(1<<7)

#define ISP_STATE_ADDR (1)
#define ISP_STATE_DATA (2)

struct ISP1301 {
	I2C_Slave i2c_slave;
	int state;
	uint8_t reg_addr;
	SigNode *irqNode;
	uint8_t mode_control_1;
	uint8_t otg_control;
	uint8_t interrupt_src;	/* should be read from input pins */
	uint8_t intlatch;
	uint8_t inten_low;
	uint8_t inten_high;
	uint8_t mode_control_2;
};

static void
update_interrupt(ISP1301 * isp)
{
	if (isp->intlatch) {
		SigNode_Set(isp->irqNode, SIG_LOW);
	} else {
		/* Open drain interrupt pin, to lazy to add external pullup */
		SigNode_Set(isp->irqNode, SIG_PULLUP);
	}
}

static uint8_t
vendor_id_low_read(ISP1301 * isp)
{
	return 0xcc;
}

static void
vendor_id_low_write(ISP1301 * isp, uint8_t value)
{
	// ignore
}

static uint8_t
vendor_id_high_read(ISP1301 * isp)
{
	return 0x04;
}

static void
vendor_id_high_write(ISP1301 * isp, uint8_t value)
{
	// ignore
}

static uint8_t
product_id_low_read(ISP1301 * isp)
{
	return 0x01;
}

static void
product_id_low_write(ISP1301 * isp, uint8_t value)
{
	// ignore
}

static uint8_t
product_id_high_read(ISP1301 * isp)
{
	return 0x13;
}

static void
product_id_high_write(ISP1301 * isp, uint8_t value)
{
	// ignore
}

static uint8_t
mode_control_1_set_read(ISP1301 * isp)
{
	return isp->mode_control_1;
}

static void
mode_control_1_set_write(ISP1301 * isp, uint8_t value)
{
	isp->mode_control_1 |= value;
	return;
}

static uint8_t
mode_control_1_clr_read(ISP1301 * isp)
{
	return 0;
}

static void
mode_control_1_clr_write(ISP1301 * isp, uint8_t value)
{
	isp->mode_control_1 &= ~value;
}

static uint8_t
otg_control_set_read(ISP1301 * isp)
{
	return isp->otg_control;
}

static void
otg_control_set_write(ISP1301 * isp, uint8_t value)
{
	isp->otg_control |= value;
}

static uint8_t
otg_control_clr_read(ISP1301 * isp)
{
	return 0;
}

static void
otg_control_clr_write(ISP1301 * isp, uint8_t value)
{
	isp->otg_control &= ~value;
}

static uint8_t
interrupt_src_read(ISP1301 * isp)
{
	return isp->interrupt_src;
}

static void
interrupt_src_write(ISP1301 * isp, uint8_t value)
{
	fprintf(stderr, "ISP1301 interrupt source is not writable !\n");
}

/*
 * -------------------------------------------------------------
 * Interrupt latch register
 * -------------------------------------------------------------
 */
static uint8_t
intlat_set_read(ISP1301 * isp)
{
	return isp->intlatch;
}

static void
intlat_set_write(ISP1301 * isp, uint8_t value)
{
	isp->intlatch |= value;
	update_interrupt(isp);
}

static uint8_t
intlat_clr_read(ISP1301 * isp)
{
	return 0;
}

static void
intlat_clr_write(ISP1301 * isp, uint8_t value)
{
	isp->intlatch &= ~value;
	update_interrupt(isp);
}

static uint8_t
inten_low_set_read(ISP1301 * isp)
{
	return isp->inten_low;
}

static void
inten_low_set_write(ISP1301 * isp, uint8_t value)
{
	isp->inten_low |= value;
}

static uint8_t
inten_low_clr_read(ISP1301 * isp)
{
	return 0;
}

static void
inten_low_clr_write(ISP1301 * isp, uint8_t value)
{
	isp->inten_low &= ~value;
}

static uint8_t
inten_high_set_read(ISP1301 * isp)
{
	return isp->inten_high;
}

static void
inten_high_set_write(ISP1301 * isp, uint8_t value)
{
	isp->inten_high |= value;
}

static uint8_t
inten_high_clr_read(ISP1301 * isp)
{
	return 0;
}

static void
inten_high_clr_write(ISP1301 * isp, uint8_t value)
{
	isp->inten_high &= ~value;
}

static uint8_t
mode_control_2_set_read(ISP1301 * isp)
{
	return isp->mode_control_2;
}

static void
mode_control_2_set_write(ISP1301 * isp, uint8_t value)
{
	isp->mode_control_2 |= value;
}

static uint8_t
mode_control_2_clr_read(ISP1301 * isp)
{
	return 0;
}

static void
mode_control_2_clr_write(ISP1301 * isp, uint8_t value)
{
	isp->mode_control_2 &= ~value;
}

/*
 * -------------------------------------------------------------------
 * This function is exported to outside for lazy direct access
 * -------------------------------------------------------------------
 */
void
ISP1301_Write(ISP1301 * isp, uint8_t data, uint8_t addr)
{
	switch (addr) {
	    case TRANS_VENODR_ID_LOW:
		    vendor_id_low_write(isp, data);
		    break;
	    case TRANS_VENDOR_ID_HIGH:
		    vendor_id_high_write(isp, data);
		    break;
	    case TRANS_PRODUCT_ID_LOW:
		    product_id_low_write(isp, data);
		    break;
	    case TRANS_PRODUCT_ID_HIGH:
		    product_id_high_write(isp, data);
		    break;
	    case TRANS_MODE_CONTROL_1_SET:
		    mode_control_1_set_write(isp, data);
		    break;
	    case TRANS_MODE_CONTROL_1_CLR:
		    mode_control_1_clr_write(isp, data);
		    break;
	    case TRANS_OTG_CONTROL_SET:
		    otg_control_set_write(isp, data);
		    break;
	    case TRANS_OTG_CONTROL_CLR:
		    otg_control_clr_write(isp, data);
		    break;
	    case TRANS_INTERRUPT_SRC:
		    interrupt_src_write(isp, data);
		    break;
	    case TRANS_INTLAT_SET:
		    intlat_set_write(isp, data);
		    break;
	    case TRANS_INTLAT_CLR:
		    intlat_clr_write(isp, data);
		    break;
	    case TRANS_INTEN_LOW_SET:
		    inten_low_set_write(isp, data);
		    break;
	    case TRANS_INTEN_LOW_CLR:
		    inten_low_clr_write(isp, data);
		    break;

	    case TRANS_INTEN_HIGH_SET:
		    inten_high_set_write(isp, data);
		    break;

	    case TRANS_INTEN_HIGH_CLR:
		    inten_high_clr_write(isp, data);
		    break;

	    case TRANS_MODE_CONTROL_2_SET:
		    mode_control_2_set_write(isp, data);
		    break;

	    case TRANS_MODE_CONTROL_2_CLR:
		    mode_control_2_clr_write(isp, data);
		    break;
	}
}

static int
isp1301_write(void *dev, uint8_t data)
{
	ISP1301 *isp = (ISP1301 *) dev;
	if (isp->state == ISP_STATE_ADDR) {
		isp->reg_addr = data;
	} else {
		ISP1301_Write(isp, data, isp->reg_addr);
		isp->reg_addr++;
	}
	return I2C_ACK;
}

/*
 * -------------------------------------------------------------------
 * This function is exported to outside for lazy direct access
 * -------------------------------------------------------------------
 */
uint8_t
ISP1301_Read(ISP1301 * isp, uint8_t addr)
{

	uint8_t data = 0;
	switch (addr) {
	    case TRANS_VENODR_ID_LOW:
		    data = vendor_id_low_read(isp);
		    break;
	    case TRANS_VENDOR_ID_HIGH:
		    data = vendor_id_high_read(isp);
		    break;
	    case TRANS_PRODUCT_ID_LOW:
		    data = product_id_low_read(isp);
		    break;
	    case TRANS_PRODUCT_ID_HIGH:
		    data = product_id_high_read(isp);
		    break;
	    case TRANS_MODE_CONTROL_1_SET:
		    data = mode_control_1_set_read(isp);
		    break;
	    case TRANS_MODE_CONTROL_1_CLR:
		    data = mode_control_1_clr_read(isp);
		    break;
	    case TRANS_OTG_CONTROL_SET:
		    data = otg_control_set_read(isp);
		    break;
	    case TRANS_OTG_CONTROL_CLR:
		    data = otg_control_clr_read(isp);
		    break;
	    case TRANS_INTERRUPT_SRC:
		    data = interrupt_src_read(isp);
		    break;
	    case TRANS_INTLAT_SET:
		    data = intlat_set_read(isp);
		    break;
	    case TRANS_INTLAT_CLR:
		    data = intlat_clr_read(isp);
		    break;

	    case TRANS_INTEN_LOW_SET:
		    data = inten_low_set_read(isp);
		    break;
	    case TRANS_INTEN_LOW_CLR:
		    data = inten_low_clr_read(isp);
		    break;

	    case TRANS_INTEN_HIGH_SET:
		    data = inten_high_set_read(isp);
		    break;

	    case TRANS_INTEN_HIGH_CLR:
		    data = inten_high_clr_read(isp);
		    break;

	    case TRANS_MODE_CONTROL_2_SET:
		    data = mode_control_2_set_read(isp);
		    break;
	    case TRANS_MODE_CONTROL_2_CLR:
		    data = mode_control_2_clr_read(isp);
		    break;
	}
	return data;
}

static int
isp1301_read(void *dev, uint8_t * data)
{
	ISP1301 *isp = (ISP1301 *) dev;
	*data = ISP1301_Read(isp, isp->reg_addr);
	isp->reg_addr++;
	return I2C_DONE;
}

static int
isp1301_start(void *dev, int i2c_addr, int operation)
{
	ISP1301 *isp = (ISP1301 *) dev;
	isp->state = ISP_STATE_ADDR;	/*  ? does repeated start go into address state ? */
	return I2C_ACK;
}

static void
isp1301_stop(void *dev)
{
	ISP1301 *isp = (ISP1301 *) dev;
	isp->state = ISP_STATE_ADDR;
}

static I2C_SlaveOps isp1301_ops = {
	.start = isp1301_start,
	.stop = isp1301_stop,
	.read = isp1301_read,
	.write = isp1301_write
};

I2C_Slave *
ISP1301_New(char *name)
{
	ISP1301 *isp = sg_new(ISP1301);
	I2C_Slave *i2c_slave;
	isp->state = ISP_STATE_ADDR;
	isp->irqNode = SigNode_New("%s.irq", name);
	if (!isp->irqNode) {
		fprintf(stderr, "Can not create interrupt node for ISP1301 USB OTG transceiver\n");
		exit(1);
	}
	isp->mode_control_1 = 0;
	isp->otg_control = 0xc;
	isp->interrupt_src = 0;
	isp->intlatch = 0;
	isp->inten_low = 0;
	isp->inten_high = 0;
	isp->mode_control_2 = 0x4;
	update_interrupt(isp);
	i2c_slave = &isp->i2c_slave;
	i2c_slave->devops = &isp1301_ops;
	i2c_slave->dev = isp;
	i2c_slave->speed = I2C_SPEED_FAST;
	fprintf(stderr, "Philips ISP1301 USB OTG transceiver created \"%s\" created\n", name);
	return i2c_slave;
}

ISP1301 *
ISP1301_GetPtr(I2C_Slave * slave)
{
	return (ISP1301 *) (((int8_t *) slave) - offsetof(ISP1301, i2c_slave));
}
