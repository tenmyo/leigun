/*
 *************************************************************************************************
 *
 * Emulation of the NS9750 Ethernet Controller
 *
 * State:
 *	Working well, statistics is missing 
 *	Multicast hashtable is missing
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

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <linux-tap.h>
#include <ns9750_eth.h>
#include <ns9750_timer.h>
#include <fio.h>
#include <bus.h>
#include <phy.h>
#include "byteorder.h"
#include "signode.h"
#include "sgstring.h"

#if 0
#define simulate_errors 1
#else
#define simulate_errors 0
#endif

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif
#define RSTSTT_ON_RD(eth) ((eth)->egcr2 & EGCR2_AUTOZ)

typedef struct RBDescriptor {
	uint32_t source;
	uint32_t blength;
	uint32_t destination; // unused
	uint32_t flags;
} RBDescriptor;


typedef struct TBDescriptor {
	uint32_t source;
	uint32_t blength;
	uint32_t destination; // unused
	uint32_t flags;
} TBDescriptor;

#define MAX_PHYS (32)
typedef struct NS9750eth {
	BusDevice bdev;
	PHY_Device *phy[MAX_PHYS];
	int ether_fd;
	FIO_FileHandler input_fh;
        int receiver_is_enabled;
        int rxint_posted;
        int txint_posted;

	uint32_t egcr1;
	uint32_t egcr2;
	uint32_t egsr;
	uint32_t etsr;
	uint32_t ersr;
	uint32_t mac1,mac2;
	uint32_t maxf;
	uint32_t safr;
	uint32_t ipgt;
	uint32_t ipgr;
	uint32_t clrt;
	uint32_t supp;
	uint32_t mcfg;
	uint32_t mcmd;
	uint32_t madr;
	uint32_t mwtd;
	uint32_t mrdd;
	uint32_t mind;
	uint8_t sa[6];
	uint32_t ht1,ht2;

	uint32_t tr64; 
	uint32_t tr127;
	uint32_t tr255;
	uint32_t tr511;
	uint32_t tr1k; 
	uint32_t trmax;
	uint32_t trmgv;
	uint32_t rbyt;
	uint32_t rpkt;
	uint32_t rfcs;
	uint32_t rmca;
	uint32_t rbca;
	uint32_t rxcf;
	uint32_t rxpf;
	uint32_t rxuo;
	uint32_t raln;
	uint32_t rflr;
	uint32_t rcde;
	uint32_t rcse;
	uint32_t rund;
	uint32_t rovr;
	uint32_t rfrg;
	uint32_t rjbr;
	uint32_t tbyt;
	uint32_t tpkt;
	uint32_t tmca;
	uint32_t tbca;
	uint32_t tdfr;
	uint32_t tedf;
	uint32_t tscl;
	uint32_t tmcl;
	uint32_t tlcl;
	uint32_t txcl;
	uint32_t tncl;
	uint32_t tjbr;
	uint32_t tfcs;
	uint32_t tovr;
	uint32_t tund;
	uint32_t tfrg;

	uint32_t car1;
	uint32_t car2;
	uint32_t cam1;
	uint32_t cam2;

	uint32_t rxptr[4];
	uint32_t rxoff[4]; 
	uint32_t eintr;
	uint32_t eintren;
	uint32_t txoff;
	uint32_t txptr; 	// initial buffer word address 
	uint32_t txrptr;	// recover buffer word address 
	uint32_t txerbd;	// error buffer word address
	uint32_t txsptr;	// stall buffer word address
	RBDescriptor int_rbdescr[4];
	uint32_t txbd_ram[256];

	uint8_t rxfifo[2048];
	uint32_t rxfifo_count;

	/* 
         * TX Fifo is only 256 Bytes in real device, 
	 * here it is used to assemble 
	 * a single packet from multiple buffers 
	 */
	uint8_t txfifo[2048];
	uint32_t txfifo_count;

	uint32_t rxstatusfifo[32];
	uint32_t rxstatusfifo_count;

	int tx_stalled;
	int tx_enabled;
	int stat_enabled;
	int mii_enabled;

	/* Endianness for the DMA Transfer of data (not transfer descriptors) */
	SigNode *dataEndianNode;
        SigTrace *dataEndianTrace;
        int dataendian;
	SigNode *sigRxIrq;
	SigNode *sigTxIrq;
} NS9750eth; 

/* Interrupt enable and Interrupt status */
#define IR_TX_IDLE 	(1<<0)
#define IR_TX_ERR  	(1<<1)
#define IR_TX_DONE 	(1<<2)
#define IR_TX_BUFNR 	(1<<3)
#define IR_TX_BUFC	(1<<4)
#define IR_ST_OVFL	(1<<6)
#define IR_RXBR		(1<<16)
#define IR_RXBUFFUL  	(1<<17) 
#define IR_RXNOBUF	(1<<18)
#define	IR_RXDONED	(1<<19)
#define	IR_RXDONEC	(1<<20)
#define	IR_RXDONEB	(1<<21)
#define	IR_RXDONEA	(1<<22)
#define IR_RXDONE(x)	(1<<(22-x))
#define IR_RXBUFC	(1<<23)
#define IR_RXOVFL_STAT  (1<<24)
#define IR_RXOVFL_DATA	(1<<25)

#define ETSR_TXCOLC_MASK	(0xf<<0)
#define ETSR_TXCRC      (1<<5)
#define ETSR_TXDEF      (1<<6)
#define ETSR_TXAJ       (1<<8)
#define ETSR_TXAUR      (1<<9)
#define ETSR_TXAEC      (1<<10)
#define ETSR_TXAED      (1<<11)
#define ETSR_TXAL       (1<<12)
#define ETSR_TXMC       (1<<13)
#define ETSR_TXBR       (1<<14)
#define ETSR_TXOK       (1<<15)


#define EGCR2_STEN	(1<<0)
#define EGCR2_CLRCNT	(1<<1)
#define EGCR2_AUTOZ	(1<<2)
#define EGCR2_TCLER	(1<<3)

#define ERSR_RXSIZE_SHIFT 	(16)
#define ERSR_RXSIZE_MASK 	(0x7ff<<16)
#define ERSR_RXCE		(1<<15)
#define ERSR_RXDV		(1<<14)
#define ERSR_RXOK		(1<<13)
#define ERSR_RXBR		(1<<12)
#define ERSR_RXMC		(1<<11)
#define ERSR_RXDR		(1<<9)
#define ERSR_RXSHT		(1<<6)

#define MAC1_SRST	(1<<15)
#define MAC1_RPEMCSR	(1<<11)
#define MAC1_RPERFUN	(1<<10)
#define MAC1_RPEMCST	(1<<9)
#define MAC1_RPETFUN	(1<<8)
#define MAC1_LOOPBK	(1<<4)
#define MAC1_RXEN	(1<<0)

#define MAC2_EDEFER	(1<<14)
#define MAC2_NOBO	(1<<12)
#define MAC2_LONGP	(1<<9)
#define MAC2_PUREP	(1<<8)
#define MAC2_AUTOP	(1<<7)
#define MAC2_VLANP	(1<<6)
#define MAC2_PADEN	(1<<5)
#define MAC2_CRCEN	(1<<4)
#define MAC2_HUGE	(1<<2)
#define MAC2_FULLD	(1<<0)

#define SAFR_BROAD      (1<<0)
#define SAFR_PRA        (1<<1)
#define SAFR_PRM        (1<<2)
#define SAFR_PRO        (1<<3)

#define MCMD_READ	(1)
#define MCMD_SCAN	(1<<1)
#define MCFG_SPRE	(1<<1)
#define MCFG_CLKS_MASK	(7<<2)
#define MCFG_CLKS(x)	(((x)>>2)&7)
#define MCFG_RMIM	(1<<15)

