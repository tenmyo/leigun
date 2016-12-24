/*
 **************************************************************************************************
 *
 * Emulation of the SJA1000 CAN Controller 
 *
 *  State: PeliCAN mode is working 
 *	   Basic CAN mode is not complete
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "bus.h"
#include "sja1000.h"
#include "socket_can.h"
#include "signode.h"
#include "sgstring.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define SJA_BASE  (0x70000000)

/*
 * ------------------------------------------
 * Ugly Macros for Fifo counters and buffers
 * ------------------------------------------
 */
#define CANRAM_SIZE (80)
#define INFIFO_SIZE (64)
#define INFIFO_MASK (INFIFO_SIZE-1)
#define INC_INFIFO_WP(sja) ({(sja)->infifo_wp = ((sja)->infifo_wp+1)&INFIFO_MASK;})
#define INFIFO_PUT(sja,val) ({(sja)->infifo[(sja)->infifo_wp]=(val); INC_INFIFO_WP(sja);})
#define INFIFO_BYTE(sja,idx) ((sja)->infifo[((sja)->infifo_rp+(idx))&INFIFO_MASK])
#define IS_PELICAN(sja) ((sja)->clkdiv&SJA_MODE)
#define IS_BASICCAN(sja) (!((sja)->clkdiv&SJA_MODE))
#define INFIFO_COUNT(sja) (((sja->infifo_wp-sja->infifo_rp)+INFIFO_SIZE)&INFIFO_MASK)
#define INFIFO_ROOM(sja) (INFIFO_SIZE - INFIFO_COUNT(sja)-1)

struct SJA1000 {
	CanController *interface;	// socket interface to outside 
	int interrupt_posted;
	BusDevice *bus;

	/* infifo and txbuf and unused ram is one block called canram */
	uint8_t canram[CANRAM_SIZE];
	uint8_t *infifo;
	uint8_t *txbuf;
	uint32_t infifo_rp;
	uint32_t infifo_wp;
	uint32_t layout;
	SigNode *irqNode;
	uint8_t mode;
	uint8_t oc;
	uint8_t sr;
	uint8_t ir;
	uint8_t btr0, btr1;

	uint8_t clkdiv;

	// PeliCAN special      
	uint8_t pc_rxcnt;
	uint8_t pc_ier;
	uint8_t pc_ewl;
	uint8_t pcr_acm[4];
	uint8_t pcr_acc[4];
};

static void sja_map_bcr(SJA1000 * sja, uint32_t base);
static void sja_map_bc(SJA1000 * sja, uint32_t base);
static void sja_map_pcr(SJA1000 * sja, uint32_t base);
static void sja_map_pc(SJA1000 * sja, uint32_t base);

/*
 * ------------------------------------------------------
 * Update Interrupts
 *	Every change in the Interrupt Register (IR)
 *	requires a check if the Interrupt signals
 *	to the system controller should be updated
 * -------------------------------------------------------
 */

static void
sja_update_interrupts(SJA1000 * sja)
{
	if (sja->ir) {
		if (!sja->interrupt_posted) {
			SigNode_Set(sja->irqNode, SIG_LOW);
			sja->interrupt_posted = 1;
		}
	} else {
		if (sja->interrupt_posted) {
			SigNode_Set(sja->irqNode, SIG_HIGH);
			sja->interrupt_posted = 0;
		}
	}
}

/*
 * ------------------------------------
 * unmap all SJA1000 registers
 * ------------------------------------
 */
void
SJA1000_UnMap(SJA1000 * sja, uint32_t base)
{
	int i;
	for (i = 0; i < 128; i++) {
		IOH_Delete8(base + i);
	}
}

/*
 * --------------------------------------------------------
 * Map Update
 *	The SJA1000 has 4 different Register Maps 
 *	(Operating Mode/Reset Mode, BasicCAN/PeliCAN)
 *	A change in the mode register will require
 *	that all registers are unmaped and the new
 *	Register Layout is mapped
 * --------------------------------------------------------
 */
void
SJA1000_Map(SJA1000 * sja, uint32_t base)
{
	if (sja->clkdiv & SJA_MODE) {
		/* PeliCAN mode */
		if (sja->mode & SJA_RM) {
			sja->layout = SJA_LOUT_PCR;
			sja_map_pcr(sja, base);
		} else {
			sja->layout = SJA_LOUT_PC;
			sja_map_pc(sja, base);
		}
	} else {
		/* Basic CAN mode */
		if (sja->mode & SJA_RM) {
			sja->layout = SJA_LOUT_BCR;
			sja_map_bcr(sja, base);

		} else {
			sja->layout = SJA_LOUT_BC;
			sja_map_bc(sja, base);
		}
	}
}

static uint32_t
unhandled_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "sja1000 unhandled read of addr %08x rqlen %d\n", address, rqlen);
	return 0;
}

