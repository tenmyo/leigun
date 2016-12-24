
#include <unistd.h>
#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "uart_tcc8k.h"
#include "serial.h"
#include "cycletimer.h"
#include "clock.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define UART_RBR(base)		((base) + 0x00)
#define UART_THR(base)		((base) + 0x00)
#define UART_DLL(base)		((base) + 0x00)
#define UART_DLM(base)		((base) + 0x04)
#define UART_IER(base)		((base) + 0x04)
#define         IER_ERXI	(1<<0)
#define         IER_ETXI	(1<<1)
#define         IER_ELSI        (1<<2)
#define         IER_EMSI	(1<<3)
#define UART_IIR(base)		((base) + 0x08)
#define		IIR_STF		(1 << 27)
#define         IIR_NPENDING    (1<<0)
#define         IIR_INTID_MASK  (7<<1)
#define         IIR_INTID_SHIFT (1)
#define                 IIR_TYPE_RLS    (3 << 1)
#define                 IIR_TYPE_RDA    (2 << 1)
#define                 IIR_TYPE_CTI    (6 << 1)
#define                 IIR_TYPE_THRE   (1 << 1)
#define                 IIR_TYPE_MST    (0 << 1)

#define UART_FCR(base)		((base) + 0x08)
#define 	FCR_RXT_1  (0 << 6)
#define 	FCR_RXT_4  (1 << 6)
#define 	FCR_RXT_8  (2 << 6)
#define 	FCR_RXT_14 (3 << 6)
#define 	FCR_TXT_16 (0 << 4)
#define 	FCR_TXT_8  (1 << 4)
#define 	FCR_TXT_4  (2 << 4)
#define 	FCR_TXT_1  (3 << 4)
#define 	FCR_DRTE   (1 << 3)
#define 	FCR_TXFR   (1 << 2)
#define 	FCR_RXFR   (1 << 1)
#define 	FCR_FE     (1 << 0)

#define UART_LCR(base)		((base) + 0x0c)
#define 	LCR_DLAB   (1 << 7)
#define 	LCR_SB     (1 << 6)
#define 	LCR_SP     (1 << 5)
#define 	LCR_EPS    (1 << 4)
#define 	LCR_PEN    (1 << 3)
#define 	LCR_STB    (1 << 2)
#define         LCR_WLS_MASK (3)
#define 	LCR_WLS_5  (0 << 0)
#define 	LCR_WLS_6  (1 << 0)
#define 	LCR_WLS_7  (2 << 0)
#define 	LCR_WLS_8  (3 << 0)

#define UART_MCR(base)		((base) + 0x10)
#define         MCR_RTS         (1 << 1)
#define         MCR_LOOP        (1 << 4)
#define	 	MCR_AFE		(1 << 5)
#define		MCR_RS		(1 << 6)

#define UART_LSR(base)		((base) + 0x14)
#define 	LSR_ERF    (1 << 7)
#define 	LSR_TEMT   (1 << 6)
#define 	LSR_THRE   (1 << 5)
#define 	LSR_BI     (1 << 4)
#define 	LSR_FE     (1 << 3)
#define 	LSR_PE     (1 << 2)
#define 	LSR_OE     (1 << 1)
#define 	LSR_DR     (1 << 0)

#define UART_MSR(base)		((base) + 0x18)
#define         MSR_DCTS        (1<<0)
//#define         MSR_DDSR        (1<<1)
//#define         MSR_TERI        (1<<2)
//#define         MSR_DDCD        (1<<3)
#define         MSR_CTS         (1<<4)
//#define         MSR_DSR         (1<<5)
//#define         MSR_RI          (1<<6)
//#define         MSR_DCD         (1<<7)