#define EGCR1_ITXA	(1<<8)
#define EGCR1_MAC_HRST (1<<9)
#define EGCR1_RXALIGN (1<<10)
#define EGCR1_PHY_MODE(x) (((x)>>14)&3)
#define EGCR1_PHY_MODE_MASK (3<<14)
#define EGCR1_ERXINIT (1<<19)
#define EGCR1_ETXDMA (1<<22)
#define EGCR1_ETX    (1<<23)
#define EGCR1_ERXSHT	(1<<28)
#define EGCR1_ERXDMA	(1<<30)
#define EGCR1_ERX	(1<<31)

#define EGSR_RXINIT (1<<20)

#define IR_TX (IR_TX_IDLE | IR_TX_ERR |  IR_TX_DONE | IR_TX_BUFNR | IR_TX_BUFC | IR_ST_OVFL)
#define IR_RX (IR_RXBR | IR_RXBUFFUL | IR_RXNOBUF | IR_RXDONED | IR_RXDONEC | IR_RXDONEB | IR_RXDONEA \
		| IR_RXBUFC | IR_RXOVFL_STAT | IR_RXOVFL_DATA)

#define RB_WRAP  (1<<31)
#define RB_IRQ   (1<<30)
#define RB_ENA   (1<<29)
#define RB_FULL  (1<<28)
#define RB_STATUS(rbd) ((rbd)->flags & 0xffff)

#define TB_WRAP 	(1<<31)
#define TB_IRQ 		(1<<30)
#define TB_LAST 	(1<<29)
#define TB_FULL		(1<<28)

#define MAC_IN_RESET(eth) ((eth)->egcr1 & EGCR1_MAC_HRST) 

/*
 * -------------------------------------------------------
 * change_dataendian
 *      Invoked when the Data-Endian Signal Line changes
 * -------------------------------------------------------
 */
static void
change_dataendian(SigNode *node,int value,void *clientData)
{
        NS9750eth *eth = clientData;
        if(value == SIG_HIGH) {
                eth->dataendian = en_BIG_ENDIAN;
        } else if(value==SIG_LOW) {
                eth->dataendian = en_LITTLE_ENDIAN;
        } else {
                fprintf(stderr,"NS9750 Eth: Data endian is neither Little nor Big\n");
                exit(3424);
        }
}

static void
update_rx_interrupt(NS9750eth *eth)
{
	if(eth->eintr & eth->eintren & IR_RX) {
		if(!eth->rxint_posted) {
			eth->rxint_posted=1;
			SigNode_Set(eth->sigRxIrq,SIG_LOW);
		}
	} else {
		if(eth->rxint_posted) {
			eth->rxint_posted=0;
			SigNode_Set(eth->sigRxIrq,SIG_PULLUP);
		}
	}	
}

static void
update_mii_state(NS9750eth *eth,int verbose) {
	uint32_t clks = MCFG_CLKS(eth->mcfg);
	int enable=1;
	if(clks < 6) {
		if(verbose) {
			fprintf(stderr,"NS9750Eth CLKS: %d < 6, Your MII is overclocked\n",clks);
		}
	} 
	if(eth->mcfg & MCFG_RMIM) {
		enable=0;
		eth->mcmd=0;
		eth->mrdd=0;
		if(verbose) {
			fprintf(stderr,"RMIM is set\n");
		}
	}
	if(eth->mac1 & MAC1_SRST) {
		if(verbose) {
			fprintf(stderr,"MAC_SRST is set\n");
		}
		enable=0;
	}
	eth->mii_enabled=enable;
}

static void
update_tx_interrupt(NS9750eth *eth)
{
	if(eth->eintr & eth->eintren & IR_TX) {
		if(!eth->txint_posted) {
			eth->txint_posted=1;
			SigNode_Set(eth->sigTxIrq,SIG_LOW);
			dbgprintf("Posted a Transmit Interrupt result %d\n",result);
		} 
	} else {
		if(eth->txint_posted) {
			eth->txint_posted=0;
			dbgprintf("UnPoste einen Transmit Interrupt\n");
			SigNode_Set(eth->sigTxIrq,SIG_PULLUP);
		} 
	}	
}

static void 
update_statistic_interrupt(NS9750eth *eth) {
	if((eth->car1 & eth->cam1)  || (eth->car2 & eth->cam2)) {
		eth->eintr |= IR_ST_OVFL;	
	} else {
		eth->eintr &= ~IR_ST_OVFL;
	}
	update_tx_interrupt(eth);
}

static void 
update_tx_statistics(NS9750eth *eth,int len,int type) {
	if(!(eth->egcr2 & EGCR2_STEN)) {
		return;
	}	
	if((len>=1519) && (len<=1522)) {
		eth->trmgv++;
		if(!eth->trmgv) {
			eth->car1 |= ETH_C1MGV;
			update_statistic_interrupt(eth);
		}
	} else if(len>=1024) {
		eth->trmax++;
		if(!eth->trmax) {
			eth->car1 |= ETH_C1MAX;
			update_statistic_interrupt(eth);
		}
	} else if(len>=512) {
		eth->tr1k++;
		if(!eth->tr1k) {
			eth->car1 |= ETH_C11K;
			update_statistic_interrupt(eth);
		}
	} else if(len>=256) {
		eth->tr511++;
		if(!eth->tr511) {
			eth->car1 |= ETH_C1511;
			update_statistic_interrupt(eth);
		}
	} else if(len>=128) {
		eth->tr255++;
		if(!eth->tr255) {
			eth->car1 |= ETH_C1255;
			update_statistic_interrupt(eth);
		}
	} else if(len>65) {
		eth->tr127++;
		if(!eth->tr127) {
			eth->car1 |= ETH_C1127;
			update_statistic_interrupt(eth);
		}
	} else if(len) { // we have no padding in frames because of linux-TAP behaviour
		eth->tr64++;
		if(!eth->tr64) {
			eth->car1 |= ETH_C164;
			update_statistic_interrupt(eth);
		}
	} 
	eth->tbyt += len;
	if(eth->tbyt<len) {
		eth->car2 |= ETH_C2TBY;
		update_statistic_interrupt(eth);
	}
	eth->tpkt++;
	if(eth->tpkt==0) {
		eth->car2 |= ETH_C2TPK;
		update_statistic_interrupt(eth);
	}
	
}
static void 
update_rx_statistics(NS9750eth *eth,int len,int type) {
	if(!(eth->egcr2 & EGCR2_STEN)) {
		return;
	}	
	eth->rbyt += len;
	if(eth->rbyt<len) {
		eth->car1 |= ETH_C1RBY;
		update_statistic_interrupt(eth);
	}
	eth->rpkt++;
	if(eth->rpkt==0) {
		eth->car1 |= ETH_C1RPK;
		update_statistic_interrupt(eth);
	}
	eth->rfrg++;
	if(eth->rfrg==0) {
		eth->car1 |= ETH_C1RFR;
		update_statistic_interrupt(eth);
	}
}
/*
 * Handle ERX from EGCR1
 */
static void
EthResetRx(NS9750eth *eth) {
	dbgprintf("Reset RX\n");
	/* EGCR1_RXINIT is not reset on RX-Reset in real device */
	/* RXPTRS are not reset on EGCR1_ERX in real device */
	eth->rxfifo_count=0;
	update_rx_interrupt(eth);
	update_mii_state(eth,0);
}