static void
unhandled_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("sja1000 unhandled write of addr %08x, value %08x,rqlen %d\n", address, value,
		  rqlen);
	return;
}

/*
 * ------------------------------------
 * Control Register in Basic CAN Mode
 * ------------------------------------
 */
static uint32_t
sja_bc_cr_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	return sja->mode | (1 << 5);
}

static void
sja_bc_cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint8_t diff = value ^ sja->mode;
	sja->mode = value;
	if (diff & 1) {
		Mem_AreaUpdateMappings(sja->bus);
		if (value & 1) {
			// remove busoff here
			// don't know what is reset in reset mode 
		}
	}
	if (value & (1 << 7)) {
		fprintf(stderr, "SJA1000: BasicCAN CR write sets reserved Bit 7\n");
	}
	return;
}

/*
 * --------------------------------------------
 * CMR Command Register 
 * --------------------------------------------
 */
static uint32_t
sja_cmr_read(void *clientData, uint32_t address, int rqlen)
{
	return 0xff;
}

/*
 * --------------------------------------------
 * Returns true if message is accepted
 * --------------------------------------------
 */
static int
sja_pc_check_acceptance(SJA1000 * sja, const CAN_MSG * msg)
{
	if (sja->mode & SJA_PC_MOD_AFM) {

		/* Single 32 Bit Filter */

		uint32_t mask, code, value;
		mask = (sja->pcr_acm[0] << 24) | (sja->pcr_acm[1] << 16)
		    | (sja->pcr_acm[2] << 8) | sja->pcr_acm[3];
		code = (sja->pcr_acc[0] << 24) | (sja->pcr_acc[1] << 16)
		    | (sja->pcr_acc[2] << 8) | sja->pcr_acc[3];
		if (CAN_MSG_11BIT(msg) && !CAN_MSG_RTR(msg)) {
			value = (CAN_ID(msg) << 21) | (msg->data[0] << 8) | msg->data[1];
			mask |= 0xf << 16;
		} else if (CAN_MSG_11BIT(msg) && CAN_MSG_RTR(msg)) {
			value =
			    (CAN_ID(msg) << 21) | (1 << 20) | (msg->data[0] << 8) | msg->data[1];
			mask |= 0xf << 16;
		} else if (CAN_MSG_29BIT(msg) && !CAN_MSG_RTR(msg)) {
			value = (CAN_ID(msg) << 3);
			mask = mask | 3;
		} else if (CAN_MSG_29BIT(msg) && CAN_MSG_RTR(msg)) {
			value = (CAN_ID(msg) << 3) | (1 << 2);
			mask = mask | 3;
		} else {
			return 0;
		}
		if (((value ^ code) | mask) == 0xffffffff) {
			return 1;
		}
		dbgprintf("v %08x mask %08x c %08x\n", value, mask, code);
		return 0;
	} else {

		/* Two 16 Bit Filters   */

		uint32_t mask1, mask2, code1, code2, value1, value2;
		if (CAN_MSG_11BIT(msg)) {
			mask1 = (sja->pcr_acm[0] << 12) | (sja->pcr_acm[1] << 7);
			mask1 |= 0xfff << 20;
			mask2 = ((sja->pcr_acm[3] & 0xf0) >> 4) | (sja->pcr_acm[2] << 4);
			mask2 |= 0xfffff << 12;
			code1 = (sja->pcr_acc[0] << 12) | (sja->pcr_acc[1] << 7);
			code2 = ((sja->pcr_acc[3] & 0xf0) >> 4) | (sja->pcr_acc[2] << 4);
			value1 = (CAN_ID(msg) << 9) | (msg->data[0]);
			value2 = (CAN_ID(msg) << 1);
		} else if (CAN_MSG_29BIT(msg)) {
			mask1 = (sja->pcr_acm[0] << 8) | sja->pcr_acm[1];
			mask1 |= 0xffff0000;
			mask2 = (sja->pcr_acm[2] << 8) | sja->pcr_acm[3];
			mask2 |= 0xffff0000;
			code1 = (sja->pcr_acc[0] << 8) | sja->pcr_acc[1];
			code2 = (sja->pcr_acc[2] << 8) | sja->pcr_acc[3];
			value1 = CAN_ID(msg) >> 13;
			value2 = CAN_ID(msg) >> 13;
		} else {
			dbgprintf("Unknown message type, not accepted\n");	// maybe we should handle Error Messages
			return 0;
		}
		if (CAN_MSG_11BIT(msg) && CAN_MSG_RTR(msg)) {
			value1 |= (1 << 8);
			value2 |= (1 << 0);
		}
		if (((value1 ^ code1) | mask1) == 0xffffffff) {
			return 1;
		}
		if (((value2 ^ code2) | mask2) == 0xffffffff) {
			return 1;
		}
		dbgprintf("v1 %08x mask1 %08x c1 %08x\n", value1, mask1, code1);
		dbgprintf("v2 %08x mask2 %08x c2 %08x\n", value2, mask2, code2);
		return 0;
	}

}

