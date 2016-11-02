/*
 ********************************************************************************************
 * Emulation of Renesas M32C87 Uart 0 - 4
 *
 * state: Working in async mode, and in SPI mode
 *
 * Copyright 2009/2010 Jochen Karrer. All rights reserved.
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
 ***********************************************************************************************
 */

#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "sgstring.h"
#include "serial.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "uart_m32c.h"
#include "spidevice.h"

#define UiMR_SMD_MSK	(7)
#define 	UiMR_SMD_DISA	(0)
#define		UiMR_SMD_SSI	(1)
#define		UiMR_SMD_I2C	(2)
#define		UiMR_SMD_UART7	(4)
#define		UiMR_SMD_UART8	(5)
#define		UiMR_SMD_UART9	(6)
#define UiMR_CKDIR	(1 << 3)
#define UiMR_STPS	(1 << 4)
#define UiMR_PRY	(1 << 5)
#define UiMR_PRYE	(1 << 6)
#define UiMR_IOPOL	(1 << 7)

#define UiC0_CLK_MSK	(3)
#define 	UiC0_CLK_F1	(0)
#define		UiC0_CLK_F8	(1)
#define		UiC0_CLK_F2N	(2)
#define		UiC0_FLK_UNDEF	(3)
#define	UiC0_CRS	(1 << 2)
#define UiC0_TXEPT	(1 << 3)
#define UiC0_CRD	(1 << 4)
#define UiC0_NCH	(1 << 5)
#define UiC0_CKPOL	(1 << 6)
#define UiC0_UFORM	(1 << 7)

#define UiC1_TE		(1)
#define UiC1_TI		(1 << 1)
#define UiC1_RE		(1 << 2)
#define UiC1_RI		(1 << 3)
#define UiC1_UiRS	(1 << 4)
#define UiC1_UiRRM	(1 << 5)
#define UiC1_UiLCH	(1 << 6)
#define UiC1_SCLKSTPB	(1 << 7)
#define UiC1_UiERE	(1 << 7)

#define UiSMR_IICM	(1 << 0)
#define UiSMR_ABC	(1 << 1)
#define UiSMR_BBS	(1 << 2)
#define UiSMR_LSYN	(1 << 3)
#define UiSMR_ABSCS	(1 << 4)
#define UiSMR_ACSE	(1 << 5)
#define UiSMR_SSS	(1 << 6)
#define UiSMR_SCLKDIV	(1 << 7)

#define UiSMR2_IICM2		(1 << 0)
#define UiSMR2_CSC		(1 << 1)
#define UiSMR2_SWC		(1 << 2)
#define UiSMR2_ALS		(1 << 3)
#define UiSMR2_STC		(1 << 4)
#define UiSMR2_SWC2		(1 << 5)
#define UiSMR2_SDHI		(1 << 6)
#define UiSMR2_SU1HIM		(1 << 7)

#define UiSMR3_SSE		(1 << 0)
#define UiSMR3_CKPH		(1 << 1)
#define UiSMR3_DINC		(1 << 2)
#define UiSMR3_NODC		(1 << 3)
#define UiSMR3_ERR		(1 << 4)
#define UiSMR3_DL_MASK		(7 << 5(
#define UiSMR3_DL_SHIFT		(5)

#define UiSMR4_STAREQ		(1)
#define UiSMR4_RSTAREQ		(1 << 1)
#define UiSMR4_STPREQ		(1 << 2)
#define UiSMR4_STSPSEL		(1 << 3)
#define UiSMR4_ACKD		(1 << 4)
#define UiSMR4_ACKC		(1 << 5)
#define UiSMR4_SCLHI		(1 << 6)
#define UiSMR4_SWC9		(1 << 7)

typedef struct UartAddrs {
	uint32_t aUiMR;
	uint32_t aUiSMR;
	uint32_t aUiSMR2;
	uint32_t aUiSMR3;
	uint32_t aUiSMR4;
	uint32_t aUiC0;
	uint32_t aUiBRG;
	uint32_t aUiC1;
	uint32_t aUiTB;
	uint32_t aUiRB;
} UartAddrs;
#define REG_IFSR 0x31F

