/*
 *************************************************************************************************
 * Emulation of MPC866 Serial Communication Controller 
 *
 * state:  Not implemented
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
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
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <bus.h>
#include <signode.h>
#include <configfile.h>
#include <clock.h>
#include <cycletimer.h>
#include <sgstring.h>
#include <serial.h>
#include "mpc8xx_scc.h"
#include "mpc8xx_dpram.h"

/* Relative to Control Register Base */

#define SCC_GSMR_L(base) 		((base) + 0x0)
#define	   GSMR_L_SIR		(1<<(31-0))
#define    GSMR_L_EDGE_MASK	(3<<(31-2))
#define	   GSMR_L_TCI		(1<<(32-3))
#define	   GSMR_L_TSYNC_MASK	(3<<(31-5))
#define	   GSMR_L_TSYNC_SHIFT	(31-5)
#define	   GSMR_L_RINV		(1<<(31-6))
#define	   GSMR_L_TINV		(1<<(31-7))
#define	   GSMR_L_TPL_MASK	(7<<(31-10))
#define	   GSMR_L_TPL_SHIFT	(31-10)
#define	   GSMR_L_TPP		(3<<(31-12))
#define	   GSMR_L_TEND		(1<<(31-13)
#define	   GSMR_L_TDCR_MASK	(3<<(31-15))
#define    GSMR_L_TDCR_SHIFT	(31-15)
#define	   GSMR_L_RDCR_MASK	(3<<(31-17))
#define	   GSMR_L_RDCR_SHIFT	(31-17)
#define	   GSMR_L_RENC_MASK	(7<<(31-20))
#define	   GSMR_L_RENC_SHIFT	(31-20)
#define	   GSMR_L_TENC_MASK	(7<<(31-23))
#define	   GSMR_L_TENC_SHIFT	(31-23)
#define    GSMR_L_DIAG_MASK	(3<<(31-25))
#define	   GSMR_L_DIAG_SHIFT	(1<<(31-25))
#define    GSMR_L_ENR		(1<<(31-26))
#define	   GSMR_L_ENT		(1<<(31-27))
#define	   GSMR_L_MODE		(1<<(31-31))

#define SCC_GSMR_H(base) 		((base) + 0x4)
#define    GSMR_H_IRP		(1<<(31-13))
#define    GSMR_H_GDE		(1<<(31-15))
#define    GSMR_H_TCRC_MASK	(3<<(31-17))
#define    GSMR_H_TCRC_SHIFT	((31-17))
#define	   GSMR_H_REVD		(1<<(31-18))
#define	   GSMR_H_TRX		(1<<(31-19))
#define	   GSMR_H_TTX		(1<<(31-20))
#define	   GSMR_H_CDP		(1<<(31-21))
#define	   GSMR_H_CTSP		(1<<(31-22))
#define	   GSMR_H_CDS		(1<<(31-23))
#define	   GSMR_H_CTSS		(1<<(31-24))
#define	   GSMR_H_TFL		(1<<(31-25))
#define	   GSMR_H_RFW		(1<<(31-26))
#define	   GSMR_H_TXSY		(1<<(31-27))
#define	   GSMR_H_SYNL_MASK	(3<<(31-29))
#define    GSMR_H_SYNL_SHIFT	((31-29))
#define	   GSMR_H_RTSM		(1<<(31-30))
#define	   GSMR_H_RSYN		(1<<(31-31))

#define UART_PSRM(base)			((base) + 0x8)
#define SCC_TODR(base)			((base) + 0xc)
#define SCC_DSR(base)			((base) + 0xe)
#define UART_SCCE(base)			((base) + 0x10)
#define UART_SCCM(base)			((base) + 0x14)
#define UART_SCCS(base)			((base) + 0x17)