/*
 * ----------------------------------------------------
 * Handle Incoming messages in PeliCAN mode
 * Write the message to the RX-fifo, increment the 
 * receive count and send an RX-Interrupt if enabled
 * When the fifo is full send a stop to the sender 
 * (here a tcp socket)
 * ---------------------------------------------------------
 *
 */
static void
sja_pelican_receive(SJA1000 * sja, const CAN_MSG * msg)
{
	int i;
	dbgprintf("Received a PeliCAN message\n");
	if (msg->can_dlc > 8) {
		fprintf(stderr, "CAN: Illegal Message length %d, message ignored\n", msg->can_dlc);
		return;
	}
	if (sja_pc_check_acceptance(sja, msg) == 0) {
		fprintf(stderr, "Message not accepted\n");
		return;
	}
	if (CAN_MSG_11BIT(msg) && !CAN_MSG_RTR(msg)) {
		dbgprintf("Typ 11\n");
		INFIFO_PUT(sja, msg->can_dlc | 0);
		INFIFO_PUT(sja, CAN_ID(msg) >> 3);
		INFIFO_PUT(sja, ((CAN_ID(msg) & 7) << 5) | 0);
		for (i = 0; i < msg->can_dlc; i++) {
			INFIFO_PUT(sja, msg->data[i]);
		}
		sja->pc_rxcnt++;
	} else if (CAN_MSG_11BIT(msg) && CAN_MSG_RTR(msg)) {
		dbgprintf("Typ 11 RTR\n");
		INFIFO_PUT(sja, msg->can_dlc | 0x40);
		INFIFO_PUT(sja, CAN_ID(msg) >> 3);
		INFIFO_PUT(sja, ((CAN_ID(msg) & 7) << 5) | 0x10);
		for (i = 0; i < msg->can_dlc; i++) {
			INFIFO_PUT(sja, msg->data[i]);
		}
		sja->pc_rxcnt++;
	} else if (CAN_MSG_29BIT(msg) && !CAN_MSG_RTR(msg)) {
		dbgprintf("Typ 29 \n");
		INFIFO_PUT(sja, msg->can_dlc | 0x80);
		INFIFO_PUT(sja, CAN_ID(msg) >> 21);
		INFIFO_PUT(sja, CAN_ID(msg) >> 13);
		INFIFO_PUT(sja, CAN_ID(msg) >> 5);
		INFIFO_PUT(sja, ((CAN_ID(msg) & 0x1f) << 3));
		for (i = 0; i < msg->can_dlc; i++) {
			INFIFO_PUT(sja, msg->data[i]);
		}
		sja->pc_rxcnt++;
	} else if (CAN_MSG_29BIT(msg) && CAN_MSG_RTR(msg)) {
		dbgprintf("Typ 29 RTR\n");
		INFIFO_PUT(sja, msg->can_dlc | 0xc0);
		INFIFO_PUT(sja, CAN_ID(msg) >> 21);
		INFIFO_PUT(sja, CAN_ID(msg) >> 13);
		INFIFO_PUT(sja, CAN_ID(msg) >> 5);
		INFIFO_PUT(sja, ((CAN_ID(msg) & 0x1f) << 3) | 0x4);
		for (i = 0; i < msg->can_dlc; i++) {
			INFIFO_PUT(sja, msg->data[i]);
		}
		sja->pc_rxcnt++;
	} else {
		fprintf(stderr, "SJA1000: CAN Message Type not implemented\n");
		return;
	}
	// Trigger some RX-signal
	if (INFIFO_ROOM(sja) < 13) {
		CanStopRx(sja->interface);
	}
	sja->sr |= SJA_SR_RBS;
	if (IS_PELICAN(sja)) {
		if (sja->pc_ier & SJA_PC_IR_RI) {
			sja->ir |= SJA_PC_IR_RI;
		}
	} else {
		if (sja->mode & SJA_BC_CR_RIE) {
			sja->ir |= SJA_BC_IR_RI;
		}
	}
	sja_update_interrupts(sja);
	dbgprintf("SJA1000 got a new CAN-message\n");
}

/*
 * ----------------------------------------------------
 * Assemple a CAN message from the tx-buffer
 * ----------------------------------------------------
 */