static void
EthResetTx(NS9750eth *eth) {
	eth->txoff=0;
	eth->txptr=0;
	eth->txrptr=0;
	eth->txerbd=0;
	eth->txsptr=0;
	eth->tx_stalled=0;
	eth->stat_enabled=0;
	update_rx_interrupt(eth);
}
static void
EthResetStatistics(NS9750eth *eth) {
	eth->tr64=0; 
	eth->tr127=0;
	eth->tr255=0;
	eth->tr511=0;
	eth->tr1k=0; 
	eth->trmax=0;
	eth->trmgv=0;
	eth->rbyt=0;
	eth->rpkt=0;
	eth->rfcs=0;
	eth->rmca=0;
	eth->rbca=0;
	eth->rxcf=0;
	eth->rxpf=0;
	eth->rxuo=0;
	eth->raln=0;
	eth->rflr=0;
	eth->rcde=0;
	eth->rcse=0;
	eth->rund=0;
	eth->rovr=0;
	eth->rfrg=0;
	eth->rjbr=0;
	eth->tbyt=0;
	eth->tpkt=0;
	eth->tmca=0;
	eth->tbca=0;
	eth->tdfr=0;
	eth->tedf=0;
	eth->tscl=0;
	eth->tmcl=0;
	eth->tlcl=0;
	eth->txcl=0;
	eth->tncl=0;
	eth->tjbr=0;
	eth->tfcs=0;
	eth->tovr=0;
	eth->tund=0;
	eth->tfrg=0;
}

static void
EthResetMac(NS9750eth *eth) {
	eth->sa[0]=eth->sa[1]=eth->sa[2]=eth->sa[3]=eth->sa[4]=eth->sa[5]=0;
	eth->mac1 = MAC1_SRST;
	eth->mac2=0x0;
	eth->ipgt=0;
	eth->ipgr=0;
	eth->clrt=0x370f;
	eth->maxf=0x600;
	eth->supp = 0x1000;
	eth->mcfg=0;
	eth->mcmd=0;
	eth->madr=0;
	eth->mwtd=0;
	eth->mrdd=0;
	eth->mind=0;
	eth->ht1=0;
	eth->ht2=0;
}

static void
EthReset(NS9750eth *eth) {
	eth->egcr1=0x00200300; // documentation says 0x00200300, real device says 0x00200200
	eth->egcr2=0x2;
	eth->egsr=0x000d0000;  // from real device 
	eth->safr=0;
	eth->rxptr[0]=eth->rxptr[1]=eth->rxptr[2]=eth->rxptr[3]=0;
	EthResetMac(eth);
	EthResetRx(eth);
	EthResetTx(eth);
	EthResetStatistics(eth);
}
/*
 * -------------------------------------------------------------
 * Rx descriptors are stored in the emulator in Host byte order
 * -------------------------------------------------------------
 */
void
save_rx_descriptor(NS9750eth *eth,RBDescriptor *rbd,uint32_t addr) {
	Bus_Write32(rbd->blength,addr+4);
	Bus_Write32(rbd->flags,addr+0xc);
	dbgprintf("wrote packet length of %d rbd to %08x\n",rbd->blength,addr);
}

void
load_rx_descriptor(NS9750eth *eth,RBDescriptor *rbd,uint32_t addr) {
	rbd->source=Bus_Read32(addr);
	rbd->blength=Bus_Read32(addr+4);
	rbd->flags=Bus_Read32(addr+0xc);
	dbgprintf("loaded rx descriptor from %08x\n",addr);

}

/*
 * --------------------------------------------
 * Read a packet from the Input source
 * --------------------------------------------
 */
static int
fill_rxfifo(NS9750eth *eth) {
        int result;
        char buf[2048];
        if(eth->rxfifo_count) {
                dbgprintf("Dropping paket because rxfifo is full !\n");
		eth->eintr |= IR_RXOVFL_DATA;	
		update_rx_interrupt(eth);
                return read(eth->ether_fd,buf,2048);
        }
        result=read(eth->ether_fd,eth->rxfifo,2048-4); // leave room for FCS
        if(result>0) {
                eth->rxfifo_count=result;
        }
        return result;
}

static inline int 
is_broadcast(uint8_t *mac) {
	if(mac[0] & 1) {
		return 1;
	} 
	return 0;
}

static inline int 
is_multicast(uint8_t *mac) {
	// not yet implemented
	return 0;
}

void
receive(NS9750eth *eth) {
	int i;
	unsigned int bus_size;
	if(eth->egcr1 & EGCR1_RXALIGN) {
		bus_size=eth->rxfifo_count+2+4;
	} else {
		bus_size=eth->rxfifo_count+4;
	}
	for(i=0;i<4;i++) {
		RBDescriptor *rbd=&eth->int_rbdescr[i];
		if(rbd->flags & RB_FULL) {
			dbgprintf("rbd %d is full\n",i);
			continue;
		}
		if(rbd->blength<bus_size) {
			continue;
		}
		if(!(rbd->flags & RB_ENA)) {
			dbgprintf("rbd %d not enabled\n",i);
			continue;
		}
		if(rbd->blength >= bus_size) {
			uint32_t descr_addr;
			//fprintf(stderr,"Fifo %d Now BusWrite RXpacket to %08x\n",i,rbd->source);
			if(rbd->source&3) {
				fprintf(stderr,"RBD alignment is wrong\n");
				rbd->source &= ~3UL;
			}
			if((eth->egcr1 & EGCR1_RXALIGN) && (bus_size>16)) {
				if(eth->dataendian!=TARGET_BYTEORDER) {
					Bus_WriteSwap32(rbd->source,eth->rxfifo,14);
					Bus_WriteSwap32(rbd->source+16,eth->rxfifo+14,bus_size-16);
				} else {
					Bus_Write(rbd->source,eth->rxfifo,14);
					Bus_Write(rbd->source+16,eth->rxfifo+14,bus_size-16);
				}
			} else {
				if(eth->dataendian!=TARGET_BYTEORDER) {
					Bus_WriteSwap32(rbd->source,eth->rxfifo,bus_size);
				} else {
					Bus_Write(rbd->source,eth->rxfifo,bus_size);
				}
			}
			eth->ersr=(bus_size<<16) | ERSR_RXOK;
			if(is_broadcast(eth->rxfifo)) {
				eth->ersr = eth->ersr |  ERSR_RXBR;
			}
			if(is_multicast(eth->rxfifo)) {
				eth->ersr = eth->ersr |  ERSR_RXMC;
			}
			if(simulate_errors) {
				static int counter=100;
				if(counter--!=0) {
					rbd->flags = (rbd->flags & 0xffff0000) | (eth->ersr & 0xffff) | RB_FULL ;
					eth->eintr |= IR_RXDONE(i);
				} else {
					rbd->flags = (rbd->flags & 0xffff0000) | (eth->ersr & 0xffff);
					counter = 100;
				}
			} else {
				rbd->flags = (rbd->flags & 0xffff0000) | (eth->ersr & 0xffff) | RB_FULL ;
				eth->eintr |= IR_RXDONE(i);
			}
			rbd->blength=bus_size;
			eth->ersr=(eth->ersr & 0xf000ffff) | (bus_size<<16);
			if(rbd->flags & RB_IRQ) {
				eth->eintr|=IR_RXBUFC;
			}
			update_rx_interrupt(eth);
			descr_addr=eth->rxptr[i]+eth->rxoff[i];
			save_rx_descriptor(eth,rbd,descr_addr); 
			if(rbd->flags & RB_WRAP) {
				//fprintf(stderr,"chain %d RB_WRAP at rxoff 0x%04x\n",i,eth->rxoff[i]);
				eth->rxoff[i]=0;
			} else {
				eth->rxoff[i]=(eth->rxoff[i] + 0x10) & 0x7ff;
				//fprintf(stderr,"chain %d new rxoff 0x%04x\n",i,eth->rxoff[i]);
			} 
			descr_addr=eth->rxptr[i]+eth->rxoff[i];
			load_rx_descriptor(eth,rbd,descr_addr);	
			eth->rxfifo_count=0;
			update_rx_statistics(eth,eth->rxfifo_count,0);
			return;
		}
	} 
	dbgprintf("NS9750_Eth: No empty slot for received packet\n");
}

/*
 * ----------------------------------------------
 * Check if incoming packet can be accepted
 * Multicast hash table is missing
 * ----------------------------------------------
 */
