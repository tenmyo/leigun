/*
 *************************************************************************************************
 * Emulation of Melexis 90614 I2C IR Temperature Sensor 
 *
 * state: Working but giving constang temperatures.
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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
#include "i2c.h"
#include "mlx90614.h"
#include "sgstring.h"
#include "crc8.h"

#define MXREG_RAW_CH1		(0x04)
#define MXREG_RAW_CH2		(0x05)
#define MXREG_TA		(0x06)
#define MXREG_TOBJ1		(0x07)
#define MXREG_TOBJ2		(0x08)
#define MXREG_TOMAX		(0x20)
#define MXREG_TOMIN		(0x21)
#define MXREG_PWMCTRL		(0x22)
#define MXREG_TARANGE		(0x23)
#define MXREG_EMI		(0x24)
#define MXREG_CONF1		(0x25)
#define MXREG_SMBADDR		(0x2e)
#define MXREG_ID0		(0x3c)
#define MXREG_ID1		(0x3d)
#define MXREG_ID2		(0x3e)
#define MXREG_ID3		(0x3f)
#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define STATE_IDLE	(0)
#define STATE_ADDR	(1)
#define STATE_DATA_H	(2)
#define STATE_DATA_L	(3)
#define STATE_CRC	(4)

typedef struct Mlx Mlx;

struct Mlx {
	I2C_Slave i2c_slave;
	uint16_t atomic16;
	uint8_t state;
	uint8_t addr;
	uint8_t crc8;
	uint16_t reg_rawCh1;
	uint16_t reg_rawCh2;
	uint16_t reg_TA;
	uint16_t reg_TObj1;
	uint16_t reg_TObj2;
	uint16_t reg_TOMax;
	uint16_t reg_TOMin;
	uint16_t reg_PwmCtrl;
	uint16_t reg_TARange;
	uint16_t reg_Emi;
	uint16_t reg_Conf1;
	uint16_t reg_SmbAddr;
	uint16_t reg_ID0;
	uint16_t reg_ID1;
	uint16_t reg_ID2;
	uint16_t reg_ID3;
};

static void
Mlx_WriteReg(Mlx * mlx, uint8_t reg, uint16_t value)
{
	switch (reg) {
	    case MXREG_RAW_CH1:
		    //mlx->reg_rawCh1;
	    case MXREG_RAW_CH2:
		    //mlx->reg_rawCh2;
	    case MXREG_TA:
		    //mlx->reg_TA;
	    case MXREG_TOBJ1:
		    //mlx->reg_TObj1;
	    case MXREG_TOBJ2:
		    //mlx->reg_TObj2;
	    case MXREG_TOMAX:
		    //mlx->reg_TOMax;       
	    case MXREG_TOMIN:
		    //mlx->reg_TOMin;
	    case MXREG_PWMCTRL:
		    //mlx->reg_PwmCtrl;
	    case MXREG_TARANGE:
		    //mlx->reg_TARange;
		    break;

	    case MXREG_EMI:
		    if (value == 0) {
			    mlx->reg_Emi = 0;
		    } else if (mlx->reg_Emi == 0) {
			    mlx->reg_Emi = value;
		    } else {
			    fprintf(stderr, "Melexis: Emi must be cleared before writing\n");
		    }
		    break;

	    case MXREG_CONF1:
		    if (value == 0) {
			    mlx->reg_Conf1 = 0;
		    } else if (mlx->reg_Conf1 == 0) {
			    mlx->reg_Conf1 = value;
		    } else {
			    fprintf(stderr, "Melexis: RegConf1 must be cleared before writing\n");
		    }
		    break;

	    case MXREG_SMBADDR:
		    //mlx->reg_SmbAddr;
	    case MXREG_ID0:
		    //mlx->reg_ID0;
	    case MXREG_ID1:
		    //mlx->reg_ID1;
	    case MXREG_ID2:
		    //mlx->reg_ID2;
	    case MXREG_ID3:
		    //mlx->reg_ID3;
		    break;
	}
}

/*
 *****************************************************************
 * Melexis Write state machine 
 *****************************************************************
 */
static int
mlx_write(void *dev, uint8_t data)
{
	Mlx *mlx = dev;
	if (mlx->state == STATE_ADDR) {
		if (data > 0x3f) {
			return I2C_NACK;
		}
		mlx->addr = data;
		mlx->state = STATE_DATA_L;
		mlx->crc8 = Crc8_Poly7(mlx->crc8, &data, 1);
	} else if (mlx->state == STATE_DATA_L) {
		mlx->atomic16 = data;
		mlx->state = STATE_DATA_H;
		mlx->crc8 = Crc8_Poly7(mlx->crc8, &data, 1);
	} else if (mlx->state == STATE_DATA_H) {
		mlx->atomic16 |= ((uint16_t) data) << 8;
		mlx->state = STATE_CRC;
		mlx->crc8 = Crc8_Poly7(mlx->crc8, &data, 1);
	} else if (mlx->state == STATE_CRC) {
		mlx->crc8 = Crc8_Poly7(mlx->crc8, &data, 1);
		if (mlx->crc8 == 0) {
			Mlx_WriteReg(mlx, mlx->addr, mlx->atomic16);
		} else {
			fprintf(stderr, "Melexis: Got bad CRC\n");
		}
	}
	return I2C_ACK;
};

