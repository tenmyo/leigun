/*
 * ----------------------------------------------------
 *
 * Definition of Internal Registers of MPC8xx
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 * ----------------------------------------------------
 */

#define MPC8xx_SIUMCR	(0)
#define		EARB	(1<<31)	
#define 	EARP(x)	(((x)>>28)&7)
#define		DSHW	(1<<23)
#define 	DBGC(x)	(((x)>>21)&3)
#define 	DBPC(x)	(((x)>>19)&3)
#define 	FRC	(1<<17)
#define 	DLK	(1<<16)
#define 	OPAR	(1<<15)
#define 	PNCS	(1<<14)
#define 	DPC	(1<<13)
#define		MLRC(x)	(((x)>>10)&3)
#define 	AEME	(1<<9)
#define		SEME	(1<<8)
#define		BSC	(1<<7)
#define		GB5E	(1<<6)
#define		B2DD	(1<<5)
#define		B3DD	(1<<4)

#define MPC8xx_SYPCR	(4)
#define 	SWTC(x)	(((x)>>16)&0xffff)
#define 	BMT(x)	(((x)>>8) & 0xff)
#define		BME	(1<<7)
#define		SWF	(1<<3)
#define		SWE	(1<<2)
#define		SWRI	(1<<1)
#define		SWP	(1<<0)

#define MPC8xx_SWSR             (0xe)

#define MPC8xx_SIPEND	(0x10)
#define 	SIPEND_IRQ(irq,x) ((x)&(1<<(31-((irq)<<1))))
#define 	SIPEND_LVL(irq,x) ((x)&(1<<(((31-(irq))<<1)+1)))

#define MPC8xx_SIMASK	(0x14)
#define		SIMASK_IRM0	(1)
#define 	SIMASK_LVM(irq,x) ((x)&(1<<(((31-(irq))<<1)+1)))
#define 	SIMASK_IRM(irq,x) ((x)&(1<<((31-(irq))<<1)))

#define MPC8xx_SIEL	(0x18)
#define 	SIEL_ED(irq,x)  ((x)&(1<<((31-(irq))<<1))) 

#define MPC8xx_SIVEC	(0x1c)
#define		SIVEC_INTC (0xff)	

#define	MPC8xx_TESR (0x20)
#define		IEXT	(1<<13)
#define		ITMT	(1<<12)
#define		IPB(x)	(((x)>>8)&0xf)
#define		DEXT	(1<<5)
#define		DTMT	(1<<4)
#define		DPB(x)	(((x)&0xf))


#define MPC8xx_BR(x)    (0x100+((x)<<3))
#define         BR_BA(x)        ((x)&0xffff8000)
#define         BR_AT(x)        (((x)>>12)&0x7)
#define         BR_PS(x)        ((x>>10)&3)
#define         BR_PARE(x)      (((x)>>9)&1)
#define         BR_WP(x)        (((x)>>8)&1)
#define         BR_MS(x)        (((x)>>6)&3)
#define         BR_V(x)         (((x)>>0)&1)

#define MPC8xx_OR(x)    (0x104+((x)<<3))
#define         OR_AM(x)        ((x)&0xffff8000)
#define         OR_ATM(x)       (((x)>>12)&0x7)
#define         OR_CSTNSAM(x)   (((x)>>11)&0x1)
#define         OR_ACS(x)       (((x)>>9)&0x3)
#define         OR_G5L(x)       (((x)>>9)&0x3)
#define         OR_BIH(x)       (((x)>>8)&0x1)
#define         OR_SCY(x)       (((x)>>4)&0xf)
#define         OR_SETA(x)      (((x)>>3)&0x1)
#define         OR_TRLX(x)      (((x)>>2)&0x1)
#define         OR_EHTR(x)      (((x)>>1)&0x1)


#define MPC8xx_MAR      (0x164)
#define MPC8xx_MCR      (0x168)
#define MPC8xx_MAMR     (0x170)
#define MPC8xx_MBMR     (0x174)
#define MPC8xx_MSTAT    (0x178)
#define MPC8xx_MPTPR    (0x17a)
#define MPC8xx_MDR      (0x17c)

#define MPC8xx_TBSCR	(0x200)
#define MPC8xx_TBREFA	(0x204)
#define MPC8xx_TBREFB	(0x208)
#define MPC8xx_PISCR	(0x240)
#define MPC8xx_PITC	(0x244)
#define MPC8xx_PITR	(0x248)

#define	MPC8xx_TBSCRK	(0x300)
#define MPC8xx_TBREFAK	(0x304)
#define MPC8xx_TBREFBK	(0x308)
#define MPC8xx_TBK	(0x30C)
#define	MPC8xx_PISCRK	(0x340)
#define MPC8xx_PITCK	(0x344)
#define MPC8xx_SCCRK	(0x380)
#define MPC8xx_PLPRCRK  (0x384)
#define MPC8xx_RSRK	(0x388)

