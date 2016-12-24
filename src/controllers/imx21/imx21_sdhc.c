/*
 *************************************************************************************************
 * Emulation of Freescale iMX21 SD-card host controller 
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "fio.h"
#include "signode.h"
#include "imx21_sdhc.h"
#include "configfile.h"
#include "mmcard.h"
#include "cycletimer.h"
#include "clock.h"
#include "sgstring.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif
#define NEW

#define UDELAY_CMD    (20)
#define UDELAY_DATA   (20)
#define UDELAY_WRITE  (10)

#define SDHC_STR_STP_CLK(base)	((base) + 0x00)
//define        STR_STP_CLK_ENDIAN              (1<<5) /* IMX1 only ??? */
#define 	STR_STP_CLK_RESET               (1<<3)
//#define       STR_STP_CLK_ENABLE              (1<<2) /* IMX1 only ??? */
#define 	STR_STP_CLK_START_CLK           (1<<1)
#define 	STR_STP_CLK_STOP_CLK            (1<<0)

#define SDHC_STATUS(base)	((base) + 0x04)
#define 	STATUS_CARD_PRESENCE            (1<<15)
#define 	STATUS_SDIO_INT_ACTIVE          (1<<14)
#define 	STATUS_END_CMD_RESP             (1<<13)
#define 	STATUS_WRITE_OP_DONE            (1<<12)

#define 	STATUS_READ_OP_DONE          	(1<<11)
#define 	STATUS_WR_CRC_ERROR_CODE_MASK   (3<<10)
#define 	STATUS_WR_CRC_ERROR_CODE_SHIFT  (10)
#define 	STATUS_CARD_BUS_CLK_RUN         (1<<8)

#define 	STATUS_APPL_BUFF_FF             (1<<7)
#define 	STATUS_APPL_BUFF_FE             (1<<6)
#define 	STATUS_RESP_CRC_ERR             (1<<5)

#define 	STATUS_CRC_READ_ERR             (1<<3)
#define 	STATUS_CRC_WRITE_ERR            (1<<2)
#define 	STATUS_TIME_OUT_RESP            (1<<1)
#define 	STATUS_TIME_OUT_READ            (1<<0)
#define 	STATUS_ERR_MASK                 0x3f
#define SDHC_CLK_RATE(base)	((base) + 0x08)
#define		CLK_RATE_PRESCALER_MASK		(0xfff<4)
#define		CLK_RATE_PRESCALER_SHIFT	(4)
#define		CLK_RATE_DIVIDER_MASK		(0xf)
#define		CLK_RATE_DIVIDER_SHIFT		(0)

#define SDHC_CMD_DAT_CONT(base)	((base) + 0x0c)
#define CMD_DAT_CONT_CMD_RESUME		(1<<15)
#define CMD_DAT_CONT_CMD_RESP_LONG_OFF  (1<<12)
#define CMD_DAT_CONT_STOP_READWAIT      (1<<11)
#define CMD_DAT_CONT_START_READWAIT     (1<<10)
#define CMD_DAT_CONT_BUS_WIDTH_1        (0<<8)
#define CMD_DAT_CONT_BUS_WIDTH_4        (2<<8)
#define CMD_DAT_CONT_BUS_WIDTH_MASK     (3<<8)
#define CMD_DAT_CONT_INIT               (1<<7)
#define CMD_DAT_CONT_WRITE              (1<<4)
#define CMD_DAT_CONT_DATA_ENABLE        (1<<3)
#define CMD_DAT_CONT_FORMAT_OF_RESPONSE_MASK	(7<<0)
#define CMD_DAT_CONT_NO_RESPONSE 	    (0)
#define CMD_DAT_CONT_RESPONSE_FORMAT_R11B56 (1)
#define CMD_DAT_CONT_RESPONSE_FORMAT_R2     (2)
#define CMD_DAT_CONT_RESPONSE_FORMAT_R34    (3)
#define SDHC_RESPONSE_TO(base)	((base) + 0x10)
#define		RESPONSE_TO_MASK		(0xff)
#define SDHC_READ_TO(base)	((base) + 0x14)
#define		READ_TO_DATA_READ_TIMEOUT_MASK	(0xffff)
#define SDHC_BLK_LEN(base)	((base) + 0x18)
#define		BLK_LEN_MASK			(0x3ff)
#define SDHC_NOB(base)		((base) + 0x1c)
#define		NOB_MASK			(0xffff)
#define SDHC_REV_NO(base)	((base) + 0x20)
#define		REV_NO_MASK			(0xffff)
#define SDHC_INT_CNTL(base)	((base) + 0x24)
#define 	INT_CNTL_CARD_DET_IRQ_EN       	(1<<15)
#define 	INT_CNTL_SDIO_WAKEUP_EN		(1<<14)
#define		INT_CNTL_DAT0_EN		(1<<5)
#define		INT_CNTL_SDIO			(1<<4)
#define		INT_CNTL_BUF_READY		(1<<3)
#define 	INT_CNTL_END_CMD_RES		(1<<2)
#define		INT_CNTL_WRITE_OP_DONE		(1<<1)
#define 	INT_CNTL_READ_OP_DONE		(1<<0)
#define SDHC_CMD(base)		((base) + 0x28)
#define		CMD_NUMBER_MASK			(0x3f)
#define SDHC_ARGH(base)		((base) + 0x2c)
#define		ARGH_MASK			(0xffff)
#define SDHC_ARGL(base)		((base) + 0x30)
#define		ARGL_MASK			(0xffff)
#define SDHC_RES_FIFO(base)	((base) + 0x34)
#define		RES_FIFO_RESPONSE_CONTENT_MASK	(0xffff)
#define SDHC_BUFFER_ACCESS(base)	((base) + 0x38)
#define		BUFFER_ACCESS_FIFO_CONTENT_MASK	(0xffff)