static UartAddrs uart_addrs[] = {
	{
	 .aUiMR = 0x368,
	 .aUiSMR = 0x367,
	 .aUiSMR2 = 0x366,
	 .aUiSMR3 = 0x365,
	 .aUiSMR4 = 0x364,
	 .aUiC0 = 0x36c,
	 .aUiBRG = 0x369,
	 .aUiC1 = 0x36d,
	 .aUiTB = 0x36a,
	 .aUiRB = 0x36e,
	 },
	{
	 .aUiMR = 0x2e8,
	 .aUiSMR = 0x2e7,
	 .aUiSMR2 = 0x2e6,
	 .aUiSMR3 = 0x2e5,
	 .aUiSMR4 = 0x2e4,
	 .aUiC0 = 0x2ec,
	 .aUiBRG = 0x2e9,
	 .aUiC1 = 0x2ed,
	 .aUiTB = 0x2ea,
	 .aUiRB = 0x2ee,
	 },
	{
	 .aUiMR = 0x338,
	 .aUiSMR = 0x337,
	 .aUiSMR2 = 0x336,
	 .aUiSMR3 = 0x335,
	 .aUiSMR4 = 0x334,
	 .aUiC0 = 0x33c,
	 .aUiBRG = 0x339,
	 .aUiC1 = 0x33d,
	 .aUiTB = 0x33a,
	 .aUiRB = 0x33e,
	 },
	{
	 .aUiMR = 0x328,
	 .aUiSMR = 0x327,
	 .aUiSMR2 = 0x326,
	 .aUiSMR3 = 0x325,
	 .aUiSMR4 = 0x324,
	 .aUiC0 = 0x32c,
	 .aUiBRG = 0x329,
	 .aUiC1 = 0x32d,
	 .aUiTB = 0x32a,
	 .aUiRB = 0x32e,
	 },
	{
	 .aUiMR = 0x2F8,
	 .aUiSMR = 0x2f7,
	 .aUiSMR2 = 0x2f6,
	 .aUiSMR3 = 0x2f5,
	 .aUiSMR4 = 0x2f4,
	 .aUiC0 = 0x2fc,
	 .aUiBRG = 0x2f9,
	 .aUiC1 = 0x2fd,
	 .aUiTB = 0x2fa,
	 .aUiRB = 0x2fe,
	 },
};

typedef struct M32C_Uart {
	BusDevice bdev;
	char *name;
	unsigned int register_set;
	Clock_t *clk_f1;
	Clock_t *clk_f8;
	Clock_t *clk_f2n;
	Clock_t *clk_sync;
	Clock_t *clk_async;

	SigNode *rxIrq;
	SigNode *txIrq;
	UartPort *backend;
	Spi_Device *spidev;
	CycleTimer tx_baud_timer;
	CycleTimer rx_baud_timer;
	CycleCounter_t byte_time;
	uint16_t tx_shift_reg;
	uint8_t reg_uimr;
	uint8_t reg_uismr;
	uint8_t reg_uismr2;
	uint8_t reg_uismr3;
	uint8_t reg_uismr4;
	uint8_t reg_uic0;
	uint8_t reg_uibrg;
	uint8_t reg_uic1;
	uint16_t reg_uitb;
	uint16_t reg_uirb;
} M32C_Uart;

static void
post_tx_interrupt(M32C_Uart * mua)
{
	SigNode_Set(mua->txIrq, SIG_LOW);
	SigNode_Set(mua->txIrq, SIG_HIGH);
}

static void
post_rx_interrupt(M32C_Uart * mua)
{
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

static uint32_t
uimr_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	return mua->reg_uimr;
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

/**
 ****************************************************************************
 * Translation Renesas / Atmel:
 * Renesas CPOL CPHA -> Atmel CPOL CPHA:
 * R00 -> A11
 * R01 -> A00
 * R10 -> A01
 * R11 -> A10
 * This means CPH is simply inverted.
 * Polarity is inverted by CPHA change -> Atmel Pol = Renesas !(cpha ^ cpol)
 ****************************************************************************
 */
static void
update_spidev_config(M32C_Uart * mua)
{

	uint8_t mode = mua->reg_uimr & UiMR_SMD_MSK;
	uint8_t ckph = !!(mua->reg_uismr3 & UiSMR3_CKPH);
	uint8_t ckpol = !!(mua->reg_uic0 & UiC0_CKPOL);
	uint32_t spi_control;
	if (mode == UiMR_SMD_SSI) {
		spi_control = SPIDEV_BITS(8) | SPIDEV_KEEP_IDLE_STATE;
		if (mua->reg_uimr & UiMR_CKDIR) {
			spi_control |= SPIDEV_SLAVE;
		} else {
			spi_control |= SPIDEV_MASTER;
		}
		/* The Phase is simply inverted */
		if (ckph) {
			spi_control |= SPIDEV_CPHA0;
		} else {
			spi_control |= SPIDEV_CPHA1;
		}
		if (ckpol ^ ckph) {
			spi_control |= SPIDEV_CPOL0;
		} else {
			spi_control |= SPIDEV_CPOL1;
		}
		if (mua->reg_uic0 & UiC0_UFORM) {
			spi_control |= SPIDEV_MSBFIRST;
		} else {
			spi_control |= SPIDEV_LSBFIRST;
		}
		SpiDev_Configure(mua->spidev, spi_control);
	} else {
		SpiDev_Configure(mua->spidev, SPIDEV_DISA);
	}

}

static void
uimr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	uint8_t mode = value & UiMR_SMD_MSK;
	uint8_t oldmode = mua->reg_uimr & UiMR_SMD_MSK;
	mua->reg_uimr = value;
	if (oldmode != mode) {
		update_spidev_config(mua);
		update_uart_config(mua);
	}
}

