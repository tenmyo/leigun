/*
 *******************************************************************************************************
 *
 * Emulation of the Infineon C161 Async Serial interface controller 
 *
 *  State: Untested
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
#include <string.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "c161_serial.h"
#include "bus.h"
#include "c16x/c16x_cpu.h"
#include "fio.h"
#include "signode.h"
#include "configfile.h"
#include "sgstring.h"

#if 1
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define S0TIR	(1<<7)
#define S0TIE	(1<<6)

#define S0RIR	(1<<7)
#define S0RIE	(1<<6)

#define S0EIR	(1<<7)
#define S0EIE	(1<<6)

#define S0TBIR	(1<<7)
#define S0TBIE	(1<<6)

typedef struct C161_Serial {
	uint16_t clc;
	uint16_t con;
	uint16_t abcon;
	uint16_t abstat;
	uint16_t bg;
	uint16_t fdv;
	uint16_t pmw;
	uint16_t tbuf;
	uint16_t rbuf;
	uint16_t tic;
	uint16_t ric;
	uint16_t eic;
	uint16_t tbic;

	int interrupt_posted;
	SigNode *irqNode;

	uint16_t tx_shift_reg;
	int tx_shift_reg_busy;
	int tbuf_busy;
	int rbuf_busy;

	int fd;
	FIO_FileHandler input_fh;
	FIO_FileHandler output_fh;
	int ifh_is_active;
	int ofh_is_active;
} C161_Serial;

static inline void enable_rx(C161_Serial *);
static inline void disable_rx(C161_Serial *);
static inline void enable_tx(C161_Serial *);
static inline void disable_tx(C161_Serial *);

static void
update_interrupts(C161_Serial * ser)
{
	int interrupt = 0;
	if ((ser->tic & (S0TIR | S0TIE)) == (S0TIR | S0TIE)) {
		interrupt = 1;
	} else if ((ser->ric & (S0RIR | S0RIE)) == (S0RIR | S0RIE)) {
		interrupt = 1;
	} else if ((ser->eic & (S0EIR | S0EIE)) == (S0EIR | S0EIE)) {
		interrupt = 1;
	} else if ((ser->tbic & (S0TBIR | S0TBIE)) == (S0TBIR | S0TBIE)) {
		interrupt = 1;
	}
	if (interrupt && !ser->interrupt_posted) {
		dbgprintf("SER: Posting interrupt\n");
		SigNode_Set(ser->irqNode, SIG_LOW);
		ser->interrupt_posted = 1;
	}
	if (!interrupt && ser->interrupt_posted) {
		SigNode_Set(ser->irqNode, SIG_HIGH);
		ser->interrupt_posted = 0;
	}
}

/*
 * -------------------------------
 * close the input/output file
 * -------------------------------
 */

static void
serial_close(C161_Serial * ser)
{
	if (ser->fd < 0) {
		return;
	}
	disable_rx(ser);
	disable_tx(ser);
	close(ser->fd);
	ser->fd = -1;
	return;
}

/*
 * ------------------------------------------------------------------------
 * serial_output
 *	The fileeventhandler called whenever Output fd is writable and
 *	txfifo is not empty
 * ------------------------------------------------------------------------
 */
static void
serial_output(void *cd, int mask)
{
	C161_Serial *ser = (C161_Serial *) cd;
	while (ser->tx_shift_reg_busy) {
		int count;
		uint8_t val = ser->tx_shift_reg;
		count = write(ser->fd, &val, 1);
		if (count > 0) {
			if (ser->tbuf_busy) {
				ser->tx_shift_reg = ser->tbuf;
				ser->tbic |= S0TBIR;
				update_interrupts(ser);
				ser->tbuf_busy = 0;
			} else {
				ser->tx_shift_reg_busy = 0;
				ser->tic |= S0TIR;
				update_interrupts(ser);
			}
		} else if (count == 0) {	// EOF
			fprintf(stderr, "C161_Serial fd: EOF on output\n");
			serial_close(ser);
			break;
		} else if ((count < 0) && (errno == EAGAIN)) {
			break;
		} else {
			fprintf(stderr, "Error on output\n");
			serial_close(ser);
			break;
		}
	}
	return;
}

