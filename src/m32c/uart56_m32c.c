/*
 *****************************************************************************************************
 * Emulation of Renesas M32C87 Uart 5 + 6 
 *
 * state: working in async mode 
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 *****************************************************************************************************
 */

#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include "sgstring.h"
#include "serial.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "uart56_m32c.h"
#include "fio.h"
#include "unistd.h"

#define UiMR_SMD_MSK	(7)
#define 	UiMR_SMD_DISA	(0)
#define		UiMR_SMD_SSI	(1)
#define		UiMR_SMD_I2C	(2)
#define		UiMR_SMD_UART7	(4)
#define		UiMR_SMD_UART8	(5)
#define		UiMR_SMD_UART9	(6)
#define		UiMR_CKDIR	(1 << 3)
#define		UiMR_STPS	(1 << 4)
#define		UiMR_PRY	(1 << 5)
#define		UiMR_PRYE	(1 << 6)

#define UiC0_CLK_MSK	(3)
#define 	UiC0_CLK_F1	(0)
#define		UiC0_CLK_F8	(1)
#define		UiC0_CLK_F2N	(2)
#define		UiC0_FLK_UNDEF	(3)
#define	UiC0_CRS	(1 << 2)
#define UiC0_TXEPT	(1 << 3)
#define UiC0_CRD	(1 << 4)
#define UiC0_CKPOL	(1 << 6)
#define UiC0_UFORM	(1 << 7)

#define UiC1_TE		(1 << 0)
#define UiC1_TI		(1 << 1)
#define UiC1_RE		(1 << 2)
#define UiC1_RI		(1 << 3)

#define	UiRB_OER	(1 << 12)
#define UiRB_FER	(1 << 13)
#define UiRB_PER	(1 << 14)
#define UiRB_SUM	(1 << 15)

/*
 *************************************************************************************
 * Now the common Registers (Partially used by Uart5 and partially by Uart6)
 *************************************************************************************
 */
#define REG_U56IS	(0x1D1)
#define		U56IS_U5CLK	(1 << 0)
#define		U56IS_U5RXD	(1 << 1)
#define		U56IS_U5CTS	(1 << 2)
#define		U56IS_U6CLK	(1 << 4)
#define		U56IS_U6RXD	(1 << 5)
#define		U56IS_U6CTS	(1 << 6)
#define REG_U56CON	(0x1D0)
#define		U56CON_U5IRS	(1 << 0)
#define		U56CON_U6IRS	(1 << 1)
#define		U56CON_U5RRM	(1 << 2)
#define		U56CON_U6RRM	(1 << 3)

typedef struct UartAddrs {
	uint32_t aUiMR;
	uint32_t aUiC0;
	uint32_t aUiBRG;
	uint32_t aUiC1;
	uint32_t aUiTB;
	uint32_t aUiRB;
} UartAddrs;

UartAddrs uart_addrs[] = {
	{
	 .aUiMR = 0x1C0,
	 .aUiC0 = 0x1C4,
	 .aUiBRG = 0x1C1,
	 .aUiC1 = 0x1C5,
	 .aUiTB = 0x1C2,
	 .aUiRB = 0x1C6,
	 },
	{
	 .aUiMR = 0x1C8,
	 .aUiC0 = 0x1CC,
	 .aUiBRG = 0x1C9,
	 .aUiC1 = 0x1CD,
	 .aUiTB = 0x1CA,
	 .aUiRB = 0x1CE,
	 }
};

#define TXFIFO_SIZE 2
#define TXFIFO_LVL(mua) ((mua)->txfifo_wp - (mua)->txfifo_rp)
#define TXFIFO_WP(mua) ((mua)->txfifo_wp % TXFIFO_SIZE)
#define TXFIFO_RP(mua) ((mua)->txfifo_rp % TXFIFO_SIZE)

typedef struct M32C_Uart {
	BusDevice bdev;
	char *name;
	unsigned int register_set;
	SigNode *rxIrq;
	SigNode *txIrq;
	UartPort *backend;
	CycleTimer tx_baud_timer;
	CycleTimer rx_baud_timer;
	CycleCounter_t byte_time;
	Clock_t *clk_f1;
	Clock_t *clk_f8;
	Clock_t *clk_f2n;
	Clock_t *clk_sync;
	Clock_t *clk_async;

	UartChar txfifo[TXFIFO_SIZE];
	uint64_t txfifo_wp;
	uint64_t txfifo_rp;

	uint16_t tx_shift_reg;
	uint8_t reg_uimr;
	uint8_t reg_uic0;
	uint8_t reg_uibrg;
	uint8_t reg_uic1;
	uint16_t reg_uitb;
	uint16_t reg_uirb;
	uint8_t msk_uirs;
	uint8_t msk_urrm;
	uint8_t msk_uirxd;
	uint8_t msk_uiclk;
	uint8_t msk_uicts;
} M32C_Uart;

