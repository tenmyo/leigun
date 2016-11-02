/*
 *************************************************************************************************
 *
 * Emulation of AT91SAM7SE SPI controller 
 *
 * state: not implemented. 
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
#include "at91sam7se_spi.h"
#include "bus.h"
#include "sgstring.h"

#define SPI_CR(base)	((base) + 0x00)
#define		CR_SPIEN	(1 << 0)
#define		CR_SPIDIS	(1 << 1)
#define		CR_SWRST	(1 << 7)
#define		CR_LASTXFER	(1 << 24)
#define SPI_MR(base)	((base) + 0x04)
#define		MR_DLYBCS_MSK	(0xff << 24)
#define		MR_PCS_MSK	(0xf << 16)
#define 	MR_LLB		(1 << 7)
#define		MR_MODFDIS	(1 << 4)
#define		MR_PSCDEC	(1 << 2)
#define		MR_PS		(1 << 1)
#define		MR_MSTR		(0)
#define SPI_RDR(base)	((base) + 0x08)
#define		RDR_PCS_MSK	(0xf << 16)
#define		RDR_RD_MSK	(0xffff)
#define SPI_TDR(base)	((base) + 0x0c)
#define		TDR_LASTXFER	(1 << 24)
#define		TDR_PCS_MSK	(0xf << 16)
#define		TDR_TD_MSK	(0xffff)
#define SPI_SR(base)	((base) + 0x10)
#define		SR_SPIENS	(1 << 16)
#define		SR_TXEMPTY	(1 << 9)
#define		SR_NSSR		(1 << 8)
#define		SR_TXBUFE	(1 << 7)
#define		SR_RXBUFF	(1 << 6)
#define		SR_ENDTX	(1 << 5)
#define		SR_ENDRX	(1 << 4)
#define		SR_OVRES	(1 << 3)
#define		SR_MODF		(1 << 2)
#define		SR_TDRE		(1 << 1)
#define		SR_RDRF		(1 << 0)
#define SPI_IER(base)	((base) + 0x14)
#define		IER_TXEMPTY	(1 << 9)
#define		IER_NSSR	(1 << 8)
#define		IER_TXBUFE	(1 << 7)
#define		IER_RXBUFF	(1 << 6)
#define		IER_ENDTX	(1 << 5)
#define		IER_ENDRX	(1 << 4)
#define		IER_OVRES	(1 << 3)
#define		IER_MODF	(1 << 2)
#define		IER_TDRE	(1 << 1)
#define		IER_RDRF	(1 << 0)
#define SPI_IDR(base)	((base) + 0x18)
#define		IDR_TXEMPTY	(1 << 9)
#define		IDR_NSSR	(1 << 8)
#define		IDR_TXBUFE	(1 << 7)
#define		IDR_RXBUFF	(1 << 6)
#define		IDR_ENDTX	(1 << 5)
#define		IDR_ENDRX	(1 << 4)
#define		IDR_OVRES	(1 << 3)
#define		IDR_MODF	(1 << 2)
#define		IDR_TDRE	(1 << 1)
#define		IDR_RDRF	(1 << 0)
#define SPI_IMR(base)	((base) + 0x1C)
#define		IMR_TXEMPTY	(1 << 9)
#define		IMR_NSSR	(1 << 8)
#define		IMR_TXBUFE	(1 << 7)
#define		IMR_RXBUFF	(1 << 6)
#define		IMR_ENDTX	(1 << 5)
#define		IMR_ENDRX	(1 << 4)
#define		IMR_OVRES	(1 << 3)
#define		IMR_MODF	(1 << 2)
#define		IMR_TDRE	(1 << 1)
#define		IMR_RDRF	(1 << 0)
#define SPI_CSR0(base)	((base) + 0x30)
#define SPI_CSR1(base)	((base) + 0x34)
#define SPI_CSR2(base)	((base) + 0x38)
#define SPI_CSR3(base)	((base) + 0x3C)
#define		CSR_DLYBCT_MSK	(0xff << 24)
#define		CSR_DLYBS_MSK	(0xff << 16)
#define		CSR_SCBR_MSK	(0xff << 8)
#define		CSR_BITS_MSK	(0xf << 4)
#define		CSR_SAAT	(1 << 3)
#define		CSR_NCPHA	(1 << 1)
#define		CSR_CPOL	(1 << 0)

#define MAX_SCRIPT_LEN	(256)

#define INSTR_SET_MOSI		(0x01000000)
#define INSTR_SET_SCK		(0x02000000)
#define INSTR_SAMPLE_MISO	(0x03000000)
#define INSTR_SET_CS		(0x04000000)

typedef struct AT91Sam_Spi {
	BusDevice bdev;
	uint32_t code[MAX_SCRIPT_LEN];
	uint32_t code_ip;
	uint32_t code_wp;
} AT91Spi;

/**
 *********************************************************
 * SPIEN Enable the Spi Controller
 * SPIDIS Disable the Spi Controller 
 * SWRST Reset the Spi
 * LASTXFER	
 *********************************************************
 */
