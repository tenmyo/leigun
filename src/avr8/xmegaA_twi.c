/*
 *************************************************************************************************
 *
 * Emulation of the AVR ATXmega A TWI controller
 *
 * (C) 2010 Jochen Karrer
 *
 *  State: Not implemented 
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sgstring.h"
#include "avr8_cpu.h"
#include "serial.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "xmegaA_twi.h"
#include "fio.h"

typedef struct XmegaA_Twi {
	uint8_t regCtrl;
	uint8_t regMCtrlA;
	uint8_t regMCtrlB;
	uint8_t regMCtrlC;
	uint8_t regMStatus;
	uint8_t regMBaud;
	uint8_t regMAddr;
	uint8_t regMData;
	uint8_t regSCtrlA;
	uint8_t regSCtrlB;
	uint8_t regSStatus;
	uint8_t regSAddr;
	uint8_t regSData;
	uint8_t regSAddrMask;
} XmegaA_Twi;

#define TWI_CTRL(base)		((base) + 0x00)
#define		CTRL_SDAHOLD	(1 << 1)
#define		CTRL_EDIEN	(1 << 0)
#define TWIM_CTRLA(base)	((base) + 0x01)
#define		MCTRLA_INTLVL_SHIFT	(6)
#define		MCTRLA_INTLVL_MASK	(3 << 6)
#define		MCTRLA_RIEN		(1 << 5)
#define		MCTRLA_WIEN		(1 << 4)
#define		MCTRLA_ENABLE		(1 << 3)
#define TWIM_CTRLB(base)	((base) + 0x02)
#define		MCTRLB_TIMEOUT_SHIFT	(2)
#define		MCTRLB_TIMEOUT_MASK	(3 << 2)
#define		MCTRLB_QCEN		(1 << 1)
#define		MCTRLB_SMEN		(1 << 0)
#define TWIM_CTRLC(base)	((base) + 0x03)
#define		MCTRLC_ACKACT		(1 << 2)
#define		MCTRLC_CMD_MASK		(3)
#define TWIM_STATUS(base)	((base) + 0x04)
#define		MSTATUS_RIF		(1 << 7)
#define		MSTATUS_WIF		(1 << 6)
#define 	MSTATUS_CLKHOLD		(1 << 5)
#define		MSTATUS_RXACK		(1 << 4)
#define		MSTATUS_ARBLOST		(1 << 3)
#define		MSTATUS_BUSERR		(1 << 2)
#define		MSTATUS_BUSSTATE_MASK	(3)
#define TWIM_BAUD(base)		((base) + 0x05)
#define TWIM_ADDR(base)		((base) + 0x06)
#define TWIM_DATA(base)		((base) + 0x07)

#define TWIS_CTRLA(base)	((base) + 0x08)
#define		SCTRLA_INTLVL_SHIFT	(6)
#define		SCTRLA_INTLVL_MASK	(3 << 6)
#define		SCTRLA_DIEN		(1 << 5)
#define		SCTRLA_APIEN		(1 << 4)
#define		SCTRLA_ENABLE		(1 << 3)
#define		SCTRLA_PIEN		(1 << 2)
#define		SCTRLA_TPMEN		(1 << 1)
#define		SCTRLA_SMEN		(1 << 0)

#define TWIS_CTRLB(base)	((base) + 0x09)
#define		SCTRLB_ACKACT		(1 << 2)
#define		SCTRLB_CMD_MASK		(3)

#define TWIS_STATUS(base)	((base) + 0x0a)
#define		SSTATUS_DIF	(1 << 7)
#define		SSTATUS_APIF	(1 << 6)
#define		SSTATUS_CLKHOLD	(1 << 5)
#define		SSTATUS_RXACK	(1 << 4)
#define		SSTATUS_COLL	(1 << 3)
#define		SSTATUS_BUSERR	(1 << 2)
#define 	SSTATUS_DIR	(1 << 1)
#define 	SSTATUS_AP	(1 << 0)
#define TWIS_ADDR(base)		((base) + 0x0b)
#define TWIS_DATA(base)		((base) + 0x0c)
#define TWIS_ADDRMASK(base)	((base) + 0x0d)
#define		SADDRMASK_ADDREN	(1 << 0)

/*
 ***********************************************************
 * Common Control Register
 * Bit 1: SDAHOLD Enables an internal hold time on SDA on 
 *        negative clock edge.
 * Bit 0: EDIEN Enable external driver four wire interface
 ***********************************************************
 */
static uint8_t
ctrl_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
ctrl_write(void *clientData,uint8_t value,uint32_t address)
{
}

