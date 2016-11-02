#include "bus.h"
#include "sgstring.h"
#include "socket_can.h"
#include "cycletimer.h"
#include "signode.h"
#include "dcan_bosch.h"
#include "clock.h"
#include <stdint.h>

#if 1 
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define CAN_CTRL(base)           ((base) + 0x00)
#define		CTRL_TEST	(1 << 7) 
#define		CTRL_CCE 	(1 << 6)
#define 	CTRL_DAR 	(1 << 5)
#define		CTRL_EIE 	(1 << 3)
#define		CTRL_SIE 	(1 << 2)
#define		CTRL_IE 	(1 << 1)
#define		CTRL_INIT	(1 << 0)
#define CAN_STATUS(base)         ((base) + 0x04)
#define		STATUS_BOFF 		(1 << 7)
#define		STATUS_EWARN 		(1 << 6)
#define		STATUS_EPASS 		(1 << 5)
#define		STATUS_RXOK 		(1 << 4)
#define		STATUS_TXOK 		(1 << 3)
#define		STATUS_LEC_MASK		(0x7)
#define		LEC_NOEV		(0x7)
#define		LEC_CRCERR		(0x6)
#define		LEC_BIT0ERR		(0x5)
#define		LEC_BIT1ERR		(0x4)
#define		LEC_ACKERR		(0x3)
#define		LEC_FORMERR		(0x2)
#define		LEC_STUFFERR		(0x1)
#define		LEC_NOERR		(0x0)

#define CAN_ERROR(base)          ((base) + 0x08)
#define		ERROR_RP	(1 << 15)
#define		ERROR_REC_MSK	((0x7f) << 8)
#define		ERROR_TEC_MSK	(0xff)
#define CAN_BIT_TIMING(base)     ((base) + 0x0C)
#define 	BIT_TIMING_TSEG2_MSK	(7 << 12)
#define		BIT_TIMING_TSEG1_MSK	(0xf << 8)
#define		BIT_TIMING_SJW_MSK	(0x3 << 6)
#define		BIT_TIMING_BRP_MSK	(0x3f)

/** Number of message object which caused the interrupt */
#define CAN_IRQ(base)            ((base) + 0x10)

#define CAN_TEST(base)           ((base) + 0x14)
#define		TEST_RX 	(1 << 7)	
#define		TEST_TX1 	(1 << 6)
#define		TEST_TX0 	(1 << 5)
#define		TEST_LBACK 	(1 << 4)
#define		TEST_SILENT	(1 << 3)
#define		TEST_BASIC	(1 << 2)
#define CAN_BRP_EXT(base)        ((base) + 0x18)
#define CAN_IF1_CMD_REQ(base)    ((base) + 0x20)
#define		CR_BUSY			(1 << 4)
#define		CR_MSG_NO_MSK		(0x1f)
#define CAN_IF1_CMD_MASK(base)   ((base) + 0x24)
#define		CM_WRRD 		(1 << 7)
#define		CM_MASK 		(1 << 6)
#define		CM_ARB 			(1 << 5)
#define		CM_CONTROL 		(1 << 4)
#define		CM_CLRINTPND 		(1 << 3)
#define		CM_TXRQST		(1 << 2)
#define		CM_NEWDAT		(1 << 2)
#define		CM_DATAA		(1 << 1)
#define		CM_DATAB		(1 << 0)
/** Mask decides if bit is used for acceptance filtering */
#define CAN_IF1_MASK1(base)      ((base) + 0x28)
#define CAN_IF1_MASK2(base)      ((base) + 0x2C)
#define		MSK2_MXTD	(1 << 15)
#define		MSK2_MDIR	(1 << 14)
/** ARBIT is the Message ID */
#define CAN_IF1_ARBIT1(base)     ((base) + 0x30)
#define CAN_IF1_ARBIT2(base)     ((base) + 0x34)
#define 	ARB2_MSGVAL	(1 << 15)
#define		ARB2_XTD	(1 << 14)
#define		ARB2_DIR	(1 << 13)
#define CAN_IF1_MSG_CTRL(base)   ((base) + 0x38)
#define 	MC_NEWDAT 		(1 << 15)
#define 	MC_MSGLST 		(1 << 14)
#define 	MC_INTPND 		(1 << 13)
#define 	MC_UMASK 		(1 << 12)
#define 	MC_TXIE 		(1 << 11)
#define 	MC_RXIE 		(1 << 10)
#define 	MC_RMTEN 		(1 << 9)
#define 	MC_TXRQST 		(1 << 8)
#define 	MC_EOB 			(1 << 7)
#define 	MC_DLC_MSK	(0xf)
#define CAN_IF1_DATA_A1(base)    ((base) + 0x3C)
#define CAN_IF1_DATA_A2(base)    ((base) + 0x40)
#define CAN_IF1_DATA_B1(base)    ((base) + 0x44)
#define CAN_IF1_DATA_B2(base)    ((base) + 0x48)

