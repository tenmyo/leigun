
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "signode.h"
#include "clock.h"
#include "imx21_uart.h"
#include "serial.h"
#include "configfile.h"
#include "senseless.h"
#include "sgstring.h"

/* TXX9 Serial Registers */
#define TXX9_SILCR(base)      ((base)+0x00)
#define		SILCR_UMODE_MASK	(3)
#define		SILCR_UMODE_SHIFT	(0)
#define 	SILCR_USBL		(1<<2)
#define		SILCR_UPEN		(1<<3)
#define		SILCR_UEPS		(1<<4)
#define		SILCR_SCS_MASK		(3<<5)
#define		SILCR_SCS_SHIFT		(5)
#define		SILCR_UODE		(1<<13)
#define		SILCR_TWUB		(1<<14)
#define		SILCR_RWUB		(1<<15)
#define TXX9_SIDICR(base)     ((base)+0x04)
#define		SIDICR_STIE_MASK	(0x3f)
#define		SIDICR_STIE_SHIFT	(0)
#define		SIDICR_CTSAC_MASK	(3<<9)
#define		SIDICR_CTSAC_SHIFT	(9)
#define		SIDICR_SPIE		(1<<11)
#define		SIDICR_RIE		(1<<12)
#define		SIDICR_TIE		(1<<13)
#define		SIDICR_RDE		(1<<14)
#define		SIDICR_TDE		(1<<15)
#define TXX9_SIDISR(base)     ((base)+0x08)
#define		SIDISR_RFDN_MASK	(0x1f)
#define		SIDISR_RFDN_SHIFT	(0)
#define		SIDISR_STIS		(1<<6)
#define		SIDISR_RDIS		(1<<7)
#define		SIDISR_TDIS		(1<<8)
#define		SIDISR_TOUT		(1<<9)
#define		SIDISR_ERI		(1<<10)
#define		SIDISR_UOER		(1<<11)
#define		SIDISR_UPER		(1<<12)
#define		SIDISR_UFER		(1<<13)
#define 	SIDISR_UVALID		(1<<14)
#define		SIDISR_UBRK		(1<<15)
#define TXX9_SISCISR(base)     ((base)+0x0c)
#define		SISCISR_UBRKD		(1<<0)
#define		SISCISR_TXALS		(1<<1)
#define		SISCISR_TRDY		(1<<2)
#define		SISCISR_RBRKD		(1<<3)
#define		SISCISR_CTSS		(1<<4)
#define		SISCISR_OERS		(1<<5)
#define TXX9_SIFCR(base)      ((base)+0x10)
#define		SIFCR_FRSTE		(1<<0)
#define		SIFCR_RFRST		(1<<1)
#define		SIFCR_TFRST		(1<<2)
#define 	SIFCR_TDIL_MASK		(3<<3)
#define		SIFCR_TDIL_SHIFT	(3)
#define 	SIFCR_RDIL_MASK		(3<<7)
#define		SIFCR_RDIL_SHIFT	(7)
#define		SIFCR_SWRST		(1<<15)
#define TXX9_SIFLCR(base)     ((base)+0x14)
#define		SIFLCR_TBRK		(1<<0)
#define		SIFLCR_RTSTL_MASK	(0xf<1)
#define		SIFLCR_RTSTL_SHIFT	(1)
#define		SIFLCR_TSDE		(1<<7)
#define		SIFLCR_RSDE		(1<<8)
#define		SIFLCR_RTSSC		(1<<9)
#define		SIFLCR_TES		(1<<11)
#define		SIFLCR_RCS		(1<<12)
#define TXX9_SIBGR(base)      ((base)+0x18)
#define		SIBGR_BRD_MASK 		(0xff)
#define		SIBGR_BRD_SHIFT		(0)
#define		SIBGR_BCLK_MASK		(3<<8)
#define		SIBGR_BCLK_SHIFT	(8)
#define TXX9_SITFIFO(base)    ((base)+0x1c)
#define TXX9_SIRFIFO(base)    ((base)+0x20)

#define RX_FIFO_SIZE (16)
#define RX_FIFO_MASK (RX_FIFO_SIZE-1)
#define RX_FIFO_COUNT(ser) ((ser)->rxfifo_wp - (ser)->rxfifo_rp)
#define RX_FIFO_SPACE(ser) (RX_FIFO_SIZE - RX_FIFO_COUNT(ser))
#define RX_FIFO_WIDX(ua) ((ua)->rxfifo_wp & RX_FIFO_MASK)
#define RX_FIFO_RIDX(ua) ((ua)->rxfifo_rp & RX_FIFO_MASK)

