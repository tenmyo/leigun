/*
 **********************************************************************************************
 * Renesas RX62N Serial Communications interface simulator  
 *
 * State: Async and synchronous mode working, baudrate is fixed. Incomplete 
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
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
 **********************************************************************************************
 */

#include "cycletimer.h"
#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "serial.h"
#include "cpu_rx.h"
#include "spidevice.h"
#include "clock.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define SCI_SMR(base)	((base) + 0x00)
#define 	SMR_CM		(1 << 7)
#define 	SMR_CHR		(1 << 6)
#define		SMR_PE		(1 << 5)
#define		SMR_PM		(1 << 4)
#define 	SMR_STOP	(1 << 3)
#define 	SMR_MP		(1 << 2)
#define		SMR_CKS_MSK	(3)
#define		SMR_CKS_SHIFT	(0)
#define			SMR_CKS_PCLK	0
#define			SMR_CKS_PCLK4	1
#define			SMR_CKS_PCLK16	2
#define			SMR_CKS_PCLK64	3
#define SCI_BRR(base)	((base) + 0x01)
#define SCI_SCR(base)	((base) + 0x02)
#define		SCR_TIE		(1 << 7)
#define		SCR_RIE		(1 << 6)
#define		SCR_TE		(1 << 5)
#define		SCR_RE		(1 << 4)
#define		SCR_MPIE	(1 << 3)
#define 	SCR_TEIE	(1 << 2)
#define		SCR_CKE_MSK	(3)
#define		SCR_CKE_SHIFT	(0)

#define SCI_TDR(base)	((base) + 0x03)
#define SCI_SSR(base)	((base) + 0x04)
#define		SSR_TDRE	(1 << 7)    /* TDRE not defined for RX63 */
#define		SSR_RDRF	(1 << 6)    /* RDRF not defined for RX63 */
#define		SSR_ORER	(1 << 5)
#define		SSR_FER		(1 << 4)
#define		SSR_PER		(1 << 3)
#define		SSR_TEND	(1 << 2)
#define		SSR_MPB		(1 << 1)
#define		SSR_MPBT	(1 << 0)

#define SCI_RDR(base)	((base) + 0x05)
#define SCI_SCMR(base)  ((base) + 0x06)
#define		SCMR_BCP2	(1 << 7)
#define		SCMR_SDIR	(1 << 3)
#define		SCMR_SINV	(1 << 2)
#define		SCMR_SMIF	(1 << 0)

#define SCI_SEMR(base)	((base) + 0x07)
#define		SEMR_ABCS	(1 << 4)
#define		SEMR_ACS0	(1 << 0)


#define SHREG_STATE_EMPTY       (0)
#define SHREG_STATE_LOADED      (1)
#define SHREG_STATE_PROCESSING  (2)

typedef struct SCI {
	BusDevice bdev;
	UartPort *backend;
	Spi_Device *spidev;
	const char *name;
	uint8_t regSMR;
	uint8_t regBRR;
	uint8_t regSCR;
	uint8_t regTDR;
	uint8_t regSSR;
	uint8_t regRDR;
	uint8_t regSCMR;
	uint8_t regSEMR;
    uint8_t txShiftRegState;
	uint16_t tx_shift_reg;

	SigNode *sigIrqERI;
	SigNode *sigIrqRXI;
	SigNode *sigIrqTXI;
	SigNode *sigIrqTEI;
	Clock_t *clkSync;
	Clock_t *clkAsync;
	Clock_t *clkIn;

//	CycleTimer rxBaudTimer;
	uint32_t byte_time;
} SCI;

static inline void
post_rx_interrupt(SCI * sci)
{
	SigNode_Set(sci->sigIrqRXI, SIG_LOW);
}

static inline void
unpost_rx_interrupt(SCI * sci)
{
	SigNode_Set(sci->sigIrqRXI, SIG_HIGH);
}

static inline void
post_err_interrupt(SCI * sci)
{
	SigNode_Set(sci->sigIrqERI, SIG_LOW);
}

static inline void
unpost_err_interrupt(SCI * sci)
{
	SigNode_Set(sci->sigIrqERI, SIG_HIGH);
}

static void
update_async_config(SCI * sci)
{
	UartCmd cmd;
	cmd.opcode = UART_OPC_SET_CSIZE;
	if (sci->regSMR & SMR_CHR) {
		cmd.arg = 7;
	} else {
		cmd.arg = 8;
	}
	if (sci->regSMR & SMR_MP) {
		cmd.arg++;
	}
	SerialDevice_Cmd(sci->backend, &cmd);
}