#define RES_FIFO_SIZE 	(8)
#define MAX_BUF_FIFO_SIZE	(32)
typedef struct IMX21Sdhc {
	BusDevice bdev;
	char *name;
	MMCDev *card;
	Clock_t *inclk;		/* Input clock from Clock / PLL module */
	Clock_t *sdclk;		/* Output clock from Clock to SD-Card  */
	CycleTimer cmd_delay_timer;
	CycleTimer write_delay_timer;
	CycleTimer data_delay_timer;
	/* If clock is not running one Timer can be made waiting for clock */
	CycleTimer *forClockWaitingTimer;
	uint16_t str_stp_clk;
	uint16_t status;
	uint16_t clk_rate;
	uint16_t cmd_dat_cont;
	uint16_t response_to;
	uint16_t read_to;
	uint16_t blk_len;
	uint16_t nob;
	int transfer_count;
	uint16_t rev_no;
	uint16_t int_cntl;
	uint16_t int_status;	/* Internal only, same bits as int_cntl */
	uint16_t cmd;
	uint16_t prev_cmd;	/* For debugging only ! */
	uint16_t argh;
	uint16_t argl;
	uint32_t res_fifo[RES_FIFO_SIZE];
	uint64_t res_fifo_wp;
	uint64_t res_fifo_rp;
	uint16_t buffer[MAX_BUF_FIFO_SIZE];
	unsigned int buf_fifo_size;
	uint64_t buf_fifo_wp;
	uint64_t buf_fifo_rp;
	SigNode *irqNode;
	SigNode *nDmaReq;
} IMX21Sdhc;

static unsigned int
onecount(const uint32_t value)
{
	uint32_t val = value;
	int ones = 0;
	while (val) {
		if (val & 1)
			ones++;
		val >>= 1;
	}
	return ones;
}

static void
update_clock(IMX21Sdhc * sdhc)
{
	uint32_t clk_divider = sdhc->clk_rate & 0xf;
	uint32_t clk_prescaler = (sdhc->clk_rate >> 4) & 0xfff;
	uint32_t divider;
	uint32_t prescaler;
	if ((onecount(clk_prescaler) > 1)) {
		fprintf(stderr, "%s: Illegal Clock Prescaler 0x%03x\n", sdhc->name, clk_prescaler);
	}
	if (clk_divider == 0) {
		fprintf(stderr, "%s: Illegal Clock divider: 0\n", sdhc->name);
	}
	if (clk_prescaler) {
		prescaler = (clk_prescaler * 2);
	} else {
		prescaler = 1;
	}
	divider = (clk_divider + 1) * (prescaler);
	/* Bad: should check if clock is enabled at all */
	Clock_MakeDerived(sdhc->sdclk, sdhc->inclk, 1, divider);
	//fprintf(stderr,"%s: DIVIDER %d value 0x%08x rate %d\n",sdhc->name,divider,sdhc->clk_rate,  freq);
}

static void
update_interrupt(IMX21Sdhc * sdhc)
{
	int interrupt = 0;
	if ((sdhc->int_status & STATUS_CARD_PRESENCE) &&
	    (sdhc->int_cntl & INT_CNTL_CARD_DET_IRQ_EN)) {
		interrupt = 1;
	}
	if ((sdhc->int_status & STATUS_SDIO_INT_ACTIVE) && !(sdhc->int_cntl & INT_CNTL_SDIO)) {
		interrupt = 1;
	}
	if ((sdhc->int_status & STATUS_END_CMD_RESP) && !(sdhc->int_cntl & INT_CNTL_END_CMD_RES)) {
		interrupt = 1;
	}
	if ((sdhc->int_status & STATUS_WRITE_OP_DONE) && !(sdhc->int_cntl & INT_CNTL_WRITE_OP_DONE)) {
		interrupt = 1;
	}
	if ((sdhc->int_status & STATUS_READ_OP_DONE) && !(sdhc->int_cntl & INT_CNTL_READ_OP_DONE)) {
		interrupt = 1;
	}
	if ((sdhc->int_status & STATUS_APPL_BUFF_FF) && !(sdhc->int_cntl & INT_CNTL_BUF_READY)) {
		interrupt = 1;
	}
	if ((sdhc->int_status & STATUS_APPL_BUFF_FE) && !(sdhc->int_cntl & INT_CNTL_BUF_READY)) {
		interrupt = 1;
	}
	if (interrupt) {
		dbgprintf("Interrupt status %08x int_status %08x int_cntl %08x\n", sdhc->status,
			  sdhc->int_status, sdhc->int_cntl);
		SigNode_Set(sdhc->irqNode, SIG_LOW);
	} else {
		dbgprintf("No Interrupt status %08x int_status %08x int_cntl %08x\n", sdhc->status,
			  sdhc->int_status, sdhc->int_cntl);
		SigNode_Set(sdhc->irqNode, SIG_HIGH);
	}
}