static int
match_address(NS9750eth *eth) {
	if(is_broadcast(eth->rxfifo)) {
		if(eth->safr & SAFR_BROAD) {
			return 1;
		}
	}
	if(eth->safr & SAFR_PRO) {
		return 1;
	}
	if(memcmp(eth->rxfifo,eth->sa,6)!=0) {
		return 0;
	}
	return 1;
}
/*
 * --------------------------------------------------
 * Fill the RX-Fifo from the Input Event source
 * and call the receiver if the paket size is good
 * --------------------------------------------------
 */
static void 
input_event(void *cd,int mask) {
        NS9750eth *eth = cd;
        int result;
        do {
                result=fill_rxfifo(eth);
		if(!match_address(eth)) {
			eth->rxfifo_count=0;
			continue;
		}
                if(eth->rxfifo_count) {
			receive(eth);
		}
        } while(result>0);
        return;
}


void
update_receiver_state(NS9750eth *eth) 
{
	int enable=1;
	if(!(eth->egcr1 & EGCR1_ERX)) {
		dbgprintf("MAC1 RX is in Reset\n");
		enable=0;
	}
	if(!(eth->egcr1 & EGCR1_ERXDMA)) {
		dbgprintf("ERXDMA is disabled\n");
		enable=0;
	}
	if(eth->egcr1 & EGCR1_MAC_HRST)  {
		enable=0;
	}
	if(eth->mac1 & MAC1_SRST) {
		dbgprintf("MAC1_SRST\n");
		enable=0;	
	}
	if(eth->mac1 & MAC1_RPERFUN) {
		dbgprintf("MAC1_RPERFUN\n");
		enable=0;	
	}
	if(eth->mac1 & MAC1_RPEMCSR) {
		dbgprintf("MAC1_RPEMCSE\n");
		enable=0;	
	}
	if(!(eth->mac1 & MAC1_RXEN)) {
		dbgprintf("!MAC1_RXEN\n");
		enable=0;	
	}
	if(eth->egcr1 & EGCR1_ERXINIT) {
		dbgprintf("ERXININT\n");
		enable=0;
	}
	if(enable) {
		if(!eth->receiver_is_enabled && (eth->ether_fd >=0)) {
			dbgprintf("ns9750 eth: enable receiver\n");
			FIO_AddFileHandler(&eth->input_fh,eth->ether_fd,FIO_READABLE,input_event,eth);
			eth->receiver_is_enabled=1;
		}
	} else {
		if(eth->receiver_is_enabled) {
			FIO_RemoveFileHandler(&eth->input_fh);
			eth->receiver_is_enabled=0;
		}
	}
	return;
}

void
update_transmitter_state(NS9750eth *eth) 
{
	int enable=1;
	if(!(eth->egcr1 & EGCR1_ETXDMA))  {
		enable=0;
	}
	if(!(eth->egcr1 & EGCR1_ETX))  {
		enable=0;
	}
	if(eth->egcr1 & EGCR1_MAC_HRST)  {
		enable=0;
	}
	eth->tx_enabled=enable;
	dbgprintf("egcr is %08x, tx_enabled is %d\n",eth->egcr1,enable);
}

/*
 * ------------------------------------------------------------------------
 * The Ethernet Controller has a builtin buffer for four RX-Descriptors
 * ------------------------------------------------------------------------
 */
static void
preload_rx_descriptors(NS9750eth *eth) {
	int i;
	uint32_t rxdesc_addr; 
	for(i=0;i<4;i++) {
		RBDescriptor *rbd=&eth->int_rbdescr[i];
		rxdesc_addr=eth->rxptr[i];						
		load_rx_descriptor(eth,rbd,rxdesc_addr);	
		eth->rxoff[i]=0; 
		dbgprintf("preload %d, from %08x, source %08x,length %08x\n",i,rxdesc_addr,rbd->source,rbd->blength);
	}
	dbgprintf("Preloaded RX-Descriptors\n");
}

/*
 * ---------------------------------------------------------
 * Load a Tx descriptor from txptr RAM 
 * Uses the 8 Bit index of the descriptors first word
 * for addressing
 * ---------------------------------------------------------
 */
void
load_tx_descriptor(NS9750eth *eth,TBDescriptor *tbd,uint8_t txptr) {
	tbd->source=eth->txbd_ram[txptr & 0xff];
        tbd->blength=eth->txbd_ram[(txptr+1)&0xff]&0x7ff;
        tbd->flags=eth->txbd_ram[(txptr+3)&0xff];
}
void
save_tx_descriptor_flags(NS9750eth *eth,TBDescriptor *tbd,uint8_t txptr) {
        eth->txbd_ram[(txptr+3)&0xff]=tbd->flags;
}

#define RET_NONE_AVAILABLE (-4)
#define RET_SUCCESS (0)
#define RET_ERROR (-1)


int
transmit_frame(NS9750eth *eth) 
{
	TBDescriptor tbd;
	eth->etsr=0;
	load_tx_descriptor(eth,&tbd,eth->txoff>>2);
	dbgprintf("called transmit frame txoff %04x\n",eth->txoff);
	eth->txfifo_count=0;
	while(1) {
		int i;
		if(!(tbd.flags & TB_FULL)) {
			eth->txsptr=eth->txoff>>2;
			eth->eintr |= IR_TX_IDLE;
			update_tx_interrupt(eth);
			dbgprintf("not full src %08x, flags %08x\n",tbd.source,tbd.flags);
			return RET_NONE_AVAILABLE;
		}		
		if(tbd.flags & TB_IRQ) {
			dbgprintf("setze einen BUFC Interrupt\n");
			eth->eintr |= IR_TX_BUFC;
		}
		if(eth->txfifo_count+tbd.blength>2047) {
			dbgprintf("TX-Packet is to big\n");	
			eth->etsr |= ETSR_TXAJ;
			break;
		}
		//fprintf(stderr,"reading packet from %08x\n",tbd.source);
		if(eth->dataendian != TARGET_BYTEORDER) {
			Bus_ReadSwap32(eth->txfifo+eth->txfifo_count,tbd.source,tbd.blength);
		} else {
			Bus_Read(eth->txfifo+eth->txfifo_count,tbd.source,tbd.blength);
		}
		eth->txfifo_count+=tbd.blength;
		for(i=0;i<6;i++) {
		//	fprintf(stderr,"%02x:",eth->txfifo[i]);
		}
		//fprintf(stderr,"\n");
		if(eth->txfifo_count>=6) {
			if((eth->egcr1 & EGCR1_ITXA)) {
				for(i=0;i<6;i++) {
					eth->txfifo[i]=eth->sa[i];
				}
			}
		}
		tbd.flags &= ~TB_FULL;
		save_tx_descriptor_flags(eth,&tbd,eth->txoff>>2);
		if(tbd.flags & TB_WRAP) {
			dbgprintf("TB_WRAP\n");
			eth->txoff=0;
		} else {
			eth->txoff=(eth->txoff+16)&0x3ff;
			dbgprintf("New TB off %08x\n",eth->txoff);
		}
		if(tbd.flags & TB_LAST) {
			int count=0;
			int len=eth->txfifo_count;
			dbgprintf("Send the packet\n");
			do  {
				int result;
				result=write(eth->ether_fd,((char*)eth->txfifo)+count,len-count);
				if(result>0) {
					count+=result;
				} else {
					if(errno!=EAGAIN) {
						eth->etsr &= ~ETSR_TXOK; 
						dbgprintf("NS9750eth: error writing to Linux TAP fd\n");
					   	return RET_ERROR;
					}
				}
			} while(count<len);
			eth->etsr |= ETSR_TXOK; // should check for multicast and broadcast also here 
			update_tx_statistics(eth,len,0 /* type */); 
			break;
		} else {
			dbgprintf("Not last: flags %08x\n",tbd.flags);
		}
		load_tx_descriptor(eth,&tbd,eth->txoff>>2);
		if(!(tbd.flags & TB_FULL)) {
			eth->txerbd = eth->txoff>>2;
			eth->eintr |= IR_TX_ERR;
			update_tx_interrupt(eth);
			return RET_ERROR;
		}
		update_tx_interrupt(eth);
	}
	return RET_SUCCESS;
}