#define UART_SCR(base)		((base) + 0x1c)
#define UART_AFT(base)		((base) + 0x20)
#define UART_UCR(base)		((base) + 0x24)
#define UART_SRBR(base)		((base) + 0x40)
#define UART_STHR(base)		((base) + 0x44)
#define UART_SDLL(base)		((base) + 0x48)
#define UART_SDLM(base)		((base) + 0x4c)
#define UART_SIER(base)		((base) + 0x50)
#define UART_SCCR(base)		((base) + 0x60)
#define UART_STC(base)		((base) + 0x64)
#define UART_IRCFG(base)	((base) + 0x80)

#define TXFIFO_SIZE 16
#define TXFIFO_LVL(uart) (uint32_t)((uart)->txfifo_wp - (uart)->txfifo_rp)
#define TXFIFO_WP(uart) ((uart)->txfifo_wp % TXFIFO_SIZE)
#define TXFIFO_RP(uart) ((uart)->txfifo_rp % TXFIFO_SIZE)
#define TXFIFO_ROOM(ser) (TXFIFO_SIZE - TXFIFO_LVL(ser))

#define RXFIFO_SIZE 16
#define RXFIFO_LVL(uart) (uint32_t)((uart)->rxfifo_wp - (uart)->rxfifo_rp)
#define RXFIFO_WP(uart) ((uart)->rxfifo_wp % RXFIFO_SIZE)
#define RXFIFO_RP(uart) ((uart)->rxfifo_rp % RXFIFO_SIZE)
#define RXFIFO_ROOM(ser) (RXFIFO_SIZE - RXFIFO_LVL(ser))

#define DLAB(tcc) ((tcc)->regLcr & LCR_DLAB)

typedef struct TCC_Uart {
	BusDevice bdev;
	UartPort *backend;
	CycleTimer tx_baud_timer;
	CycleTimer rx_baud_timer;
	CycleCounter_t byte_time;
	Clock_t *clk_in;
	Clock_t *clk_baud;
	SigNode *sigIrq;
	int interrupt_posted;
	UartChar rxfifo[RXFIFO_SIZE];
	uint32_t rxfifo_wp;
	uint32_t rxfifo_rp;
	int rxfifo_size;
	UartChar txfifo[TXFIFO_SIZE];
	uint32_t txfifo_wp;
	uint32_t txfifo_rp;
	int txfifo_size;

	uint8_t regLcr;
	uint8_t regMcr;
	uint8_t regLsr;
	uint8_t regMsr;
	uint8_t regDll;
	uint8_t regDlm;
	uint8_t regIer;
	uint8_t regIir;
	uint8_t regFcr;
	uint8_t regScr;
} TCC_Uart;

static void
update_interrupts(TCC_Uart * uart)
{
	/* do Priority encoding */
	int interrupt;
	uint8_t ier = uart->regIer;
	uint8_t int_id = 0;
	if ((ier & IER_ELSI) && (uart->regLsr & (LSR_OE | LSR_PE | LSR_FE | LSR_BI))) {

		int_id = IIR_TYPE_RLS;
		interrupt = 1;

	} else if ((ier & IER_ERXI) && (uart->regLsr & LSR_DR)) {
		// Fifo trigger level should be checked here also

		int_id = IIR_TYPE_RDA;
		interrupt = 1;
		/* 
		 * Character Timeout indication (IIR_TYPE_CTI) ommited here because 
		 * trigger level IRQ mode is  not implemented 
		 */
	} else if ((ier & IER_ETXI) && (uart->regLsr & LSR_THRE)) {

		int_id = IIR_TYPE_THRE;
		interrupt = 1;

	} else if ((ier & IER_EMSI) && uart->regMsr & (MSR_DCTS)) {
		int_id = IIR_TYPE_MST;
		interrupt = 1;

	} else {
		int_id = 0;
		interrupt = 0;
	}
	uart->regIir = (uart->regIir & ~IIR_INTID_MASK) | int_id;
//      fprintf(stderr,"IIR: %08x lsr %08x, ier %08x\n",uart->regIir,uart->regLsr,uart->regIer);
	if (interrupt) {
		uart->regIir &= ~IIR_NPENDING;
		if (!uart->interrupt_posted) {
			SigNode_Set(uart->sigIrq, SIG_HIGH);
			uart->interrupt_posted = 1;
		}
	} else {
		uart->regIir |= IIR_NPENDING;
		if (uart->interrupt_posted) {
			SigNode_Set(uart->sigIrq, SIG_LOW);
			uart->interrupt_posted = 0;
		}
	}

}