#define TX_FIFO_SIZE (16)
#define TX_FIFO_MASK (TX_FIFO_SIZE-1)
#define TX_FIFO_COUNT(ser) ((ser)->txfifo_wp-(ser)->txfifo_rp)
#define TX_FIFO_SPACE(ser) (TX_FIFO_SIZE-TX_FIFO_COUNT(ser))
#define TX_FIFO_WIDX(ua) ((ua)->txfifo_wp & TX_FIFO_MASK)
#define TX_FIFO_RIDX(ua) ((ua)->txfifo_rp & TX_FIFO_MASK)

typedef struct TX49xx_Uart {
	BusDevice bdev;
	Clock_t *in_clk;
	Clock_t *baud_clk;
	ClockTrace_t *baud_trace;
	SigNode *irqNode;
	UartPort *port;
	char *name;
	uint8_t rxfifo[RX_FIFO_SIZE];
	uint64_t rxfifo_wp;
	uint64_t rxfifo_rp;

	UartChar txfifo[TX_FIFO_SIZE];
	uint64_t txfifo_wp;
	uint64_t txfifo_rp;

	uint32_t silcr;
	uint32_t sidicr;
	uint32_t sidisr;
	uint32_t siscisr;
	uint32_t sifcr;
	uint32_t siflcr;
	uint32_t sibgr;

} TX49xx_Uart;

static void
tx49xx_uart_reset(TX49xx_Uart * ua)
{
	ua->silcr = 0x4040;	/* From real device */
	ua->sidicr = 0;
	ua->sidisr = 0x4100;
	ua->siscisr = 0x6;	/* Bit 4 should be cts status (real device shows 0x16) */
	ua->sifcr = 0;
	ua->siflcr = 0x182;
	ua->sibgr = 0x3ff;
	ua->txfifo_rp = ua->txfifo_wp = 0;
	ua->rxfifo_rp = ua->rxfifo_wp = 0;
	Clock_MakeDerived(ua->baud_clk, ua->in_clk, 1, 128 * 255);
}

static void
update_baudrate(Clock_t * clock, void *clientData)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	UartCmd cmd;
	cmd.opcode = UART_OPC_SET_BAUDRATE;
	cmd.arg = Clock_Freq(clock);
	SerialDevice_Cmd(ua->port, &cmd);
	return;

}

static void
update_interrupt(TX49xx_Uart * ua)
{
	int interrupt = 0;
	if ((ua->sidicr & SIDICR_RIE) && (ua->sidisr & SIDISR_RDIS)) {
		interrupt = 1;
	}
	if ((ua->sidicr & SIDICR_TIE) && (ua->sidisr & SIDISR_TDIS)) {
		interrupt = 1;
	}
	if ((ua->sidicr & SIDICR_SPIE) && (ua->sidisr & SIDISR_ERI)) {
		interrupt = 1;
	}
#if 0
	if ((ua->sidicr & SIDICR_CTSAC) && (ua->sidisr & SIDISR_CTSS)) {
		interrupt = 1;
	}
#endif
	if (interrupt) {
		SigNode_Set(ua->irqNode, SIG_LOW);
	} else {
		SigNode_Set(ua->irqNode, SIG_HIGH);
	}

}

static bool
serial_output(void *cd, UartChar * c)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) cd;
	if (TX_FIFO_COUNT(ua) > 0) {
		*c = ua->txfifo[TX_FIFO_RIDX(ua)];
		ua->txfifo_rp += 1;
	} else {
		fprintf(stderr, "Bug in %s %s\n", __FILE__, __func__);
	}
	if (TX_FIFO_COUNT(ua) == 0) {
		SerialDevice_StopTx(ua->port);
	}
	update_interrupt(ua);
	return true;

}

static void
serial_input(void *cd, UartChar c)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) cd;
	int fifocount;
	int rx_triglev;
	int space;
	space = RX_FIFO_SPACE(ua);
	if (space < 1) {
		return;
	}
	ua->rxfifo[RX_FIFO_WIDX(ua)] = c;
	ua->rxfifo_wp++;
	fifocount = RX_FIFO_COUNT(ua);
	rx_triglev = ((ua->sifcr & SIFCR_RDIL_MASK) >> SIFCR_RDIL_SHIFT) << 2;
	if (rx_triglev == 0) {
		rx_triglev = 1;
	}
	if (fifocount >= rx_triglev) {
		ua->sidisr |= SIDISR_RDIS;
	}
	//ua->sidisr = (ua->sidisr & ~SIDISR_RFDN_MASK) | fifocount;
	update_interrupt(ua);
	return;
}

/*
 * ------------------------------------------------
 * SILCR 	Line Control Register
 *	UMODE	8/7 Bit
 *	USBL	Stop bit length
 *	UPEN	Parity enable
 *	UEPS	Even Parity select
 *	SCS	Clock select
 *	UODE	Open drain enable
 *	TWUB	Transmit Wake up
 *	RWUB	Receive Wake up
 * ------------------------------------------------
 */

