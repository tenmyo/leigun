 /* 
 ************************************************************************************************ 
 *
 * R8C Uarts 
 *
 * State: Working but Baudrate not implemented.
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
 ************************************************************************************************ 
 */


#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "sgstring.h"
#include "serial.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "uart_r8c.h"
#include "spidevice.h"
#include "m16c_cpu.h"
#include "fio.h"

#define UiMR_SMD_MSK	(7)
#define 	UiMR_SMD_DISA	(0)
#define		UiMR_SMD_SSI	(1)
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
#define		UiC0_CLK_F32	(2)
#define		UiC0_FLK_UNDEF	(3)
#define UiC0_TXEPT	(1 << 3)
#define UiC0_NCH	(1 << 5) 
#define UiC0_CKPOL	(1 << 6)
#define UiC0_UFORM	(1 << 7)

#define UiC1_TE		(1)
#define UiC1_TI		(1 << 1)
#define UiC1_RE		(1 << 2)
#define UiC1_RI		(1 << 3)
#define UiC1_UiRS	(1 << 4)
#define UiC1_UiRRM	(1 << 5)

typedef struct UartAddrs {
	uint32_t aUiTB;
	uint32_t aUiRB;
	uint32_t aUiBRG;
	uint32_t aUiMR;
	uint32_t aUiC0;
	uint32_t aUiC1;
} UartAddrs;

#define REG_U1SR 0xf5
#define REG_PMR	 0xf8

static UartAddrs uart_addrs[] = {
	{
		.aUiTB = 0xa2,
		.aUiRB = 0xa6,
		.aUiBRG = 0xa1,
		.aUiMR = 0xa0,
		.aUiC0 = 0xa4,
		.aUiC1 = 0xa5,
	},
	{
		.aUiTB = 0xaa,
		.aUiRB = 0xae,
		.aUiBRG = 0xa9,
		.aUiMR = 0xa8,
		.aUiC0 = 0xac,
		.aUiC1 = 0xad,
	},
};


#define TXFIFO_SIZE 8 
#define TXFIFO_LVL(mua) ((mua)->txfifo_wp - (mua)->txfifo_rp)
#define TXFIFO_WP(mua) ((mua)->txfifo_wp % TXFIFO_SIZE)
#define TXFIFO_RP(mua) ((mua)->txfifo_rp % TXFIFO_SIZE)

typedef struct R8C_Uart {
	BusDevice bdev;
	unsigned int register_set;
	SigNode *rxIrq;
	SigNode *txIrq;
	UartPort *backend;
	Spi_Device *spidev;
        CycleCounter_t byte_time;
        CycleTimer rx_baud_timer;
        CycleTimer tx_baud_timer;
	
        UartChar txfifo[TXFIFO_SIZE];
        uint64_t txfifo_wp;
        uint64_t txfifo_rp;


	uint16_t tx_shift_reg;
	uint8_t reg_uimr;
	uint8_t reg_uibrg;
	uint8_t reg_uic0;
	uint8_t reg_uic1;
	uint16_t reg_uitb; 
	uint16_t reg_uirb;
} R8C_Uart;

static void 
post_tx_interrupt(R8C_Uart *mua) 
{
	SigNode_Set(mua->txIrq,SIG_LOW);
	SigNode_Set(mua->txIrq,SIG_HIGH);
}

static void 
post_rx_interrupt(R8C_Uart *mua) 
{
	SigNode_Set(mua->rxIrq,SIG_LOW);
	SigNode_Set(mua->rxIrq,SIG_HIGH);
}

static uint32_t
uimr_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Uart *mua = (R8C_Uart *) clientData;
        return mua->reg_uimr;
}