static void
serial_input(void *eventData, UartChar c)
{
	SCI *sci = eventData;
	if (sci->regSSR & SSR_RDRF) {
		sci->regSSR |= SSR_ORER;
		fprintf(stderr, "Overflow char %02x\n", c);
		return;
	} else if(sci->regSSR & SSR_ORER) {
		fprintf(stderr, "SCI Overflow flag is set\n");
		return;
	}
	sci->regRDR = c;
	sci->regSSR |= SSR_RDRF;
	if (sci->regSMR & SMR_MP) {
		bool bit_mp;
		if (sci->regSMR & SMR_CHR) {
			bit_mp = ! !(c & 0x80);
		} else {
			bit_mp = ! !(c & 0x100);
		}
		if (bit_mp) {
			sci->regSSR |= SSR_MPB;
		} else {
			sci->regSSR &= ~SSR_MPB;
		}
	}
	if (sci->regSCR & SCR_RIE) {
		post_rx_interrupt(sci);
	}
#if 0
    if (strcmp(sci->name, "sci10") == 0) {
   fprintf(stderr,"Serial Input at %llu\n", CyclesToMicroseconds(CycleCounter_Get()));
    }
#endif
//	SerialDevice_StopRx(sci->backend);
//	CycleTimer_Mod(&sci->rxBaudTimer, sci->byte_time);
}

/**
 *************************************************************************
 *************************************************************************
 */
static void
post_tx_interrupt(SCI * sci)
{
	SigNode_Set(sci->sigIrqTXI, SIG_LOW);
}

static void
post_te_interrupt(SCI * sci)
{
	SigNode_Set(sci->sigIrqTEI, SIG_LOW);
}

static void
unpost_tx_interrupt(SCI * sci)
{
	SigNode_Set(sci->sigIrqTXI, SIG_HIGH);
}

static void
unpost_te_interrupt(SCI * sci)
{
	SigNode_Set(sci->sigIrqTEI, SIG_HIGH);
}

static void
update_clocks(SCI * sci)
{
	uint32_t div;
	bool abcs = ! !(sci->regSEMR & SEMR_ABCS);
    UartCmd uartCmd;
	switch (sci->regSMR & SMR_CKS_MSK) {
	    case SMR_CKS_PCLK:
		    div = 1;
		    break;
	    case SMR_CKS_PCLK4:
		    div = 4;
		    break;
	    case SMR_CKS_PCLK16:
		    div = 16;
		    break;
	    default:
	    case SMR_CKS_PCLK64:
		    div = 64;
		    break;
	}
	div = 4 * div * (sci->regBRR + 1);
	Clock_MakeDerived(sci->clkSync, sci->clkIn, 1, div);
	if (abcs == true) {
		Clock_MakeDerived(sci->clkAsync, sci->clkIn, 1, 4 * div);
	} else {
		Clock_MakeDerived(sci->clkAsync, sci->clkIn, 1, 8 * div);
	}
    if ((sci->regSCR & SCR_RE) || (sci->regSCR & SCR_TE)) {
        uartCmd.opcode = UART_OPC_SET_BAUDRATE;
        uartCmd.arg = Clock_Freq(sci->clkAsync);
        SerialDevice_Cmd(sci->backend, &uartCmd);
    }
}

/**
 *************************************************************
 * Load the TX shiftreg from TDR and MPBT
 *************************************************************
 */
static inline void
load_tx_shift_reg(SCI * sci)
{
	UartChar data = sci->regTDR;
	if (sci->regSMR & SMR_MP) {
		uint16_t value = 0x100;
		if (sci->regSMR & SMR_CHR) {
			value >>= 1;
		}
		if (sci->regSSR & SSR_MPBT) {
			data |= value;
		} else {
			data &= ~value;
		}
		//fprintf(stderr,"MP output %02x\n",data);
	}
#if 0
    if (strcmp(sci->name, "sci2") == 0) {
        fprintf(stderr,"Shift reg loaded at %llu\n", CyclesToMicroseconds(CycleCounter_Get()));
    }
#endif
	sci->tx_shift_reg = data;
    sci->txShiftRegState = SHREG_STATE_LOADED;
}