static uint32_t
cr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

/*
 ********************************************************************************
 * Spi Mode register
 * Bit 24: MR_DLYBCS_MSK
 * Bit 16-20:  MR_PCS fixed chipselect
 * Bit 7: MR_LLB Local loopback 
 * Bit 4  MR_MODFDIS Mode fault disable	
 * Bit 2: MR_PSCDEC 1 = Chip select through 4 to 16 decoder, 0 = direct CS
 * Bit 1: MR_PS
 * Bit 0: MR_MSTR 1 = Master Mode, 0 = Slave mode
 ********************************************************************************
 */
static uint32_t
mr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
mr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

/**
 ********************************************************************************
 * Receive data register
 ********************************************************************************
 */

static uint32_t
rdr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
rdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

static uint32_t
tdr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
tdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

static uint32_t
sr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

static uint32_t
ier_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

static uint32_t
idr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
idr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

static uint32_t
imr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
imr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

static uint32_t
csr0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
csr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

static uint32_t
csr1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
csr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

static uint32_t
csr2_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
csr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

static uint32_t
csr3_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
csr3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return;
}

static void
AT91Spi_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(SPI_CR(base));
	IOH_Delete32(SPI_MR(base));
	IOH_Delete32(SPI_RDR(base));
	IOH_Delete32(SPI_TDR(base));
	IOH_Delete32(SPI_SR(base));
	IOH_Delete32(SPI_IER(base));
	IOH_Delete32(SPI_IDR(base));
	IOH_Delete32(SPI_IMR(base));
	IOH_Delete32(SPI_CSR0(base));
	IOH_Delete32(SPI_CSR1(base));
	IOH_Delete32(SPI_CSR2(base));
	IOH_Delete32(SPI_CSR3(base));
}

static void
AT91Spi_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Spi *spi = (AT91Spi *) owner;
	IOH_New32(SPI_CR(base), cr_read, cr_write, spi);
	IOH_New32(SPI_MR(base), mr_read, mr_write, spi);
	IOH_New32(SPI_RDR(base), rdr_read, rdr_write, spi);
	IOH_New32(SPI_TDR(base), tdr_read, tdr_write, spi);
	IOH_New32(SPI_SR(base), sr_read, sr_write, spi);
	IOH_New32(SPI_IER(base), ier_read, ier_write, spi);
	IOH_New32(SPI_IDR(base), idr_read, idr_write, spi);
	IOH_New32(SPI_IMR(base), imr_read, imr_write, spi);
	IOH_New32(SPI_CSR0(base), csr0_read, csr0_write, spi);
	IOH_New32(SPI_CSR1(base), csr1_read, csr1_write, spi);
	IOH_New32(SPI_CSR2(base), csr2_read, csr2_write, spi);
	IOH_New32(SPI_CSR3(base), csr3_read, csr3_write, spi);
}

BusDevice *
AT91Sam7Se_SpiNew(const char *name)
{
	AT91Spi *spi = sg_new(AT91Spi);
	spi->bdev.first_mapping = NULL;
	spi->bdev.Map = AT91Spi_Map;
	spi->bdev.UnMap = AT91Spi_UnMap;
	spi->bdev.owner = spi;
	spi->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &spi->bdev;
}