static void
update_clock(TCC_Uart * uart)
{
	uint32_t divisor;
	divisor = uart->regDll + (uart->regDlm << 8);
	divisor <<= 4;
	if (divisor) {
		Clock_MakeDerived(uart->clk_baud, uart->clk_in, 1, divisor);
	} else {
		Clock_MakeDerived(uart->clk_baud, uart->clk_in, 0, 1);
	}
}

static void
update_serconfig(TCC_Uart * uart)
{
	UartCmd cmd;
	tcflag_t bits;
	tcflag_t parodd;
	tcflag_t parenb;
	tcflag_t crtscts;
	/* Does 16550 really have no automatic rtscts handshaking ? */
	crtscts = 0;
	if (uart->regLcr & LCR_EPS) {
		parodd = 0;
	} else {
		parodd = 1;
	}
	if (uart->regLcr & LCR_PEN) {
		parenb = 1;
	} else {
		parenb = 0;
	}
	switch (uart->regLcr & LCR_WLS_MASK) {
	    case LCR_WLS_5:
		    bits = 5;
		    break;
	    case LCR_WLS_6:
		    bits = 6;
		    break;
	    case LCR_WLS_7:
		    bits = 7;
		    break;
	    case LCR_WLS_8:
		    bits = 8;
		    break;
		    /* Can not be reached */
	    default:
		    bits = 8;
		    break;
	}

	if (crtscts) {
		cmd.opcode = UART_OPC_CRTSCTS;
		cmd.arg = 1;
		SerialDevice_Cmd(uart->backend, &cmd);
	} else {
		cmd.opcode = UART_OPC_CRTSCTS;
		cmd.arg = 0;
		SerialDevice_Cmd(uart->backend, &cmd);

		cmd.opcode = UART_OPC_SET_RTS;
		/* Set the initial state of RTS */
#if 0
		if (iuart->ucr2 & UCR2_CTS) {
			cmd.arg = UART_RTS_ACT;
		} else {
			cmd.arg = UART_RTS_INACT;
		}
		SerialDevice_Cmd(uart->backend, &cmd);
#endif
	}
	cmd.opcode = UART_OPC_PAREN;
	cmd.arg = parenb;
	SerialDevice_Cmd(uart->backend, &cmd);

	cmd.opcode = UART_OPC_PARODD;
	cmd.arg = parodd;
	SerialDevice_Cmd(uart->backend, &cmd);

	cmd.opcode = UART_OPC_SET_CSIZE;
	cmd.arg = bits;
	SerialDevice_Cmd(uart->backend, &cmd);

}

static void
reset_rx_fifo(TCC_Uart * uart)
{
	uart->rxfifo_rp = uart->rxfifo_wp = 0;
	uart->regLsr &= ~LSR_DR;
	update_interrupts(uart);
}

static void
reset_tx_fifo(TCC_Uart * uart)
{
	uart->txfifo_rp = uart->txfifo_wp = 0;
	uart->regLsr |= LSR_THRE;
	/* 
	 ****************************************************
	 * TEMT is not really emptied by reset but 
	 * a separeate shift register is not implemented
	 ****************************************************
	 */
	uart->regLsr |= LSR_TEMT;
	update_interrupts(uart);
}

/*
 ***************************************************************
 * Filehandler for TX-Fifo
 *      Write chars from TX-Fifo to backend. 
 ***************************************************************
 */