static void
serial_input(void *cd, int mask)
{
	C161_Serial *ser = (C161_Serial *) cd;
	char c;
	int count = read(ser->fd, &c, 1);
	if (ser->fd < 0) {
		fprintf(stderr, "Serial input with illegal fd %d\n", ser->fd);
		return;
	}
	if (count == 1) {
		ser->rbuf_busy = 1;
		ser->rbuf = c;
		ser->ric |= S0RIR;
		update_interrupts(ser);
		disable_rx(ser);
	} else if (count == 0) {	// EOF
		fprintf(stderr, "EOF reading from serial\n");
		serial_close(ser);
	} else if ((count < 0) && (errno == EAGAIN)) {
		// nothing, we are in nonblocking mode
	} else {
		fprintf(stderr, "Error on input\n");
		serial_close(ser);
	}
	return;
}

/*
 * -----------------------------------------------------------------
 * enable_rx
 * 	Add the file event handler for receiving
 * -----------------------------------------------------------------
 */
static void
enable_rx(C161_Serial * ser)
{
	if ((ser->fd >= 0) && !(ser->ifh_is_active)) {
		FIO_AddFileHandler(&ser->input_fh, ser->fd, FIO_READABLE, serial_input, ser);
		ser->ifh_is_active = 1;
	}
}

static inline void
disable_rx(C161_Serial * ser)
{
	if (ser->ifh_is_active) {
		FIO_RemoveFileHandler(&ser->input_fh);
	}
	ser->ifh_is_active = 0;
}

/*
 * -----------------------------------------------------------------
 * enable_tx
 * 	Add the file event handler for transmission 
 * -----------------------------------------------------------------
 */

static inline void
enable_tx(C161_Serial * ser)
{
	if ((ser->fd >= 0) && !(ser->ofh_is_active)) {
		FIO_AddFileHandler(&ser->output_fh, ser->fd, FIO_WRITABLE, serial_output, ser);
		ser->ofh_is_active = 1;
	}
}

static inline void
disable_tx(C161_Serial * ser)
{
	if (ser->ofh_is_active) {
		FIO_RemoveFileHandler(&ser->output_fh);
	}
	ser->ofh_is_active = 0;
}

/*
 * --------------------------------------------
 * Write one character into the TX-Fifo
 * --------------------------------------------
 */
static inline void
tx_shiftreg_write(C161_Serial * ser, uint16_t c)
{
	ser->tx_shift_reg = c;
	ser->tx_shift_reg_busy = 1;
	ser->tbic |= S0TBIR;
	update_interrupts(ser);
	enable_tx(ser);
}

static uint32_t
clc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "clc read not implemented\n");
	return 0;
}

static void
clc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "clc write not implemented\n");

}

#if 0
static uint32_t
con_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "con read not implemented\n");
	return 0;
}

static void
con_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "con write not implemented\n");
}
#endif

static uint32_t
abcon_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "abcon read not implemented\n");
	return 0;
}

static void
abcon_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "abcon write not implemented\n");
}

static uint32_t
abstat_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "abstat read not implemented\n");
	return 0;
}

static void
abstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "abstat write not implemented\n");

}

static uint32_t
bg_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "bg read not implemented\n");
	return 0;
}

static void
bg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "bg write not implemented\n");

}

static uint32_t
fdv_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "fdv read not implemented\n");
	return 0;
}

static void
fdv_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	fprintf(stderr, "fdv write not implemented\n");
}

static uint32_t
pmw_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "pmw read not implemented\n");
	return 0;
}

static void
pmw_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	fprintf(stderr, "pmw write not implemented\n");
}

static uint32_t
tbuf_read(void *clientData, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	dbgprintf("C161 tbuf read 0x%04x\n", ser->tbuf);
	return ser->tbuf;
}

static void
tbuf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	dbgprintf("C161 tbuf write 0x%04x\n", value);
	//exit(2342);
	ser->tbuf = value & 0x1ff;
	if (!ser->tx_shift_reg_busy) {
		tx_shiftreg_write(ser, value);
	} else if (!ser->tbuf_busy) {
		ser->tbuf_busy = 1;
	} else {
		fprintf(stderr, "tbuf output overflow\n");
	}
}

/*
 * -----------------------------------------------
 * The CPU documentation does not tell when
 * data is removed from the RX-buffer
 * I suspect that this is done by reading the
 * rbuf register
 * -----------------------------------------------
 */
