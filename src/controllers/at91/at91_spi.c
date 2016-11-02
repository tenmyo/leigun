/*
 ****************************************************************************************************
 *
 * Emulation of the AT91 SPI interface 
 *
 *  State: nothing implemented 
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
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

#include "signode.h"
#include "bus.h"
#include "sgstring.h"
#include "sglib.h"
#include "clock.h"
#include <stdio.h>
#include <stdlib.h>
#include "at91_spi.h"
#include "spidevice.h"

#define SPI_CR(base)	((base) + 0x00)
#define	CR_SPIEN	(1 << 0)
#define CR_SPIDIS	(1 << 1)
#define SPI_MR(base)	((base) + 0x04)
#define SPI_RDR(base)	((base) + 0x08)
#define SPI_TDR(base)	((base) + 0x0c)
#define SPI_SR(base)	((base) + 0x10)
#define SR_RDRF		(1 << 0)
#define SR_TDRE		(1 << 1)
#define SR_MODF		(1 << 2)
#define SR_OVRES	(1 << 3)
#define SR_ENDRX	(1 << 4)
#define SR_ENDTX	(1 << 5)
#define SR_RXBUFF	(1 << 6)
#define SR_TXBUFE	(1 << 7)
#define SR_NSSR		(1 << 8)
#define SR_TXEMPTY	(1 << 9)
#define SR_SPIENS	(1 << 16)
#define SPI_IER(base)	((base) + 0x14)
#define SPI_IDR(base)	((base) + 0x18)
#define SPI_IMR(base)	((base) + 0x1c)
#define SPI_CSR0(base)	((base) + 0x30)
#define SPI_CSR1(base)  ((base) + 0x34)
#define SPI_CSR2(base)	((base) + 0x38)
#define SPI_CSR3(base)  ((base) + 0x3c)

#define SPI_CR(base) 	((base) + 0x00)
#define SPI_MR(base) 	((base) + 0x04)
#define SPI_RDR(base)	((base) + 0x08)
#define SPI_TDR(base)	((base) + 0x0c)
#define	SPI_SR(base)	((base) + 0x10)
#define SPI_IER(base)	((base) + 0x14)
#define SPI_IDR(base)	((base) + 0x18)
#define SPI_IMR(base)	((base) + 0x1c)
#define SPI_CSR0(base)	((base) + 0x30)
#define SPI_CSR1(base)	((base) + 0x34)
#define SPI_CSR2(base)	((base) + 0x38)
#define SPI_CSR3(base)	((base) + 0x3c)

typedef struct AT91Spi {
	BusDevice bdev;
	char *name;
	Spi_Device *spidev;
	SigNode *sigMiso;
        SigNode *sigMosi;
        SigNode *sigSck;
        SigNode *sigCS[4];
        SigNode *sigIrq;
	
	Clock_t *clkIn;
	Clock_t *clkSpi;
} AT91Spi;

static uint32_t
cr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
cr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
mr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
mr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
rdr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
rdr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
tdr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
tdr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
sr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return SR_SPIENS | SR_RXBUFF;
}

static void
sr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
ier_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
ier_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
idr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
idr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
imr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
imr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
csr0_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
csr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
csr1_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
csr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
csr2_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
csr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static uint32_t
csr3_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
        return 0;
}

static void
csr3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SPI controller %s not implemented\n",__func__);
}

static void
AT91Spi_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	AT91Spi *spi = owner;
	IOH_New32(SPI_CR(base),cr_read,cr_write,spi);
	IOH_New32(SPI_MR(base),mr_read,mr_write,spi);	
	IOH_New32(SPI_RDR(base),rdr_read,rdr_write,spi);
	IOH_New32(SPI_TDR(base),tdr_read,tdr_write,spi);
	IOH_New32(SPI_SR(base),sr_read,sr_write,spi);	
	IOH_New32(SPI_IER(base),ier_read,ier_write,spi);
	IOH_New32(SPI_IDR(base),idr_read,idr_write,spi);
	IOH_New32(SPI_IMR(base),imr_read,imr_write,spi);
	IOH_New32(SPI_CSR0(base),csr0_read,csr0_write,spi);
	IOH_New32(SPI_CSR1(base),csr1_read,csr1_write,spi);
	IOH_New32(SPI_CSR2(base),csr2_read,csr2_write,spi);
	IOH_New32(SPI_CSR3(base),csr3_read,csr3_write,spi);
}

static void
AT91Spi_UnMap(void *owner,uint32_t base,uint32_t mask)
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
spidev_xmit(void *owner,uint8_t *data,int bits) {
#if 0
        M32C_Uart *mua = owner;
        if((mua->reg_uic1 & UiC1_RE)) {
                mua->reg_uirb = *data;
                if(!(mua->reg_uic1 & UiC1_RI)) {
                        mua->reg_uic1 |= UiC1_RI;
                        post_rx_interrupt(mua);
                }
        }
#endif
}

BusDevice *
AT91Spi_New(const char *name)
{
	AT91Spi *spi = sg_new(AT91Spi);
	char *spidev_name = alloca(30 + strlen(name));
	//char *spidev_clkname = alloca(34 + strlen(name));

        spi->bdev.first_mapping = NULL;
        spi->bdev.Map = AT91Spi_Map;
        spi->bdev.UnMap = AT91Spi_UnMap;
        spi->bdev.owner = spi;
        spi->bdev.hw_flags = MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	sprintf(spidev_name,"%s.spi",name);
        spi->name = sg_strdup(name);
	spi->spidev = SpiDev_New(spidev_name,spidev_xmit,spi);

        fprintf(stderr,"AT91 SPI controller \"%s\" created\n",name);
        return &spi->bdev;
}