static uint32_t
uismr_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	return mua->reg_uismr;
}

static void
uismr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	mua->reg_uismr = value;
}

static uint32_t
uismr2_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	return mua->reg_uismr2;
}

static void
uismr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	mua->reg_uismr2 = value;
}

static uint32_t
uismr3_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	return mua->reg_uismr3;
}

static void
uismr3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	mua->reg_uismr3 = value;
	update_spidev_config(mua);
}

static uint32_t
uismr4_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	return mua->reg_uismr4;
}

static void
uismr4_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	mua->reg_uismr4 = value;
}

static uint32_t
uic0_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
//      fprintf(stderr,"uic0 read %02x\n",mua->reg_uic0);
	return mua->reg_uic0;
}

static void
uic0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	mua->reg_uic0 = (value & ~UiC0_TXEPT) | (mua->reg_uic0 & UiC0_TXEPT);
//      fprintf(stderr,"uic0 written to %02x\n",mua->reg_uic0);
	update_spidev_config(mua);
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
#if 0
	if ((mua->reg_uimr & 3) == 1) {
		return UiC1_RI;
	}
#endif
	return mua->reg_uic1;
}

static void
uic1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	/* Keep RI and TI */
	mua->reg_uic1 = (value & ~UiC1_TI) | (mua->reg_uic1 & (UiC1_TI | UiC1_RI));
	if ((mua->reg_uimr & UiMR_SMD_MSK) != UiMR_SMD_SSI) {
		if (mua->reg_uic1 & UiC1_RE) {
			SerialDevice_StartRx(mua->backend);
		}
	}
}

static uint32_t
uitb_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

#include "cpu_m32c.h"
static void
uitb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	mua->reg_uitb = value & 0x1ff;
	if (value > 0x1ff) {
		//fprintf(stderr,"Warning: UITB %s write with 0x%04x\n",mua->name,value);
	}
	if (!(mua->reg_uic1 & UiC1_TE)) {
		fprintf(stderr, "Uart 3 UITB write with disabled transmitter %02x\n",
			mua->reg_uic1);
		return;
	}
#if 0
	if (mua->register_set == 0) {
		fprintf(stdout, "Uart 0 UITB write %02x at 0x%06x \n", value,M32C_REG_PC);
	}
	if (mua->register_set == 3) {
		fprintf(stdout, "Uart 3 UITB write %02x \n", value);
	}
#endif
	if (mua->reg_uic0 & UiC0_TXEPT) {
		mua->tx_shift_reg = value;
		mua->reg_uic0 &= ~UiC0_TXEPT;
		if ((mua->reg_uimr & UiMR_SMD_MSK) == UiMR_SMD_SSI) {
			uint8_t data = mua->tx_shift_reg;
			//fprintf(stderr,"Spidev started xmit uic1 %02x\n",mua->reg_uic1);
			SpiDev_StartXmit(mua->spidev, &data, 8);
		} else {
			//fprintf(stderr,"Ser started tx\n");
			SerialDevice_StartTx(mua->backend);
		}
		/* 
		 *********************************************************
		 * If interrupt occurs on no data in uitb register 
		 * the interrupt is triggered immediately.
		 *********************************************************
		 */
		if (!(mua->reg_uic1 & UiC1_UiRS)) {
			post_tx_interrupt(mua);
		}
	} else {
		mua->reg_uic1 &= ~UiC1_TI;
	}
//      fprintf(stderr,"Exit after uart write %04x\n",value);
}