#define CAN_IF2_CMD_REQ(base)    ((base) + 0x80)
#define CAN_IF2_CMD_MASK(base)   ((base) + 0x84)
#define CAN_IF2_MASK1(base)      ((base) + 0x88)
#define CAN_IF2_MASK2(base)      ((base) + 0x8C)
#define CAN_IF2_ARBIT1(base)     ((base) + 0x90)
#define CAN_IF2_ARBIT2(base)     ((base) + 0x94)
#define CAN_IF2_MSG_CTRL(base)   ((base) + 0x98)
#define CAN_IF2_DATA_A1(base)    ((base) + 0x9C)
#define CAN_IF2_DATA_A2(base)    ((base) + 0xA0)
#define CAN_IF2_DATA_B1(base)    ((base) + 0xA4)
#define CAN_IF2_DATA_B2(base)    ((base) + 0xA8)

/** Transmission Request Register: 1 Bit per message Object */
#define CAN_TX_REQ_1(base)       ((base) + 0x100)
#define CAN_TX_REQ_2(base)       ((base) + 0x104)
/** New Data Register: 1 Bit per message Object */
#define CAN_NEW_DATA_1(base)     ((base) + 0x120)
#define CAN_NEW_DATA_2(base)     ((base) + 0x124)
/** Interrupt pending Register: 1 Bit per message Object, copied from message object */
#define CAN_IRQ_PEND_1(base)     ((base) + 0x140)
#define CAN_IRQ_PEND_2(base)     ((base) + 0x144)
#define CAN_MSG_VALID_1(base)    ((base) + 0x160)
#define CAN_MSG_VALID_2(base)    ((base) + 0x164)

/** From page 260 */
typedef struct MsgObj {
	uint16_t Msk1;
	uint16_t Msk2;
	uint16_t MsgCtrl;	
	uint8_t TxRqst;
	uint16_t Arbit1;
	uint16_t Arbit2;
	uint16_t DataA1;
	uint16_t DataA2;
	uint16_t DataB1;
	uint16_t DataB2;
} MsgObj;

typedef struct Bosch_Can {
	BusDevice bdev;
	CanController *backend;
	CycleTimer baud_timer;
	Clock_t *bitclk;
	Clock_t *clk;
	SigNode *sigIrq;
	CycleCounter_t if1_busy_until;
	CycleCounter_t if2_busy_until;
	uint16_t regCtrl;
	uint16_t regStatus;
	uint16_t regError;
	uint16_t regBitTiming;
	uint16_t regIrq;
	uint16_t regTest;
	uint16_t regBrpExt;
	uint16_t regIf1CmdReq;
	uint16_t regIf1CmdMask;
	uint16_t regIf1Mask1;
	uint16_t regIf1Mask2;
	uint16_t regIf1Arbit1;
	uint16_t regIf1Arbit2;
	uint16_t regIf1MsgCtrl;
	uint16_t regIf1DataA1;
	uint16_t regIf1DataA2;
	uint16_t regIf1DataB1;
	uint16_t regIf1DataB2;
	uint16_t regIf2CmdReq;
	uint16_t regIf2CmdMask;
	uint16_t regIf2Mask1;
	uint16_t regIf2Mask2;
	uint16_t regIf2Arbit1;
	uint16_t regIf2Arbit2;
	uint16_t regIf2MsgCtrl;
	uint16_t regIf2DataA1;
	uint16_t regIf2DataA2;
	uint16_t regIf2DataB1;
	uint16_t regIf2DataB2;
	uint16_t regTxReq1;
	uint16_t regTxReq2;
	uint16_t regNewData1;
	uint16_t regNewData2;
	uint16_t regIrqPend1;
	uint16_t regIrqPend2;
	uint16_t regMsgValid1;
	uint16_t regMsgValid2;
	MsgObj msgObj[32];
} Bosch_Can;