static bool
serial_output(void *eventData, UartChar * c)
{
	SCI *sci = eventData;
    bool retval = true;
	dbgprintf("Serial output called with %02x\n", *c);
    if (sci->txShiftRegState == SHREG_STATE_LOADED) {
        sci->txShiftRegState = SHREG_STATE_PROCESSING;
	    *c = sci->tx_shift_reg;
    } else if(sci->txShiftRegState == SHREG_STATE_PROCESSING) {
        if (sci->regSSR & SSR_TDRE) {
            /* No new data in Transmit register */
		    sci->regSSR |= SSR_TEND;
            sci->txShiftRegState = SHREG_STATE_EMPTY;
            SerialDevice_StopTx(sci->backend);
            if (sci->regSCR & SCR_TEIE) {
                post_te_interrupt(sci);
            }
            retval = false;
	    } else {
            /* Reload the Shift register from TDR */
            load_tx_shift_reg(sci);
            *c = sci->tx_shift_reg;
            sci->txShiftRegState = SHREG_STATE_PROCESSING;
            sci->regSSR |= SSR_TDRE;
            if (sci->regSCR & SCR_TIE) {
                dbgprintf("Post TIE, sr now 0x%02x\n", sci->tx_shift_reg);
                post_tx_interrupt(sci);
                unpost_tx_interrupt(sci);
            }
        }
    }
#if 0

	/* 
	 *******************************************************************
	 * If TDRE is empty then shift register can not be refilled.
	 * Trigger a transmitter empty interrupt and set TEND.
	 *******************************************************************
	 */
    if (sci->tx_shift_reg_full == false) {
		SerialDevice_StopTx(sci->backend);
		if (sci->regSCR & SCR_TEIE) {
			post_te_interrupt(sci);
        }
        retval = false;
    } else if (sci->regSSR & SSR_TDRE) {
		sci->regSSR |= SSR_TEND;
	    *c = sci->tx_shift_reg;
        sci->tx_shift_reg_full = false;
	} else {
        /* Reload the Shift register from TDR */
	    *c = sci->tx_shift_reg;
        sci->tx_shift_reg_full = false;
		load_tx_shift_reg(sci);
		sci->regSSR |= SSR_TDRE;
		if (sci->regSCR & SCR_TIE) {
			dbgprintf("Post TIE, sr now 0x%02x\n", sci->tx_shift_reg);
			post_tx_interrupt(sci);
			unpost_tx_interrupt(sci);
		}
	}
#endif
	return retval;
}

static void
spidev_xmit(void *owner, uint8_t * data, int bits)
{
	SCI *sci = owner;
	if (sci->regSCR & SCR_RE) {
		if (sci->regSSR & SSR_RDRF) {
			sci->regSSR |= SSR_ORER;
			fprintf(stderr, "Overflow char %02x\n", *data);
		}
		sci->regRDR = *data;
		sci->regSSR |= SSR_RDRF;
		if (sci->regSCR & SCR_RIE) {
			post_rx_interrupt(sci);
		}
	}
	if (sci->regSSR & SSR_TDRE) {
		sci->regSSR |= SSR_TEND;
        sci->txShiftRegState = SHREG_STATE_EMPTY;
		if (sci->regSCR & SCR_TEIE) {
			post_te_interrupt(sci);
		}
	} else {
		uint8_t data;
		data = sci->tx_shift_reg = sci->regTDR;
        sci->txShiftRegState = SHREG_STATE_PROCESSING;
		sci->regSSR |= SSR_TDRE;
		if (sci->regSCR & SCR_TIE) {
			dbgprintf("Post TIE, sr now 0x%02x\n", sci->tx_shift_reg);
			post_tx_interrupt(sci);
			unpost_tx_interrupt(sci);
		}
		SpiDev_StartXmit(sci->spidev, &data, 8);
	}
	return;
}

static void
update_spidev_config(SCI * sci)
{

	uint8_t cke = sci->regSCR & SCR_CKE_MSK;
	uint32_t spi_control;
	if (!(sci->regSMR & SMR_CM)) {
		SpiDev_Configure(sci->spidev, SPIDEV_DISA);
		return;
	}
	spi_control = SPIDEV_BITS(8) | SPIDEV_KEEP_IDLE_STATE;
	if ((cke == 0) || (cke == 1)) {
		spi_control |= SPIDEV_MASTER;
	} else {
		spi_control |= SPIDEV_SLAVE;
	}
	spi_control |= SPIDEV_CPHA1;
	spi_control |= SPIDEV_CPOL1;
	SpiDev_Configure(sci->spidev, spi_control);
}

#if 0
static void
rx_baud_timer(void *eventData)
{
	SCI *sci = eventData;
	SerialDevice_StartRx(sci->backend);
}
#endif