static uint32_t
silcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "silcr not implemented\n");
	return 0;
}

static void
silcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "silcr not implemented\n");
}

/*
 * -----------------------------------------------------------
 * SIDICR 	DMA/Interrupt Control Register
 *   STIE  Status change interrupt enable 
 *	   Overrun,ctss,rbreak,txemtpy TXcomplete, UBreak
 *   CTSAC CTS active condition rising/falling,high,low
 *   SPIE  Recption Error interrupt enable
 *   RIE   Reception Data Full Interrupt Enable
 *   TIE   Transmit data empty interrupt enable
 *   RDE   Receive DMA Transfer enable
 *   TDE   Transmit DMA Transfer Enable
 * -----------------------------------------------------------
 */
static uint32_t
sidicr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "silcr not implemented\n");
	return 0;
}

static void
sidicr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "silcr not implemented\n");
}

/*
 * ---------------------------------------------------------------
 * SIDISR	DMA/Interrupt Status Register
 *	RFDN Reception Data stage status: 0-16 stages remaining
 * 	STIS Status Change Interrupt status
 * 	RDIS Reception Data Full (datacnt >= Trigger level)
 *      TDIS Transmission Data empty: datacnt <= Trigger level 
 * 	TOUT Reception Time out
 * 	ERI  Exception error interrupt (frame,parityoverrun)
 * 	UOER Overrun error	
 * 	UPER Parity error
 *	UFER Frame error
 *	UVALID  Receive FIFO available status 
 *	UBRK	Detected breaks
 *	
 * ---------------------------------------------------------------
 */
static uint32_t
sidisr_read(void *clientData, uint32_t address, int rqlen)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	int fifocount = RX_FIFO_COUNT(ua);
	ua->sidisr = (ua->sidisr & ~SIDISR_RFDN_MASK) | fifocount;
	return ua->sidisr;
}

static void
sidisr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	uint32_t andmask = value |
	    ~(SIDISR_ERI | SIDISR_TOUT | SIDISR_TDIS | SIDISR_RDIS | SIDISR_STIS);
	ua->sidisr &= andmask;
	update_interrupt(ua);
}

/*
 * -------------------------------------------------------------
 * SISCISR
 *   UBRKD Uart Break Detected 
 *   TXALS Transmission complete 
 *   TRDY  Transmission Data empty (at least one byte)
 *   RBRKD Receiving Break autocleared
 *   CTSS  CTS line status
 *   OERS  Overrun error status
 * -------------------------------------------------------------
 */
static uint32_t
siscisr_read(void *clientData, uint32_t address, int rqlen)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	if (TX_FIFO_SPACE(ua) > 0) {
		ua->siscisr |= SISCISR_TRDY;
	} else {
		ua->siscisr &= ~SISCISR_TRDY;
	}
	if (TX_FIFO_COUNT(ua) == 0) {
		ua->siscisr |= SISCISR_TXALS;
	} else {
		ua->siscisr &= ~SISCISR_TXALS;
	}
	return ua->siscisr;
}

static void
siscisr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	uint32_t andmask = ~SISCISR_UBRKD & value;
	ua->siscisr &= andmask;
}

/* 
 * -----------------------------------------------------------------------
 * SIFCR 	Fifo Control Register
 *   FRSTE Fifo reset enable
 *   RFRST Receive FIFO reset
 *   TFRST Transmit FIFO reset
 *   TDIL Transmit FIFO request Trigger level,1,4,8 
 *   RDIL Receive Fifo Rqeuest Trigger level
 *   SWRST Software reset
 * -----------------------------------------------------------------------
 */
static uint32_t
sifcr_read(void *clientData, uint32_t address, int rqlen)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	return ua->sifcr;
}

static void
sifcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	if (value & (SIFCR_FRSTE | SIFCR_RFRST)) {
		ua->rxfifo_rp = ua->rxfifo_wp = 0;
	}
	if (value & (SIFCR_FRSTE | SIFCR_TFRST)) {
		ua->txfifo_rp = ua->txfifo_wp = 0;
	}
	ua->sifcr = value & 0x19f;
	if (value & SIFCR_SWRST) {
		/* sifcr is also cleared by this reset */
		tx49xx_uart_reset(ua);
	}
}

/*
 * ------------------------------------------------------------------------
 * SIFLCR	Flow Control Register
 *   TBRK    Transmit a break
 *   RTSTL   RTS Active Trigger Level (0-15 Bytes)	
 *   TSDE    Serial Data Transmit Disable (default 1)
 *   RSDE    Serial Data Reception disable
 *   RTSSC   RTS Software Control
 *   TES     CTS Signal Control enable 
 *   RCS     RTS Signal Control select
 * ------------------------------------------------------------------------
 */