/*
 * --------------------------------------------------------------------
 * shdc_reset:
 *	Set initial controller state  
 *
 * register dump from real board
 *	str_stp_clk 00000000
 *	status      0000c000
 *	clk_rate    00000008
 *	cmddatcont  00000000
 *	respto      00000040
 *	readto      0000ffff
 *	blk_len     00000000
 *	nob         00000000
 *	rev_no      00000400
 *	int_cntl    00000000
 *	cmd         00000000
 *	argh        00000000
 *	argl        00000000
 *	res_fifo    00000000
 *	bufaccess   00000000
 * --------------------------------------------------------------------
 */

static void
sdhc_reset(IMX21Sdhc * sdhc)
{
	sdhc->str_stp_clk = 0;
	/* SDIO_INT_ACTIVE is from real board register dump, not from docu */
	sdhc->status = STATUS_CARD_PRESENCE | STATUS_SDIO_INT_ACTIVE;
	sdhc->clk_rate = 8;
	sdhc->cmd_dat_cont = 0;
	sdhc->response_to = 0x40;
	sdhc->read_to = 0xffff;
	sdhc->blk_len = 0;
	sdhc->nob = 0;
	sdhc->rev_no = 0x400;
	sdhc->int_cntl = 0;
	sdhc->int_status = INT_CNTL_CARD_DET_IRQ_EN;
	sdhc->cmd = 0;
	sdhc->argh = 0;
	sdhc->argl = 0;
}

/*
 * ---------------------------------------------------------------------------------------
 * Clock Control Register
 * Bit 3:	Reset the MMC/SD host controller
 * Bit 1:	Start Clock immediately or delayed, depending on transmission state
 * Bit 0:	Stop Clock immediately or delayed, depending on transmission state
 *
 * ---------------------------------------------------------------------------------------
 */
static uint32_t
str_stp_clk_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	/* Always reads 0 */
	return sdhc->str_stp_clk;
}

static void
str_stp_clk_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	if (value & STR_STP_CLK_RESET) {
		sdhc_reset(sdhc);
	}
	if (value & STR_STP_CLK_START_CLK) {
		if (!(sdhc->status & STATUS_CARD_BUS_CLK_RUN)) {
			sdhc->status |= STATUS_CARD_BUS_CLK_RUN;
			if (sdhc->forClockWaitingTimer == &sdhc->data_delay_timer) {
				CycleTimer_Mod(&sdhc->data_delay_timer,
					       MicrosecondsToCycles(UDELAY_DATA));
			} else if (sdhc->forClockWaitingTimer == &sdhc->cmd_delay_timer) {
				CycleTimer_Mod(&sdhc->cmd_delay_timer,
					       MicrosecondsToCycles(UDELAY_CMD));
			} else if (sdhc->forClockWaitingTimer == &sdhc->write_delay_timer) {
				CycleTimer_Mod(&sdhc->write_delay_timer,
					       MicrosecondsToCycles(UDELAY_WRITE));
			}
			sdhc->forClockWaitingTimer = NULL;
		}
	} else if (value & STR_STP_CLK_STOP_CLK) {
		/* Clock run should be disabled delayed (after transmission) */
		sdhc->status &= ~(STATUS_CARD_BUS_CLK_RUN | STATUS_TIME_OUT_READ
				  | STATUS_TIME_OUT_RESP | STATUS_CRC_WRITE_ERR |
				  STATUS_CRC_READ_ERR | STATUS_RESP_CRC_ERR | STATUS_READ_OP_DONE |
				  STATUS_WRITE_OP_DONE | STATUS_END_CMD_RESP);
	} else if ((value & STR_STP_CLK_START_CLK) && (value & STR_STP_CLK_STOP_CLK)) {
		fprintf(stderr, "%s: START and STOP clock at same time is prohibited\n",
			sdhc->name);
	}
	return;
}

/*
 * ---------------------------------------------------------------------------------------------
 * Status Register
 *
 * Bit 15: Card Presence: Warning: seems to work only outside transmission periods
 *         because DAT[3] is used during transmission 
 * Bit 14: SDIO_INT_ACTIVE Indicates if SDIO card sends interrupt condition. (w1c)
 * Bit 13: END_CMD_RESP Indicates that cmd is transmitted to card and a response or timeout
 *         was received. Check RESP_CRC_ERR and TIME_OUT_RESP ! (w1c)
 * Bit 12: WRITE_OP_DONE: A write operation is finished (including writting card buffer to flash)
 *	   Check WRITE_CRC_ERR also ! (w1 or stop mmcclk to clear)
 * Bit 11: READ_OP_DONE: Read operation is finished. Check READ_CRC_ERR and TIME_OUT_READ 
 *         (w1 or stop clock to clear)
 * Bit 9,10: WR_CRC_ERROR_CODE: 00: good, 01: Transm. error 10: No crc 11: reserved
 * Bit 8:  CARD_BUS_CLK_RUN: Indicates if the clock is running
 * Bit 7:  APPL_BUFF_FF Data fifo is full
 * Bit 6:  APPL_BUFF_FE Data fifo is empty
 * Bit 5:  RESP_CRC_ERROR: 1=crc error in response fount (w1c or stop)
 * Bit 3:  CRC Read error: CRC error on Dat line during read  (w1c or stop)
 * Bit 2:  CRC_WRITE_ERROR: CRC Write error, see also error code
 * Bit 1:  TIME_OUT_RESP: Reponse timed out (w1c or stop)
 * Bit 0:  TIME_OUT_READ: no data received within READ_TO (w1c or stop)
 * 
 * ----------------------------------------------------------------------------------------------
 */