static uint32_t
uirb_read(void *clientData, uint32_t address, int rqlen)
{
	uint16_t uirb;
	M32C_Uart *mua = (M32C_Uart *) clientData;
	uirb = mua->reg_uirb;
	if (!(mua->reg_uic1 & UiC1_RI)) {
		fprintf(stderr, "Read UIRB while nothing received %d\n", uirb);
	}
	//fprintf(stdout,"Read uirb %d\n",uirb);
	mua->reg_uic1 &= ~UiC1_RI;
	//update_rx_interrupt(mua);
	return uirb;
}

static void
uirb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static void
rx_next(void *clientData)
{
	M32C_Uart *mua = (M32C_Uart *) clientData;
	if ((mua->reg_uimr & UiMR_SMD_MSK) != UiMR_SMD_SSI) {
		if (mua->reg_uic1 & UiC1_RE) {
			SerialDevice_StartRx(mua->backend);
		}
	}
}

static void
tx_done(void *clientData)
{
#if 0
	M32C_Uart *mua = (M32C_Uart *) clientData;
	/* Force the emulator not to be faster than the output channel */
	while ((usart->txfifo_wp - usart->txfifo_rp) == TXFIFO_SIZE) {
		FIO_HandleInput();
	}
	update_interrupts(usart);
#endif
}

static void
serial_input(void *cd, UartChar c)
{
	M32C_Uart *mua = cd;
	mua->reg_uirb = c;
	if (!(mua->reg_uic1 & UiC1_RI)) {
		mua->reg_uic1 |= UiC1_RI;
		post_rx_interrupt(mua);
	}
	SerialDevice_StopRx(mua->backend);
	CycleTimer_Mod(&mua->rx_baud_timer, mua->byte_time);
}

static void
spidev_xmit(void *owner, uint8_t * data, int bits)
{
	M32C_Uart *mua = owner;
	if ((mua->reg_uic1 & UiC1_RE)) {
		mua->reg_uirb = *data;
		if (!(mua->reg_uic1 & UiC1_RI)) {
			mua->reg_uic1 |= UiC1_RI;
			post_rx_interrupt(mua);
		}
	}
	if (mua->reg_uic1 & UiC1_TI) {
		/* Nothing in UiTB -> Transmitter is now empty */
		mua->reg_uic0 |= UiC0_TXEPT;
		if (mua->reg_uic1 & UiC1_UiRS) {
			post_tx_interrupt(mua);
		}
	} else {
		/* 
		 ***************************************************
		 * Move the uitb to the shift register 
		 * and indicate uitb empty 
		 ***************************************************
		 */
		uint8_t data;
		//fprintf(stderr,"Spidev start xmit uic1 %02x",mua->reg_uic1);
		mua->tx_shift_reg = mua->reg_uitb;
		mua->reg_uic1 |= UiC1_TI;
		//fprintf(stderr,"->%02x\n",mua->reg_uic1);
		/* Check if interrupt is triggered by empty uitb */
		if (!(mua->reg_uic1 & UiC1_UiRS)) {
			post_tx_interrupt(mua);
		}
		data = mua->tx_shift_reg;
		SpiDev_StartXmit(mua->spidev, &data, 8);
	}
	return;
}

static bool
serial_output(void *cd, UartChar * c)
{
	M32C_Uart *mua = cd;
	if (!(mua->reg_uic1 & UiC1_TE)) {
		fprintf(stderr, "Bug: Tx with disabled transmitter\n");
		return false;
	}
	if (mua->reg_uic0 & UiC0_TXEPT) {
		fprintf(stderr, "Bug: Tx with empty transmitter\n");
		return false;
	}
	*c = mua->tx_shift_reg;
	if (mua->reg_uic1 & UiC1_TI) {
		/* Nothing in UiTB -> Transmitter is now empty */
		mua->reg_uic0 |= UiC0_TXEPT;
		SerialDevice_StopTx(mua->backend);
		/* Check if interrupt is triggered by completed operation */
		if (mua->reg_uic1 & UiC1_UiRS) {
			post_tx_interrupt(mua);
		}
	} else {
		/* 
		 ***************************************************
		 * Move the uitb to the shift register 
		 * and indicate uitb empty 
		 ***************************************************
		 */
		mua->tx_shift_reg = mua->reg_uitb;
		mua->reg_uic1 |= UiC1_TI;
		/* Check if interrupt is triggered by empty uitb */
		if (!(mua->reg_uic1 & UiC1_UiRS)) {
			post_tx_interrupt(mua);
		}
	}
#if 0
	/* Feed the byte back in Syncmode for now */
	if ((mua->reg_uimr & UiMR_SMD_MSK) == UiMR_SMD_SSI) {
		//fprintf(stderr,"sout Feedback sync %02x\n",mua->reg_uimr);
		//exit(1);
		mua->reg_uirb = data;
		if (!(mua->reg_uic1 & UiC1_RI)) {
			mua->reg_uic1 |= UiC1_RI;
			post_rx_interrupt(mua);
		}
	}
#endif
	return true;
}

