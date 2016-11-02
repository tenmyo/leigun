/**
 **************************************************************************
 *
 **************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "i2c.h"
#include "i2c_serdes.h"
#include "bus.h"
#include "signode.h"
#include "clock.h"
#include "cycletimer.h"
#include "i2c_rx62t.h"
#include "sgstring.h"

#define REG_ICCR1(base)		((base) + 0x00)
#define		ICCR1_ICE	(1 << 7)
#define		ICCR1_IICRST	(1 << 6)
#define		ICCR1_CLO	(1 << 5)
#define		ICCR1_SOWP	(1 << 4)
#define 	ICCR1_SCLO	(1 << 3)
#define		ICCR1_SDAO	(1 << 2)
#define 	ICCR1_SCLI	(1 << 1)
#define		ICCR1_SDAI	(1 << 0)
#define REG_ICCR2(base)		((base) + 0x01)
#define		ICCR2_BBSY	(1 << 7)
#define		ICCR2_MST	(1 << 6)
#define		ICCR2_TRS	(1 << 5)
#define		ICCR2_SP	(1 << 3)
#define		ICCR2_RS	(1 << 2)
#define		ICCR2_ST	(1 << 1)
#define REG_ICMR1(base)		((base) + 0x02)
#define		ICMR1_BC_MSK	(7)
#define 	ICMR1_BCWP	(1 << 3)
#define 	ICMR1_CKS_MSK	(7 << 4)
#define		ICMR1_MTWP	(1 << 7)
#define REG_ICMR2(base)		((base) + 0x03)
#define		ICMR2_DLCS	(1 << 7)
#define		ICMR2_SDDL_MSK	(7 << 4)
#define		ICMR2_TMOH	(1 << 2)
#define		ICMR2_TMOL	(1 << 1)
#define		ICMR2_TMOS	(1 << 0)
#define REG_ICMR3(base)		((base) + 0x04)
#define		ICMR3_SMBS	(1 << 7)
#define		ICMR3_WAIT	(1 << 6)
#define		ICMR3_RDRFS	(1 << 5)
#define		ICMR3_ACKWP	(1 << 4)
#define		ICMR3_ACKBT	(1 << 3)
#define		ICMR3_ACKBR	(1 << 2)
#define		ICMR3_NF_MSK	(3 << 0)
#define REG_ICFER(base)		((base) + 0x05)
#define		ICFER_SCLE	(1 << 6)
#define		ICFER_NFE	(1 << 5)
#define		ICFER_NACKE	(1 << 4)
#define		ICFER_SALE	(1 << 3)
#define		ICFER_NALE	(1 << 2)
#define		ICFER_MALE	(1 << 1)
#define		ICFER_TMOE	(1 << 0)
#define REG_ICSER(base)		((base) + 0x06)
#define		ICSER_HOAE	(1 << 7)
#define		ICSER_DIDE	(1 << 5)
#define		ICSER_GCAE	(1 << 3)
#define		ICSER_SAR2E	(1 << 2)
#define		ICSER_SAR1E	(1 << 1)
#define		ICSER_SAR0E	(1 << 0)
#define REG_ICIER(base)		((base) + 0x07)
#define		ICIER_TIE	(1 << 7)
#define		ICIER_TEIE	(1 << 6)
#define		ICIER_RIE	(1 << 5)
#define		ICIER_NAKIE	(1 << 4)
#define		ICIER_SPIE	(1 << 3)
#define		ICIER_STIE	(1 << 2)
#define		ICIER_ALIE	(1 << 1)
#define		ICIER_TMOIE	(1 << 0)
#define REG_ICSR1(base)		((base) + 0x08)
#define		ICSR1_HOA	(1 << 7)
#define		ICSR1_DID	(1 << 5)
#define		ICSR1_GCA	(1 << 3)
#define		ICSR1_AAS2	(1 << 2)
#define		ICSR1_AAS1	(1 << 1)
#define		ICSR1_AAS0	(1 << 0)

#define REG_ICSR2(base)		((base) + 0x09)
#define		ICSR2_TDRE	(1 << 7)
#define		ICSR2_TEND	(1 << 6)
#define		ICSR2_RDRF	(1 << 5)
#define		ICSR2_NACKF	(1 << 4)
#define		ICSR2_STOP	(1 << 3)
#define		ICSR2_START	(1 << 2)
#define		ICSR2_AL	(1 << 1)
#define		ICSR2_TMOF	(1 << 0)
#define REG_SARL0(base)		((base) + 0x0a)
#define REG_SARU0(base)		((base) + 0x0b)
#define REG_SARL1(base)		((base) + 0x0c)
#define REG_SARU1(base)		((base) + 0x0d)
#define REG_SARL2(base)		((base) + 0x0e)
#define REG_SARU2(base)		((base) + 0x0f)
#define REG_ICBRL(base)		((base) + 0x10)
#define REG_ICBRH(base)		((base) + 0x11)
#define REG_ICDRT(base)		((base) + 0x12)
#define REG_ICDRR(base)		((base) + 0x13)

/* Definitions for the programmable state machine */
#define CODE_MEM_SIZE (512)