static uint32_t
status_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	if (sdhc->card) {
		sdhc->status |= STATUS_CARD_PRESENCE;
	} else {
		sdhc->status &= ~STATUS_CARD_PRESENCE;
	}
	return sdhc->status;
}

static void
status_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	uint32_t clear = value & (STATUS_SDIO_INT_ACTIVE | STATUS_END_CMD_RESP
				  | STATUS_WRITE_OP_DONE | STATUS_READ_OP_DONE | STATUS_RESP_CRC_ERR
				  | STATUS_CRC_WRITE_ERR | STATUS_TIME_OUT_RESP |
				  STATUS_TIME_OUT_READ);
	sdhc->status &= ~clear;
	return;
}

/*
 * -----------------------------------------------------------------
 * Clock Rate Register
 * 
 *	Bit 0-3:	CLK_DIVIDER
 *	Bit 4-15:	CLK_PRESCALER
 * -----------------------------------------------------------------
 */
static uint32_t
clk_rate_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->clk_rate;
}

static void
clk_rate_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	sdhc->clk_rate = value;
	update_clock(sdhc);
	return;
}

/*
 * ------------------------------------------------------------------------------------
 * CMD_DAT_CONT
 * 	CMD_RESUME: Do not start new command, resume only
 * 	RESP_LONG_OFF : allow Bit clearance on status read (Which Bit ????)  
 *	STOP_READWAIT: End the readwait cycle
 *	START_READWAIT: Start the readwait cycle
 *	BUS_WIDTH: 1 or 4 bit (MMC/SD)
 *	INIT:	Add 80 Cycles Initialization time before command
 *	WRITE:	DATA transfer is write
 *	DATA_ENABLE: command involves data tranfer  
 *	Response FORMAT: defines response format (required for size)
 * ------------------------------------------------------------------------------------
 */
static uint32_t
cmd_dat_cont_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->cmd_dat_cont;
}

/*
 * ----------------------------------------------------------------
 * do_transfer_write 
 *	Sends the data from the fifo buffer to the SD/MM Card.
 * 	Called after a short delay whenever the fifo buffer 
 *      becomes full because of buffer_access write by DMA or CPU.
 * ----------------------------------------------------------------
 */
static void
do_transfer_write(void *clientData)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	uint8_t buf[64];
	int count;
	int result;
	int rqlen = sdhc->nob * sdhc->blk_len;
	int i;
	count = (sdhc->buf_fifo_wp - sdhc->buf_fifo_rp) << 1;
	if ((count + sdhc->transfer_count) > rqlen) {
		count = rqlen - sdhc->transfer_count;
		if (count == 0) {
			fprintf(stderr, "Write complete shouldn't happen here, stop DMA\n");
			SigNode_Set(sdhc->nDmaReq, SIG_HIGH);
			return;
		}
	}
	if ((count < 0) || (count > 64)) {
		fprintf(stderr, "Sdhc: Emulator bug: Illegal fifo count %d\n", count);
		exit(1);
	}
	if (count == 0) {
		/* wait for data */
		return;
	}
	for (i = 0; i < count; i += 2) {
		uint16_t data = sdhc->buffer[sdhc->buf_fifo_rp % sdhc->buf_fifo_size];
		sdhc->buf_fifo_rp++;
		/* Little endian ??? */
		buf[i] = data & 0xff;
		buf[i + 1] = (data >> 8) & 0xff;
	}
	sdhc->transfer_count += count;
	sdhc->status &= ~STATUS_APPL_BUFF_FF;
	if (sdhc->buf_fifo_wp == sdhc->buf_fifo_rp) {
		sdhc->status |= STATUS_APPL_BUFF_FE;
		sdhc->int_status |= STATUS_APPL_BUFF_FE;
		update_interrupt(sdhc);
		if (sdhc->transfer_count < rqlen) {
			SigNode_Set(sdhc->nDmaReq, SIG_LOW);
		}
	}
	if (!sdhc->card)
		return;
	result = MMCDev_Write(sdhc->card, buf, count);
	/* What to do with result ??? */
	if (sdhc->transfer_count == rqlen) {
		/* 
		 * -----------------------------------------------------------
		 * Real board sets this when card is ready writing 
		 * its internal buffer to the flash ???? MMC doku says that
		 * that the dat line doesn't tell that the buffer is written, but
		 * it tells that there is a free buffer. This
		 * is about 6ms????6us after write done.
		 * So this should be delayed and next data transaction has
		 * to wait until it is done !!!! Do this soon now, causes
		 * emulator to work with code where driver doesn't work
		 * on real board
		 * -----------------------------------------------------------
		 */
		static int request_count = 0;
		//if(request_count != 100) {
		sdhc->status |= STATUS_WRITE_OP_DONE;
		sdhc->int_status |= STATUS_WRITE_OP_DONE;
		update_interrupt(sdhc);
		//}
		request_count++;
	}
	return;
}