static void
update_spidev_config(R8C_Uart *mua) 
{
	
	uint8_t mode = mua->reg_uimr & UiMR_SMD_MSK;
	uint32_t spi_control;
	if(mode == UiMR_SMD_SSI) {
		spi_control = SPIDEV_BITS(8);
		if(mua->reg_uimr & UiMR_CKDIR) {
			spi_control |= SPIDEV_SLAVE;
		} else {
			spi_control |= SPIDEV_MASTER;
		}
		spi_control |= SPIDEV_CPHA1;
		if(mua->reg_uic0 & UiC0_CKPOL) {
			spi_control |= SPIDEV_CPOL0;
		} else {
			spi_control |= SPIDEV_CPOL1;
		}
		if(mua->reg_uic0 & UiC0_UFORM) {
			spi_control |= SPIDEV_MSBFIRST; 
		} else {
			spi_control |= SPIDEV_LSBFIRST;
		}
		SpiDev_Configure(mua->spidev,spi_control);
	} else {
		SpiDev_Configure(mua->spidev,SPIDEV_DISA);
	}
	
}

static void
uimr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Uart *mua = (R8C_Uart*) clientData;
	uint8_t mode = value & UiMR_SMD_MSK;
	uint8_t oldmode = mua->reg_uimr & UiMR_SMD_MSK;
	mua->reg_uimr = value;
	if(oldmode != mode) {
		update_spidev_config(mua);
	}
}

static uint32_t
uic0_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Uart *mua = (R8C_Uart *) clientData;
//	fprintf(stderr,"uic0 read %02x\n",mua->reg_uic0);
        return mua->reg_uic0;
}

static void
uic0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Uart *mua = (R8C_Uart *) clientData;
        mua->reg_uic0 = (value & ~UiC0_TXEPT) | (mua->reg_uic0 & UiC0_TXEPT);
//	fprintf(stderr,"uic0 written to %02x\n",mua->reg_uic0);
	update_spidev_config(mua);
}

static uint32_t
uibrg_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Uart *mua = (R8C_Uart *) clientData;
        return mua->reg_uibrg;
}

static void
uibrg_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

	//fprintf(stderr,"UBRG write: \n");
	//sleep(1);
}

static uint32_t
uic1_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Uart *mua = (R8C_Uart *) clientData;
	fprintf(stderr,"UIC1 read: \n");
#if 0
	if((mua->reg_uimr & 3) == 1) {
		return UiC1_RI;
	}
#endif
        return mua->reg_uic1;
}

static void
uic1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Uart *mua = (R8C_Uart *) clientData;
	mua->reg_uic1 = value;
	
	//fprintf(stderr,"UIC1 write: %04x\n",value);
	//sleep(1);
       	if((mua->reg_uimr & UiMR_SMD_MSK) != UiMR_SMD_SSI) {
		if(mua->reg_uic1 & UiC1_RE) {
			SerialDevice_StartRx(mua->backend);
		}
	}
}

static uint32_t
uitb_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

#include "m16c_cpu.h"
static void
uitb_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Uart *mua = (R8C_Uart *)clientData;
	mua->reg_uitb = value; 
	if(!(mua->reg_uic1 & UiC1_TE)) {
		fprintf(stderr,"Uart 3 UITB write %04x at %08x with disabled transmitter %02x\n",value,M16C_REG_PC,mua->reg_uic1);
		return;
	}
	//CycleTimer_Mod(&mua->tx_baud_timer,mua->byte_time);

	if(mua->reg_uic0 & UiC0_TXEPT) {
		mua->tx_shift_reg = value;
		mua->reg_uic0 &= ~UiC0_TXEPT;
        	if((mua->reg_uimr & UiMR_SMD_MSK) == UiMR_SMD_SSI) {
			uint8_t data = mua->tx_shift_reg;
			SpiDev_StartXmit(mua->spidev,&data,8);
			//fprintf(stderr,"Spidev started xmit\n");
		} else  {
			CycleTimer_Mod(&mua->tx_baud_timer,mua->byte_time);
		}
		/* 
 		 *********************************************************
		 * If interrupt occurs on no data in uitb register 
		 * the interrupt is triggered immediately.
 		 *********************************************************
 		 */
		if(!(mua->reg_uic1 & UiC1_UiRS)) {
			post_tx_interrupt(mua);	
		}
	} else {
		mua->reg_uic1 &= ~UiC1_TI;
	}