#define INSTR_SDA_L             (0x01000000)
#define INSTR_SDA_H             (0x02000000)
#define INSTR_SCL_L             (0x03000000)
#define INSTR_SCL_H             (0x04000000)
#define INSTR_CHECK_BUSFREE     (0x05000000)
#define INSTR_NDELAY            (0x06000000)
#define INSTR_SYNC              (0x07000000)
#define INSTR_END               (0x08000000)
#define INSTR_INTERRUPT         (0x09000000)
#define INSTR_MSTR_STATE        (0x0f000000)
#define ADD_CODE(riic,x) ((riic)->mstr_code[((riic)->mstr_icount++) % CODE_MEM_SIZE] = (x))

#define STARTMODE_REPSTART (1)
#define STARTMODE_START (2)

#define T_HDSTA(riic)    ((riic)->timing.t_hdsta)
#define T_LOW(riic)      ((riic)->timing.t_low)
#define T_HIGH(riic)     ((riic)->timing.t_high)
#define T_SUSTA(riic)    ((riic)->timing.t_susta)
#define T_HDDAT_MAX(riic)        ((riic)->timing.t_hddat_max)
#define T_HDDAT(riic)    (T_HDDAT_MAX(riic) >> 1)
#define T_SUDAT(riic)    ((riic)->timing.t_sudat)
#define T_SUSTO(riic)    ((riic)->timing.t_susto)
#define T_BUF(riic)      ((riic)->timing.t_buf)

typedef struct I2C_Timing {
	int speed;
	uint32_t t_hdsta;
	uint32_t t_low;
	uint32_t t_high;
	uint32_t t_susta;
	uint32_t t_hddat_max;
	uint32_t t_sudat;
	uint32_t t_susto;
	uint32_t t_buf;
} I2C_Timing;

//define MSTR_ACK     (1)
//define MSTR_NACK    (0)

typedef struct RxI2c {
	BusDevice bdev;
	SigNode *sigSda;
	SigNode *sigScl;
	uint8_t regICCR1;
	uint8_t regICCR2;
	uint8_t regICMR1;
	uint8_t regICMR2;
	uint8_t regICMR3;
	uint8_t regIICFER;
	uint8_t regIICSER;
	uint8_t regIICIER;
	uint8_t regIICSR1;
	uint8_t regIICSR2;
	uint8_t regISARL0;
	uint8_t regISARU0;
	uint8_t regISARL1;
	uint8_t regISARU1;
	uint8_t regISARL2;
	uint8_t regISARU2;
	uint8_t regIICBRL;
	uint8_t regIICBRH;
	uint8_t regIICDRT;
	uint8_t regIICDRR;

	I2C_Timing timing;
	/* The programmable I2C state machine */
	SigTrace *mstr_sclStretchTrace;
	//CycleTimer mstr_delay_timer;
	//CycleTimer mstr_scl_timer;
	//CycleTimer slv_release_scl_timer;
	uint16_t mstr_ip;	/* The Instruction pointer in codemem */
	uint32_t mstr_code[CODE_MEM_SIZE];
	uint32_t mstr_icount;	/* The number of instructions in code memory */
} RxI2c;

/**
 * Assemble the sequence for a start condition
 */