static bool
serial_output(void *cd, UartChar * c)
{
	TCC_Uart *uart = cd;
	if (TXFIFO_LVL(uart) > 0) {
		*c = uart->txfifo[TXFIFO_RP(uart)];
		uart->txfifo_rp++;
	} else {
		fprintf(stderr, "Bug in %s %s\n", __FILE__, __func__);
	}
	SerialDevice_StopTx(uart->backend);
	if (TXFIFO_LVL(uart) == 0) {
		uart->regLsr |= LSR_TEMT;
	} else {
		CycleTimer_Mod(&uart->tx_baud_timer, uart->byte_time);
	}
	if (TXFIFO_ROOM(uart) > 0) {
		uart->regLsr |= LSR_THRE;
	}
	update_interrupts(uart);
	return true;
}

/*
 ************************************************************
 * Put one byte to the rxfifo
 ************************************************************
 */
static inline int
serial_put_rx_fifo(TCC_Uart * uart, UartChar c)
{
	int room = RXFIFO_ROOM(uart);
	if (room < 1) {
		return -1;
	}
	uart->rxfifo[RXFIFO_WP(uart)] = c;
	uart->rxfifo_wp++;
	/* We do not stop on fifo overflow */
#if 0
	if (room == 1) {
		if (CycleTimer_IsActive(&uart->rx_baud_timer)) {
			CycleTimer_Remove(&uart->rx_baud_timer);
		}
		return 0;
	}
#endif
	return 1;
}

/**
 **************************************************************************************************
 *
 **************************************************************************************************
 */
static void
serial_input(void *cd, UartChar c)
{
	TCC_Uart *uart = cd;
	int fifocount;
	//fprintf(stderr,"Serial input \"%c\"\n",c);
	if (RXFIFO_LVL(uart) < RXFIFO_SIZE) {
		uart->rxfifo[RXFIFO_WP(uart)] = c;
		uart->rxfifo_wp++;
	} else {
		fprintf(stderr, "Fifo full\"%c\"\n", c);
	}
	SerialDevice_StopRx(uart->backend);
	CycleTimer_Mod(&uart->rx_baud_timer, uart->byte_time);
	fifocount = RXFIFO_LVL(uart);
	if (fifocount) {
		uart->regLsr |= LSR_DR;
		update_interrupts(uart);
	}
}

/**
 *****************************************************************************************
 * Uart receive buffer register. This is a 16 byte fifo
 *****************************************************************************************
 */
#include "arm9cpu.h"
static uint32_t
rbr_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	uint8_t value;
	dbgprintf("RBR read start at %08x, iir %08x, lsr 0x%02x\n", ARM_GET_CIA, uart->regIir,
		  uart->regLsr);
	if (RXFIFO_LVL(uart) == 0) {
		if (uart->regLsr & LSR_DR) {
			fprintf(stderr, "Bug: DR with no bytes in fifo\n");
			uart->regLsr &= ~LSR_DR;
			update_interrupts(uart);
		}
		return 0;
	}
	value = uart->rxfifo[RXFIFO_RP(uart)];
	uart->rxfifo_rp++;
	dbgprintf("RBR read %02x\n", value);
	if (RXFIFO_LVL(uart) == 0) {
		uart->regLsr &= ~LSR_DR;
		update_interrupts(uart);
	}
	return value;
}

static void
rbr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RBR is not writable\n");
}

static uint32_t
thr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "THR is not readable\n");
	return 0;
}

static void
thr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	unsigned int room;
	if (TXFIFO_LVL(uart) >= TXFIFO_SIZE) {
		fprintf(stderr, "Fifo overflow, LSR %08x\n", uart->regLsr);
		return;
	}
	uart->txfifo[TXFIFO_WP(uart)] = value;
	uart->txfifo_wp++;
	uart->regLsr &= ~LSR_TEMT;
	room = TXFIFO_ROOM(uart);
	if (room > 0) {
		uart->regLsr |= LSR_THRE;
	} else {
		uart->regLsr &= ~LSR_THRE;
	}
	update_interrupts(uart);
	if (!CycleTimer_IsActive(&uart->tx_baud_timer)) {
		CycleTimer_Mod(&uart->tx_baud_timer, uart->byte_time);
	}