void
sja_assemble_tx_message(SJA1000 * sja, CAN_MSG * msg)
{
	int i;
	msg->can_dlc = sja->txbuf[0] & 0xf;
	if (msg->can_dlc > 8) {
		fprintf(stderr, "Warning, CAN message to big: %d bytes\n", msg->can_dlc);
		msg->can_dlc = 8;
	}
	if ((sja->txbuf[0] & 0xc0) == 0) {
		CAN_MSG_T_11(msg);
		CAN_SET_ID(msg, (sja->txbuf[1] << 3) | (sja->txbuf[2] >> 5));
		for (i = 0; i < msg->can_dlc; i++) {
			msg->data[i] = sja->txbuf[3 + i];
		}
	} else if ((sja->txbuf[0] & 0xc0) == 0x40) {
		CAN_MSG_T_11_RTR(msg);
		CAN_SET_ID(msg, (sja->txbuf[1]) << 3 | (sja->txbuf[2] >> 5));
		for (i = 0; i < msg->can_dlc; i++) {
			msg->data[i] = sja->txbuf[3 + i];
		}
	} else if ((sja->txbuf[0] & 0xc0) == 0x80) {
		CAN_MSG_T_29(msg);
		CAN_SET_ID(msg, (sja->txbuf[1] << 21) | (sja->txbuf[2] << 13)
			   | (sja->txbuf[2] << 5) | (sja->txbuf[3] >> 3));
		for (i = 0; i < msg->can_dlc; i++) {
			msg->data[i] = sja->txbuf[5 + i];
		}
	} else {
		CAN_MSG_T_29_RTR(msg);
		CAN_SET_ID(msg, (sja->txbuf[1] << 21) | (sja->txbuf[2] << 13)
			   | (sja->txbuf[2] << 5) | (sja->txbuf[3] >> 3));
		for (i = 0; i < msg->can_dlc; i++) {
			msg->data[i] = sja->txbuf[5 + i];
		}
	}

}

/*
 * ------------------------------------------------------------
 * Command Register:
 * 	Bitfield where every set bit triggers a command	
 * ------------------------------------------------------------
 */
static void
sja_pc_cmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	/*
	 * ----------------------
	 * Start a transmit
	 * ----------------------
	 */
	if ((value & SJA_PC_CMR_TR) && !(sja->mode & SJA_PC_MOD_LOM)) {
		CAN_MSG msg;
		sja_assemble_tx_message(sja, &msg);
		CanSend(sja->interface, &msg);
		sja->ir |= SJA_PC_IR_TI & sja->pc_ier;
		sja_update_interrupts(sja);
		dbgprintf("SJA1000: Transmit request\n");
	} else if ((value & SJA_PC_CMR_SRR) && !(sja->mode & SJA_PC_MOD_LOM)) {
		/* Setting SRR and TR simultanously ignores SRR */
		CAN_MSG msg;
		dbgprintf("Self reception request\n");
		sja_assemble_tx_message(sja, &msg);
		CanSend(sja->interface, &msg);
		sja->ir |= SJA_PC_IR_TI & sja->pc_ier;
		sja_pelican_receive(sja, &msg);
		sja_update_interrupts(sja);
	}
	if (value & SJA_PC_CMR_AT) {
		// Ignore
	}
	/*
	 * ----------------------------------------------------------------
	 * Release receive Buffer:
	 *      Forward the fifo readpointer by length of first message
	 *      update interrupts and allow incomming data from socket 
	 * ----------------------------------------------------------------
	 */
	if (value & SJA_PC_CMR_RRB) {
		if (!sja->sr & SJA_SR_RBS) {
			dbgprintf("SJA1000: Warning, try to release empty receive buffer\n");
			return;
		}
		if (IS_PELICAN(sja)) {
			int ff = INFIFO_BYTE(sja, 0) & 0x80;
			unsigned int len = (INFIFO_BYTE(sja, 0) & 0xf) + 3;
			if (ff) {
				len += 2;
			}
			if (INFIFO_COUNT(sja) < len) {
				sja->infifo_rp = sja->infifo_wp = 0;
				sja->pc_rxcnt = 0;
				fprintf(stderr, "SJA: Input fifo corrupted, resetted\n");
			} else {
				sja->infifo_rp = (sja->infifo_rp + len) & INFIFO_MASK;
				sja->pc_rxcnt--;
			}
		} else {
			unsigned int len = (INFIFO_BYTE(sja, 1) & 0xf) + 2;
			if (INFIFO_COUNT(sja) < len) {
				sja->infifo_rp = sja->infifo_wp = 0;
				fprintf(stderr, "SJA: Input fifo corrupted, resetted\n");
			} else {
				sja->infifo_rp = (sja->infifo_rp + len) & INFIFO_MASK;
			}
		}
		if (INFIFO_COUNT(sja) == 0) {
			sja->sr &= ~SJA_SR_RBS;
			sja->ir &= ~SJA_PC_IR_RI;
			sja_update_interrupts(sja);
		}
		if (INFIFO_ROOM(sja) > 13) {
			CanStartRx(sja->interface);
		}

	}
	/*
	 * ------------------------------
	 * clear data overrun status
	 * ------------------------------
	 */
	if (value & SJA_PC_CMR_CDO) {
		sja->sr &= ~SJA_SR_DOS;
	}
	/*
	 * ------------------------------------------------
	 * Start a transmit with self reception
	 * ------------------------------------------------
	 */
	return;
}

/*
 * ------------------------------------
 * Mode Register in PeliCAN Mode
 * ------------------------------------
 */