#define PRAM_RBASE(base)		((base) + 0x0)
#define PRAM_TBASE(base)		((base) + 0x2)
#define PRAM_RFCR(base)			((base) + 0x4)
#define PRAM_TFCR(base)			((base) + 0x5)
#define PRAM_MRBLR(base)		((base) + 0x6)
#define PRAM_RSTATE(base)		((base) + 0x8)
#define PRAM_RIP(base)			((base) + 0xc)
#define PRAM_RBPTR(base)		((base) + 0x10)
#define PRAM_RCOUNT(base)		((base) + 0x12)
#define PRAM_RTEMP(base)		((base) + 0x14)
#define PRAM_TSTATE(base)		((base) + 0x18)
#define PRAM_TIP(base)			((base) + 0x1c)
#define PRAM_TBPTR(base)		((base) + 0x20)
#define PRAM_TCOUNT(base)		((base) + 0x22)
#define PRAM_TTEMP(base)		((base) + 0x24)
#define PRAM_RCRC(base)			((base) + 0x28)
#define PRAM_TCRC(base)			((base) + 0x2c)
#define UART_MAX_IDL(base)		((base) + 0x38)
#define UART_IDLC(base)			((base) + 0x3a)
#define UART_BRKCR(base)		((base) + 0x3c)
#define UART_PAREC(base)		((base) + 0x3e)
#define UART_FRMEC(base)		((base) + 0x40)
#define UART_NOSEC(base)		((base) + 0x42)
#define UART_BRKEC(base)		((base) + 0x44)
#define UART_BRKLN(base)		((base) + 0x46)
#define UART_UADDR1(base)		((base) + 0x48)
#define UART_UADDR2(base)		((base) + 0x4a)
#define UART_TOSEQ(base)		((base) + 0x4e)
#define UART_CHARACTER(base,x)		((base) + 0x50 + 2 * (x))
#define UART_RCCM(base)			((base) + 0x60)
#define UART_RCCR(base)			((base) + 0x62)
#define UART_RLBC(base)			((base) + 0x64)

/*
 *************************************************************
 * Buffer descriptors are located in dual port RAM
 *************************************************************
 */

/* From Linux kernel header */

#define BD_SC_EMPTY     ((uint16_t)0x8000)	/* Receive is empty */
#define BD_SC_READY     ((uint16_t)0x8000)	/* Transmit is ready */
#define BD_SC_WRAP      ((uint16_t)0x2000)	/* Last buffer descriptor */
#define BD_SC_INTRPT    ((uint16_t)0x1000)	/* Interrupt on change */
#define BD_SC_LAST      ((uint16_t)0x0800)	/* Last buffer in frame */
#define BD_SC_TC        ((uint16_t)0x0400)	/* Transmit CRC */
#define BD_SC_CM        ((uint16_t)0x0200)	/* Continous mode */
#define BD_SC_ID        ((uint16_t)0x0100)	/* Rec'd too many idles */
#define BD_SC_P         ((uint16_t)0x0100)	/* xmt preamble */
#define BD_SC_BR        ((uint16_t)0x0020)	/* Break received */
#define BD_SC_FR        ((uint16_t)0x0010)	/* Framing error */
#define BD_SC_PR        ((uint16_t)0x0008)	/* Parity error */
#define BD_SC_NAK       ((uint16_t)0x0004)	/* NAK - did not respond */
#define BD_SC_OV        ((uint16_t)0x0002)	/* Overrun */
#define BD_SC_UN        ((uint16_t)0x0002)	/* Underrun */
#define BD_SC_CD        ((uint16_t)0x0001)	/* ?? */
#define BD_SC_CL        ((uint16_t)0x0001)	/* Collision */

typedef struct SCC_BD {
	/*
	 * R: Ready
	 * W: Wrap
	 * I: Interrupt
	 * CR: Clear to send report
	 * A: Address, valid only in multidrop mode
	 * CM: Continuous mode
	 * P:  Preamble
	 * NS: No stop bit or shaved stop bit sent
	 * CT: CTS lost
	 */
	uint16_t statusControl;
	uint16_t dataLength;
	uint16_t hoBufferPointer;
	uint16_t loBufferPointer;
} SCC_BD;

typedef struct MpcScc {
	BusDevice bdev;
	BusDevice *dpram;
	UartPort *port;
	uint32_t conf_base;
	uint32_t param_offs;
} Scc;