//      fprintf(stderr,"%c %08x ",value,ARM_NIA);
}

/**
 *******************************************************************
 * Uart Divisor latch register
 *******************************************************************
 */
static uint32_t
dll_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	return uart->regDll;
}

static void
dll_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	uart->regDll = value;
	dbgprintf("dll 0x%08x\n", value);
	update_clock(uart);
}

static uint32_t
reg0_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *tcc = clientData;
	if (DLAB(tcc)) {
		return dll_read(tcc, address, rqlen);
	} else {
		return rbr_read(tcc, address, rqlen);
	}
}

static void
reg0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Uart *tcc = clientData;
	if (DLAB(tcc)) {
		dll_write(tcc, value, address, rqlen);
	} else {
		thr_write(tcc, value, address, rqlen);
	}
}

/**
 */
static uint32_t
dlm_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	return uart->regDlm;
}

static void
dlm_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	uart->regDlm = value;
	dbgprintf("dlm 0x%08x\n", value);
	update_clock(uart);
}

/**
 **************************************************************************
 * Interrupt register
 **************************************************************************
 */
static uint32_t
ier_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	//fprintf(stderr,"IER read %08x\n",uart->regIer);
	return uart->regIer;
}

static void
ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	uart->regIer = value;
	//fprintf(stderr,"IER write %08x\n",value);
	update_interrupts(uart);
	return;
}

static uint32_t
reg1_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *tcc = clientData;
	if (DLAB(tcc)) {
		return dlm_read(tcc, address, rqlen);	/* Divisor latch most significant */
	} else {
		return ier_read(tcc, address, rqlen);
	}
	return 0;
}

static void
reg1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Uart *tcc = clientData;
	if (DLAB(tcc)) {
		dlm_write(tcc, value, address, rqlen);
	} else {
		ier_write(tcc, value, address, rqlen);
	}
	return;
}

/**
 ******************************************************************************
 * Interrupt ident register
 ******************************************************************************
 */

static uint32_t
iir_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	uint32_t retval;
	dbgprintf("TCC8K UART: %s: IIR is: %02x\n", __func__, uart->regIir);
//              fprintf(stderr,"TCC8K UART: %s: IIR is: %02x\n",__func__,uart->regIir);
	if (uart->interrupt_posted && ((uart->regIir & IIR_INTID_MASK) == 0)) {
		fprintf(stderr, "TCC8K UART: %s: Bug: %02x\n", __func__, uart->regIir);
	}
	if (uart->regIir & (IIR_INTID_MASK == IIR_TYPE_THRE)) {
//              uart->regIir &= ~IIR_INTID_MASK;
	}
	retval = uart->regIir;
	return retval;
}

/**
 *****************************************************************************
 * Fifo control register
 *****************************************************************************
 */

static void
fcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
	TCC_Uart *uart = (TCC_Uart *) clientData;
	uint8_t diff = value ^ uart->regFcr;
	if (diff & FCR_FE) {
		reset_rx_fifo(uart);
		reset_tx_fifo(uart);
		if (value & FCR_FE) {
			uart->txfifo_size = 16;
			uart->rxfifo_size = 16;
		} else {
			uart->txfifo_size = 1;
			uart->rxfifo_size = 1;
		}
	}
	/* If fifo enable not 1 the other bits will not be programmed */
	if (!(value & FCR_FE)) {
		return;
	}
	if (value & FCR_RXFR) {
		uart->rxfifo_wp = uart->rxfifo_rp = 0;
	} else if (value & FCR_TXFR) {
		uart->txfifo_wp = uart->txfifo_rp = 0;
	}
	uart->regFcr = value;
	return;

}