static uint32_t
sja_pc_mod_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	return sja->mode;
}

static void
sja_pc_mod_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint8_t diff = value ^ sja->mode;
	sja->mode = value;
	if (diff & 1) {
		Mem_AreaUpdateMappings(sja->bus);
	}
	return;
}

/*
 * ------------------------------------
 * Status Register all Modes;
 * ------------------------------------
 */
static uint32_t
sja_sr_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	return sja->sr | SJA_SR_TBS | SJA_SR_TCS | SJA_SR_RS | SJA_SR_TS;
}

/*
 * ------------------------------------
 * Interrupt register  Basic CAN 
 * ------------------------------------
 */
static uint32_t
sja_bc_ir_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	return sja->ir | 0xe;
}

/*
 * ------------------------------------
 * Interrupt register PeliCAN 
 * ------------------------------------
 */
static uint32_t
sja_pc_ir_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t data;
	if ((sja->sr & sja->pc_ier) & 1) {
		sja->ir |= 1;
	}
	data = sja->ir;
	sja->ir = sja->ir & SJA_PC_IR_RI;
	sja_update_interrupts(sja);
	return data;
}

/*
 * ------------------------------------
 * PC/PCR Interrupt enable registers
 * ------------------------------------
 */
static uint32_t
sja_pc_ier_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	return sja->pc_ier;
}

static void
sja_pc_ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint8_t diff = value ^ sja->pc_ier;
	sja->pc_ier = value;
	if ((sja->sr & sja->pc_ier) & 1) {
		sja->ir |= 1;
	}
	if (diff) {
		sja_update_interrupts(sja);
	}
	return;
}

/*
 * -----------------------------------------------------------
 *  BTR Bit Timing Registers, only writable in reset modes
 * -----------------------------------------------------------
 */
static uint32_t
sja_btr0_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	return sja->btr0;
}

static void
sja_btr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	sja->btr0 = value;
	return;
}

static uint32_t
sja_btr1_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	return sja->btr1;
}

static void
sja_btr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	sja->btr1 = value;
	return;
}

/*
 * -----------------------------------------
 * OC: All modes Output Control Register
 * -----------------------------------------
 */
static uint32_t
sja_oc_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t l = sja->layout;
	if ((l == SJA_LOUT_PCR) || (l = SJA_LOUT_BCR) || (l = SJA_LOUT_PC)) {
		return sja->oc;
	} else {
		fprintf(stderr, "SJA1000 Warning: Read of OC in Basic Can Reset Mode\n");
		return 0xff;
	}
}

static void
sja_oc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	if ((sja->layout == SJA_LOUT_PCR) || (sja->layout == SJA_LOUT_BCR)) {
		sja->oc = value;
	} else {
		fprintf(stderr, "SJA1000 ignore OC write in mode %08x\n", sja->layout);
	}
	return;
}

/*
 * ------------------------------------
 * PCR ACM Acceptance mask 
 * ------------------------------------
 */
static uint32_t
sja_pcr_acm_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_PCR_ACM0);
	if (idx < 4) {
		return sja->pcr_acm[idx];
	} else {
		fprintf(stderr, "Emulator Bug: illegal index %d\n", idx);
		return 0;
	}
}

static void
sja_pcr_acm_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_PCR_ACM0);
	if (idx < 4) {
		dbgprintf("setze acm %d to %08x\n", idx, value);
		sja->pcr_acm[idx] = value;
	} else {
		fprintf(stderr, "illegal index %d\n", idx);
	}
	return;
}

/*
 * ------------------------------------
 * PCR ACC Acceptance code
 * ------------------------------------
 */
static uint32_t
sja_pcr_acc_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_PCR_ACC0);
	if (idx < 4) {
		return sja->pcr_acc[idx];
	} else {
		fprintf(stderr, "illegal index %d\n", idx);
		return 0;
	}
}

static void
sja_pcr_acc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_PCR_ACC0);
	if (idx < 4) {
		dbgprintf("setze acc %d to %08x\n", idx, value);
		sja->pcr_acc[idx] = value;
	} else {
		fprintf(stderr, "illegal index %d\n", idx);
	}
	return;
}

/*
 * -------------------------------------
 * PeliCAN Read from 13 Byte RX-Window 
 * -------------------------------------
 */
static uint32_t
sja_pc_rx_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_PC_RXSFF);
	if (idx > 12) {
		fprintf(stderr, "Illegal index %d\n", idx);
		return -1;
	}
	return INFIFO_BYTE(sja, idx);
}

/*
 * -------------------------------------
 * PeliCAN Write to  txbuf 
 * -------------------------------------
 */
static void
sja_pc_tx_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_PC_TXSFF);
	if (idx > 12) {
		fprintf(stderr, "Illegal index %d\n", idx);
		return;
	}
	sja->txbuf[idx] = value;
	return;
}

