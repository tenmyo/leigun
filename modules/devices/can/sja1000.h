/*
 * ----------------------------------------------------
 *
 * Emulation of the SJA1000 CAN Controller
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 * ----------------------------------------------------
 */

#define SJA_REG(x) (x&0x7f)
void SJA_Init();

/*
 * --------------------------------------------
 * Encode the layout into the Register number
 * to avoid mistakes
 * --------------------------------------------
 */

#define SJA_LOUT_BCR	(0)
#define SJA_LOUT_BC	(1)
#define SJA_LOUT_PC	(2)
#define SJA_LOUT_PCR	(3)

/*
 * -----------------------------------------------
 * Register definitions for Basic CAN Reset Mode 
 * -----------------------------------------------
 */
#define SJA_BCR_CR	(0)
#define		SJA_RM		(1)
#define		SJA_BCR_CR_RR	(1<<0)
#define		SJA_BCR_CR_RIE	(1<<1)
#define		SJA_BCR_CR_TIE	(1<<2)
#define		SJA_BCR_CR_EIE	(1<<3)
#define		SJA_BCR_CR_OIE	(1<<4)

#define SJA_BCR_CMR	(1)
#define SJA_BCR_SR	(2)
#define		SJA_SR_RBS	(1)
#define		SJA_SR_DOS	(1<<1)
#define		SJA_SR_TBS	(1<<2)
#define		SJA_SR_TCS	(1<<3)
#define		SJA_SR_RS	(1<<4)
#define 	SJA_SR_TS	(1<<5)
#define		SJA_SR_ES	(1<<6)
#define		SJA_SR_BS	(1<<7)
#define SJA_BCR_IR	(3)
#define SJA_BCR_ACC	(4)
#define SJA_BCR_ACM	(5)
#define SJA_BCR_BTR0	(6)
#define SJA_BCR_BTR1	(7)
#define SJA_BCR_OC	(8)
#define SJA_BCR_TST	(9)
#define SJA_BCR_TXB	(10)
#define SJA_BCR_RXB	(20)
#define SJA_BCR_CLKDIV  (31)
#define 	SJA_MODE	(1<<7)
/*
 * -----------------------------------------------
 * Register definitions for Basic CAN Mode 
 * -----------------------------------------------
 */
#define SJA_BC_CR	(0)
#define		SJA_BC_CR_RR	(1<<0)
#define		SJA_BC_CR_RIE	(1<<1)
#define		SJA_BC_CR_TIE	(1<<2)
#define		SJA_BC_CR_EIE	(1<<3)
#define		SJA_BC_CR_OIE	(1<<4)
#define SJA_BC_CMR	(1)
#define SJA_BC_SR	(2)
#define SJA_BC_IR	(3)
#define		SJA_BC_IR_RI 	(1)
#define 	SJA_BC_IR_TI	(1<<1)
#define		SJA_BC_IR_EI	(1<<2)
#define		SJA_BC_IR_DOI	(1<<3)
#define		SJA_BC_IR_WUI	(1<<4)
#define SJA_BC_BTR0	(6)
#define SJA_BC_BTR1	(7)
#define SJA_BC_OC	(8)
#define SJA_BC_TXB	(10)
#define SJA_BC_RXB	(20)
#define SJA_BC_CLKDIV  	(31)

/*
 * -----------------------------------------
 * Register definitions for PeliCAN Mode
 * -----------------------------------------
 */
#define SJA_PC_MOD	(0)
#define		SJA_PC_MOD_RM	(1<<0)
#define		SJA_PC_MOD_LOM	(1<<1)
/* STM Ignores missing ack, but still sends and receives real frames on the bus */
#define		SJA_PC_MOD_STM	(1<<2)
#define		SJA_PC_MOD_AFM	(1<<3)
#define 	SJA_PC_MOD_SM	(1<<4)

#define SJA_PC_CMR  	(1)
#define		SJA_PC_CMR_TR	(1)
#define		SJA_PC_CMR_AT	(1<<1)
#define		SJA_PC_CMR_RRB	(1<<2)
#define  	SJA_PC_CMR_CDO	(1<<3)
#define	        SJA_PC_CMR_SRR	(1<<4)