//	fprintf(stderr,"Exit after uart write %04x\n",value);
}

static uint32_t
uirb_read(void *clientData,uint32_t address,int rqlen)
{
	uint16_t uirb;
	R8C_Uart *mua = (R8C_Uart *)clientData;
	uirb = mua->reg_uirb;
	mua->reg_uic1 &= ~UiC1_RI;
	//update_rx_interrupt(mua);
        return uirb;
}

static void
uirb_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static void
rx_next(void *clientData)
{
        R8C_Uart *mua = (R8C_Uart *)clientData;
       	if((mua->reg_uimr & UiMR_SMD_MSK) != UiMR_SMD_SSI) {
		if(mua->reg_uic1 & UiC1_RE) {
			SerialDevice_StartRx(mua->backend);
		}
	}
}

static void
tx_done(void *clientData)
{
        R8C_Uart *mua = (R8C_Uart *)clientData;
	if(TXFIFO_LVL(mua) == TXFIFO_SIZE) {
		CycleTimer_Mod(&mua->tx_baud_timer,0);
		return;
        }
        mua->txfifo[TXFIFO_WP(mua)] = mua->tx_shift_reg;
        mua->txfifo_wp++;
        if(TXFIFO_LVL(mua) == 1) {
                SerialDevice_StartTx(mua->backend);
        }
	if(mua->reg_uic1 & UiC1_TI) {
		/* Nothing in UiTB -> Transmitter is now empty */ 
		mua->reg_uic0 |= UiC0_TXEPT;
		/* Check if interrupt is triggered by completed operation */ 
		if(mua->reg_uic1 & UiC1_UiRS) {
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
		if(!(mua->reg_uic1 & UiC1_UiRS)) {
			post_tx_interrupt(mua);	
		}
		CycleTimer_Mod(&mua->tx_baud_timer,mua->byte_time);
	}
	/* Feed the byte back in Syncmode for now */
        if((mua->reg_uimr & UiMR_SMD_MSK) == UiMR_SMD_SSI) {
		//fprintf(stderr,"sout Feedback sync %02x\n",mua->reg_uimr);
		//exit(1);
                //mua->reg_uirb = data;
                if(!(mua->reg_uic1 & UiC1_RI)) {
                        mua->reg_uic1 |= UiC1_RI;
                        post_rx_interrupt(mua);
                }
        }
        return;

#if 0
        R8C_Uart *mua = (R8C_Uart *)clientData;
        /* Force the emulator not to be faster than the output channel */
        while((usart->txfifo_wp - usart->txfifo_rp) == TXFIFO_SIZE) {
                FIO_HandleInput();
        }
        update_interrupts(usart);
#endif
}

static void
serial_input(void *cd) {
        R8C_Uart *mua = cd;
        UartChar c;
        int count = SerialDevice_Read(mua->backend,&c,1);
        if(count == 1) {
		mua->reg_uirb = c;
		if(!(mua->reg_uic1 & UiC1_RI)) {
			mua->reg_uic1 |= UiC1_RI;
			post_rx_interrupt(mua);
		}
		SerialDevice_StopRx(mua->backend);
		CycleTimer_Mod(&mua->rx_baud_timer, mua->byte_time);
        } 
}

static void
spidev_xmit(void *owner,uint8_t *data,int bits) {
        R8C_Uart *mua = owner;
	if((mua->reg_uic1 & UiC1_RE)) {
		mua->reg_uirb = *data;
		if(!(mua->reg_uic1 & UiC1_RI)) {
			mua->reg_uic1 |= UiC1_RI;
			post_rx_interrupt(mua);
		}
	}
	if(mua->reg_uic1 & UiC1_TI) {
		/* Nothing in UiTB -> Transmitter is now empty */ 
		mua->reg_uic0 |= UiC0_TXEPT;
        	SerialDevice_StopTx(mua->backend);
		/* Check if interrupt is triggered by completed operation */ 
		if(mua->reg_uic1 & UiC1_UiRS) {
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
		mua->tx_shift_reg = mua->reg_uitb;
		mua->reg_uic1 |= UiC1_TI;
		/* Check if interrupt is triggered by empty uitb */ 
		if(!(mua->reg_uic1 & UiC1_UiRS)) {
			post_tx_interrupt(mua);	
		}
		data = mua->tx_shift_reg;
		SpiDev_StartXmit(mua->spidev,&data,8);
	}
        return;
}


static void
serial_output(void *cd) {
	R8C_Uart *mua = cd;
        int count;
        UartChar data;
        while(mua->txfifo_rp != mua->txfifo_wp) {
                data = mua->txfifo[TXFIFO_RP(mua)];
                count = SerialDevice_Write(mua->backend,&data,1);
                if(count == 0) {
                        return;
                }
                mua->txfifo_rp++;
        }
        SerialDevice_StopTx(mua->backend);

}

static void
R8CUart_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	R8C_Uart *mua = (R8C_Uart *) owner;
   	UartAddrs *ar = &uart_addrs[mua->register_set];
	IOH_Delete16(ar->aUiTB); 
	IOH_Delete16(ar->aUiRB);
	IOH_Delete8(ar->aUiMR);
	IOH_Delete8(ar->aUiBRG);
	IOH_Delete8(ar->aUiC0);
	IOH_Delete8(ar->aUiC1);

}

static void
R8CUart_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	R8C_Uart *mua = (R8C_Uart *) owner;
   	UartAddrs *ar = &uart_addrs[mua->register_set];
	IOH_New16(ar->aUiTB,uitb_read,uitb_write,mua); 
	IOH_New16(ar->aUiRB,uirb_read,uirb_write,mua);
	IOH_New8(ar->aUiMR,uimr_read,uimr_write,mua);
	IOH_New8(ar->aUiBRG,uibrg_read,uibrg_write,mua);
	IOH_New8(ar->aUiC0,uic0_read,uic0_write,mua);
	IOH_New8(ar->aUiC1,uic1_read,uic1_write,mua);
}