static M32C_Uart *g_Uart[2] = { NULL, NULL };

static uint8_t reg_u56is;
static uint8_t reg_u56con;

static void
post_tx_interrupt(M32C_Uart * mua)
{
	//fprintf(stderr,"Posting TX interrupt\n");
	//fprintf(stderr,"Tx %llu\n",CycleCounter_Get());
	SigNode_Set(mua->txIrq, SIG_LOW);
	SigNode_Set(mua->txIrq, SIG_HIGH);
}

static void
post_rx_interrupt(M32C_Uart * mua)
{
	//fprintf(stderr,"Post rx interrupt\n");
	SigNode_Set(mua->rxIrq, SIG_LOW);
	SigNode_Set(mua->rxIrq, SIG_HIGH);
}

static void
update_clock(M32C_Uart * mua)
{
	uint8_t brg = mua->reg_uibrg;
	uint8_t uic0 = mua->reg_uic0;
	int divider = 2 * (brg + 1);
	UartCmd uartCmd;
	Clock_t *clk_src;
	Clock_Decouple(mua->clk_sync);
	Clock_Decouple(mua->clk_async);
	switch (uic0 & UiC0_CLK_MSK) {
	    case UiC0_CLK_F1:
		    clk_src = mua->clk_f1;
		    break;
	    case UiC0_CLK_F8:
		    clk_src = mua->clk_f8;
		    break;
	    case UiC0_CLK_F2N:
		    clk_src = mua->clk_f2n;
		    break;
	    default:
		    fprintf(stderr, "Illegal clock value in UART\n");
		    return;
	}
	Clock_MakeDerived(mua->clk_sync, clk_src, 1, divider);
	Clock_MakeDerived(mua->clk_async, clk_src, 1, 8 * divider);
	uartCmd.opcode = UART_OPC_SET_BAUDRATE;
	uartCmd.arg = Clock_Freq(mua->clk_async);
	SerialDevice_Cmd(mua->backend, &uartCmd);
}

static void
update_uart_config(M32C_Uart * mua)
{
	UartCmd cmd;
	uint8_t mode = mua->reg_uimr & UiMR_SMD_MSK;
	cmd.opcode = UART_OPC_SET_CSIZE;
	if (mode == UiMR_SMD_UART7) {
		cmd.arg = 7;
		SerialDevice_Cmd(mua->backend, &cmd);
	} else if (mode == UiMR_SMD_UART8) {
		cmd.arg = 8;
		SerialDevice_Cmd(mua->backend, &cmd);
	} else if (mode == UiMR_SMD_UART9) {
		cmd.arg = 9;
		SerialDevice_Cmd(mua->backend, &cmd);
	}
}

static uint32_t
uimr_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	return mua->reg_uimr;
}

static void
uimr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	uint8_t mode = value & UiMR_SMD_MSK;
	uint8_t oldmode = mua->reg_uimr & UiMR_SMD_MSK;
	mua->reg_uimr = value;
	if (oldmode != mode) {
		update_uart_config(mua);
	}
}

static uint32_t
uic0_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	return mua->reg_uic0;
}

static void
uic0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	M32C_Uart *mua = (M32C_Uart *) clientData;
	mua->reg_uic0 = (value & ~UiC0_TXEPT) | (mua->reg_uic0 & UiC0_TXEPT);
	update_clock(mua);
}

static uint32_t
uibrg_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	return mua->reg_uibrg;
}

static void
uibrg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	mua->reg_uibrg = value;
	update_clock(mua);
}

static uint32_t
uic1_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	return mua->reg_uic1;
}

static void
uic1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	mua->reg_uic1 = (value & ~UiC1_TI) | (mua->reg_uic1 & UiC1_TI);
	if (mua->reg_uic1 & UiC1_RE) {
		SerialDevice_StartRx(mua->backend);
	}

}