static void
M32CUart_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	M32C_Uart *mua = (M32C_Uart *) owner;
	UartAddrs *ar = &uart_addrs[mua->register_set];
	IOH_Delete8(ar->aUiMR);
	IOH_Delete8(ar->aUiSMR);
	IOH_Delete8(ar->aUiSMR2);
	IOH_Delete8(ar->aUiSMR3);
	IOH_Delete8(ar->aUiSMR4);
	IOH_Delete8(ar->aUiC0);
	IOH_Delete8(ar->aUiBRG);
	IOH_Delete8(ar->aUiC1);
	IOH_Delete16(ar->aUiTB);
	IOH_Delete16(ar->aUiRB);

}

static void
M32CUart_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	M32C_Uart *mua = (M32C_Uart *) owner;
	UartAddrs *ar = &uart_addrs[mua->register_set];
	IOH_New8(ar->aUiMR, uimr_read, uimr_write, mua);
	IOH_New8(ar->aUiSMR, uismr_read, uismr_write, mua);
	IOH_New8(ar->aUiSMR2, uismr2_read, uismr2_write, mua);
	IOH_New8(ar->aUiSMR3, uismr3_read, uismr3_write, mua);
	IOH_New8(ar->aUiSMR4, uismr4_read, uismr4_write, mua);
	IOH_New8(ar->aUiC0, uic0_read, uic0_write, mua);
	IOH_New8(ar->aUiBRG, uibrg_read, uibrg_write, mua);
	IOH_New8(ar->aUiC1, uic1_read, uic1_write, mua);
	IOH_New16(ar->aUiTB, uitb_read, uitb_write, mua);
	IOH_New16(ar->aUiRB, uirb_read, uirb_write, mua);
}

BusDevice *
M32CUart_New(const char *name, unsigned int register_set)
{
	M32C_Uart *mua = sg_new(M32C_Uart);
	char *spidev_name = alloca(30 + strlen(name));
	char *spidev_clkname = alloca(34 + +strlen(name));
	char *uart_syncclkname = alloca(34 + +strlen(name));
	sprintf(spidev_name, "%s.spi", name);
	mua->name = sg_strdup(name);
	mua->bdev.first_mapping = NULL;
	mua->bdev.Map = M32CUart_Map;
	mua->bdev.UnMap = M32CUart_Unmap;
	mua->bdev.owner = mua;
	mua->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	mua->backend = Uart_New(name, serial_input, serial_output, NULL, mua);
	mua->spidev = SpiDev_New(spidev_name, spidev_xmit, mua);
	if (register_set > array_size(uart_addrs)) {
		fprintf(stderr, "Illegal register set index for uart %s\n", name);
		exit(1);
	}
	mua->register_set = register_set;
	mua->reg_uimr = 0;
	mua->reg_uic0 = UiC0_TXEPT;
	mua->reg_uic1 = UiC1_TI;
	mua->reg_uismr = 0;
	mua->reg_uismr2 = 0;
	mua->reg_uismr3 = 0;
	mua->reg_uismr4 = 0;
	CycleTimer_Init(&mua->tx_baud_timer, tx_done, mua);
	CycleTimer_Init(&mua->rx_baud_timer, rx_next, mua);
	mua->byte_time = 1000;
	mua->rxIrq = SigNode_New("%s.rxirq", name);
	mua->txIrq = SigNode_New("%s.txirq", name);
	if (!mua->rxIrq || !mua->txIrq) {
		fprintf(stderr, "Can not create interrupt line for \"%s\"\n", name);
		exit(1);
	}
	SigNode_Set(mua->rxIrq, SIG_HIGH);
	SigNode_Set(mua->txIrq, SIG_HIGH);
	mua->clk_f1 = Clock_New("%s.f1", name);
	mua->clk_f8 = Clock_New("%s.f8", name);
	mua->clk_f2n = Clock_New("%s.f2n", name);
	mua->clk_sync = Clock_New("%s.sync", name);
	mua->clk_async = Clock_New("%s.async", name);
	/* Now link the SPI child */
	sprintf(spidev_clkname, "%s.spi.clk", name);
	sprintf(uart_syncclkname, "%s.sync", name);
	Clock_Link(spidev_clkname, uart_syncclkname);
	fprintf(stderr, "M32C Uart \"%s\" created\n", name);
	return &mua->bdev;
}