static void
read_buffer_descriptor(Scc * scc, SCC_BD * bd, uint32_t addr)
{
	bd->statusControl = MPC_DPRamRead(scc->dpram, addr, 2);
	bd->dataLength = MPC_DPRamRead(scc->dpram, addr + 2, 2);
	bd->hoBufferPointer = MPC_DPRamRead(scc->dpram, addr + 4, 2);
	bd->loBufferPointer = MPC_DPRamRead(scc->dpram, addr + 6, 2);
}

static void
write_buffer_descriptor(Scc * scc, SCC_BD * bd, uint32_t addr)
{
	MPC_DPRamWrite(scc->dpram, bd->statusControl, addr, 2);
	MPC_DPRamWrite(scc->dpram, bd->dataLength, addr + 2, 2);
	MPC_DPRamWrite(scc->dpram, bd->hoBufferPointer, addr + 4, 2);
	MPC_DPRamWrite(scc->dpram, bd->loBufferPointer, addr + 6, 2);
}

static void
serial_input(void *cd, UartChar c)
{
	uint32_t bufp;
	uint8_t u8data;
	Scc *scc = (Scc *) cd;
	SCC_BD bd;
	SCC_BD *bdp = &bd;
	uint16_t rbuf = MPC_DPRamRead(scc->dpram, PRAM_RBPTR(0), 2);
	read_buffer_descriptor(scc, bdp, rbuf);
	if (!(bdp->statusControl & BD_SC_EMPTY)) {
		SerialDevice_StopRx(scc->port);
		return;
	}
	bufp = (bdp->hoBufferPointer << 16) | bdp->loBufferPointer;
	u8data = c;
	Bus_Write(bufp, &u8data, 1);
	bdp->statusControl &= ~BD_SC_EMPTY;
	write_buffer_descriptor(scc, bdp, rbuf);
	if (bdp->statusControl & BD_SC_WRAP) {
		rbuf = MPC_DPRamRead(scc->dpram, PRAM_RBASE(0), 2);
	} else {
		rbuf += 8;
	}
	MPC_DPRamWrite(scc->dpram, rbuf, PRAM_RBPTR(0), 2);
}

static bool
serial_output(void *cd, UartChar * c)
{
	uint32_t bufp;
	uint8_t u8data;
	Scc *scc = (Scc *) cd;
	SCC_BD bd;
	SCC_BD *bdp = &bd;
	uint16_t tbuf = MPC_DPRamRead(scc->dpram, PRAM_TBPTR(0), 2);
	read_buffer_descriptor(scc, bdp, tbuf);
	if (!(bdp->statusControl & BD_SC_READY)) {
		SerialDevice_StopTx(scc->port);
		return false;
	}
	bufp = (bdp->hoBufferPointer << 16) | bdp->loBufferPointer;
	Bus_Read(&u8data, bufp, 1);
	*c = u8data;
	bdp->statusControl &= ~BD_SC_READY;
	write_buffer_descriptor(scc, bdp, tbuf);
	if (bdp->statusControl & BD_SC_WRAP) {
		tbuf = MPC_DPRamRead(scc->dpram, PRAM_TBASE(0), 2);
	} else {
		tbuf += 8;
	}
	MPC_DPRamWrite(scc->dpram, tbuf, PRAM_TBPTR(0), 2);
	return true;
}

/*
 *********************************************************************
 * SIR Serial infrared encoding
 * EDGE Clock Edge
 * TCI Transmit clock invert
 * TSNC Transmit sense
 * RINV DPLL Rx input invert data.
 * TINV DPLL Tx input invert data.
 * TPL Tx preamble length
 * TPP Tx preamble pattern
 * TEND Transmitter frame ending
 * TDCR Transmitter DPLL clock rate.
 * RDCR Receiver DPLL clock rate. 
 * RENC/TENC Receiver decoding / transmitter encoding method
 * DIAG Diagnostic mode
 * ENR Enable receiver
 * ENT Enable transmit
 * MODE HDLC,APPLE Talk SS7 UART ASYNC BISYNC Ethernet
 *********************************************************************
 */
static uint32_t
gsmr_l_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: gsmr_l not implemented\n");
	return 0;
}