static uint32_t
uitb_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
uitb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	mua->reg_uitb = value;
	if (value > 0x1ff) {
		fprintf(stderr, "Warning: UITB %s write 0x%04x\n", mua->name, value);
	}
	if (!(mua->reg_uic1 & UiC1_TE)) {
		return;
	}
	if (mua->reg_uic0 & UiC0_TXEPT) {
		mua->tx_shift_reg = value;
		mua->reg_uic0 &= ~UiC0_TXEPT;
		CycleTimer_Mod(&mua->tx_baud_timer, mua->byte_time);
		if (!(reg_u56con & mua->msk_uirs)) {
			post_tx_interrupt(mua);
		}
	} else {
		mua->reg_uic1 &= ~UiC1_TI;
	}
}

static uint32_t
uirb_read(void *clientData, uint32_t address, int rqlen)
{
	uint16_t uirb;
	M32C_Uart *mua = (M32C_Uart *) clientData;
	uirb = mua->reg_uirb;
	mua->reg_uic1 &= ~UiC1_RI;
	//update_rx_interrupt(mua);
	return uirb;
}

static void
uirb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
u56con_read(void *clientData, uint32_t address, int rqlen)
{
	return reg_u56con;
}

static void
u56con_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	reg_u56con = value;
}

static uint32_t
u56is_read(void *clientData, uint32_t address, int rqlen)
{
	return reg_u56is;
}

static void
u56is_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	reg_u56is = value;
}

static void
rx_next(void *clientData)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	if (mua->reg_uic1 & UiC1_RE) {
		SerialDevice_StartRx(mua->backend);
	}
}

static uint32_t
testreg_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
testreg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	static FILE *file = NULL;
	uint8_t c;
	if(isprint(value) || isspace(value)) {
		fprintf(stdout,"%c",value);
	} else {
		fprintf(stdout,"\\x%02x",value);
	}
#if 0
	if(file == NULL) {
		file = fopen("testlog.dump","w+");
		if(!file) {
			return;
		}
	}
	c = value;
	fwrite(&value,1,4,file);
	fflush(file);
#endif
}
static void
tx_done(void *clientData)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	/* Force the emulator not to be faster than the output channel */
	if (TXFIFO_LVL(mua) == TXFIFO_SIZE) {
		CycleTimer_Mod(&mua->tx_baud_timer, 0);
		return;
	}
	mua->txfifo[TXFIFO_WP(mua)] = mua->tx_shift_reg;
	mua->txfifo_wp++;
	if (TXFIFO_LVL(mua) == 1) {
		SerialDevice_StartTx(mua->backend);
	}
	/* Feed the byte back in Syncmode for now */
	if ((mua->reg_uimr & UiMR_SMD_MSK) == UiMR_SMD_SSI) {
		mua->reg_uirb = mua->tx_shift_reg;
		if (!(mua->reg_uic1 & UiC1_RI)) {
			mua->reg_uic1 |= UiC1_RI;
			post_rx_interrupt(mua);
		}
	}
	if (mua->reg_uic1 & UiC1_TI) {
		/* Nothing in UiTB -> Transmitter is now empty */
		mua->reg_uic0 |= UiC0_TXEPT;
		//fprintf(stderr,"Stop Tx\n");
		//SerialDevice_StopTx(mua->backend);
		if (reg_u56con & mua->msk_uirs) {
			post_tx_interrupt(mua);
		}
	} else {
		/* Move the uitb to the shift register */
		/* and indicate uitb empty */
		mua->tx_shift_reg = mua->reg_uitb;
		mua->reg_uic1 |= UiC1_TI;
		if (!(reg_u56con & mua->msk_uirs)) {
			post_tx_interrupt(mua);
		}
		CycleTimer_Mod(&mua->tx_baud_timer, mua->byte_time);
	}
	return;
}

static bool
serial_output(void *cd, UartChar * c)
{
	M32C_Uart *mua = cd;
	if (mua->txfifo_rp != mua->txfifo_wp) {
		*c = mua->txfifo[TXFIFO_RP(mua)];
		mua->txfifo_rp++;
	}
	if (mua->txfifo_rp == mua->txfifo_wp) {
		SerialDevice_StopTx(mua->backend);
	}
	return true;
}

static void
serial_input(void *cd, UartChar c)
{
	M32C_Uart *mua = cd;
	//fprintf(stderr,"Got char %04x\n",c);
	mua->reg_uirb = c;
	if (!(mua->reg_uic1 & UiC1_RI)) {
		mua->reg_uic1 |= UiC1_RI;
		post_rx_interrupt(mua);
	}
	SerialDevice_StopRx(mua->backend);
	CycleTimer_Mod(&mua->rx_baud_timer, mua->byte_time);
}