static void
update_interrupt(Bosch_Can *can)
{

}

static void
update_bitrate(Bosch_Can *can) 
{
	uint32_t brp;
	uint32_t tseg1,tseg2;
	uint32_t sync_seg = 1;
	uint32_t tq;
	uint32_t div;
	brp = can->regBitTiming & 0x3f;
	brp |= (can->regBrpExt & 0xf) << 6;
	brp++;
	tseg2 = ((can->regBitTiming >> 12) & 0xf) + 1;
	tseg1 = ((can->regBitTiming >> 8) & 0xf) + 1;
	tq = sync_seg + tseg1 + tseg2;	
	div = brp * tq;
	Clock_MakeDerived(can->bitclk,can->clk,1,div);
}

static void
if2_request(Bosch_Can *can,uint16_t msgobj) 
{
	uint16_t msk = can->regIf2CmdMask;
	MsgObj *obj = &can->msgObj[msgobj & 31];	
	if(msk & CM_WRRD) {
		if(msk & CM_MASK) {
			obj->Msk1 = can->regIf2Mask1;
			obj->Msk2 = can->regIf2Mask2;
		}
		if(msk & CM_ARB) {
			obj->Arbit1 = can->regIf2Arbit1;
			obj->Arbit2 = can->regIf2Arbit2;
		}
		if(msk & CM_CONTROL) {
			obj->MsgCtrl = can->regIf2MsgCtrl;
		}
		if(msk & CM_CLRINTPND) {
			/* No effect */
		}
		if(msk & CM_TXRQST) {
			obj->MsgCtrl |= MC_TXRQST;
		}
		if(msk & CM_DATAA) {
			obj->DataA1 = can->regIf2DataA1;
			obj->DataA2 = can->regIf2DataA2;
		}
		if(msk & CM_DATAB) {
			obj->DataB1 = can->regIf2DataB1;
			obj->DataB2 = can->regIf2DataB2;
		}
		// update_interrupts();
		/* Can be set in two ways ? */
		if(obj->MsgCtrl & MC_TXRQST) {
		}
	} else {
		if(msk & CM_MASK) {
			can->regIf2Mask1 = obj->Msk1;
			can->regIf2Mask2 = obj->Msk2;
		}
		if(msk & CM_ARB) {
			can->regIf2Arbit1 = obj->Arbit1;
			can->regIf2Arbit2 = obj->Arbit2;
		}
		if(msk & CM_CONTROL) {
			can->regIf2MsgCtrl = obj->MsgCtrl;	
		}
		if(msk & CM_CLRINTPND) {
			obj->MsgCtrl &= ~MC_INTPND;
		}
		if(msk & CM_NEWDAT) {
			obj->MsgCtrl &= ~MC_NEWDAT;
		}
		if(msk & CM_DATAA) {
			can->regIf2DataA1 = obj->DataA1;
			can->regIf2DataA2 = obj->DataA2;
		}
		if(msk & CM_DATAB) {
			can->regIf2DataB1 = obj->DataB1;
			can->regIf2DataB2 = obj->DataB2;
		}
	}
}