BusDevice *
R8CUart_New(const char *name,unsigned int register_set) 
{
	R8C_Uart *mua = sg_new(R8C_Uart);
	char *spidev_name = alloca(30 + strlen(name));
	sprintf(spidev_name,"%s.spi",name);
	mua->bdev.first_mapping=NULL;
	mua->bdev.Map=R8CUart_Map;
	mua->bdev.UnMap=R8CUart_Unmap;
	mua->bdev.owner=mua;
	mua->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	mua->backend = Uart_New(name,serial_input,serial_output,NULL,mua);
	mua->spidev = SpiDev_New(spidev_name,spidev_xmit,mua);
	if(register_set > array_size(uart_addrs)) {
		fprintf(stderr,"Illegal register set index for uart %s\n",name);
		exit(1);
	}
	mua->register_set = register_set;
	mua->reg_uimr = 0;
	mua->reg_uic0 = UiC0_TXEPT;
	mua->reg_uic1 = 0x2;
	CycleTimer_Init(&mua->tx_baud_timer,tx_done,mua);
        CycleTimer_Init(&mua->rx_baud_timer,rx_next,mua);
	mua->byte_time = 1000;
	mua->rxIrq = SigNode_New("%s.rxirq",name);
	mua->txIrq = SigNode_New("%s.txirq",name);
	if(!mua->rxIrq || !mua->txIrq) {
		fprintf(stderr,"Can not create interrupt line for \"%s\"\n",name);
		exit(1);
	}
        fprintf(stderr,"R8C Uart \"%s\" created\n",name);
        return &mua->bdev;
	
}