void
transmit(NS9750eth *eth) 
{
	int result;
	if(!eth->tx_enabled) {
		fprintf(stderr,"Transmitter is disabled, egcr1 %08x\n",eth->egcr1);
		return;
	}	
	while((result=transmit_frame(eth))==RET_SUCCESS) {

	}
	eth->eintr |= IR_TX_DONE; // ??? what if nothing was transmitted
	eth->tx_stalled=1;
	update_tx_interrupt(eth);
}

static uint32_t
egcr1_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->egcr1;		
}

static void 
egcr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t diff=eth->egcr1 ^ value;
	eth->egcr1=value;

	if(diff & EGCR1_ETX) {
		EthResetTx(eth);
	}
	if(diff & EGCR1_ERX) {
		EthResetRx(eth);	
	}
	if(diff & EGCR1_PHY_MODE_MASK) {
		if(!(eth->egcr1&EGCR1_MAC_HRST)) {
			fprintf(stderr,"Warning, PHY_MODE Change while not MAC reset\n");
		}
	}
	if(diff & value & EGCR1_MAC_HRST) {
		EthResetMac(eth); 
	}
	update_transmitter_state(eth); 
	update_receiver_state(eth);

	/* Real device does RX Init only on positive edge */
	if((diff & value & EGCR1_ERXINIT) && (value & EGCR1_ERX)) {
		preload_rx_descriptors(eth); 
		eth->egsr |= EGSR_RXINIT;
	}
	if((value & EGCR1_ETX) && (value & EGCR1_ETXDMA)) {
		if(!eth->tx_stalled) {
			eth->txoff=eth->txptr<<2;
			transmit(eth);
		}	
	}
	if(!(value & (1<<21)) || (value &((1<<20)|(1<<11)|(1<<12)))) {
		fprintf(stderr,"NS9750Eth EGCR1 Reserved fields initialized wrong\n");
	}
}

static uint32_t 
egcr2_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->egcr2;	
}

static void 
egcr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t diff=eth->egcr2 ^ value;
	eth->egcr2=value;
	if((diff & EGCR2_TCLER) && (value & EGCR2_TCLER)) {
		if(eth->tx_stalled) {
			eth->tx_stalled=0;
			eth->txoff=eth->txrptr<<2;
			transmit(eth);
		} else {
			dbgprintf("clear error flag without error\n");
		}
	}
	if(value&EGCR2_CLRCNT) {
		EthResetStatistics(eth);
	}
	if(value&EGCR2_STEN) {
		eth->stat_enabled=1;
	} else {
		eth->stat_enabled=0;
	}
}

/*
 * -----------------------------------------------------------
 * The GSR register shows when RXINIT is done. Cleared by
 * writing a 1 to the register 
 * Missing in the documentation, wrong in netos 
 * -----------------------------------------------------------
 */
static uint32_t
egsr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->egsr;
}
static void 
egsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(value&EGSR_RXINIT) {
		eth->egsr &= ~EGSR_RXINIT;
	} else {
		fprintf(stderr,"Possible Bug in Ethernet driver: try to clear EGSR_RXINIT by writing a 0\n");
	}
	eth->egsr=value;
}


static uint32_t
etsr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;
	return eth->etsr;
}
static void 
etsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"ETSR is not writable\n");
}

static uint32_t
ersr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;
	return eth->ersr;
}
static void 
ersr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"ERSR is not writable\n");
}

static uint32_t
mac1_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;
	return eth->mac1;
}

static void 
mac1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"MAC1 write while in Reset mode\n");
		return;
	}
	eth->mac1=value;
	dbgprintf("New mac1 %08x\n",value);
	update_receiver_state(eth);
	update_mii_state(eth,0);
	return;
}

static uint32_t
mac2_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;
	return eth->mac2;
}
static void 
mac2_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"MAC2 write while in Reset mode\n");
		return;
	}
	eth->mac2=value;
}

static uint32_t 
ipgt_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->ipgt;
}

static void 
ipgt_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"ipgt write while in Reset mode\n");
		return;
	}
	eth->ipgt=value;
	return;
}

static uint32_t 
ipgr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->ipgr;
}
static void 
ipgr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t ipgr1,ipgr2;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"Warning: IPGR write while MAC in reset\n");
		return;
	}
	eth->ipgr=value;
	ipgr1=(value>>8)&0x7f;
	ipgr2=(value)&0x7f;
	if(ipgr1<0xc) {
		fprintf(stderr,"Warning: < recommended ipgr1 %d\n",ipgr1); 
	}
	if(ipgr2<0x12) {
		fprintf(stderr,"Warning: < recommended ipgr2 %d\n",ipgr2); 
	}
	return;
}
static uint32_t 
clrt_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->clrt;
}
static void 
clrt_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"Warning: CLRT write while MAC in reset\n");
		return;
	}
	eth->clrt=value;
	return;
}
static uint32_t 
maxf_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->maxf;
}
static void 
maxf_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"Warning: MAXF write while MAC in reset\n");
		return;
	}
	eth->maxf=value&0xffff;
}
static uint32_t 
supp_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->supp;
}
static void 
supp_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"Warning: supp write while MAC in reset\n");
		return;
	}
	eth->supp=value;
	// trigger some action
	return;
}
static uint32_t 
mcfg_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->mcfg;
}
static void 
mcfg_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"Warning: write mcfg while MAC HRST\n");
		return;
	}
	eth->mcfg=value;
	update_mii_state(eth,0);
}
static uint32_t 
mcmd_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;	
	return eth->mcmd;
}
static void 
mcmd_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	PHY_Device *phy;
	uint32_t old = eth->mcmd;
	eth->mcmd=value;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"Warning: mcmd write while MAC in reset\n");
		return;
	}
	if(!eth->mii_enabled) {
		fprintf(stderr,"mcmd write: Warning MII not setup correctly\n");
		update_mii_state(eth,1);	
		return;
	}
	if(value & MCMD_SCAN) {

	} else if ((value & MCMD_READ) && !(old & MCMD_READ)) {
		unsigned int nr_phy = (eth->madr >> 8)&0x1f;
		unsigned int reg_nr = (eth->madr &0x1f);
		uint16_t result=0xffff;
		phy=eth->phy[nr_phy];
		if(!phy) {
			dbgprintf("No phy with address %d\n",nr_phy);
		} else {
			PHY_ReadRegister(phy,&result,reg_nr);
		}
		eth->mrdd=result;
	} else if((value&MCMD_READ) && (old & MCMD_READ)) {
		fprintf(stderr,"Warning: NS9750 ETH MCMD does not autoreset MCMD_READ !\n");
	}
	return;
}

static uint32_t 
madr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;	
	return eth->madr;
}
static void 
madr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"Warning: madr write while MAC in reset\n");
		return;
	}
	eth->madr=value;
//	fprintf(stderr,"New madr %08x\n",value);
	return;
}

static uint32_t 
mwtd_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->mwtd;
}
static void 
mwtd_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	PHY_Device *phy;
	unsigned int nr_phy = (eth->madr >> 8)&0x1f;
	unsigned int reg_nr = (eth->madr &0x1f);
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"Warning mwtd write while MAC in reset\n");
		return;
	}
	if(!eth->mii_enabled) {
		fprintf(stderr,"mwtd write: Warning MII not setup correctly\n");
		update_mii_state(eth,1);	
		return;
	}
	phy=eth->phy[nr_phy];
	if(!phy) {
		dbgprintf("No phy with address %d\n",nr_phy);
		return;
	}
	PHY_WriteRegister(phy,value,reg_nr);
	eth->mwtd=value;
}