static uint32_t
smr_read(void *clientData, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	return sci->regSMR;
}

static void
smr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	sci->regSMR = value;
	update_spidev_config(sci);
	update_async_config(sci);
	update_clocks(sci);
}

static uint32_t
brr_read(void *clientData, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	return sci->regBRR;
}

static void
brr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	sci->regBRR = value;
	update_clocks(sci);
}

static uint32_t
scr_read(void *clientData, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	return sci->regSCR;
}

static void
scr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	uint8_t diff = value ^ sci->regSCR;
	sci->regSCR = value;
    UartCmd uartCmd;
    if ((diff & SCR_TEIE) && !(value & SCR_TEIE)) {
        unpost_te_interrupt(sci); 
    }
    if (diff & SCR_RE) {
        if (sci->regSCR & SCR_RE) {
            SerialDevice_StartRx(sci->backend);
            //CycleTimer_Mod(&sci->rxBaudTimer, sci->byte_time);
        } else {
            SerialDevice_StopRx(sci->backend);
            sci->regSSR &= ~SSR_RDRF;
            unpost_rx_interrupt(sci);
            //CycleTimer_Remove(&sci->rxBaudTimer);
        }
    }
	/* On Clearing TE SSR_TEND is set */
	if ((diff & SCR_TE) && !(value & SCR_TE)) {
		sci->regSSR |= SSR_TEND;
		unpost_te_interrupt(sci);
	}
	update_spidev_config(sci);
    if ((diff & (SCR_RE | SCR_TE)) && ((value & SCR_RE) || (value & SCR_TE))) {
        uartCmd.opcode = UART_OPC_SET_BAUDRATE;
        uartCmd.arg = Clock_Freq(sci->clkAsync);
        SerialDevice_Cmd(sci->backend, &uartCmd);
        fprintf(stderr,"SCR Write: The clock of SCI is %" PRIu64 "\n", Clock_Freq(sci->clkAsync));
    }
}

static uint32_t
tdr_read(void *clientData, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	return sci->regTDR;
}

static void
tdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	sci->regTDR = value;
	if ((sci->regSCR & SCR_TE) && (sci->regSSR & SSR_TDRE)) {
		if (sci->regSSR & SSR_TEND) {
			dbgprintf("TDR: Starting tx: 0x%02x\n", value);
			/* Transmited to shift register immediately */
			load_tx_shift_reg(sci);
			if (sci->regSCR & SCR_TIE) {
				post_tx_interrupt(sci);
				unpost_tx_interrupt(sci);
            }
			sci->regSSR &= ~SSR_TEND;
			unpost_te_interrupt(sci);
			if ((sci->regSMR & SMR_CM)) {
				uint8_t data = sci->tx_shift_reg;
                sci->txShiftRegState = SHREG_STATE_PROCESSING;
				SpiDev_StartXmit(sci->spidev, &data, 8);
			} else {
				SerialDevice_StartTx(sci->backend);
			}
		} else {
			dbgprintf("TDR: %02x, PC: %08x\n", value, RX_REG_PC);
			sci->regSSR &= ~SSR_TDRE;
		}
	} else {
		fprintf(stderr, "SCI TDR: transmitter disabled or TDR busy\n");
	}
}

static uint32_t
ssr_read(void *clientData, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	return sci->regSSR;
}

static void
ssr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	uint8_t clearmask;
	//fprintf(stdout,"SSR: from: %02x ",sci->regSSR);
	sci->regSSR = sci->regSSR & (SSR_TEND | SSR_TDRE | SSR_RDRF | SSR_MPB);
	clearmask = SSR_PER | SSR_FER | SSR_ORER;
	sci->regSSR = (sci->regSSR & ~clearmask) | (value & clearmask);
	if (value & SSR_MPBT) {
		sci->regSSR |= SSR_MPBT;
	}
	if (sci->regSSR & (SSR_PER | SSR_FER | SSR_ORER)) {
		post_err_interrupt(sci);
	} else {
		unpost_err_interrupt(sci);
	}
	//fprintf(stdout,"to: %02x, value %02x\n",sci->regSSR,value);
}

static uint32_t
rdr_read(void *clientData, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	sci->regSSR &= ~SSR_RDRF;
    //fprintf(stderr, "RDR %02x\n", sci->regRDR);
	unpost_rx_interrupt(sci);
	return sci->regRDR;
}

static void
rdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RX SCI write reg RDR not implemented\n");
}