/*
 * ----------------------------------------------------------------
 * receive_data 
 *	sink for data comming  from the SD-Card is 
 * 	written into fifo and the dma request is initiated. 
 *	Called by the MM/SD-Card when it sents data. 
 * ----------------------------------------------------------------
 */
static int
receive_data(void *dev, const uint8_t * buf, int len)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) dev;
	int i;
	/* Read direction */
	int fifobytes;
	int count;
	int rqlen;
	if (!sdhc->card) {
		fprintf(stderr, "Emulator Bug: Handler for non registered MMC card called\n");
		return 0;
	}
	/* Should check cmdat for read direction also here */
	if ((sdhc->cmd_dat_cont & CMD_DAT_CONT_BUS_WIDTH_MASK) == CMD_DAT_CONT_BUS_WIDTH_4) {
		fifobytes = 64;
	} else {
		fifobytes = 16;
	}
	count = fifobytes - 2 * (sdhc->buf_fifo_wp - sdhc->buf_fifo_rp);
	rqlen = sdhc->nob * sdhc->blk_len;
	if (count < len) {
		fprintf(stderr, "SDHC: not enough room for data from mmcard: tfc %d, len %d\n",
			sdhc->transfer_count, len);
	}
	if (count + sdhc->transfer_count > rqlen) {
		count = rqlen - sdhc->transfer_count;
	}
	if (count < 0) {
		fprintf(stderr, "Sdhc bug: less than zero room in transfer fifo\n");
	} else if (count < len) {
		/* Thats OK, because command might be of infinite length */
		len = count;
	} else if (count > 64) {
		fprintf(stderr, "Bug in %s line %d\n", __FILE__, __LINE__);
		exit(1);
	}
	dbgprintf("mmc read result %d bytes\n", result);
	if (len > 0 && (len <= fifobytes)) {
		sdhc->status &= ~STATUS_APPL_BUFF_FE;
		for (i = 0; i < len; i += 2) {
			sdhc->buffer[sdhc->buf_fifo_wp % sdhc->buf_fifo_size] =
			    buf[i] | (buf[i + 1] << 8);
			sdhc->buf_fifo_wp++;
		}
		sdhc->transfer_count += len;
		//fprintf(stderr,"transfer count is now %d, rqlen %d\n",sdhc->transfer_count,rqlen);
		/* Is READ_OP_DONE set when data read from Card is done or when fifo dma is done ??? */
		if (sdhc->transfer_count == rqlen) {
			sdhc->status |= STATUS_READ_OP_DONE;
			sdhc->int_status |= STATUS_READ_OP_DONE;
			update_interrupt(sdhc);
		}
		if ((sdhc->buf_fifo_wp - sdhc->buf_fifo_rp) == (fifobytes >> 1)) {
			sdhc->status |= STATUS_APPL_BUFF_FF;
			sdhc->int_status |= STATUS_APPL_BUFF_FF;
			update_interrupt(sdhc);
		} else if ((sdhc->buf_fifo_wp - sdhc->buf_fifo_rp) > (fifobytes >> 1)) {
			fprintf(stderr, "Sdhc emulator bug: more than fifosize bytes in fifo\n");
		}
		SigNode_Set(sdhc->nDmaReq, SIG_LOW);
	} else {
		return 0;
	}
	return len;
}

/*
 * -----------------------------------------------------------------------
 * Do the data transaction some time after the CMD phase 
 * Write transaction: Set the DMA request line to low
 * Read transaction: see  do_read_transfer
 * -----------------------------------------------------------------------
 */
static void
do_data_delayed(void *clientData)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	if (sdhc->cmd_dat_cont & CMD_DAT_CONT_DATA_ENABLE) {
		dbgprintf("Data involved in transfer\n");
		if ((sdhc->cmd_dat_cont & CMD_DAT_CONT_WRITE)) {
			/* should check fifo full before ? */
			// if(full)do_transfer_write(sdhc);
			SigNode_Set(sdhc->nDmaReq, SIG_LOW);
		}
	}
}

/*
 * ----------------------------------------------------------------------------------
 * Send a command to the SD/MMC card some time after the command was issued
 * by writing to the command register
 * ----------------------------------------------------------------------------------
 */