static void
if1_request(Bosch_Can *can,uint16_t msgobj) 
{
	uint16_t msk = can->regIf1CmdMask;
	MsgObj *obj = &can->msgObj[msgobj & 31];	
	if(msk & CM_WRRD) {
		if(msk & CM_MASK) {
			obj->Msk1 = can->regIf1Mask1;
			obj->Msk2 = can->regIf1Mask2;
		}
		if(msk & CM_ARB) {
			obj->Arbit1 = can->regIf1Arbit1;
			obj->Arbit2 = can->regIf1Arbit2;
		}
		if(msk & CM_CONTROL) {
			obj->MsgCtrl = can->regIf1MsgCtrl;
		}
		if(msk & CM_CLRINTPND) {
			/* No effect */
		}
		if(msk & CM_TXRQST) {
			obj->MsgCtrl |= MC_TXRQST;
		}
		if(msk & CM_DATAA) {
			obj->DataA1 = can->regIf1DataA1;
			obj->DataA2 = can->regIf1DataA2;
		}
		if(msk & CM_DATAB) {
			obj->DataB1 = can->regIf1DataB1;
			obj->DataB2 = can->regIf1DataB2;
		}
		// update_interrupts();
		/* Can be set in two ways ? */
		if(obj->MsgCtrl & MC_TXRQST) {
		}
	} else {
		if(msk & CM_MASK) {
			can->regIf1Mask1 = obj->Msk1;
			can->regIf1Mask2 = obj->Msk2;
		}
		if(msk & CM_ARB) {
			can->regIf1Arbit1 = obj->Arbit1;
			can->regIf1Arbit2 = obj->Arbit2;
		}
		if(msk & CM_CONTROL) {
			can->regIf1MsgCtrl = obj->MsgCtrl;	
		}
		if(msk & CM_CLRINTPND) {
			obj->MsgCtrl &= ~MC_INTPND;
		}
		if(msk & CM_NEWDAT) {
			obj->MsgCtrl &= ~MC_NEWDAT;
		}
		if(msk & CM_DATAA) {
			can->regIf1DataA1 = obj->DataA1;
			can->regIf1DataA2 = obj->DataA2;
		}
		if(msk & CM_DATAB) {
			can->regIf1DataB1 = obj->DataB1;
			can->regIf1DataB2 = obj->DataB2;
		}
	}
}

static void
make_busy_if1(Bosch_Can *can,uint32_t nanoseconds)
{
        can->if1_busy_until = CycleCounter_Get() + NanosecondsToCycles(nanoseconds);
}

static void
make_busy_if2(Bosch_Can *can,uint32_t nanoseconds)
{
        can->if2_busy_until = CycleCounter_Get() + NanosecondsToCycles(nanoseconds);
}

/*
 ****************************************************************************************
 * Bit 7: TEST
 * Bit 6: CCE Enable access to the Bittiming register (during init == 1)
 * Bit 5: DAR Disable automatic retransmission
 * Bit 3: EIE Error interrupt enable
 * Bit 2: SIE Status change interrupt enable
 * Bit 1: IE  Module interrupt enable
 * Bit 0: INIT
 ****************************************************************************************
 */
static uint32_t
ctrl_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regCtrl;
}

static void
ctrl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regCtrl =value;
	fprintf(stderr,"Bosch CAN: %s only partially implemented\n",__func__);
}

/**
 *******************************************************************************
 * Bit 7: BOFF 		Bus Off
 * Bit 6: EWARN		Warning Level
 * Bit 5: EPASS		Bus Passive
 * Bit 4: RXOK		Successfull reception since last reset of this bit.	
 * Bit 3: TXOK		Successfull trasnmission since last reset of this bit.
 * Bit 0+1: LEC_MASK	Last error code
 * Error codes:
 *	NOEV
 *	CRCERR
 *	BIT0ERR
 *	BIT1ERR	
 *	ACKERR
 *	FORMERR
 *	STUFFERR
 *	NOERR
 * Reading the status register clears the status change interrupt in the
 * irq-status register.
 *******************************************************************************
 */
static uint32_t
status_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	uint16_t value = can->regStatus;
	if(can->regIrq & 0x8000) {
		can->regIrq &= ~0x8000;
		update_interrupt(can);
	}
	return value;
}

/**
 *****************************************************************************
 * RXOK and TXOK are writable but i don't know if it is clear-only
 *****************************************************************************
 */
static void
status_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	uint16_t wmask = STATUS_RXOK | STATUS_TXOK;
	can->regStatus = (can->regStatus & ~wmask) | (value & wmask);
	fprintf(stderr,"Bosch CAN: %s not implemented\n",__func__);
}

/**
 ************************************************************************
 * The error counter register is readolny
 ************************************************************************
 */
static uint32_t
error_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regError;
}

static void
error_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s writing to readonly register\n",__func__);
}

/**
 * the bittiming register
 */
static uint32_t
bit_timing_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regBitTiming;
}

static void
bit_timing_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	uint16_t enmask = CTRL_CCE | CTRL_INIT;
	if((can->regCtrl & enmask) == enmask) {
		can->regBitTiming = value;
		can->regBrpExt = value & 0xf;
		dbgprintf("CAN: Bit Timing Write %08x\n",value);
		update_bitrate(can);
	} else {
		fprintf(stderr,"Bit Timing Write: Writing is not enabled\n");
	}
}