static uint32_t
rbuf_read(void *clientData, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	if (ser->rbuf_busy) {
		ser->rbuf_busy = 0;
		enable_rx(ser);
	}
	return ser->rbuf;
}

/* 
 * -----------------------------------------------------------------
 * The documentation tells in one location that rbuf is not writable
 * in other location it says that lower 8 Bits are writable. 
 * -----------------------------------------------------------------
 */
static void
rbuf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	ser->rbuf = value & 0xff;
	fprintf(stderr, "rbuf write (possible nonsesnse)  \n");
}

static uint32_t
tic_read(void *clientData, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	dbgprintf("TIC read %04x\n", ser->tic);
	return ser->tic;
}

static void
tic_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	C161_Serial *ser = (C161_Serial *) clientData;
	ser->tic = value;
	dbgprintf("TIC write %04x\n", ser->tic);
	update_interrupts(ser);
}

static uint32_t
ric_read(void *clientData, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	dbgprintf("RIC read %04x\n", ser->ric);
	return ser->ric;
}

static void
ric_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	ser->ric = value;
	dbgprintf("RIC write %04x\n", value);
	update_interrupts(ser);
}

static uint32_t
eic_read(void *clientData, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	return ser->eic;
}

static void
eic_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	ser->eic = value;
	update_interrupts(ser);
}

static uint32_t
tbic_read(void *clientData, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	dbgprintf("TBIC read %04x\n", ser->tbic);
	return ser->tbic;
}

static void
tbic_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C161_Serial *ser = (C161_Serial *) clientData;
	dbgprintf("TBIC write %04x\n", value);
	ser->tbic = value;
	update_interrupts(ser);
}

void
C161_SerialNew(char *devname)
{
	C161_Serial *ser;
	char *filename;

	fprintf(stderr, "Creating C161 Serial Interface emulator\n");
	/* All register reset values are 0 (UserManual p. 257) */
	ser = sg_new(C161_Serial);
	ser->irqNode = SigNode_New("%s.irq", devname);
	if (!ser->irqNode) {
		fprintf(stderr, "C161_Serial: Can not create IrqRequest Node for dev %s\n",
			devname);
		exit(3425);
	}

	IOH_New16(SFR_ADDR(SFR_S0CLC), clc_read, clc_write, ser);
	IOH_New16(SFR_ADDR(SFR_S0CON), clc_read, clc_write, ser);
	IOH_New16(SFR_ADDR(SFR_ABS0CON), abcon_read, abcon_write, ser);
	IOH_New16(SFR_ADDR(SFR_ABSTAT), abstat_read, abstat_write, ser);
	IOH_New16(SFR_ADDR(SFR_S0BG), bg_read, bg_write, ser);
	IOH_New16(SFR_ADDR(SFR_S0FDV), fdv_read, fdv_write, ser);
	IOH_New16(SFR_ADDR(SFR_S0PMW), pmw_read, pmw_write, ser);

	IOH_New16(SFR_ADDR(SFR_S0TBUF), tbuf_read, tbuf_write, ser);
	IOH_New16(SFR_ADDR(SFR_S0RBUF), rbuf_read, rbuf_write, ser);
	IOH_New16(SFR_ADDR(SFR_S0TIC), tic_read, tic_write, ser);
	IOH_New16(SFR_ADDR(SFR_S0RIC), ric_read, ric_write, ser);
	IOH_New16(SFR_ADDR(SFR_S0EIC), eic_read, eic_write, ser);
	IOH_New16(ESFR_ADDR(ESFR_S0TBIC), tbic_read, tbic_write, ser);
	filename = Config_ReadVar("C161", devname);
	if (filename) {
		/* does cygwin have /dev/stdin ? */
		if (!strcmp(filename, "stdin")) {
			ser->fd = 0;
		} else {
			ser->fd = open(filename, O_RDWR);
		}
		if (ser->fd < 0) {
			fprintf(stderr, "%s: Cannot open %s\n", devname, filename);
			sleep(1);
		} else {
			fcntl(ser->fd, F_SETFL, O_NONBLOCK);
			enable_rx(ser);	// should be updaterx
			fprintf(stderr, "C161 Serial %s Connected to %s\n", devname, filename);
		}
	} else {
		fprintf(stderr, "C161 Serial %s connected to nowhere\n", devname);
	}
}