static uint32_t
lcr_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	return uart->regLcr;
}

/**
 ******************************************************************************
 * Line control register
 ******************************************************************************
 */
static void
lcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	uart->regLcr = value;
	update_serconfig(uart);
	return;
}

/**
 *********************************************************************************
 * Modem control register
 *********************************************************************************
 */
static uint32_t
mcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
	return 0;
}

static void
mcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	uint8_t diff = uart->regMcr ^ value;
	UartCmd cmd;

	if (diff & (MCR_RTS)) {

		cmd.opcode = UART_OPC_SET_RTS;
		if (value & MCR_RTS) {
			cmd.arg = 1;
		} else {
			cmd.arg = 0;
		}
		SerialDevice_Cmd(uart->backend, &cmd);

	}
	uart->regMcr = value;
	return;
}

/**
 ******************************************************************************
 * Line Status register
 ******************************************************************************
 */
static uint32_t
lsr_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	//fprintf(stderr,"TCC8K UART: %s: 0x%08x\n",__func__,uart->regLsr);
	//usleep(10000);
	return uart->regLsr;
}

static void
lsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not writable ?\n", __func__);
}

/*
 ****************************************************************************
 * Modem status register.
 ****************************************************************************
 */
static uint32_t
msr_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	uint8_t msr;
	UartCmd cmd;
	msr = uart->regMsr;
	msr = msr & ~(MSR_CTS);

	/* Ask the backend */

	cmd.opcode = UART_OPC_GET_CTS;
	SerialDevice_Cmd(uart->backend, &cmd);
	if (cmd.retval) {
		msr |= MSR_CTS;
	}

	/* Now determine the deltas */
	if ((uart->regMsr ^ msr) & MSR_CTS) {
		msr |= MSR_DCTS;
	}
	uart->regMsr = msr & ~(MSR_DCTS);
#if 0
	if ((uart->regIer & IER_EMSI) && (uart->regMsr != msr)) {
		update_interrupts(uart);
	}
#endif
	return msr;
}

static void
msr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not writable ?\n", __func__);
}

/**
 ********************************************************************************
 * Scratch register
 ********************************************************************************
 */
static uint32_t
scr_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	return uart->regScr;
}

static void
scr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Uart *uart = clientData;
	uart->regScr = value;
}

/**
 ***************************************************************************
 * AFC Trigger Level Register.
 ***************************************************************************
 */
static uint32_t
aft_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
	return 0;
}

static void
aft_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
}

/**
 ****************************************************************************
 * Uart Control register
 ****************************************************************************
 */
static uint32_t
ucr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ucr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
}

/**
 ***********************************************************************************
 * Uart Smart card configuration register.
 ***********************************************************************************
 */
static uint32_t
sccr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
	return 0;
}

static void
sccr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
}

/**
 *****************************************************************************************
 * Smart card TX count
 *****************************************************************************************
 */
static uint32_t
stc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
	return 0;
}

static void
stc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
}

/**
 ***************************************
 * Reserved
 ***************************************
 */
static uint32_t
ircfg_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ircfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K UART: %s: Register not implemented\n", __func__);
}

static void
tx_next(void *cd)
{
	TCC_Uart *uart = (TCC_Uart *) cd;
	//fprintf(stderr,"NextTx");
	SerialDevice_StartTx(uart->backend);
}

static void
rx_next(void *clientData)
{
	TCC_Uart *uart = (TCC_Uart *) clientData;
	SerialDevice_StartRx(uart->backend);
}