static uint32_t 
mrdd_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->mrdd;
	if(!eth->mii_enabled) {
		fprintf(stderr,"mrdd_read: Warning MII not setup correctly\n");
		update_mii_state(eth,1);	
		return data;
	}
	if(eth->mcmd & MCMD_SCAN) {
		PHY_Device *phy;
		uint16_t result=0xffff;
		unsigned int nr_phy = (eth->madr >> 8)&0x1f;
		unsigned int reg_nr = (eth->madr &0x1f);
		phy=eth->phy[nr_phy];
		if(!phy) {
			dbgprintf("No phy with address %d\n",nr_phy);
		}
		PHY_ReadRegister(phy,&result,reg_nr);
		eth->mrdd=result;
	}
	return data;
}
static void 
mrdd_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"mrdd register is not writable\n");
}
static uint32_t 
mind_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->mind;
}
static void 
mind_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"NS9750 eth: mind register is not writable\n");
}
static uint32_t 
sa1_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return  (eth->sa[5]<<8) | eth->sa[4];
}
static void 
sa1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"NS9750 eth: Write Station Address while HRST\n");
		return;
	}
	eth->sa[5]=(value>>8)&0xff;
	eth->sa[4]=value&0xff;
}
static uint32_t
sa2_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return (eth->sa[3]<<8) | eth->sa[2];
}
static void 
sa2_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"NS9750 eth: Write Station Address while HRST\n");
		return;
	}
	eth->sa[3]=(value>>8)&0xff;
	eth->sa[2]=value&0xff;
}
static uint32_t 
sa3_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return (eth->sa[1]<<8) | eth->sa[0];
}
static void 
sa3_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		fprintf(stderr,"Warning: Write Station Address while HRST\n");
		return;
	}
	eth->sa[1]=(value>>8)&0xff;
	eth->sa[0]=value&0xff;
	return;
}

static uint32_t 
safr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->safr;
}
static void 
safr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		return;
	}
	eth->safr=value;
	return;
}
static uint32_t 
ht1_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->ht1;
}
static void 
ht1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		return;
	}
	eth->ht1=value;
	return;
}

static uint32_t 
ht2_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->ht2;
}
static void 
ht2_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	if(MAC_IN_RESET(eth)) {
		return;
	}
	eth->ht2=value;
	return;
}


static uint32_t 
tr64_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tr64;
	if(RSTSTT_ON_RD(eth)) {
		eth->tr64=0;
	}
	return data;
}

static void 
tr64_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tr127_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tr127;
	if(RSTSTT_ON_RD(eth)) {
		eth->tr127=0;
	}
	return data;
}

static void 
tr127_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tr255_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tr255;
	if(RSTSTT_ON_RD(eth)) {
		eth->tr255=0;
	}
	return data; 
}

static void 
tr255_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tr511_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tr511;
	if(RSTSTT_ON_RD(eth)) {
		eth->tr511=0;
	}
	return data;
}

static void 
tr511_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tr1k_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tr1k;
	if(RSTSTT_ON_RD(eth)) {
		eth->tr1k=0;
	}
	return data;
}

static void 
tr1k_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t  
trmax_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->trmax;
	if(RSTSTT_ON_RD(eth)) {
		eth->trmax=0;
	}
	return data;
}

static void 
trmax_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t 
trmgv_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->trmgv;
	if(RSTSTT_ON_RD(eth)) {
		eth->trmgv=0;
	}
	return data;
}

static void 
trmgv_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t 
rbyt_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rbyt;
	if(RSTSTT_ON_RD(eth)) {
		eth->rbyt=0;
	}
	return data;
}   
static void 
rbyt_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t 
rpkt_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rpkt;
	if(RSTSTT_ON_RD(eth)) {
		eth->rpkt = 0;
	}
	return data;
}   
static void 
rpkt_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
rfcs_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rfcs;
	if(RSTSTT_ON_RD(eth)) {
		eth->rfcs = 0;
	}
	return data;
}   
static void 
rfcs_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
rmca_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rmca;
	if(RSTSTT_ON_RD(eth)) {
		eth->rmca = 0;
	}
	return data;
}   
static void 
rmca_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
rbca_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rbca;
	if(RSTSTT_ON_RD(eth)) {
		eth->rbca = 0;
	}
	return data;
}   
static void 
rbca_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
rxcf_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rxcf;
	if(RSTSTT_ON_RD(eth)) {
		eth->rxcf = 0;
	}
	return data;
}   
static void
rxcf_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t 
rxpf_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rxpf;
	if(RSTSTT_ON_RD(eth)) {
		eth->rxpf = 0;
	}
	return data;
}   

static void
rxpf_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
rxuo_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rxuo;
	if(RSTSTT_ON_RD(eth)) {
		eth->rxuo = 0;
	}
	return data;
}   
static void 
rxuo_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
raln_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->raln;
	if(RSTSTT_ON_RD(eth)) {
		eth->raln = 0;
	}
	return data;
}   
static void 
raln_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t  
rflr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rflr;
	if(RSTSTT_ON_RD(eth)) {
		eth->rflr = 0;
	}
	return data;
}   
static void 
rflr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t 
rcde_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rcde;
	if(RSTSTT_ON_RD(eth)) {
		eth->rcde = 0;
	}
	return data;
}   
static void 
rcde_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t 
rcse_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rcse;
	if(RSTSTT_ON_RD(eth)) {
		eth->rcse = 0;
	}
	return data;
}   
static void 
rcse_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
rund_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rund;
	if(RSTSTT_ON_RD(eth)) {
		eth->rund = 0;
	}
	return data;
}   
static void 
rund_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
rovr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rovr;
	if(RSTSTT_ON_RD(eth)) {
		eth->rovr = 0;
	}
	return data;
}   

static void 
rovr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
rfrg_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rfrg;
	if(RSTSTT_ON_RD(eth)) {
		eth->rfrg = 0;
	}
	return data;
}   

static void 
rfrg_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t 
rjbr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->rjbr;
	if(RSTSTT_ON_RD(eth)) {
		eth->rjbr = 0;
	}
	return data;
}   
static void 
rjbr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tbyt_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tbyt;
	if(RSTSTT_ON_RD(eth)) {
		eth->tbyt = 0;
	}
	return data;
}   
static void 
tbyt_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tpkt_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;
	uint32_t data = eth->tpkt;
	if(RSTSTT_ON_RD(eth)) {
		eth->tpkt = 0;
	}
	return data;
}   
static void 
tpkt_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tmca_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tmca;
	if(RSTSTT_ON_RD(eth)) {
		eth->tmca = 0;
	}
	return data;
}   
static void 
tmca_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tbca_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tbca;
	if(RSTSTT_ON_RD(eth)) {
		eth->tbca = 0;
	}
	return data;
}   
static void 
tbca_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tdfr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tdfr;
	if(RSTSTT_ON_RD(eth)) {
		eth->tdfr = 0;
	}
	return data;
}   
static void 
tdfr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tedf_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tedf;
	if(RSTSTT_ON_RD(eth)) {
		eth->tedf = 0;
	}
	return data;
}   
static void 
tedf_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tscl_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tscl;
	if(RSTSTT_ON_RD(eth)) {
		eth->tscl = 0;
	}
	return data;
}   
static void 
tscl_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tmcl_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tmcl;
	if(RSTSTT_ON_RD(eth)) {
		eth->tmcl = 0;
	}
	return data; 
}   
static void 
tmcl_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tlcl_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tlcl;
	if(RSTSTT_ON_RD(eth)) {
		eth->tlcl = 0;
	}
	return data;
}   
static void
tlcl_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t 
txcl_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->txcl;
	if(RSTSTT_ON_RD(eth)) {
		eth->txcl = 0;
	}
	return data;
}   
static void 
txcl_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tncl_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tncl;
	if(RSTSTT_ON_RD(eth)) {
		eth->tncl = 0;
	}
	return data;
}   
static void 
tncl_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t  
tjbr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tjbr;
	if(RSTSTT_ON_RD(eth)) {
		eth->tjbr = 0;
	}
	return data;
}   
static void 
tjbr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t  
tfcs_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data =  eth->tfcs;
	if(RSTSTT_ON_RD(eth)) {
		eth->tfcs = 0;
	}
	return data;
}   
static void 
tfcs_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tovr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tovr;
	if(RSTSTT_ON_RD(eth)) {
		eth->tovr = 0;
	}
	return data;
}   
static void 
tovr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tund_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data = eth->tund;
	if(RSTSTT_ON_RD(eth)) {
		eth->tund = 0;
	}
	return data;
}   
static void 
tund_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
tfrg_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t data =  eth->tfrg;
	if(RSTSTT_ON_RD(eth)) {
		eth->tfrg = 0;
	}
	return data;
}   
static void 
tfrg_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
car1_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"NS9750 eth: car1 not implemented\n");
	return 0;
}   
static void 
car1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
car2_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"NS9750 eth: car2 not implemented\n");
	return 0;
}   
static void 
car2_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t 
cam1_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"NS9750 eth: cam1 not implemented\n");
	return 0;
}   
static void 
cam1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}
static uint32_t 
cam2_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"NS9750 eth: cam2 not implemented\n");
	return 0;
}   
static void 
cam2_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	return;
}