static void
do_cmd_delayed(void *clientData)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	MMCResponse resp;
	int rformat = sdhc->cmd_dat_cont & CMD_DAT_CONT_FORMAT_OF_RESPONSE_MASK;
	int result;
	uint32_t arg;
	int i;
	if (!sdhc->card) {
		dbgprintf("No card found in slot\n");
		sdhc->status |= STATUS_END_CMD_RESP;
		sdhc->status |= STATUS_TIME_OUT_RESP;
		sdhc->int_status |= STATUS_END_CMD_RESP;
		update_interrupt(sdhc);
		return;
	}
	arg = sdhc->argl | ((uint32_t) sdhc->argh << 16);
	dbgprintf("Start cmd 0x%02x \n", sdhc->cmd);
	result = MMCDev_DoCmd(sdhc->card, sdhc->cmd, arg, &resp);
	if (result != MMC_ERR_NONE) {
		sdhc->status |= STATUS_END_CMD_RESP;
		sdhc->status |= STATUS_TIME_OUT_RESP;	/* Hack, I don't know real response */
		sdhc->int_status |= STATUS_END_CMD_RESP;
		update_interrupt(sdhc);
		return;
	}
	if (resp.len > 17) {
		fprintf(stderr, "Emulator bug: SD/MMC card response longer than 17 Bytes\n");
		resp.len = 16;
	}
	/* Handle the response from SD/MMC card */
	if ((rformat == CMD_DAT_CONT_RESPONSE_FORMAT_R11B56)
	    || (rformat == CMD_DAT_CONT_RESPONSE_FORMAT_R34)) {
		for (i = 0; i < 6; i += 2) {
			sdhc->res_fifo[sdhc->res_fifo_wp % RES_FIFO_SIZE] =
			    (resp.data[i] << 8) | (resp.data[i + 1] << 0);
			sdhc->res_fifo_wp++;
		}
	} else if (rformat == CMD_DAT_CONT_RESPONSE_FORMAT_R2) {
		for (i = 1; i < 17; i += 2) {
			sdhc->res_fifo[sdhc->res_fifo_wp % RES_FIFO_SIZE] =
			    (resp.data[i] << 8) | (resp.data[i + 1] << 0);
			dbgprintf("%04x ", sdhc->res_fifo[sdhc->res_fifo_wp % RES_FIFO_SIZE]);
			sdhc->res_fifo_wp++;
		}
		dbgprintf("\n");
	} else if (rformat == CMD_DAT_CONT_NO_RESPONSE) {
		/* put nothing to fifo */
	} else {
		fprintf(stderr, "SDHC emulator: Illegal response format %d\n", rformat);
	}
	sdhc->status |= STATUS_END_CMD_RESP;
	sdhc->int_status |= STATUS_END_CMD_RESP;
	update_interrupt(sdhc);
	/* 
	 * ----------------------------------------------------------
	 * For the case that the transfer involves a data packet
	 * trigger the data transaction
	 * ----------------------------------------------------------
	 */
	if (sdhc->cmd_dat_cont & CMD_DAT_CONT_DATA_ENABLE) {
		sdhc->transfer_count = 0;
		if (sdhc->cmd_dat_cont & CMD_DAT_CONT_WRITE) {
			if (sdhc->status & STATUS_CARD_BUS_CLK_RUN) {
				CycleTimer_Mod(&sdhc->data_delay_timer,
					       MicrosecondsToCycles(UDELAY_DATA));
			} else {
				//fprintf(stderr,"Delay Data transaction until clock is running\n");
				sdhc->forClockWaitingTimer = &sdhc->data_delay_timer;
			}
		}
	}
}

static void
cmd_dat_cont_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	sdhc->cmd_dat_cont = value;
	if (value & CMD_DAT_CONT_CMD_RESUME) {
		/* ???????????? */
		return;
	}
	if (sdhc->res_fifo_wp != sdhc->res_fifo_rp) {
		fprintf(stderr, "SDHC: Response fifo not empty cmd %d, prev %d, rp %d, wp %d\n",
			sdhc->cmd, sdhc->prev_cmd, (int)sdhc->res_fifo_rp, (int)sdhc->res_fifo_wp);
		sdhc->res_fifo_rp = sdhc->res_fifo_wp;
	}
	if (sdhc->status & STATUS_CARD_BUS_CLK_RUN) {
		//fprintf(stderr,"Clock already running on start cmd %02x\n",sdhc->cmd);
		CycleTimer_Mod(&sdhc->cmd_delay_timer, MicrosecondsToCycles(UDELAY_CMD));
	} else {
		sdhc->forClockWaitingTimer = &sdhc->cmd_delay_timer;
	}
	return;
}

/* 
 * -------------------------------------------------------------------------
 * Response timeout has currently no effect
 * -------------------------------------------------------------------------
 */
static uint32_t
response_to_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->response_to;
}

static void
response_to_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	sdhc->response_to = value & 0xff;
	return;
}

static uint32_t
read_to_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->read_to;
}

static void
read_to_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	sdhc->read_to = value;
	return;
}

static uint32_t
blk_len_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->blk_len;
}

static void
blk_len_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	/* Documentation is bad ! value 0-2048 does not fit into 10 Bits */
	sdhc->blk_len = value;
	return;
}

static uint32_t
nob_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->nob;
}

static void
nob_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	sdhc->nob = value;
	return;
}

static uint32_t
rev_no_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->rev_no;
}

static void
rev_no_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMX21 SDHC: Rev. no is not writable\n");
	return;
}

/*
 * ---------------------------------------------------------------------
 * Interrupt control register
 *
 *  CARD_DET_IRQ_EN	Card detect irq enable
 *  SDIO_WAKEUP_EN	wakeup irq en
 *  DAT0_EN	
 *  SDIO
 *  BUF_READY
 *  END_CMD_RES	
 *  WRITE_OP_DONE
 *  INT_CNTL_READ_OP_DONE
 * ---------------------------------------------------------------------
 */