/*
 ***********************************************************
 * Master Control Register 
 * Bits 6 and 7: Interrupt Level
 * Bit  5: RIEN	Read Interrupt enable
 * Bit  4: WIEN	Write Interrupt enable
 * Bit  3: ENABLE Enable TWI Master
 ***********************************************************
 */
static uint8_t
mctrla_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
mctrla_write(void *clientData,uint8_t value,uint32_t address)
{
}

static uint8_t
mctrlb_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
mctrlb_write(void *clientData,uint8_t value,uint32_t address)
{
}

static uint8_t
mctrlc_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
mctrlc_write(void *clientData,uint8_t value,uint32_t address)
{
}

static uint8_t
mstatus_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
mstatus_write(void *clientData,uint8_t value,uint32_t address)
{
}

static uint8_t
mbaud_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
mbaud_write(void *clientData,uint8_t value,uint32_t address)
{
}

static uint8_t
maddr_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
maddr_write(void *clientData,uint8_t value,uint32_t address)
{
}

static uint8_t
mdata_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
mdata_write(void *clientData,uint8_t value,uint32_t address)
{
}

/*
 ***********************************************************
 * Slave control register A
 * Bit 6+7: INTLVL 
 * Bit 5: DIEN	Data Interrupt Enable
 * Bit 4: APIEN Address/Stop Interrupt Enable
 * Bit 3: ENABLE Enable the TWI Slave
 * Bit 2: PIEN Stop Interrupt enable
 * Bit 1: PMEN Promiscuous Mode Enable (accept all addresses)
 * Bit 0: SMEN Smart Mode enable, Send Ack immediately
 ***********************************************************
 */
static uint8_t
sctrla_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
sctrla_write(void *clientData,uint8_t value,uint32_t address)
{
}

/*
 ***********************************************************
 * Slave Control register B 
 * Bit 2: ACKACT Acknowlegde action
 * Bit 0+1: CMD
 ***********************************************************
 */ 
static uint8_t
sctrlb_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
sctrlb_write(void *clientData,uint8_t value,uint32_t address)
{
}

/*
 *************************************************************
 * Slave Status Register
 * Bit 7: DIF Data Interrupt flag
 * Bit 6: APIF Address/Stop Interrupt Flag
 * Bit 5: CLKHOLD Clock Hold
 * Bit 4: RXACK Received Acknowledge
 * Bit 3: COLL Collision is detected.
 * Bit 2: Buserr TWI Slave Bus error
 * Bit 1: Read Write Direction
 * Bit 0: Slave Address or STOP
 *************************************************************
 */
static uint8_t
sstatus_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
sstatus_write(void *clientData,uint8_t value,uint32_t address)
{
}

static uint8_t
saddr_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
saddr_write(void *clientData,uint8_t value,uint32_t address)
{
}

static uint8_t
sdata_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
sdata_write(void *clientData,uint8_t value,uint32_t address)
{
}

static uint8_t
saddrmask_read(void *clientData,uint32_t address)
{
	return 0;
}

static void
saddrmask_write(void *clientData,uint8_t value,uint32_t address)
{
}


void
XMegaA_TWINew(const char *name,uint32_t base)
{

	XmegaA_Twi *twi = sg_new(XmegaA_Twi);
	AVR8_RegisterIOHandler(TWI_CTRL(base),ctrl_read,ctrl_write,twi);
	AVR8_RegisterIOHandler(TWIM_CTRLA(base),mctrla_read,mctrla_write,twi);
	AVR8_RegisterIOHandler(TWIM_CTRLB(base),mctrlb_read,mctrlb_write,twi);
	AVR8_RegisterIOHandler(TWIM_CTRLC(base),mctrlc_read,mctrlc_write,twi);
	AVR8_RegisterIOHandler(TWIM_STATUS(base),mstatus_read,mstatus_write,twi);
	AVR8_RegisterIOHandler(TWIM_BAUD(base),mbaud_read,mbaud_write,twi);
	AVR8_RegisterIOHandler(TWIM_ADDR(base),maddr_read,maddr_write,twi);
	AVR8_RegisterIOHandler(TWIM_DATA(base),mdata_read,mdata_write,twi);
	AVR8_RegisterIOHandler(TWIS_CTRLA(base),sctrla_read,sctrla_write,twi);
	AVR8_RegisterIOHandler(TWIS_CTRLB(base),sctrlb_read,sctrlb_write,twi);
	AVR8_RegisterIOHandler(TWIS_STATUS(base),sstatus_read,sstatus_write,twi);
	AVR8_RegisterIOHandler(TWIS_ADDR(base),saddr_read,saddr_write,twi);
	AVR8_RegisterIOHandler(TWIS_DATA(base),sdata_read,sdata_write,twi);
	AVR8_RegisterIOHandler(TWIS_ADDRMASK(base),saddrmask_read,saddrmask_write,twi);

}