static uint32_t 
rxaptr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->rxptr[0];
}
static void 
rxaptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	eth->rxptr[0]=value;
}
static uint32_t 
rxbptr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->rxptr[1];
}
static void 
rxbptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	eth->rxptr[1]=value;
}
static uint32_t 
rxcptr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->rxptr[2];
}
static void
rxcptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	eth->rxptr[2]=value;
	return;
}
static uint32_t 
rxdptr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->rxptr[3];
}
static void 
rxdptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	eth->rxptr[3]=value;
}
/*
 * ----------------------------------------------------------------
 * Pending Interrupts can be cleared by writing a 1
 * Interrupts can be set by writing a 1 when none is pending
 * ----------------------------------------------------------------
 */
static uint32_t 
eintr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->eintr;	
}
static void 
eintr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;
	eth->eintr ^= value;
//	fprintf(stderr,"Clear interrupts %08x now %08x\n",value,eth->eintr);
	update_rx_interrupt(eth);
	update_tx_interrupt(eth);
}
static uint32_t  
eintren_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;
	return eth->eintren;
}
static void 
eintren_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth = clientData;
	eth->eintren=value;
	update_rx_interrupt(eth);
	update_tx_interrupt(eth);
}
static uint32_t 
txptr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->txptr;
}
static void 
txptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	eth->txrptr=value&0xff;
}

static uint32_t 
txrptr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->txrptr;
}
static void 
txrptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	eth->txrptr=value&0xff;
}

static uint32_t 
txerbd_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->txerbd;
}
static void 
txerbd_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"NS9750 eth: TXERBD is readonly\n");
}

static uint32_t 
txsptr_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->txsptr;
}

static void 
txsptr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"NS9750 eth: TXSPTR is readonly\n");
	return;
}
/*
 * -----------------------------------------------------------------------------
 * RX offset Registers read 0 during ERX reset but old value 
 * reapears after removing the  Reset
 * -----------------------------------------------------------------------------
 */
static uint32_t 
rxoff_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	unsigned int index=(address-NS9750_ETH_RXAOFF)>>2;
	if(index>3) {
		fprintf(stderr,"Bug in %s line %d\n",__FILE__,__LINE__);
		return 0;
	}
	if(!(eth->egcr1 & EGCR1_ERX)) {
		return 0;
	} else {
		return eth->rxoff[index];
	}
}
static void 
rxoff_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"RXOFFs are not writable\n"); // ignored by real device
	return;
}

static uint32_t 
txoff_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	return eth->txoff;
}

static void 
txoff_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"NS9750 eth: Tx-Offset is not writable\n");
	return;
}

static uint32_t 
rxfree_read(void *clientData,uint32_t address,int rqlen) {
	return 0;
}

static void 
rxfree_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	//?????
	return;
}

/*
 * ------------------------------------------------
 * 256 * 32 Bit Transfer Descriptor RAM
 * ------------------------------------------------
 */
static uint32_t 
txbd_read(void *clientData,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t index=(address&0x3ff)>>2;	
	if(!(eth->egcr1 & EGCR1_ETX) || (rqlen !=4)) {
		fprintf(stderr,"NS9750 Eth: CPU crash accessing TXBD RAM while ETX in EGCR1 disabled\n"); 
		exit(345);
	}
//	fprintf(stderr,"TXBD read index %d,value %08x\n",index,*value);
	return eth->txbd_ram[index];	
}
static void 
txbd_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	NS9750eth *eth=clientData;
	uint32_t index=(address&0x3ff)>>2;	

	if(!(eth->egcr1 & EGCR1_ETX) || (rqlen !=4)) {
		// Some exception ? */
		fprintf(stderr,"NS9750 Eth: CPU crash accessing TXBD RAM while ETX in EGCR1 disabled\n"); 
		exit(347);
	}
	eth->txbd_ram[index]=value;	
//	fprintf(stderr,"wrote to tdbd ram index %d,value %08x\n",index,value);
	return;
}
/*
 * ------------------------------------------------------------------
 *  This currently ignores  Baseaddress
 * ------------------------------------------------------------------
 */