static uint16_t
Mlx_ReadReg(Mlx * mlx, uint8_t reg)
{
	switch (reg) {
	    case MXREG_RAW_CH1:
		    return mlx->reg_rawCh1;
	    case MXREG_RAW_CH2:
		    return mlx->reg_rawCh2;
	    case MXREG_TA:
		    return mlx->reg_TA;
	    case MXREG_TOBJ1:
		    return mlx->reg_TObj1;
	    case MXREG_TOBJ2:
		    return mlx->reg_TObj2;
	    case MXREG_TOMAX:
		    return mlx->reg_TOMax;
	    case MXREG_TOMIN:
		    return mlx->reg_TOMin;
	    case MXREG_PWMCTRL:
		    return mlx->reg_PwmCtrl;
	    case MXREG_TARANGE:
		    return mlx->reg_TARange;
	    case MXREG_EMI:
		    return mlx->reg_Emi;
	    case MXREG_CONF1:
		    return mlx->reg_Conf1;
	    case MXREG_SMBADDR:
		    return mlx->reg_SmbAddr;
	    case MXREG_ID0:
		    return mlx->reg_ID0;
	    case MXREG_ID1:
		    return mlx->reg_ID1;
	    case MXREG_ID2:
		    return mlx->reg_ID2;
	    case MXREG_ID3:
		    return mlx->reg_ID3;
	}
	return 0xff;
}

static int
mlx_read(void *dev, uint8_t * data)
{
	Mlx *mlx = dev;
	if (mlx->state == STATE_DATA_L) {
		mlx->atomic16 = Mlx_ReadReg(mlx, mlx->addr);
		*data = mlx->atomic16 & 0xff;
		mlx->crc8 = Crc8_Poly7(mlx->crc8, data, 1);
		mlx->state = STATE_DATA_H;
	} else if (mlx->state == STATE_DATA_H) {
		*data = mlx->atomic16 >> 8;
		mlx->crc8 = Crc8_Poly7(mlx->crc8, data, 1);
		mlx->state = STATE_CRC;
	} else if (mlx->state == STATE_CRC) {
		*data = mlx->crc8;
	}
	return I2C_DONE;
};

static int
mlx_start(void *dev, int i2c_addr, int operation)
{
	Mlx *mlx = dev;
	dbgprintf("Melxis start\n");
	uint8_t data = i2c_addr << 1;
	//fprintf(stderr,"I2CA: %02x\n",i2c_addr);
	if (operation == I2C_WRITE) {
		mlx->state = STATE_ADDR;
		mlx->crc8 = Crc8_Poly7(0, &data, 1);
	} else {
		data |= 1;
		mlx->crc8 = Crc8_Poly7(mlx->crc8, &data, 1);
		if (mlx->state != STATE_DATA_L) {
			return I2C_NACK;
		}
	}
	return I2C_ACK;
}

static void
mlx_stop(void *dev)
{
	Mlx *mlx = dev;
	mlx->state = STATE_IDLE;
	dbgprintf("MLX stop\n");
}

static I2C_SlaveOps mlx_ops = {
	.start = mlx_start,
	.stop = mlx_stop,
	.read = mlx_read,
	.write = mlx_write
};

I2C_Slave *
MLX90614_New(char *name)
{
	Mlx *mlx = sg_new(Mlx);
	I2C_Slave *i2c_slave;
	i2c_slave = &mlx->i2c_slave;
	i2c_slave->devops = &mlx_ops;
	i2c_slave->dev = mlx;
	i2c_slave->speed = I2C_SPEED_FAST;
	mlx->reg_rawCh1 = 0x9280;
	mlx->reg_rawCh2 = 0x8039;
	mlx->reg_TA = 0x3a8a;
	mlx->reg_TObj1 = 0x3a45;
	mlx->reg_TObj2 = 0x3a06;
	mlx->reg_TOMax = 0x9993;
	mlx->reg_TOMin = 0x7c62;
	mlx->reg_PwmCtrl = 0x0201;
	mlx->reg_TARange = 0xf71c;
	mlx->reg_Emi = 0xf332;
	mlx->reg_Conf1 = 0xb7f0;
	mlx->reg_SmbAddr = 0x5abc;
	mlx->reg_ID0 = 0x4809;
	mlx->reg_ID1 = 0xe196;
	mlx->reg_ID2 = 0x24a7;
	mlx->reg_ID3 = 0xc88a;
	fprintf(stderr, "Melexis 90614 Temperature Sensor \"%s\" created\n", name);
	return i2c_slave;
}