/**
 *****************************************************************************
 * IRQ Read searches for the first active Interrupt.
 * First comes the Status Interrupt, Then the 32 Message objects in RAM. 
 *****************************************************************************
 */
static uint32_t
irq_read(void *clientData,uint32_t address,int rqlen)
{
	//Bosch_Can *can = clientData;
	fprintf(stderr,"Bosch CAN: %s not implemented\n",__func__);
	return 0;
}

static void
irq_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s not implemented\n",__func__);
}

static uint32_t
test_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s not implemented\n",__func__);
	return 0;
}

static void
test_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s not implemented\n",__func__);
}

/**
 * 
 */
static uint32_t
brp_ext_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regBrpExt;
}

static void
brp_ext_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	uint16_t enmask = CTRL_CCE | CTRL_INIT;
	if((can->regCtrl & enmask) == enmask) {
		can->regBrpExt = value & 0xf;
		update_bitrate(can);
	} else {
		fprintf(stderr,"Writing bitrate is disabled\n");
	}
}

/**
 * \fn static uint32_t if1_cmd_req_read(void *clientData,uint32_t address,int rqlen)
 * Used for command request busy polling.
 */
static uint32_t
if1_cmd_req_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
     	if(CycleCounter_Get() < can->if1_busy_until) {
		return can->regIf1CmdReq | CR_BUSY;
        } else {
		return can->regIf1CmdReq & ~CR_BUSY;
	}
}

static void
if1_cmd_req_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	uint16_t msgobj = (value - 1) & 0x1f; 
	if1_request(can,msgobj);
	make_busy_if1(can,100);
}

static uint32_t
if1_cmd_mask_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf1CmdMask;
}

static void
if1_cmd_mask_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf1CmdMask = value;
}

static uint32_t
if1_mask1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf1Mask1;
}

static void
if1_mask1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf1Mask1 = value;
}

static uint32_t
if1_mask2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf1Mask2;
}

static void
if1_mask2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf1Mask2 = value;
}

static uint32_t
if1_arbit1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf1Arbit1;
}

static void
if1_arbit1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf1Arbit1 = value;
}

static uint32_t
if1_arbit2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf1Arbit2;
}

static void
if1_arbit2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf1Arbit2 = value;
}

static uint32_t
if1_msg_ctrl_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf1MsgCtrl;
}

static void
if1_msg_ctrl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf1MsgCtrl = value;
}

static uint32_t
if1_data_a1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf1DataA1;
}

static void
if1_data_a1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf1DataA1 = value;
}

static uint32_t
if1_data_a2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf1DataA2;
}

static void
if1_data_a2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf1DataA2 = value;
}

static uint32_t
if1_data_b1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf1DataB1;
}

static void
if1_data_b1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf1DataB1 = value;
}

static uint32_t
if1_data_b2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf1DataB2;
}

static void
if1_data_b2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf1DataB2 = value;
}

static uint32_t
if2_cmd_req_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
     	if(CycleCounter_Get() < can->if2_busy_until) {
		return can->regIf2CmdReq | CR_BUSY;
        } else {
		return can->regIf2CmdReq & ~CR_BUSY;
	}
}

static void
if2_cmd_req_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	uint16_t msgobj = (value - 1) & 0x1f; 
	if2_request(can,msgobj);
	make_busy_if2(can,100);
}

static uint32_t
if2_cmd_mask_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf2CmdMask;
}

static void
if2_cmd_mask_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf2CmdMask = value;
}

static uint32_t
if2_mask1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf2Mask1;
}

static void
if2_mask1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf2Mask1 = value;
}

static uint32_t
if2_mask2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf2Mask2;
}

static void
if2_mask2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf2Mask2 = value;
}


static uint32_t
if2_arbit1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf2Arbit1;
}

static void
if2_arbit1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf2Arbit1 = value;
}

static uint32_t
if2_arbit2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf2Arbit2;
}

static void
if2_arbit2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf2Arbit2 = value;
}

static uint32_t
if2_msg_ctrl_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf2MsgCtrl;
}