static void
NS9750Eth_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags) {
        NS9750eth *eth=owner;
	int i;
	IOH_New32(NS9750_ETH_EGCR1,egcr1_read,egcr1_write,eth);
	IOH_New32(NS9750_ETH_EGCR2,egcr2_read,egcr2_write,eth);
	IOH_New32(NS9750_ETH_EGSR,egsr_read,egsr_write,eth);
	IOH_New32(NS9750_ETH_ETSR,etsr_read,etsr_write,eth);
	IOH_New32(NS9750_ETH_ERSR,ersr_read,ersr_write,eth);
	IOH_New32(NS9750_ETH_MAC1,mac1_read,mac1_write,eth);
	IOH_New32(NS9750_ETH_MAC2,mac2_read,mac2_write,eth);
	IOH_New32(NS9750_ETH_IPGT,ipgt_read,ipgt_write,eth);
	IOH_New32(NS9750_ETH_IPGR,ipgr_read,ipgr_write,eth);
	IOH_New32(NS9750_ETH_CLRT,clrt_read,clrt_write,eth);
	IOH_New32(NS9750_ETH_MAXF,maxf_read,maxf_write,eth);
	IOH_New32(NS9750_ETH_SUPP,supp_read,supp_write,eth);
	IOH_New32(NS9750_ETH_MCFG,mcfg_read,mcfg_write,eth);
	IOH_New32(NS9750_ETH_MCMD,mcmd_read,mcmd_write,eth);
	IOH_New32(NS9750_ETH_MADR,madr_read,madr_write,eth);
	IOH_New32(NS9750_ETH_MWTD,mwtd_read,mwtd_write,eth);
	IOH_New32(NS9750_ETH_MRDD,mrdd_read,mrdd_write,eth);
	IOH_New32(NS9750_ETH_MIND,mind_read,mind_write,eth);
	IOH_New32(NS9750_ETH_SA1,sa1_read,sa1_write,eth);
	IOH_New32(NS9750_ETH_SA2,sa2_read,sa2_write,eth);
	IOH_New32(NS9750_ETH_SA3,sa3_read,sa3_write,eth);
	IOH_New32(NS9750_ETH_SAFR,safr_read,safr_write,eth);
	IOH_New32(NS9750_ETH_HT1,ht1_read,ht1_write,eth);
	IOH_New32(NS9750_ETH_HT2,ht2_read,ht2_write,eth);
	IOH_New32(NS9750_ETH_TR64,tr64_read,tr64_write,eth);
	IOH_New32(NS9750_ETH_TR127,tr127_read,tr127_write,eth); 
	IOH_New32(NS9750_ETH_TR255,tr255_read,tr255_write,eth);
	IOH_New32(NS9750_ETH_TR511,tr511_read,tr511_write,eth);
	IOH_New32(NS9750_ETH_TR1K,tr1k_read,tr1k_write,eth); 
	IOH_New32(NS9750_ETH_TRMAX,trmax_read,trmax_write,eth);
	IOH_New32(NS9750_ETH_TRMGV,trmgv_read,trmgv_write,eth);

	IOH_New32(NS9750_ETH_RBYT,rbyt_read,rbyt_write,eth);
	IOH_New32(NS9750_ETH_RPKT,rpkt_read,rpkt_write,eth);
	IOH_New32(NS9750_ETH_RFCS,rfcs_read,rfcs_write,eth);
	IOH_New32(NS9750_ETH_RMCA,rmca_read,rmca_write,eth);
	IOH_New32(NS9750_ETH_RBCA,rbca_read,rbca_write,eth);
	IOH_New32(NS9750_ETH_RXCF,rxcf_read,rxcf_write,eth);
	IOH_New32(NS9750_ETH_RXPF,rxpf_read,rxpf_write,eth);
	IOH_New32(NS9750_ETH_RXUO,rxuo_read,rxuo_write,eth);
	IOH_New32(NS9750_ETH_RALN,raln_read,raln_write,eth);
	IOH_New32(NS9750_ETH_RFLR,rflr_read,rflr_write,eth);
	IOH_New32(NS9750_ETH_RCDE,rcde_read,rcde_write,eth);
	IOH_New32(NS9750_ETH_RCSE,rcse_read,rcse_write,eth);
	IOH_New32(NS9750_ETH_RUND,rund_read,rund_write,eth);
	IOH_New32(NS9750_ETH_ROVR,rovr_read,rovr_write,eth);
	IOH_New32(NS9750_ETH_RFRG,rfrg_read,rfrg_write,eth);
	IOH_New32(NS9750_ETH_RJBR,rjbr_read,rjbr_write,eth);
	IOH_New32(NS9750_ETH_TBYT,tbyt_read,tbyt_write,eth);
	IOH_New32(NS9750_ETH_TPKT,tpkt_read,tpkt_write,eth);
	IOH_New32(NS9750_ETH_TMCA,tmca_read,tmca_write,eth);
	IOH_New32(NS9750_ETH_TBCA,tbca_read,tbca_write,eth);
	IOH_New32(NS9750_ETH_TDFR,tdfr_read,tdfr_write,eth);
	IOH_New32(NS9750_ETH_TEDF,tedf_read,tedf_write,eth);
	IOH_New32(NS9750_ETH_TSCL,tscl_read,tscl_write,eth);
	IOH_New32(NS9750_ETH_TMCL,tmcl_read,tmcl_write,eth);
	IOH_New32(NS9750_ETH_TLCL,tlcl_read,tlcl_write,eth);
	IOH_New32(NS9750_ETH_TXCL,txcl_read,txcl_write,eth);
	IOH_New32(NS9750_ETH_TNCL,tncl_read,tncl_write,eth);
	IOH_New32(NS9750_ETH_TJBR,tjbr_read,tjbr_write,eth);
	IOH_New32(NS9750_ETH_TFCS,tfcs_read,tfcs_write,eth);
	IOH_New32(NS9750_ETH_TOVR,tovr_read,tovr_write,eth);
	IOH_New32(NS9750_ETH_TUND,tund_read,tund_write,eth);
	IOH_New32(NS9750_ETH_TFRG,tfrg_read,tfrg_write,eth);
	IOH_New32(NS9750_ETH_CAR1,car1_read,car1_write,eth);
	IOH_New32(NS9750_ETH_CAR2,car2_read,car2_write,eth); 
	IOH_New32(NS9750_ETH_CAM1,cam1_read,cam1_write,eth);
	IOH_New32(NS9750_ETH_CAM2,cam2_read,cam2_write,eth); 


	IOH_New32(NS9750_ETH_RXAPTR,rxaptr_read,rxaptr_write,eth);
	IOH_New32(NS9750_ETH_RXBPTR,rxbptr_read,rxbptr_write,eth);
	IOH_New32(NS9750_ETH_RXCPTR,rxcptr_read,rxcptr_write,eth);
	IOH_New32(NS9750_ETH_RXDPTR,rxdptr_read,rxdptr_write,eth);
	IOH_New32(NS9750_ETH_EINTR,eintr_read,eintr_write,eth);
	IOH_New32(NS9750_ETH_EINTREN,eintren_read,eintren_write,eth);
	IOH_New32(NS9750_ETH_TXPTR,txptr_read,txptr_write,eth);
	IOH_New32(NS9750_ETH_TXRPTR,txrptr_read,txrptr_write,eth);
	IOH_New32(NS9750_ETH_TXERBD,txerbd_read,txerbd_write,eth); 
	IOH_New32(NS9750_ETH_TXSPTR,txsptr_read,txsptr_write,eth);
	IOH_New32(NS9750_ETH_RXAOFF,rxoff_read,rxoff_write,eth);
	IOH_New32(NS9750_ETH_RXBOFF,rxoff_read,rxoff_write,eth);
	IOH_New32(NS9750_ETH_RXCOFF,rxoff_read,rxoff_write,eth);
	IOH_New32(NS9750_ETH_RXDOFF,rxoff_read,rxoff_write,eth);
	IOH_New32(NS9750_ETH_TXOFF,txoff_read,txoff_write,eth);
	IOH_New32(NS9750_ETH_RXFREE,rxfree_read,rxfree_write,eth);
	for(i=0;i<1024;i+=4) {
		IOH_New32(NS9750_ETH_TXBD+i,txbd_read,txbd_write,eth); 
	}

}

static void
NS9750Eth_UnMap(void *owner,uint32_t base,uint32_t mask) {
	uint32_t u;
	for(u=0xa0600000;u<0xa0601400;u+=4) {
		IOH_Delete32(u);
	}
}

int 
NS9750_EthRegisterPhy(BusDevice *dev,PHY_Device *phy,unsigned int phy_addr) 
{
	NS9750eth *eth=dev->owner;
	if(phy_addr >= MAX_PHYS) {
		fprintf(stderr,"NS9750 Eth:Illegal PHY address %d\n",phy_addr);
		return -1;
	} else {
		eth->phy[phy_addr]=phy;
		return 0;
	}

}

BusDevice *
NS9750_EthInit(const char *devname) {
        NS9750eth *eth = sg_new(NS9750eth);
	eth->ether_fd = Net_CreateInterface(devname);
        fcntl(eth->ether_fd,F_SETFL,O_NONBLOCK);

        eth->bdev.first_mapping=NULL;
        eth->bdev.Map=NS9750Eth_Map;
        eth->bdev.UnMap=NS9750Eth_UnMap;
        eth->bdev.owner=eth;
        eth->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;

        eth->dataEndianNode = SigNode_New("%s.dataendian",devname);
        if(!eth->dataEndianNode) {
                fprintf(stderr,"Can not create Ser. EndianNode\n");
                exit(3429);
        }
        eth->dataEndianTrace = SigNode_Trace(eth->dataEndianNode,change_dataendian,eth);
	eth->sigRxIrq = SigNode_New("%s.rx_irq",devname);
	eth->sigTxIrq = SigNode_New("%s.tx_irq",devname);
	if(!eth->sigRxIrq || !eth->sigTxIrq) {
		fprintf(stderr,"Can not create Interrupts for %s\n",devname);
		exit(1);
	}
#if 0 
	SigNode_Set(eth->sigRxIrq,SIG_PULLUP);
	SigNode_Set(eth->sigTxIrq,SIG_PULLUP);
#endif

	EthReset(eth);
        Mem_AreaAddMapping(&eth->bdev,NS9750_ETH_BASE,NS9750_ETH_MAPSIZE,MEM_FLAG_WRITABLE|MEM_FLAG_READABLE);
	fprintf(stderr,"NS9750 Ethernet Controller created\n");
        return &eth->bdev;
}