/*
 * -----------------------------------------------
 * BasicCAN read from 10 Byte RX-Window
 * -----------------------------------------------
 */

static uint32_t
sja_bc_rx_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_BC_RXB);
	int fifo_idx;
	if (idx > 9) {
		fprintf(stderr, "Illegal index %d\n", idx);
		return 0;
	}
	fifo_idx = (sja->infifo_rp + idx) & INFIFO_MASK;
	return sja->infifo[fifo_idx];
}

static void
sja_bc_rx_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_BC_RXB);
	int fifo_idx;
	if (idx > 9) {
		fprintf(stderr, "Illegal index %d\n", idx);
		return;
	}
	fifo_idx = (sja->infifo_rp + idx) & INFIFO_MASK;
	sja->infifo[fifo_idx] = value;
	return;
}

static uint32_t
sja_bc_tx_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_BC_TXB);
	if (idx > 9) {
		fprintf(stderr, "Illegal index %d\n", idx);
		return 0;
	}
	return sja->txbuf[idx];
}

static uint32_t
sja_bcr_tx_read(void *clientData, uint32_t address, int rqlen)
{
	return 0xff;
}

static void
sja_bc_tx_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_BC_TXB);
	if (idx > 9) {
		fprintf(stderr, "Illegal index %d\n", idx);
		return;
	}
	sja->txbuf[idx] = value;
	return;
}

/*
 * -------------------------------------------------
 * EWL
 * 	Set/Get Error Warning Limit 
 * -------------------------------------------------
 */
static uint32_t
sja_pc_ewl_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	return sja->pc_ewl;
}

static void
sja_pcr_ewl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	sja->pc_ewl = value;
	return;
}

/*
 * -------------------------------------------------
 * Number of messages in fifo
 * -------------------------------------------------
 */
static uint32_t
sja_pc_rxcnt_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	return sja->pc_rxcnt & 0x1f;
}

/*
 * -------------------------------------------------
 * Access to the receive Fifo pointer (Receive
 * Buffer start address)
 * -------------------------------------------------
 */
static uint32_t
sja_pc_rbsa_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	return sja->infifo_rp & 0x3f;
}

static void
sja_pcr_rbsa_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	sja->infifo_rp = value;
	return;
}

/* 
 * ----------------------------------------------
 * 
 * ----------------------------------------------
 */
static uint32_t
sja_clkdiv_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	dbgprintf("SJA1000 clkdiv read 0x%02x\n", sja->clkdiv);
	return sja->clkdiv;
}

static void
sja_clkdiv_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint8_t diff = sja->clkdiv ^ value;
	sja->clkdiv = value;
	if (diff & SJA_MODE) {
		Mem_AreaUpdateMappings(sja->bus);
	}
	if (sja->layout != SJA_LOUT_PCR && sja->layout != SJA_LOUT_BCR) {
		fprintf(stderr, "SJA1000: Warning, CLKDIV write in  mem layout %08x\n",
			sja->layout);
	}
	dbgprintf("SJA1000 clkdiv write 0x%02x\n", value);
	return;
}

/*
 * -----------------------------------------------------------------
 * Raw access to internal RAM (RX-Fifo +TX-Buffer+Unused) 
 * -----------------------------------------------------------------
 */
static uint32_t
sja_pc_canram_read(void *clientData, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_PC_CANRAM);
	if (idx >= CANRAM_SIZE) {
		fprintf(stderr, "Illegal canram index %d\n", idx);
		return -1;
	}
	return sja->canram[idx];
}

static void
sja_pc_canram_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SJA1000 *sja = clientData;
	uint32_t idx = (address & 0x7f) - SJA_REG(SJA_PC_CANRAM);
	if (idx >= CANRAM_SIZE) {
		fprintf(stderr, "Illegal canram index %d\n", idx);
		return;
	}
	sja->canram[idx] = value;
	return;
}

/*
 * --------------------------------------------------
 * For registers which are fixed to 0
 * --------------------------------------------------
 */
static uint32_t
zero_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
ignore_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	return;
}

/*
 * -----------------------------------------------
 * Map the registers for Basic CAN reset mode
 * -----------------------------------------------
 */