#if 0
static void
assemble_script_start(RxI2c * ri, int startmode)
{
	/* For repeated start do not assume SDA and SCL state */
	ADD_CODE(ri, INSTR_MSTR_STATE /* | TWIM_STATE_START */ );
	if (startmode == STARTMODE_REPSTART) {
		ADD_CODE(ri, INSTR_SDA_H);
		ADD_CODE(ri, INSTR_NDELAY | (T_LOW(ri) - T_HDDAT(ri)));

		ADD_CODE(ri, INSTR_SCL_H);
		ADD_CODE(ri, INSTR_SYNC);
		ADD_CODE(ri, INSTR_NDELAY | T_HIGH(ri));
	} else {
		ADD_CODE(ri, INSTR_NDELAY | T_BUF(ri));
	}

	/* Generate a start condition */
	/* Try again if bus not free somehow ???????????????? */
	if (startmode == STARTMODE_START) {
		ADD_CODE(ri, INSTR_CHECK_BUSFREE /* | TWISL_STATE_ADDR */ );
	}
	ADD_CODE(ri, INSTR_SDA_L);
	ADD_CODE(ri, INSTR_NDELAY | T_HDSTA(ri));
	/* Enter the interrupt with clock low and T_HDDAT waited */
	ADD_CODE(ri, INSTR_SCL_L);
	ADD_CODE(ri, INSTR_NDELAY | T_HDDAT(ri));
	if (startmode == STARTMODE_REPSTART) {
		ADD_CODE(ri, INSTR_INTERRUPT /* | TW_REP_START */ );
	} else {
		ADD_CODE(ri, INSTR_INTERRUPT /* | TW_START */ );
	}
}
#endif

/*
 ******************************************************************
 * ICCR1 Register
 * Bit 7: ICE I2C Bus Interface enable switches between portmode and I2C
 * Bit 6: IICRST I2C Reset 1=reset 0=not in reset
 * Bit 5: CLO  SCL extra clock cycle
 * Bit 4: SOWP SCL/SDA Output write protection (0 = unprotected, read 1)
 * Bit 3: SCLO SCL Output Control
 * Bit 2: SDAO SDA Output Control
 * Bit 1: SCLI SCL Input Monitor
 * Bit 0: SDAI SDA Input Monitor
 ******************************************************************
*/
static uint32_t
iccr1_read(void *clientData, uint32_t address, int rqlen)
{
	RxI2c *ri = (RxI2c *) clientData;
	ri->regICCR1 &= ~(ICCR1_SDAI | ICCR1_SCLI | ICCR1_SDAO | ICCR1_SCLO);
	if (SigNode_Val(ri->sigSda) == SIG_HIGH) {
		ri->regICCR1 |= ICCR1_SDAI;
	}
	if (SigNode_Val(ri->sigScl) == SIG_HIGH) {
		ri->regICCR1 |= ICCR1_SCLI;
	}
	if (SigNode_State(ri->sigSda) != SIG_LOW) {
		ri->regICCR1 |= ICCR1_SDAO;
	}
	if (SigNode_State(ri->sigScl) != SIG_LOW) {
		ri->regICCR1 |= ICCR1_SCLO;
	}
	return ri->regICCR1 | ICCR1_SOWP;
}

static void
iccr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//RxI2c *ri = clientData;
	//if(ri->)
	//
}

/*
 **********************************************************************
 * ICCR Register
 * BBSY:	Bus busy detection flag
 * MST:		Master/Slave mode
 * TRS:		Transmit/Receive Mode
 * SP:		Stop Condition Request
 * RS:		Restart condition request
 * ST:		Start condition request
 **********************************************************************
 */
static uint32_t
iccr2_read(void *clientData, uint32_t address, int rqlen)
{
	RxI2c *ri = clientData;
	return ri->regICCR2;
}

static void
iccr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

/*
 ***********************************************************************
 * IIC Mode register 1
 * Bit 0 - 3: BC Bit counter 
 * Bit 3: BCWP Bit counter write protect (0 = enabled)
 * Bit 4 - 6: CKS  Clock select 
 * Bit 7: MTWP  MST and TRS write protect.
 ***********************************************************************
 */
static uint32_t
icmr1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icmr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

/**
 ***********************************************************
 * DLCS SDA output delay clock source selection
 * SDDL SDA output delay counter.
 * TMOH Timeout H count control
 * TMOL Timeout L count control
 * TMOS Timeout detection time selection
 ***********************************************************
 */
static uint32_t
icmr2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icmr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

/**
 **************************************************************
 * SMBS: SMBus/I2C-Bus selection
 * WAIT: Wait after ninth clock cycle by SCL stretching
 * RDRFS: 
 * ACKWP:
 * ACKBT:
 * ACKBR:
 * NF_MSK:
 **************************************************************
 */

static uint32_t
icmr3_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icmr3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

/*
 *********************************************************************
 * SCLE: Scl synchronous circuit enable
 * NFE: digital noise filter enable
 * NACKE: nack reception transfer suspension enable
 * SALE: slave arbitration lost detection enable
 * NALE: nack transmission arbitration lost detection enable
 * MALE: master arbitration loss detection enable
 * TMOE: Enable the timeout function
 * (FMPE) Fast mode plus ???
 *********************************************************************
 */