static uint32_t
scmr_read(void *clientData, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	return sci->regSCMR;
}

static void
scmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RX SCI write reg SCMR not implemented\n");
}

static uint32_t
semr_read(void *clientData, uint32_t address, int rqlen)
{
	SCI *sci = clientData;
	return sci->regSEMR;
}

static void
semr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	SCI *sci = clientData;
	sci->regSEMR = value & 0x11;
	update_clocks(sci);
}

/**
 *************************************************************************************
 * \fn static void SCI_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
 * Map the Serial Interface to the memory space.
 *************************************************************************************
 */

static void
SCI_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	SCI *sci = (SCI *) owner;
	IOH_New8(SCI_SMR(base), smr_read, smr_write, sci);
	IOH_New8(SCI_BRR(base), brr_read, brr_write, sci);
	IOH_New8(SCI_SCR(base), scr_read, scr_write, sci);
	IOH_New8(SCI_TDR(base), tdr_read, tdr_write, sci);
	IOH_New8(SCI_SSR(base), ssr_read, ssr_write, sci);
	IOH_New8(SCI_RDR(base), rdr_read, rdr_write, sci);
	IOH_New8(SCI_SCMR(base), scmr_read, scmr_write, sci);
	IOH_New8(SCI_SEMR(base), semr_read, semr_write, sci);
}

/**
 *******************************************************************************
 * \fn static void SCI_Unmap(void *owner,uint32_t base,uint32_t mask)
 * Unmap the Serial Interface from the memory space.
 *******************************************************************************
 */
static void
SCI_Unmap(void *owner, uint32_t base, uint32_t mask)
{

	IOH_Delete8(SCI_SMR(base));
	IOH_Delete8(SCI_BRR(base));
	IOH_Delete8(SCI_SCR(base));
	IOH_Delete8(SCI_TDR(base));
	IOH_Delete8(SCI_SSR(base));
	IOH_Delete8(SCI_RDR(base));
	IOH_Delete8(SCI_SCMR(base));
	IOH_Delete8(SCI_SEMR(base));
}

/**
 ***********************************************************************
 * BusDevice * SCI_New(const char *name) 
 * Create a new Serial interface.
 ***********************************************************************
 */
BusDevice *
SCI_New(const char *name)
{
	SCI *sci = sg_new(SCI);
	char *spidev_name = alloca(30 + strlen(name));
	char *spidev_clkname = alloca(34 + +strlen(name));
	char *uart_syncclkname = alloca(34 + +strlen(name));

	sci->name = sg_strdup(name);
	sci->bdev.first_mapping = NULL;
	sci->bdev.Map = SCI_Map;
	sci->bdev.UnMap = SCI_Unmap;
	sci->bdev.owner = sci;
	sci->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	sci->backend = Uart_New(name, serial_input, serial_output, NULL, sci);
	sprintf(spidev_name, "%s.spi", name);
	sci->spidev = SpiDev_New(spidev_name, spidev_xmit, sci);
	sci->byte_time = 7600;
	//CycleTimer_Init(&sci->rxBaudTimer, rx_baud_timer, sci);
	sci->regSMR = 0;
	sci->regBRR = 0xFF;
	sci->regSCR = 0x00;
	sci->regTDR = 0xFF;
	sci->regSSR = 0x84;
	sci->regRDR = 0x00;
	sci->regSCMR = 0xF2;
	sci->regSEMR = 0x00;
	sci->sigIrqERI = SigNode_New("%s.irqERI", name);
	sci->sigIrqRXI = SigNode_New("%s.irqRXI", name);
	sci->sigIrqTXI = SigNode_New("%s.irqTXI", name);
	sci->sigIrqTEI = SigNode_New("%s.irqTEI", name);
	if (!sci->sigIrqERI || !sci->sigIrqRXI || !sci->sigIrqTXI || !sci->sigIrqTEI) {
		fprintf(stderr, "Can not create Interrupt lines for %s\n", name);
		exit(1);
	}
	sprintf(spidev_clkname, "%s.spi.clk", name);
	sprintf(uart_syncclkname, "%s.sync", name);
	sci->clkSync = Clock_New("%s.sync", name);
	sci->clkAsync = Clock_New("%s.async", name);
	sci->clkIn = Clock_New("%s.clk", name);
	Clock_Link(spidev_clkname, uart_syncclkname);
	update_clocks(sci);
	fprintf(stderr, "Created Serial Communication Interface \"%s\"\n", name);
	return &sci->bdev;
}