static void
if2_msg_ctrl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf2MsgCtrl = value;
}

static uint32_t
if2_data_a1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf2DataA1;
}

static void
if2_data_a1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf2DataA1 = value;
}

static uint32_t
if2_data_a2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf2DataA2;
}

static void
if2_data_a2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf2DataA2 = value;
}

static uint32_t
if2_data_b1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf2DataB1;
}

static void
if2_data_b1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf2DataB1 = value;
}

static uint32_t
if2_data_b2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	return can->regIf2DataB2;
}

static void
if2_data_b2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	can->regIf2DataB2 = value;
}

static uint32_t
tx_req_1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	int i;
	uint16_t value = 0;
	for(i = 0; i < 16; i++) {
		MsgObj *obj = &can->msgObj[i];
		if(obj->MsgCtrl & MC_TXRQST) {
			value |= (1 << i);
		}
	}
	return value;
}

static void
tx_req_1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s: Writing to readonly register\n",__func__);
}

static uint32_t
tx_req_2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	int i;
	uint16_t value = 0;
	for(i = 0; i < 16; i++) {
		MsgObj *obj = &can->msgObj[i + 16];
		if(obj->MsgCtrl & MC_TXRQST) {
			value |= (1 << i);
		}
	}
	return value;
}

static void
tx_req_2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s: Writing to readonly register\n",__func__);
}

static uint32_t
new_data_1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	int i;
	uint16_t value = 0;
	for(i = 0; i < 16; i++) {
		MsgObj *obj = &can->msgObj[i];
		if(obj->MsgCtrl & MC_NEWDAT) {
			value |= (1 << i);
		}
	}
	return value;
}

static void
new_data_1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s: Writing to readonly register\n",__func__);
}

static uint32_t
new_data_2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	int i;
	uint16_t value = 0;
	for(i = 0; i < 16; i++) {
		MsgObj *obj = &can->msgObj[i + 16];
		if(obj->MsgCtrl & MC_NEWDAT) {
			value |= (1 << i);
		}
	}
	return value;
}

static void
new_data_2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s: Writing to readonly register\n",__func__);
}

static uint32_t
irq_pend_1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	int i;
	uint16_t value = 0;
	for(i = 0; i < 16; i++) {
		MsgObj *obj = &can->msgObj[i];
		if(obj->MsgCtrl & MC_INTPND) {
			value |= (1 << i);
		}
	}
	return value;
}

static void
irq_pend_1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s: Writing to readonly register\n",__func__);
}

static uint32_t
irq_pend_2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	int i;
	uint16_t value = 0;
	for(i = 0; i < 16; i++) {
		MsgObj *obj = &can->msgObj[i + 16];
		if(obj->MsgCtrl & MC_INTPND) {
			value |= (1 << i);
		}
	}
	return value;
}

static void
irq_pend_2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s: Writing to readonly register\n",__func__);
}


static uint32_t
msg_valid_1_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	int i;
	uint16_t value = 0;
	for(i = 0; i < 16; i++) {
		MsgObj *obj = &can->msgObj[i];
		if(obj->Arbit2 & ARB2_MSGVAL) {
			value |= (1 << i);
		}
	}
	return value;
}

static void
msg_valid_1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s: Writing to readonly register\n",__func__);
}

static uint32_t
msg_valid_2_read(void *clientData,uint32_t address,int rqlen)
{
	Bosch_Can *can = clientData;
	int i;
	uint16_t value = 0;
	for(i = 0; i < 16; i++) {
		MsgObj *obj = &can->msgObj[i + 16];
		if(obj->Arbit2 & ARB2_MSGVAL) {
			value |= (1 << i);
		}
	}
	return value;
}

static void
msg_valid_2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Bosch CAN: %s: Writing to readonly register\n",__func__);
}

static void
resume_rx(void *clientData)
{
        Bosch_Can *can = (Bosch_Can *)clientData;
        CanStartRx(can->backend);
}