static void
M32CUart_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	M32C_Uart *mua = (M32C_Uart *) owner;
	UartAddrs *ar = &uart_addrs[mua->register_set];
	IOH_Delete8(ar->aUiMR);
	IOH_Delete8(ar->aUiC0);
	IOH_Delete8(ar->aUiBRG);
	IOH_Delete8(ar->aUiC1);
	IOH_Delete16(ar->aUiTB);
	IOH_Delete16(ar->aUiRB);
	if (mua->register_set == 0) {
		IOH_Delete8(REG_U56IS);
		IOH_Delete8(REG_U56CON);
	}

}


static void
M32CUart_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	M32C_Uart *mua = (M32C_Uart *) owner;
	static int exists = 0;
	UartAddrs *ar = &uart_addrs[mua->register_set];
	IOH_New8(ar->aUiMR, uimr_read, uimr_write, mua);
	IOH_New8(ar->aUiC0, uic0_read, uic0_write, mua);
	IOH_New8(ar->aUiBRG, uibrg_read, uibrg_write, mua);
	IOH_New8(ar->aUiC1, uic1_read, uic1_write, mua);
	IOH_New16(ar->aUiTB, uitb_read, uitb_write, mua);
	IOH_New16(ar->aUiRB, uirb_read, uirb_write, mua);
	if (mua->register_set == 0) {
		IOH_New8(REG_U56IS, u56is_read, u56is_write, NULL);
		IOH_New8(REG_U56CON, u56con_read, u56con_write, NULL);
	}
#if 1
	if(!exists) {
		IOH_New8(0x500000, testreg_read, testreg_write, mua);
		exists = 1;
	}
#endif
}

/**
 ***************************************************************************************
 * \fn BusDevice * M32CUart56_New(const char *name,unsigned int register_set) 
 ***************************************************************************************
 */
BusDevice *
M32CUart56_New(const char *name, unsigned int register_set)
{
	M32C_Uart *mua = sg_new(M32C_Uart);
	if (register_set == 0) {
		g_Uart[0] = mua;
	} else {
		g_Uart[1] = mua;
	}
	mua->name = sg_strdup(name);
	mua->bdev.first_mapping = NULL;
	mua->bdev.Map = M32CUart_Map;
	mua->bdev.UnMap = M32CUart_Unmap;
	mua->bdev.owner = mua;
	mua->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	mua->backend = Uart_New(name, serial_input, serial_output, NULL, mua);
	if (register_set > array_size(uart_addrs)) {
		fprintf(stderr, "Illegal register set index for uart %s\n", name);
		exit(1);
	}
	if (register_set == 0) {
		mua->msk_uirs = U56CON_U5IRS;
		mua->msk_urrm = U56CON_U5RRM;
		mua->msk_uirxd = U56IS_U5RXD;
		mua->msk_uiclk = U56IS_U5CLK;
		mua->msk_uicts = U56IS_U5CTS;
	} else {
		mua->msk_uirs = U56CON_U6IRS;
		mua->msk_urrm = U56CON_U6RRM;
		mua->msk_uirxd = U56IS_U6RXD;
		mua->msk_uiclk = U56IS_U6CLK;
		mua->msk_uicts = U56IS_U6CTS;
	}
	mua->register_set = register_set;
	mua->reg_uimr = 0;
	mua->reg_uic0 = 0x8;
	mua->reg_uic1 = 0x2;
	CycleTimer_Init(&mua->tx_baud_timer, tx_done, mua);
	CycleTimer_Init(&mua->rx_baud_timer, rx_next, mua);
	mua->byte_time = 1000;
	mua->rxIrq = SigNode_New("%s.rxirq", name);
	mua->txIrq = SigNode_New("%s.txirq", name);
	if (!mua->rxIrq || !mua->txIrq) {
		fprintf(stderr, "Can not create interrupt line for \"%s\"\n", name);
		exit(1);
	}
	SigNode_Set(mua->txIrq, SIG_HIGH);
	SigNode_Set(mua->rxIrq, SIG_HIGH);

	mua->clk_f1 = Clock_New("%s.f1", name);
	mua->clk_f8 = Clock_New("%s.f8", name);
	mua->clk_f2n = Clock_New("%s.f2n", name);
	mua->clk_sync = Clock_New("%s.sync", name);
	mua->clk_async = Clock_New("%s.async", name);

	fprintf(stderr, "M32C Uart \"%s\" created\n", name);
	return &mua->bdev;
}