static void
gsmr_l_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: gsmr_l not implemented\n");
}

/*
 *****************************************************************
 * Bit 13 IRP Infrared Rx polarity.
 * Bit 15 GDE Glitch detect enable.
 * Bits 16+17 Transparent CRC
 * Bit 18 REVD Reverse data  
 * Bit 19+20 TRX,TTX Transparent receiver/transmitter
 * CDP, CTSP CD/CTS pulse.
 * TFL Transmit FIFO Length
 * RFW Rx Fifo width
 * TXSY Transmitter synchronized to the receiver
 * Bits 28+29 SYNL Sync length
 * RTSM RTS Mode 
 * RSYN Receive synchronization timing
 *****************************************************************
 */
static uint32_t
gsmr_h_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: gsmr_h not implemented\n");
	return 0;
}

static void
gsmr_h_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: gsmr_h not implemented\n");
}

/*
 *****************************************************************
 * Data Syncronization Register (DSR)
 * Uart: fractional stop bit transmission
 * BISYNC and transparent: Sync pattern
 * Ethernet: 0xd555
 * HDLC 0x7e7e (HDLC flag)
 *
 * UART:
 *	1-4 FSB
 *****************************************************************
 */
static uint32_t
dsr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: dsr not implemented\n");
	return 0;
}

static void
dsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: dsr not implemented\n");
}

/*
 **************************************************************
 * Transmit on Demand Register (TODR)
 **************************************************************
 */
static uint32_t
todr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: todr not implemented\n");
	return 0;
}

static void
todr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: todr not implemented\n");
}

/*
 *****************************************************************
 * UART_PSMR
 * Protocol specific mode register for UART mode
 * FLC	Flow Control
 * SL	Stop length
 * CL	Character length
 * UM Uart mode 00 = normal
 * FRZ Freeze transmission. 
 * RZS Receive zero stop bits
 * SYN Synchronous mode
 * DRT Disable receiver whiel transmitting
 * PEN Parity enable
 * RPM/TPM Receiver Transmitter parity mode
 *****************************************************************
 */
static uint32_t
uart_psrm_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: psrm not implemented\n");
	return 0;
}

static void
uart_psrm_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: psrm not implemented\n");
}

/*
 ****************************************************************
 * SCC Event Register. Read this to determine interrupt source
 * GLR Glitch on Rx
 * GLT Glitch on transmit
 * AB  Autobaud
 * IDL Idle sequence status changed
 * GRA Graceful stop complete
 * BRKE: Break end.
 * BRKS: Break start.
 * CCR:  control character received and rejected.
 * BSY:  Busy
 * TX:   Tx event. 
 * RX:   Rx event.
 ****************************************************************
 */
static uint32_t
uart_scce_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: scce not implemented\n");
	return 0;
}

static void
uart_scce_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: scce not implemented\n");
}

/*
 **************************************************************
 * SCC Mask Register
 * Same Bitfield names as in Event register.
 **************************************************************
 */
static uint32_t
uart_sccm_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: sccm not implemented\n");
	return 0;
}

static void
uart_sccm_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: sccm not implemented\n");
}

/*
 ***************************************************************
 * SCC status register This 8-bit read-only register
 * ID: Idle status 0 = not idle 1 = idle
 ***************************************************************
 */
static uint32_t
uart_sccs_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: sccs not implemented\n");
	return 0;
}

static void
uart_sccs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: sccs not implemented\n");
}

/*
 **********************************************************************
 * rbase is the offset of first the receive buffer descriptor
 * in the Dualport RAM
 **********************************************************************
 */
static uint32_t
rbase_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rbase not implemented\n");
	return 0;
}

static void
rbase_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rbase not implemented\n");
}

/*
 * tbase is the offset of first transmit buffer descriptor
 * in the Dualport RAM
 */
static uint32_t
tbase_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tbase not implemented\n");
	return 0;
}

static void
tbase_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tbase not implemented\n");
}

/*
 * Receive function code registers
 * BO: Byte ordering
 * AT[1-3] Address type. 
 */