static  void
Bosch_CanReceive (void *clientData,CAN_MSG *msg)
{
	Bosch_Can *can = clientData;
	uint32_t nr_bits;
	uint32_t cycles;
	FractionU64_t frac;

	CanStopRx(can->backend);
	if(msg->can_id & CAN_EFF_FLAG) {
		nr_bits = 29 + 15;
	} else {
		nr_bits = 11 + 15;
	}
	nr_bits += msg->can_dlc * 8;
	frac = Clock_MasterRatio(can->bitclk);
        if(frac.nom) {
		cycles = nr_bits * frac.denom / frac.nom;
        	CycleTimer_Mod(&can->baud_timer,cycles);
	} else {
       		CycleTimer_Mod(&can->baud_timer,1000000000);
	}
}


static void
TCan_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	Bosch_Can *can = owner;
//        IOH_New32(ASC_BAUDRATE(base),baudrate_read,baudrate_write,asc);
	IOH_New16(CAN_CTRL(base),ctrl_read,ctrl_write,can);
	IOH_New16(CAN_STATUS(base),status_read,status_write,can);
	IOH_New16(CAN_ERROR(base),error_read,error_write,can);
	IOH_New16(CAN_BIT_TIMING(base),bit_timing_read,bit_timing_write,can);
	IOH_New16(CAN_IRQ(base),irq_read,irq_write,can);
	IOH_New16(CAN_TEST(base),test_read,test_write,can);
	IOH_New16(CAN_BRP_EXT(base),brp_ext_read,brp_ext_write,can);
	IOH_New16(CAN_IF1_CMD_REQ(base),if1_cmd_req_read,if1_cmd_req_write,can);
	IOH_New16(CAN_IF1_CMD_MASK(base),if1_cmd_mask_read,if1_cmd_mask_write,can);
	IOH_New16(CAN_IF1_MASK1(base),if1_mask1_read,if1_mask1_write,can);
	IOH_New16(CAN_IF1_MASK2(base),if1_mask2_read,if1_mask2_write,can);
	IOH_New16(CAN_IF1_ARBIT1(base),if1_arbit1_read,if1_arbit1_write,can);
	IOH_New16(CAN_IF1_ARBIT2(base),if1_arbit2_read,if1_arbit2_write,can);
	IOH_New16(CAN_IF1_MSG_CTRL(base),if1_msg_ctrl_read,if1_msg_ctrl_write,can);
	IOH_New16(CAN_IF1_DATA_A1(base),if1_data_a1_read,if1_data_a1_write,can);
	IOH_New16(CAN_IF1_DATA_A2(base),if1_data_a2_read,if1_data_a2_write,can);
	IOH_New16(CAN_IF1_DATA_B1(base),if1_data_b1_read,if1_data_b1_write,can);
	IOH_New16(CAN_IF1_DATA_B2(base),if1_data_b2_read,if1_data_b2_write,can);
	IOH_New16(CAN_IF2_CMD_REQ(base),if2_cmd_req_read,if2_cmd_req_write,can); 
	IOH_New16(CAN_IF2_CMD_MASK(base),if2_cmd_mask_read,if2_cmd_mask_write,can);
	IOH_New16(CAN_IF2_MASK1(base),if2_mask1_read,if2_mask1_write,can);
	IOH_New16(CAN_IF2_MASK2(base),if2_mask2_read,if2_mask2_write,can); 
	IOH_New16(CAN_IF2_ARBIT1(base),if2_arbit1_read,if2_arbit1_write,can);
	IOH_New16(CAN_IF2_ARBIT2(base),if2_arbit2_read,if2_arbit2_write,can),
	IOH_New16(CAN_IF2_MSG_CTRL(base),if2_msg_ctrl_read,if2_msg_ctrl_write,can);
	IOH_New16(CAN_IF2_DATA_A1(base),if2_data_a1_read,if2_data_a1_write,can);
	IOH_New16(CAN_IF2_DATA_A2(base),if2_data_a2_read,if2_data_a2_write,can);
	IOH_New16(CAN_IF2_DATA_B1(base),if2_data_b1_read,if2_data_b1_write,can);
	IOH_New16(CAN_IF2_DATA_B2(base),if2_data_b2_read,if2_data_b2_write,can);
	IOH_New16(CAN_TX_REQ_1(base),tx_req_1_read,tx_req_1_write,can);
	IOH_New16(CAN_TX_REQ_2(base),tx_req_2_read,tx_req_2_write,can);
	IOH_New16(CAN_NEW_DATA_1(base),new_data_1_read,new_data_1_write,can);
	IOH_New16(CAN_NEW_DATA_2(base),new_data_2_read,new_data_2_write,can);
	IOH_New16(CAN_IRQ_PEND_1(base),irq_pend_1_read,irq_pend_1_write,can);
	IOH_New16(CAN_IRQ_PEND_2(base),irq_pend_2_read,irq_pend_2_write,can);
	IOH_New16(CAN_MSG_VALID_1(base),msg_valid_1_read,msg_valid_1_write,can);
	IOH_New16(CAN_MSG_VALID_2(base),msg_valid_2_read,msg_valid_2_write,can);

}