static uint32_t
icfer_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icfer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

/**
 *****************************************************************
 * I2C Bus status enable register
 *****************************************************************
 */
static uint32_t
icser_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icser_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
icier_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
icsr1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icsr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
icsr2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icsr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
sarl0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
sarl0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
saru0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
saru0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
sarl1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
sarl1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
saru1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
saru1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
sarl2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
sarl2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
saru2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
saru2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
icbrl_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icbrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
icbrh_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icbrh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
icdrt_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icdrt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
icdrr_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
icdrr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static void
RxI2c_UnMap(void *owner, uint32_t base, uint32_t mapsize)
{
	IOH_Delete8(REG_ICCR1(base));
	IOH_Delete8(REG_ICCR2(base));
	IOH_Delete8(REG_ICMR1(base));
	IOH_Delete8(REG_ICMR2(base));
	IOH_Delete8(REG_ICMR3(base));
	IOH_Delete8(REG_ICFER(base));
	IOH_Delete8(REG_ICSER(base));
	IOH_Delete8(REG_ICIER(base));
	IOH_Delete8(REG_ICSR1(base));
	IOH_Delete8(REG_ICSR2(base));
	IOH_Delete8(REG_SARL0(base));
	IOH_Delete8(REG_SARU0(base));
	IOH_Delete8(REG_SARL1(base));
	IOH_Delete8(REG_SARU1(base));
	IOH_Delete8(REG_SARL2(base));
	IOH_Delete8(REG_SARU2(base));
	IOH_Delete8(REG_ICBRL(base));
	IOH_Delete8(REG_ICBRH(base));
	IOH_Delete8(REG_ICDRT(base));
	IOH_Delete8(REG_ICDRR(base));
}

static void
RxI2c_Map(void *owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	RxI2c *ri = owner;
	IOH_New8(REG_ICCR1(base), iccr1_read, iccr1_write, ri);
	IOH_New8(REG_ICCR2(base), iccr2_read, iccr2_write, ri);
	IOH_New8(REG_ICMR1(base), icmr1_read, icmr1_write, ri);
	IOH_New8(REG_ICMR2(base), icmr2_read, icmr2_write, ri);
	IOH_New8(REG_ICMR3(base), icmr3_read, icmr3_write, ri);
	IOH_New8(REG_ICFER(base), icfer_read, icfer_write, ri);
	IOH_New8(REG_ICSER(base), icser_read, icser_write, ri);
	IOH_New8(REG_ICIER(base), icier_read, icier_write, ri);
	IOH_New8(REG_ICSR1(base), icsr1_read, icsr1_write, ri);
	IOH_New8(REG_ICSR2(base), icsr2_read, icsr2_write, ri);
	IOH_New8(REG_SARL0(base), sarl0_read, sarl0_write, ri);
	IOH_New8(REG_SARU0(base), saru0_read, saru0_write, ri);
	IOH_New8(REG_SARL1(base), sarl1_read, sarl1_write, ri);
	IOH_New8(REG_SARU1(base), saru1_read, saru1_write, ri);
	IOH_New8(REG_SARL2(base), sarl2_read, sarl2_write, ri);
	IOH_New8(REG_SARU2(base), saru2_read, saru2_write, ri);
	IOH_New8(REG_ICBRL(base), icbrl_read, icbrl_write, ri);
	IOH_New8(REG_ICBRH(base), icbrh_read, icbrh_write, ri);
	IOH_New8(REG_ICDRT(base), icdrt_read, icdrt_write, ri);
	IOH_New8(REG_ICDRR(base), icdrr_read, icdrr_write, ri);
}

BusDevice *
RX62T_I2CNew(const char *name)
{
	RxI2c *ri = sg_new(RxI2c);
	ri->sigSda = SigNode_New("%s.sda", name);
	ri->sigScl = SigNode_New("%s.scl", name);
	if (!ri->sigSda || !ri->sigScl) {
		fprintf(stderr, "Can not create RX-I2C signal lines\n");
		exit(1);
	}
	ri->bdev.first_mapping = NULL;
	ri->bdev.Map = RxI2c_Map;
	ri->bdev.UnMap = RxI2c_UnMap;
	ri->bdev.owner = ri;
	ri->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "RX62t I2C Controller created\n");
	return &ri->bdev;
}