static uint32_t
rfcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rfcr not implemented\n");
	return 0;
}

static void
rfcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rfcr not implemented\n");
}

/*
 * Transmit function code register
 */
static uint32_t
tfcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tfcr not implemented\n");
	return 0;
}

static void
tfcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tfcr not implemented\n");
}

/*
 ****************************************************************
 * MRBLR
 * Maximum receive buffer length. 
 * Maximum number of bytes written to one receive buffer before
 * the next one is used.
 ****************************************************************
 */
static uint32_t
mrblr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: mrblr not implemented\n");
	return 0;
}

static void
mrblr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: mrblr not implemented\n");
}

static uint32_t
rstate_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rstate not implemented\n");
	return 0;
}

static void
rstate_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rstate not implemented\n");
}

static uint32_t
rip_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rip not implemented\n");
	return 0;
}

static void
rip_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rip not implemented\n");
}

static uint32_t
rbptr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rbptr not implemented\n");
	return 0;
}

static void
rbptr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rbptr not implemented\n");
}

static uint32_t
rcount_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rcount not implemented\n");
	return 0;
}

static void
rcount_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rcount not implemented\n");
}

static uint32_t
rtemp_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rtemp not implemented\n");
	return 0;
}

static void
rtemp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rtemp not implemented\n");
}

static uint32_t
tstate_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tstate not implemented\n");
	return 0;
}

static void
tstate_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tstate not implemented\n");
}

static uint32_t
tip_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tip not implemented\n");
	return 0;
}

static void
tip_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tip not implemented\n");
}

static uint32_t
tbptr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tbptr not implemented\n");
	return 0;
}

static void
tbptr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tbptr not implemented\n");
}

static uint32_t
tcount_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tcount not implemented\n");
	return 0;
}

static void
tcount_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tcount not implemented\n");
}

static uint32_t
ttemp_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: ttemp not implemented\n");
	return 0;
}

static void
ttemp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: ttemp not implemented\n");
}

static uint32_t
rcrc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rcrc not implemented\n");
	return 0;
}

static void
rcrc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: rcrc not implemented\n");
}

static uint32_t
tcrc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tcrc not implemented\n");
	return 0;
}

static void
tcrc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: tcrc not implemented\n");
}

static uint32_t
uart_max_idl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_max_idl not implemented\n");
	return 0;
}

static void
uart_max_idl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_max_idl not implemented\n");
}

static uint32_t
uart_idlc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_idlc not implemented\n");
	return 0;
}

static void
uart_idlc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_idlc not implemented\n");
}

static uint32_t
uart_brkcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_brkcr not implemented\n");
	return 0;
}

static void
uart_brkcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_brkcr not implemented\n");
}

static uint32_t
uart_parec_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_parec not implemented\n");
	return 0;
}

static void
uart_parec_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_parec not implemented\n");
}

static uint32_t
uart_frmec_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_frmec not implemented\n");
	return 0;
}

static void
uart_frmec_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_frmec not implemented\n");
}

static uint32_t
uart_nosec_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_nosec not implemented\n");
	return 0;
}

static void
uart_nosec_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_nosec not implemented\n");
}

static uint32_t
uart_brkec_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_brkec not implemented\n");
	return 0;
}

static void
uart_brkec_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_brkec not implemented\n");
}

static uint32_t
uart_brkln_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_brkln not implemented\n");
	return 0;
}

static void
uart_brkln_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_brkln not implemented\n");
}

static uint32_t
uart_uaddr1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_uaddr1 not implemented\n");
	return 0;
}

static void
uart_uaddr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_uaddr1 not implemented\n");
}

static uint32_t
uart_uaddr2_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_uaddr2 not implemented\n");
	return 0;
}

static void
uart_uaddr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_uaddr2 not implemented\n");
}

/*
 * Transmit Out of Sequence Register TOSEQ
 */
static uint32_t
uart_toseq_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_toseq not implemented\n");
	return 0;
}

static void
uart_toseq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_toseq not implemented\n");
}

static uint32_t
uart_character_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_character not implemented\n");
	return 0;
}