static void
sja_map_bcr(SJA1000 * sja, uint32_t base)
{
	int i;
	IOH_New8(base + SJA_BCR_CR, sja_bc_cr_read, sja_bc_cr_write, sja);
	IOH_New8(base + SJA_BCR_CMR, sja_cmr_read, unhandled_write, sja);
	IOH_New8(base + SJA_BCR_SR, sja_sr_read, unhandled_write, sja);
	IOH_New8(base + SJA_BCR_IR, sja_bc_ir_read, unhandled_write, sja);
	IOH_New8(base + SJA_BCR_ACC, unhandled_read, unhandled_write, sja);
	IOH_New8(base + SJA_BCR_ACM, unhandled_read, unhandled_write, sja);
	IOH_New8(base + SJA_BCR_BTR0, sja_btr0_read, sja_btr0_write, sja);
	IOH_New8(base + SJA_BCR_BTR1, sja_btr1_read, sja_btr1_write, sja);
	IOH_New8(base + SJA_BCR_OC, sja_oc_read, sja_oc_write, sja);
	IOH_New8(base + SJA_BCR_TST, unhandled_read, unhandled_write, sja);
	IOH_New8(base + SJA_BCR_CLKDIV, sja_clkdiv_read, sja_clkdiv_write, sja);
	for (i = 0; i < 10; i++) {
		IOH_New8(base + SJA_BCR_RXB + i, sja_bc_rx_read, sja_bc_rx_write, sja);
		IOH_New8(base + SJA_BCR_TXB + i, sja_bcr_tx_read, unhandled_write, sja);
	}
}

/*
 * -----------------------------------------------
 * Map the registers for Basic CAN  mode
 * -----------------------------------------------
 */

static void
sja_map_bc(SJA1000 * sja, uint32_t base)
{
	int i;
	IOH_New8(base + SJA_BC_CR, sja_bc_cr_read, sja_bc_cr_write, sja);
	IOH_New8(base + SJA_BC_CMR, sja_cmr_read, unhandled_write, sja);
	IOH_New8(base + SJA_BC_SR, sja_sr_read, unhandled_write, sja);
	IOH_New8(base + SJA_BC_IR, sja_bc_ir_read, unhandled_write, sja);
	IOH_New8(base + SJA_BC_BTR0, sja_btr0_read, unhandled_write, sja);
	IOH_New8(base + SJA_BC_BTR1, sja_btr1_read, unhandled_write, sja);
	IOH_New8(base + SJA_BC_OC, sja_oc_read, sja_oc_write, sja);
	IOH_New8(base + SJA_BC_CLKDIV, sja_clkdiv_read, sja_clkdiv_write, sja);
	for (i = 0; i < 10; i++) {
		IOH_New8(base + SJA_BC_RXB + i, sja_bc_rx_read, sja_bc_rx_write, sja);
		IOH_New8(base + SJA_BC_TXB + i, sja_bc_tx_read, sja_bc_tx_write, sja);
	}
}

/*
 * -----------------------------------------------
 * Map the registers for PeliCAN  reset mode
 * -----------------------------------------------
 */

static void
sja_map_pcr(SJA1000 * sja, uint32_t base)
{
	int i;
	IOH_New8(base + SJA_PCR_MOD, sja_pc_mod_read, sja_pc_mod_write, sja);
	IOH_New8(base + SJA_PCR_CMR, sja_cmr_read, sja_pc_cmr_write, sja);
	IOH_New8(base + SJA_PCR_SR, sja_sr_read, unhandled_write, sja);
	IOH_New8(base + SJA_PCR_IR, sja_pc_ir_read, unhandled_write, sja);
	IOH_New8(base + SJA_PCR_IER, sja_pc_ier_read, sja_pc_ier_write, sja);
	IOH_New8(base + SJA_PCR_OC, sja_oc_read, sja_oc_write, sja);
	IOH_New8(base + SJA_PCR_BTR0, sja_btr0_read, sja_btr0_write, sja);
	IOH_New8(base + SJA_PCR_BTR1, sja_btr1_read, sja_btr1_write, sja);
	IOH_New8(base + SJA_PCR_ACC0, sja_pcr_acc_read, sja_pcr_acc_write, sja);
	IOH_New8(base + SJA_PCR_ACC1, sja_pcr_acc_read, sja_pcr_acc_write, sja);
	IOH_New8(base + SJA_PCR_ACC2, sja_pcr_acc_read, sja_pcr_acc_write, sja);
	IOH_New8(base + SJA_PCR_ACC3, sja_pcr_acc_read, sja_pcr_acc_write, sja);
	IOH_New8(base + SJA_PCR_ACM0, sja_pcr_acm_read, sja_pcr_acm_write, sja);
	IOH_New8(base + SJA_PCR_ACM1, sja_pcr_acm_read, sja_pcr_acm_write, sja);
	IOH_New8(base + SJA_PCR_ACM2, sja_pcr_acm_read, sja_pcr_acm_write, sja);
	IOH_New8(base + SJA_PCR_ACM3, sja_pcr_acm_read, sja_pcr_acm_write, sja);
	IOH_New8(base + SJA_PCR_EWL, sja_pc_ewl_read, sja_pcr_ewl_write, sja);
	IOH_New8(base + SJA_PCR_RXCNT, sja_pc_rxcnt_read, unhandled_write, sja);
	IOH_New8(base + SJA_PCR_RBSA, sja_pc_rbsa_read, sja_pcr_rbsa_write, sja);
	IOH_New8(base + SJA_PCR_CLKDIV, sja_clkdiv_read, sja_clkdiv_write, sja);
	for (i = 0; i < CANRAM_SIZE; i++) {
		IOH_New8(base + SJA_PCR_CANRAM + i, sja_pc_canram_read, sja_pc_canram_write, sja);
	}
	for (i = 112; i < 128; i++) {
		IOH_New8(base + i, zero_read, ignore_write, sja);
	}
}