static void
TUart_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	TCC_Uart *uart = owner;
	IOH_New32(UART_RBR(base), reg0_read, reg0_write, uart);
	IOH_New32(UART_DLM(base), reg1_read, reg1_write, uart);
	IOH_New32(UART_IIR(base), iir_read, fcr_write, uart);
	IOH_New32(UART_LCR(base), lcr_read, lcr_write, uart);
	IOH_New32(UART_MCR(base), mcr_read, mcr_write, uart);
	IOH_New32(UART_LSR(base), lsr_read, lsr_write, uart);
	IOH_New32(UART_MSR(base), msr_read, msr_write, uart);
	IOH_New32(UART_SCR(base), scr_read, scr_write, uart);
	IOH_New32(UART_AFT(base), aft_read, aft_write, uart);
	IOH_New32(UART_UCR(base), ucr_read, ucr_write, uart);
	IOH_New32(UART_SRBR(base), rbr_read, rbr_write, uart);
	IOH_New32(UART_STHR(base), thr_read, thr_write, uart);
	IOH_New32(UART_SDLL(base), dll_read, dll_write, uart);
	IOH_New32(UART_SDLM(base), dlm_read, dlm_write, uart);
	IOH_New32(UART_SIER(base), ier_read, ier_write, uart);
	IOH_New32(UART_SCCR(base), sccr_read, sccr_write, uart);
	IOH_New32(UART_STC(base), stc_read, stc_write, uart);
	IOH_New32(UART_IRCFG(base), ircfg_read, ircfg_write, uart);
}

static void
TUart_UnMap(void *owner, uint32_t base, uint32_t mask)
{

	IOH_Delete32(UART_RBR(base));
	IOH_Delete32(UART_DLM(base));
	IOH_Delete32(UART_IIR(base));
	IOH_Delete32(UART_LCR(base));
	IOH_Delete32(UART_MCR(base));
	IOH_Delete32(UART_LSR(base));
	IOH_Delete32(UART_MSR(base));
	IOH_Delete32(UART_SCR(base));
	IOH_Delete32(UART_AFT(base));
	IOH_Delete32(UART_UCR(base));
	IOH_Delete32(UART_SRBR(base));
	IOH_Delete32(UART_STHR(base));
	IOH_Delete32(UART_SDLL(base));
	IOH_Delete32(UART_SDLM(base));
	IOH_Delete32(UART_SIER(base));
	IOH_Delete32(UART_SCCR(base));
	IOH_Delete32(UART_STC(base));
	IOH_Delete32(UART_IRCFG(base));
}

BusDevice *
TCC8K_UartNew(const char *name)
{
	TCC_Uart *uart = sg_new(TCC_Uart);
	uart->bdev.first_mapping = NULL;
	uart->bdev.Map = TUart_Map;
	uart->bdev.UnMap = TUart_UnMap;
	uart->bdev.owner = uart;
	uart->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	uart->backend = Uart_New(name, serial_input, serial_output, NULL, uart);
	uart->byte_time = MicrosecondsToCycles(10) >> 1;
	uart->rxfifo_size = 1;
	uart->txfifo_size = 1;

	//uart->backend = CanSocketInterface_New(&canOps,name,can);
	uart->sigIrq = SigNode_New("%s.irq", name);
	if (!uart->sigIrq) {
		fprintf(stderr, "Can not create interrupt line for %s\n", name);
		exit(1);
	}
	uart->clk_in = Clock_New("%s.clk", name);
	uart->clk_baud = Clock_New("%s.baudrate", name);
	if (!uart->clk_in || !uart->clk_baud) {
		fprintf(stderr, "Can not create Uart clocks for \"%s\"\n", name);
		exit(1);
	}
	update_clock(uart);
	CycleTimer_Init(&uart->tx_baud_timer, tx_next, uart);
	CycleTimer_Init(&uart->rx_baud_timer, rx_next, uart);
	reset_tx_fifo(uart);	/* may be this should be removed */
	reset_rx_fifo(uart);
	SerialDevice_StartRx(uart->backend);
	fprintf(stderr, "TCC8000 Uart \"%s\" created\n", name);
	return &uart->bdev;
}