static void
uart_character_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_character not implemented\n");
}

static uint32_t
uart_rccm_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_rccm not implemented\n");
	return 0;
}

static void
uart_rccm_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_rccm not implemented\n");
}

static uint32_t
uart_rccr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_rccr not implemented\n");
	return 0;
}

static void
uart_rccr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_rccr not implemented\n");
}

static uint32_t
uart_rlbc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_rlbc not implemented\n");
	return 0;
}

static void
uart_rlbc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCC: uart_rlbc not implemented\n");
}

static void
Scc_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	Scc *scc = (Scc *) owner;
	uint32_t conf_base = scc->conf_base;
	//uint32_t pram = base; 
	//int i;
	IOH_Delete16(SCC_GSMR_L(conf_base));
	IOH_Delete16(SCC_GSMR_H(conf_base));
	IOH_Delete16(SCC_TODR(conf_base));
	IOH_Delete16(SCC_DSR(conf_base));

#if 0
	IOH_Delete16(UART_PSRM(base));
	IOH_Delete16(UART_SCCE(base));
	IOH_Delete16(UART_SCCM(base));
	IOH_Delete8(UART_SCCS(base));

	IOH_Delete16(PRAM_RBASE(pram));
	IOH_Delete16(PRAM_TBASE(pram));
	IOH_Delete8(PRAM_RFCR(pram));
	IOH_Delete8(PRAM_TFCR(pram));
	IOH_Delete16(PRAM_MRBLR(pram));
	IOH_Delete32(PRAM_RSTATE(pram));
	IOH_Delete32(PRAM_RIP(pram));
	IOH_Delete16(PRAM_RBPTR(pram));
	IOH_Delete16(PRAM_RCOUNT(pram));
	IOH_Delete32(PRAM_RTEMP(pram));
	IOH_Delete32(PRAM_TSTATE(pram));
	IOH_Delete32(PRAM_TIP(pram));
	IOH_Delete16(PRAM_TBPTR(pram));
	IOH_Delete16(PRAM_TCOUNT(pram));
	IOH_Delete32(PRAM_TTEMP(pram));
	IOH_Delete32(PRAM_RCRC(pram));
	IOH_Delete32(PRAM_TCRC(pram));

	IOH_Delete16(UART_MAX_IDL(pram));
	IOH_Delete16(UART_IDLC(pram));
	IOH_Delete16(UART_BRKCR(pram));
	IOH_Delete16(UART_PAREC(pram));
	IOH_Delete16(UART_FRMEC(pram));
	IOH_Delete16(UART_NOSEC(pram));
	IOH_Delete16(UART_BRKEC(pram));
	IOH_Delete16(UART_BRKLN(pram));
	IOH_Delete16(UART_UADDR1(pram));
	IOH_Delete16(UART_UADDR2(pram));
	IOH_Delete16(UART_TOSEQ(pram));
	for (i = 0; i <= 7; i++) {
		IOH_Delete16(UART_CHARACTER(pram, i));
	}
	IOH_Delete16(UART_RCCM(pram));
	IOH_Delete16(UART_RCCR(pram));
	IOH_Delete16(UART_RLBC(pram));
#endif
}

static void
Scc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	Scc *scc = (Scc *) owner;
	uint32_t conf_base = scc->conf_base;
	uint32_t pram = base;
	int i;

	IOH_New16(SCC_GSMR_L(conf_base), gsmr_l_read, gsmr_l_write, scc);
	IOH_New16(SCC_GSMR_H(conf_base), gsmr_h_read, gsmr_h_write, scc);
	IOH_New16(SCC_TODR(conf_base), todr_read, todr_write, scc);
	IOH_New16(SCC_DSR(conf_base), dsr_read, dsr_write, scc);