/*
 * -----------------------------------------------
 * Map the registers for PeliCAN  mode
 * -----------------------------------------------
 */
static void
sja_map_pc(SJA1000 * sja, uint32_t base)
{
	int i;
	IOH_New8(base + SJA_PC_MOD, sja_pc_mod_read, sja_pc_mod_write, sja);
	IOH_New8(base + SJA_PC_CMR, sja_cmr_read, sja_pc_cmr_write, sja);
	IOH_New8(base + SJA_PC_SR, sja_sr_read, unhandled_write, sja);
	IOH_New8(base + SJA_PC_IR, sja_pc_ir_read, unhandled_write, sja);
	IOH_New8(base + SJA_PC_IER, sja_pc_ier_read, sja_pc_ier_write, sja);
	IOH_New8(base + SJA_PC_BTR0, sja_btr0_read, unhandled_write, sja);
	IOH_New8(base + SJA_PC_BTR1, sja_btr1_read, unhandled_write, sja);
	IOH_New8(base + SJA_PC_OC, sja_oc_read, sja_oc_write, sja);
	IOH_New8(base + SJA_PC_EWL, sja_pc_ewl_read, unhandled_write, sja);
	for (i = 0; i < 13; i++) {
		IOH_New8(base + SJA_PC_RXSFF + i, sja_pc_rx_read, sja_pc_tx_write, sja);
	}
	IOH_New8(base + SJA_PC_RXCNT, sja_pc_rxcnt_read, unhandled_write, sja);
	IOH_New8(base + SJA_PC_RBSA, sja_pc_rbsa_read, unhandled_write, sja);
	IOH_New8(base + SJA_PC_CLKDIV, sja_clkdiv_read, sja_clkdiv_write, sja);

	for (i = 0; i < CANRAM_SIZE; i++) {
		IOH_New8(base + SJA_PC_CANRAM + i, sja_pc_canram_read, ignore_write, sja);
	}
}

static void
sja_basic_receive(SJA1000 * sja, const CAN_MSG * msg)
{
	int i;
	fprintf(stderr, "SJA1000 got a new BasicCAN-message\n");
	if (msg->can_dlc > 8) {
		fprintf(stderr, "CAN: Illegal Message length %d, message ignored\n", msg->can_dlc);
		return;
	}
	if (CAN_MSG_11BIT(msg) && !CAN_MSG_RTR(msg)) {
		INFIFO_PUT(sja, CAN_ID(msg) >> 3);
		INFIFO_PUT(sja, ((CAN_ID(msg) & 7) << 5) | msg->can_dlc);
		for (i = 0; i < msg->can_dlc; i++) {
			INFIFO_PUT(sja, msg->data[i]);
		}
	} else if (CAN_MSG_11BIT(msg) && CAN_MSG_RTR(msg)) {
		INFIFO_PUT(sja, CAN_ID(msg) >> 3);
		INFIFO_PUT(sja, ((CAN_ID(msg) & 7) << 5) | 0x10 | msg->can_dlc);
		for (i = 0; i < msg->can_dlc; i++) {
			INFIFO_PUT(sja, msg->data[i]);
		}
	} else {
		fprintf(stderr, "SJA1000: CAN Message Type not implemented\n");
		return;
	}
	// Trigger some RX-signal

}

static void
sja_receive(void *clientData, const CAN_MSG * msg)
{
	SJA1000 *sja = clientData;
	if (sja->layout == SJA_LOUT_PC) {
		if ((sja->mode & 7) == 0) {
			sja_pelican_receive(sja, msg);
		}
	} else if (sja->layout == SJA_LOUT_BC) {
		sja_basic_receive(sja, msg);
	} else {
		dbgprintf("received was in lout %08x\n", sja->layout);
	}
}

static CanChipOperations sja_iface_ops = {
	.receive = sja_receive
};

SJA1000 *
SJA1000_New(BusDevice * bus, const char *name)
{
	SJA1000 *sja;
	sja = sg_new(SJA1000);
	sja->irqNode = SigNode_New("%s.irq", name);
	if (!sja->irqNode) {
		exit(3242);
	}
	SigNode_Set(sja->irqNode, SIG_HIGH);
	sja->layout = SJA_LOUT_BCR;
	sja->mode = 1;		// reset_mode
	sja->pc_ewl = 96;
	sja->txbuf = sja->canram + 64;
	sja->infifo = sja->canram;
	sja->bus = bus;

	/* Start with BCR reset mode mapping */
	Mem_AreaUpdateMappings(sja->bus);
	sja->interface = CanSocketInterface_New(&sja_iface_ops, name, sja);
	return sja;
}