#define SJA_PC_SR	(2)
#define SJA_PC_IR 	(3)
#define		SJA_PC_IR_RI 	(1)
#define 	SJA_PC_IR_TI	(1<<1)
#define		SJA_PC_IR_EI	(1<<2)
#define		SJA_PC_IR_DOI	(1<<3)
#define		SJA_PC_IR_WUI	(1<<4)
#define		SJA_PC_IR_EPI	(1<<5)
#define		SJA_PC_IR_ALI	(1<<6)
#define		SJA_PC_IR_BEI	(1<<7)
#define SJA_PC_IER 	(4)
#define         SJA_PC_IER_RIE   (1)
#define         SJA_PC_IER_TIE   (1<<1)
#define         SJA_PC_IER_EIE   (1<<2)
#define         SJA_PC_IER_DOIE  (1<<3)
#define         SJA_PC_IER_WUIE  (1<<4)
#define         SJA_PC_IER_EPIE  (1<<5)
#define         SJA_PC_IER_ALIE  (1<<6)
#define         SJA_PC_IER_BEIE  (1<<7)

#define SJA_PC_BTR0	(6)
#define SJA_PC_BTR1	(7)
#define SJA_PC_OC	(8)
#define SJA_PC_TEST	(9)
#define SJA_PC_ALC	(11)
#define SJA_PC_ECC	(12)
#define SJA_PC_EWL	(13)
#define SJA_PC_RXERR	(14)
#define SJA_PC_TXERR	(15)
#define SJA_PC_RXSFF	(16)
#define SJA_PC_RXEFF	(16)
#define SJA_PC_TXSFF	(16)
#define SJA_PC_TXEFF	(16)

#define SJA_PC_SFF_RXID_1	(17)
#define SJA_PC_SFF_RXID_2	(18)
#define SJA_PC_EFF_RXID_1	(17)
#define SJA_PC_EFF_RXID_2	(18)
#define SJA_PC_EFF_RXID_3	(19)
#define SJA_PC_EFF_RXID_4	(20)
#define SJA_PC_SFF_RXDATA_1	(19)
#define SJA_PC_SFF_TXDATA_1	(19)
#define SJA_PC_SFF_RXDATA_2	(20)
#define SJA_PC_SFF_TXDATA_2	(20)
#define SJA_PC_SFF_RXDATA_3	(21)
#define SJA_PC_SFF_TXDATA_3	(21)
#define SJA_PC_SFF_RXDATA_4	(22)
#define SJA_PC_SFF_TXDATA_4	(22)
#define SJA_PC_SFF_RXDATA_5	(23)
#define SJA_PC_SFF_TXDATA_5	(23)
#define SJA_PC_SFF_RXDATA_6	(24)
#define SJA_PC_SFF_TXDATA_6	(24)
#define SJA_PC_SFF_RXDATA_7	(25)
#define SJA_PC_SFF_TXDATA_7	(25)
#define SJA_PC_SFF_RXDATA_8	(26)
#define SJA_PC_SFF_TXDATA_8	(26)

#define SJA_PC_EFF_RXDATA_1	(21)
#define SJA_PC_EFF_TXDATA_1	(21)

#define SJA_PC_RXCNT	(29)
#define SJA_PC_RBSA	(30)
#define SJA_PC_CLKDIV	(31)
#define SJA_PC_CANRAM	(32)

/*
 * ---------------------------------
 * Pelican Reset Mode
 * ---------------------------------
 */

#define SJA_PCR_MOD	(0)
#define		SJA_PCR_MOD_RM	(1<<0)
#define		SJA_PCR_MOD_LOM	(1<<1)
#define		SJA_PCR_MOD_STM	(1<<2)
#define		SJA_PCR_MOD_AFM	(1<<3)
#define 	SJA_PCR_MOD_SM	(1<<4)

#define SJA_PCR_CMR	(1)
#define SJA_PCR_SR	(2)
#define SJA_PCR_IR	(3)
#define SJA_PCR_IER	(4)
#define SJA_PCR_BTR0	(6)
#define SJA_PCR_BTR1	(7)
#define SJA_PCR_OC	(8)
#define SJA_PCR_TST	(9)
#define SJA_PCR_ALC	(11)
#define SJA_PCR_ECC	(12)
#define SJA_PCR_EWL	(13)
#define SJA_PCR_RXERR	(14)
#define SJA_PCR_TXERR	(15)
#define SJA_PCR_ACC0	(16)
#define SJA_PCR_ACC1	(17)
#define SJA_PCR_ACC2	(18)
#define SJA_PCR_ACC3	(19)
#define SJA_PCR_ACM0	(20)
#define SJA_PCR_ACM1	(21)
#define SJA_PCR_ACM2	(22)
#define SJA_PCR_ACM3	(23)
#define SJA_PCR_RXCNT	(29)
#define SJA_PCR_RBSA	(30)
#define SJA_PCR_CLKDIV	(31)
#define SJA_PCR_CANRAM	(32)

typedef struct SJA1000 SJA1000;
void SJA1000_UnMap(SJA1000 * sja, uint32_t base);
void SJA1000_Map(SJA1000 * sja, uint32_t base);
SJA1000 *SJA1000_New(BusDevice * dev, const char *name);