static uint32_t
int_cntl_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->int_cntl;
}

static void
int_cntl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	sdhc->int_cntl = value;
	/* Card presence ??? seems to be level triggered */
	sdhc->int_status &= ~(STATUS_CARD_PRESENCE | STATUS_SDIO_INT_ACTIVE | STATUS_END_CMD_RESP
			      | STATUS_WRITE_OP_DONE | STATUS_READ_OP_DONE | STATUS_APPL_BUFF_FF |
			      STATUS_APPL_BUFF_FE);

	if (value & INT_CNTL_SDIO) {
		//if(resolved)
		sdhc->status &= ~STATUS_SDIO_INT_ACTIVE;
	}
	if (value & INT_CNTL_END_CMD_RES) {
		sdhc->status &= ~STATUS_END_CMD_RESP;
	}
	if (value & INT_CNTL_WRITE_OP_DONE) {
		sdhc->status &= ~STATUS_WRITE_OP_DONE;
	}
	update_interrupt(sdhc);
	return;
}

static uint32_t
cmd_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->cmd;
}

static void
cmd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	sdhc->prev_cmd = sdhc->cmd;
	sdhc->cmd = value & 0x3f;
	return;
}

static uint32_t
argh_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->argh;
}

static void
argh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	sdhc->argh = value;
	return;
}

static uint32_t
argl_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	return sdhc->argl;
}

static void
argl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	sdhc->argl = value;
	return;
}

/*
 * ------------------------------------------------------------------------ 
 * res_fifo read/write
 * The response fifo contains the response to Card commands (not data)
 * ------------------------------------------------------------------------ 
 */
static uint32_t
res_fifo_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	uint16_t value;
	value = sdhc->res_fifo[sdhc->res_fifo_rp % RES_FIFO_SIZE];
	if ((sdhc->res_fifo_wp - sdhc->res_fifo_rp) > 16) {
		fprintf(stderr, "sdhc: response fifo overrun\n");
		// shit 
	}
	if (sdhc->res_fifo_rp < sdhc->res_fifo_wp) {
		sdhc->res_fifo_rp++;
	}
	dbgprintf("res fifo read 0x%04x\n", value);
	return value;
}

static void
res_fifo_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMX21 SDHC: response fifo not writable\n");
	return;
}

/*
 * -----------------------------------------------------------------------------
 * BUFFER_ACCESS register
 *	Writing to the register fills the Fifo and triggers an DMA when full
 *	Reading from the register empties the fifo and triggers an DMA
 *	when empty
 * -----------------------------------------------------------------------------
 */
static uint32_t
buffer_access_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	uint16_t value;
	value = sdhc->buffer[sdhc->buf_fifo_rp & (sdhc->buf_fifo_size - 1)];
	sdhc->status = sdhc->status & ~(STATUS_APPL_BUFF_FF);
	if (sdhc->buf_fifo_rp < sdhc->buf_fifo_wp) {
		sdhc->buf_fifo_rp++;
		if (sdhc->buf_fifo_rp == sdhc->buf_fifo_wp) {
			sdhc->status = sdhc->status | STATUS_APPL_BUFF_FE;
			sdhc->int_status = sdhc->int_status | STATUS_APPL_BUFF_FE;
			dbgprintf("Release DMA request\n");
			SigNode_Set(sdhc->nDmaReq, SIG_HIGH);
			update_interrupt(sdhc);
		}
	}
	return value;
}

static void
buffer_access_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) clientData;
	int fifosize;
	sdhc->status = sdhc->status & ~(STATUS_APPL_BUFF_FE);
	if ((sdhc->cmd_dat_cont & CMD_DAT_CONT_BUS_WIDTH_MASK) == CMD_DAT_CONT_BUS_WIDTH_4) {
		fifosize = 32;
	} else {
		fifosize = 8;
	}
	if ((sdhc->buf_fifo_wp - sdhc->buf_fifo_rp) < fifosize) {
		sdhc->buffer[sdhc->buf_fifo_wp & (sdhc->buf_fifo_size - 1)] = value;
		sdhc->buf_fifo_wp++;
		if ((sdhc->buf_fifo_wp - sdhc->buf_fifo_rp) == fifosize) {
			sdhc->status = sdhc->status | STATUS_APPL_BUFF_FF;
			sdhc->int_status = sdhc->int_status | STATUS_APPL_BUFF_FF;
			dbgprintf("Stop the DMA request because of full fifo\n");
			SigNode_Set(sdhc->nDmaReq, SIG_HIGH);
			update_interrupt(sdhc);
			if (sdhc->status & STATUS_CARD_BUS_CLK_RUN) {
				CycleTimer_Mod(&sdhc->write_delay_timer,
					       MicrosecondsToCycles(UDELAY_WRITE));
			} else {
				//fprintf(stderr,"Delay WRITE transaction until clock is running\n");
				sdhc->forClockWaitingTimer = &sdhc->write_delay_timer;
			}
		}
	} else {
		fprintf(stderr, "Sdhc: Buffer access fifo write overflow\n");
	}
	return;
}