#if 1
	IOH_New16(UART_PSRM(base), uart_psrm_read, uart_psrm_write, scc);
	IOH_New16(UART_SCCE(base), uart_scce_read, uart_scce_write, scc);
	IOH_New16(UART_SCCM(base), uart_sccm_read, uart_sccm_write, scc);
	IOH_New8(UART_SCCS(base), uart_sccs_read, uart_sccs_write, scc);
	IOH_New16(PRAM_RBASE(pram), rbase_read, rbase_write, scc);
	IOH_New16(PRAM_TBASE(pram), tbase_read, tbase_write, scc);
	IOH_New8(PRAM_RFCR(pram), rfcr_read, rfcr_write, scc);
	IOH_New8(PRAM_TFCR(pram), tfcr_read, tfcr_write, scc);
	IOH_New16(PRAM_MRBLR(pram), mrblr_read, mrblr_write, scc);
	IOH_New32(PRAM_RSTATE(pram), rstate_read, rstate_write, scc);
	IOH_New32(PRAM_RIP(pram), rip_read, rip_write, scc);
	IOH_New16(PRAM_RBPTR(pram), rbptr_read, rbptr_write, scc);
	IOH_New16(PRAM_RCOUNT(pram), rcount_read, rcount_write, scc);
	IOH_New32(PRAM_RTEMP(pram), rtemp_read, rtemp_write, scc);
	IOH_New32(PRAM_TSTATE(pram), tstate_read, tstate_write, scc);
	IOH_New32(PRAM_TIP(pram), tip_read, tip_write, scc);
	IOH_New16(PRAM_TBPTR(pram), tbptr_read, tbptr_write, scc);
	IOH_New16(PRAM_TCOUNT(pram), tcount_read, tcount_write, scc);
	IOH_New32(PRAM_TTEMP(pram), ttemp_read, ttemp_write, scc);
	IOH_New32(PRAM_RCRC(pram), rcrc_read, rcrc_write, scc);
	IOH_New32(PRAM_TCRC(pram), tcrc_read, tcrc_write, scc);
	IOH_New16(UART_MAX_IDL(pram), uart_max_idl_read, uart_max_idl_write, scc);
	IOH_New16(UART_IDLC(pram), uart_idlc_read, uart_idlc_write, scc);
	IOH_New16(UART_BRKCR(pram), uart_brkcr_read, uart_brkcr_write, scc);
	IOH_New16(UART_PAREC(pram), uart_parec_read, uart_parec_write, scc);
	IOH_New16(UART_FRMEC(pram), uart_frmec_read, uart_frmec_write, scc);
	IOH_New16(UART_NOSEC(pram), uart_nosec_read, uart_nosec_write, scc);
	IOH_New16(UART_BRKEC(pram), uart_brkec_read, uart_brkec_write, scc);
	IOH_New16(UART_BRKLN(pram), uart_brkln_read, uart_brkln_write, scc);
	IOH_New16(UART_UADDR1(pram), uart_uaddr1_read, uart_uaddr1_write, scc);
	IOH_New16(UART_UADDR2(pram), uart_uaddr2_read, uart_uaddr2_write, scc);
	IOH_New16(UART_TOSEQ(pram), uart_toseq_read, uart_toseq_write, scc);
	for (i = 0; i <= 7; i++) {
		IOH_New16(UART_CHARACTER(pram, i), uart_character_read, uart_character_write, scc);
	}
	IOH_New16(UART_RCCM(pram), uart_rccm_read, uart_rccm_write, scc);
	IOH_New16(UART_RCCR(pram), uart_rccr_read, uart_rccr_write, scc);
	IOH_New16(UART_RLBC(pram), uart_rlbc_read, uart_rlbc_write, scc);
#endif

}

BusDevice *
MPC8xx_SCCNew(BusDevice * dpram, const char *name, uint32_t conf_base, uint32_t param_offs)
{

	Scc *scc = sg_calloc(sizeof(Scc));
	scc->dpram = dpram;
	scc->conf_base = conf_base;
	scc->param_offs = param_offs;
	scc->port = Uart_New(name, serial_input, serial_output, NULL, scc);
	scc->bdev.first_mapping = NULL;
	scc->bdev.Map = Scc_Map;
	scc->bdev.UnMap = Scc_Unmap;
	scc->bdev.owner = scc;
	scc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	DPRam_Trace(dpram, UART_SCCM(0), 2, uart_sccm_write, dpram);
	return &scc->bdev;
}