static uint32_t
siflcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "silcr not implemented\n");
	return 0;
}

static void
siflcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "silcr not implemented\n");
}

/*
 * ------------------------------------------------------------------------
 * BGR 	Baudrate Control Register
 * ------------------------------------------------------------------------
 */
static uint32_t
sibgr_read(void *clientData, uint32_t address, int rqlen)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	return ua->sibgr;
}

static void
sibgr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	int brd = value & 0xff;
	int bclk = value & 0x300 >> 8;
	int prediv = 2 << (bclk * 2);
	ua->sibgr = value & 0x3ff;
	/* clock is traced */
	Clock_MakeDerived(ua->baud_clk, ua->in_clk, 1, prediv * brd);
}

/*
 * -----------------------------------------------------------------
 * TFIFO: TX Fifo
 * -----------------------------------------------------------------
 */
static uint32_t
sitfifo_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Warning: TX-Fifo not readable in real device (crash) \n");
	return 0;
}

static void
sitfifo_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	int space = TX_FIFO_SPACE(ua);
	if (!space) {
		fprintf(stderr, "TX Fifo overflow\n");
		return;
	}
	ua->txfifo[TX_FIFO_WIDX(ua)] = value & 0xff;
	ua->txfifo_wp++;
	// set some flags is missing here
	//ua->
	update_interrupt(ua);
	return;
}

/*
 * -----------------------------------------------------------------
 * RFIFO: RX Fifo
 * -----------------------------------------------------------------
 */
static uint32_t
sirfifo_read(void *clientData, uint32_t address, int rqlen)
{
	TX49xx_Uart *ua = (TX49xx_Uart *) clientData;
	int fill = TX_FIFO_COUNT(ua);
	uint32_t val;
	val = ua->txfifo[TX_FIFO_RIDX(ua)];
	if (fill) {
		ua->txfifo_rp++;
	}
	// update some flags
	update_interrupt(ua);
	return val;
}

static void
sirfifo_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Uart: RX-Fifo is not writable\n");
}

static void
TX49xx_Uart_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(TXX9_SILCR(base));
	IOH_Delete32(TXX9_SIDICR(base));
	IOH_Delete32(TXX9_SIDISR(base));
	IOH_Delete32(TXX9_SISCISR(base));
	IOH_Delete32(TXX9_SIFCR(base));
	IOH_Delete32(TXX9_SIFLCR(base));
	IOH_Delete32(TXX9_SIBGR(base));
	IOH_Delete32(TXX9_SITFIFO(base));
	IOH_Delete32(TXX9_SIRFIFO(base));
}

static void
TX49xx_Uart_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	TX49xx_Uart *ua = owner;
	/* TXX9 Serial Registers */
	IOH_New32(TXX9_SILCR(base), silcr_read, silcr_write, ua);
	IOH_New32(TXX9_SIDICR(base), sidicr_read, sidicr_write, ua);
	IOH_New32(TXX9_SIDISR(base), sidisr_read, sidisr_write, ua);
	IOH_New32(TXX9_SISCISR(base), siscisr_read, siscisr_write, ua);
	IOH_New32(TXX9_SIFCR(base), sifcr_read, sifcr_write, ua);
	IOH_New32(TXX9_SIFLCR(base), siflcr_read, siflcr_write, ua);
	IOH_New32(TXX9_SIBGR(base), sibgr_read, sibgr_write, ua);
	IOH_New32(TXX9_SITFIFO(base), sitfifo_read, sitfifo_write, ua);
	IOH_New32(TXX9_SIRFIFO(base), sirfifo_read, sirfifo_write, ua);

}

BusDevice *
TX49xx_Uart_New(const char *name)
{
	TX49xx_Uart *ua = sg_new(TX49xx_Uart);
	ua->irqNode = SigNode_New("%s.irq", name);
	if (!ua->irqNode) {
		fprintf(stderr, "Can not create uart signal lines\n");
	}
	ua->in_clk = Clock_New("%s.clk", name);
	ua->baud_clk = Clock_New("%s.baud", name);
	ua->baud_trace = Clock_Trace(ua->baud_clk, update_baudrate, ua);
	ua->bdev.first_mapping = NULL;
	ua->bdev.Map = TX49xx_Uart_Map;
	ua->bdev.UnMap = TX49xx_Uart_UnMap;
	ua->bdev.owner = ua;
	ua->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	ua->name = sg_strdup(name);
	ua->port = Uart_New(name, serial_input, serial_output, NULL, ua);
	return &ua->bdev;
}