static void
IMXSdhc_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete16(SDHC_STR_STP_CLK(base));
	IOH_Delete16(SDHC_STATUS(base));
	IOH_Delete16(SDHC_CLK_RATE(base));
	IOH_Delete16(SDHC_CMD_DAT_CONT(base));
	IOH_Delete16(SDHC_RESPONSE_TO(base));
	IOH_Delete16(SDHC_READ_TO(base));
	IOH_Delete16(SDHC_BLK_LEN(base));
	IOH_Delete16(SDHC_NOB(base));
	IOH_Delete16(SDHC_REV_NO(base));
	IOH_Delete16(SDHC_INT_CNTL(base));
	IOH_Delete16(SDHC_CMD(base));
	IOH_Delete16(SDHC_ARGH(base));
	IOH_Delete16(SDHC_ARGL(base));
	IOH_Delete16(SDHC_RES_FIFO(base));
	IOH_Delete16(SDHC_BUFFER_ACCESS(base));
}

static void
IMXSdhc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	IMX21Sdhc *sdhc = (IMX21Sdhc *) owner;
	IOH_New16(SDHC_STR_STP_CLK(base), str_stp_clk_read, str_stp_clk_write, sdhc);
	IOH_New16(SDHC_STATUS(base), status_read, status_write, sdhc);
	IOH_New16(SDHC_CLK_RATE(base), clk_rate_read, clk_rate_write, sdhc);
	IOH_New16(SDHC_CMD_DAT_CONT(base), cmd_dat_cont_read, cmd_dat_cont_write, sdhc);
	IOH_New16(SDHC_RESPONSE_TO(base), response_to_read, response_to_write, sdhc);
	IOH_New16(SDHC_READ_TO(base), read_to_read, read_to_write, sdhc);
	IOH_New16(SDHC_BLK_LEN(base), blk_len_read, blk_len_write, sdhc);
	IOH_New16(SDHC_NOB(base), nob_read, nob_write, sdhc);
	IOH_New16(SDHC_REV_NO(base), rev_no_read, rev_no_write, sdhc);
	IOH_New16(SDHC_INT_CNTL(base), int_cntl_read, int_cntl_write, sdhc);
	IOH_New16(SDHC_CMD(base), cmd_read, cmd_write, sdhc);
	IOH_New16(SDHC_ARGH(base), argh_read, argh_write, sdhc);
	IOH_New16(SDHC_ARGL(base), argl_read, argl_write, sdhc);
	IOH_New16(SDHC_RES_FIFO(base), res_fifo_read, res_fifo_write, sdhc);
	IOH_New16(SDHC_BUFFER_ACCESS(base), buffer_access_read, buffer_access_write, sdhc);
}

int
IMX21Sdhc_InsertCard(BusDevice * dev, MMCDev * card)
{
	IMX21Sdhc *sdhc = dev->owner;
	if (sdhc->card) {
		fprintf(stderr, "Error: Can not plug a second SD-Card\n");
		return -1;
	}
	sdhc->card = card;
	MMCard_AddListener(card, sdhc, 16, receive_data);
	return 0;
}

int
IMX21Sdhc_RemoveCard(BusDevice * dev, MMCDev * card)
{
	IMX21Sdhc *sdhc = dev->owner;
	if (!sdhc->card) {
		fprintf(stderr, "Error: SD Card Slot is already empty, can not remove\n");
		return -1;
	}
	MMCard_RemoveListener(card, sdhc);
	sdhc->card = NULL;
	return 0;
}

BusDevice *
IMX21_SdhcNew(const char *name)
{
	IMX21Sdhc *sdhc = sg_new(IMX21Sdhc);
	sdhc->irqNode = SigNode_New("%s.irq", name);
	sdhc->name = sg_strdup(name);
	if (!sdhc->irqNode) {
		fprintf(stderr, "i.MX21 SDHC: can not create interrupt line\n");
		exit(1);
	}
	sdhc->nDmaReq = SigNode_New("%s.dma_req", name);
	if (!sdhc->nDmaReq) {
		fprintf(stderr, "i.MX21 SDHC: can not create DMA request line\n");
		exit(1);
	}
	sdhc_reset(sdhc);
	sdhc->buf_fifo_size = MAX_BUF_FIFO_SIZE;
	sdhc->bdev.first_mapping = NULL;
	sdhc->bdev.Map = IMXSdhc_Map;
	sdhc->bdev.UnMap = IMXSdhc_Unmap;
	sdhc->bdev.owner = sdhc;
	sdhc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	sdhc->inclk = Clock_New("%s.inclk", name);
	sdhc->sdclk = Clock_New("%s.sdclk", name);
	/* Documentation has contradictions about clock source for SD-Clk */
	Clock_SetFreq(sdhc->inclk, 66500013.419 / 2);	/* Should be connected to clock module ipg_perclk or perclk2  */
	CycleTimer_Init(&sdhc->cmd_delay_timer, do_cmd_delayed, sdhc);
	CycleTimer_Init(&sdhc->data_delay_timer, do_data_delayed, sdhc);
	CycleTimer_Init(&sdhc->write_delay_timer, do_transfer_write, sdhc);
	fprintf(stderr, "i.MX21 SD-Card host controller \"%s\" created\n", name);
	return &sdhc->bdev;
}