static void
TCan_UnMap(void *owner,uint32_t base,uint32_t mask)
{
       // IOH_Delete32(ASC_BAUDRATE(base));
	IOH_Delete16(CAN_CTRL(base));
	IOH_Delete16(CAN_STATUS(base));
	IOH_Delete16(CAN_ERROR(base));
	IOH_Delete16(CAN_BIT_TIMING(base));
	IOH_Delete16(CAN_IRQ(base));
	IOH_Delete16(CAN_TEST(base));
	IOH_Delete16(CAN_BRP_EXT(base));
	IOH_Delete16(CAN_IF1_CMD_REQ(base));
	IOH_Delete16(CAN_IF1_CMD_MASK(base));
	IOH_Delete16(CAN_IF1_MASK1(base));
	IOH_Delete16(CAN_IF1_MASK2(base));
	IOH_Delete16(CAN_IF1_ARBIT1(base));
	IOH_Delete16(CAN_IF1_ARBIT2(base));
	IOH_Delete16(CAN_IF1_MSG_CTRL(base));
	IOH_Delete16(CAN_IF1_DATA_A1(base));
	IOH_Delete16(CAN_IF1_DATA_A2(base));
	IOH_Delete16(CAN_IF1_DATA_B1(base));
	IOH_Delete16(CAN_IF1_DATA_B2(base));
	IOH_Delete16(CAN_IF2_CMD_REQ(base));
	IOH_Delete16(CAN_IF2_CMD_MASK(base));
	IOH_Delete16(CAN_IF2_MASK1(base));
	IOH_Delete16(CAN_IF2_MASK2(base));
	IOH_Delete16(CAN_IF2_ARBIT1(base));
	IOH_Delete16(CAN_IF2_ARBIT2(base));
	IOH_Delete16(CAN_IF2_MSG_CTRL(base));
	IOH_Delete16(CAN_IF2_DATA_A1(base));
	IOH_Delete16(CAN_IF2_DATA_A2(base));
	IOH_Delete16(CAN_IF2_DATA_B1(base));
	IOH_Delete16(CAN_IF2_DATA_B2(base));
	IOH_Delete16(CAN_TX_REQ_1(base));
	IOH_Delete16(CAN_TX_REQ_2(base));
	IOH_Delete16(CAN_NEW_DATA_1(base));
	IOH_Delete16(CAN_NEW_DATA_2(base));
	IOH_Delete16(CAN_IRQ_PEND_1(base));
	IOH_Delete16(CAN_IRQ_PEND_2(base));
	IOH_Delete16(CAN_MSG_VALID_1(base));
	IOH_Delete16(CAN_MSG_VALID_2(base));
}

static CanChipOperations canOps =  {
        .receive = Bosch_CanReceive,
};

BusDevice *
Bosch_CanNew(const char *name)
{
	Bosch_Can *can = sg_new(Bosch_Can);
        can->bdev.first_mapping = NULL;
        can->bdev.Map = TCan_Map;
        can->bdev.UnMap = TCan_UnMap;
        can->bdev.owner = can;
        can->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	can->backend = CanSocketInterface_New(&canOps,name,can);
	can->sigIrq = SigNode_New("%s.irq",name);
	if(!can->sigIrq) {
		fprintf(stderr,"Can not create interrupt line for %s\n",name);
		exit(1);
	}
	can->clk = Clock_New("%s.clk",name);
	can->bitclk = Clock_New("%s.bitrate",name);
	if(!can->bitclk || !can->clk) {
		fprintf(stderr,"Can not create clock for %s\n",name);
		exit(1);
	}
	update_bitrate(can);
	CycleTimer_Init(&can->baud_timer,resume_rx,can);
	return &can->bdev;
}
