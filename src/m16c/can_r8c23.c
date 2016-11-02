/* 
 ************************************************************************************************ 
 *
 * R8C23 CAN controller simulation 
 *
 * State: Only a dummy to make the Software happy.
 *
 * Copyright 2009/2010 Jochen Karrer. All rights reserved.
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
 ************************************************************************************************ 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "bus.h"
#include "sgstring.h"
#include "signode.h"
#include "clock.h"
#include "cycletimer.h"
#include "socket_can.h"
#include "arpa/inet.h"
#include "can_r8c23.h"

typedef struct CANRegisters {
	uint32_t aCiMCTL;
	uint32_t aCiCTLR;
	uint32_t aCiSTR;
	uint32_t aCiSSTR;
	uint32_t aCiICR;
	uint32_t aCiIDR;
	uint32_t aCiRECR;
	uint32_t aCiTECR;
	uint32_t aCiAFS;
	uint32_t aCiCCLKR;
	uint32_t aCiGMR0;	
	uint32_t aCiGMR1;	
	uint32_t aCiGMR2;	
	uint32_t aCiGMR3;	
	uint32_t aCiGMR4;	
	uint32_t aCiLMAR0;
	uint32_t aCiLMAR1;
	uint32_t aCiLMAR2;
	uint32_t aCiLMAR3;
	uint32_t aCiLMAR4;
	uint32_t aCiLMBR0;
	uint32_t aCiLMBR1;
	uint32_t aCiLMBR2;
	uint32_t aCiLMBR3;
	uint32_t aCiLMBR4;
	uint32_t aCiCONR;

	uint32_t aCiSLOT0_0;	
	uint32_t aCiSLOT1_0;	
	uint32_t aCiSLOT2_0;	
	uint32_t aCiSLOT3_0;	
	uint32_t aCiSLOT4_0;	
	uint32_t aCiSLOT5_0;	
	uint32_t aCiSLOT6_0;	
	uint32_t aCiSLOT7_0;	
	uint32_t aCiSLOT8_0;	
	uint32_t aCiSLOT9_0;	
	uint32_t aCiSLOT10_0;	
	uint32_t aCiSLOT11_0;	
	uint32_t aCiSLOT12_0;	
	uint32_t aCiSLOT13_0;	
	uint32_t aCiSLOT14_0;	
	uint32_t aCiSLOT15_0;	
} CANRegisters;

static CANRegisters canRegs[] = {
	{
		.aCiMCTL = 0x1300,
		.aCiCTLR = 0x1310,
		.aCiSTR = 0x1312,
		.aCiSSTR = 0x1314,
		.aCiICR	= 0x1316,
		.aCiIDR = 0x1318,
		.aCiCONR = 0x131a,
		.aCiRECR = 0x131c,
		.aCiTECR = 0x131d,
		.aCiAFS = 0x1343,
		.aCiCCLKR = 0x135F,
		.aCiGMR0 = 0x1460,
		.aCiGMR1 = 0x1461,
		.aCiGMR2 = 0x1462,
		.aCiGMR3 = 0x1463,
		.aCiGMR4 = 0x1464,	
		.aCiLMAR0 = 0x1466,
		.aCiLMAR1 = 0x1467,
		.aCiLMAR2 = 0x1468,
		.aCiLMAR3 = 0x1469,
		.aCiLMAR4 = 0x147a,
		.aCiLMBR0 = 0x146c,
		.aCiLMBR1 = 0x146d,
		.aCiLMBR2 = 0x146e,
		.aCiLMBR3 = 0x146f, 
		.aCiLMBR4 = 0x1470,

		.aCiSLOT0_0 = 0x1360,	
		.aCiSLOT1_0 = 0x1370,	
		.aCiSLOT2_0 = 0x1380,	
		.aCiSLOT3_0 = 0x1390,	
		.aCiSLOT4_0 = 0x13A0,	
		.aCiSLOT5_0 = 0x13B0,	
		.aCiSLOT6_0 = 0x13C0,	
		.aCiSLOT7_0 = 0x13D0,	
		.aCiSLOT8_0 = 0x13E0,	
		.aCiSLOT9_0 = 0x13F0,	
		.aCiSLOT10_0 = 0x1400,	
		.aCiSLOT11_0 = 0x1410,	
		.aCiSLOT12_0 = 0x1420,	
		.aCiSLOT13_0 = 0x1430,	
		.aCiSLOT14_0 = 0x1440,	
		.aCiSLOT15_0 = 0x1450,	
	}, 
};

#define CiCTRL0_RESET0		(1 << 0)
#define CiCTRL0_LOOPBACK	(1 << 1)
#define CiCTRL0_MSGORDER	(1 << 2)
#define CiCTRL0_BASICCAN	(1 << 3)
#define CiCTRL0_BUSERR_EN	(1 << 4)
#define CiCTRL0_SLEEP		(1 << 5)
#define CiCTRL0_PORT_EN		(1 << 6)
#define CiCTRL0_TSPRE0		(1 << 8)
#define CiCTRL0_TSPRE1		(1 << 9)
#define CiCTRL0_TSRESET		(1 << 10)
#define CiCTRL0_RESETBOFF	(1 << 11)

#define CiCTLR1_BANKSEL		(1 << 3)
#define CiCTLR1_INTSEL		(1 << 6)

#define CiSTR_MBOX_MSK		(0xf)
#define CiSTR_TRMSUCC		(1 << 4)
#define CiSTR_RECSUCC		(1 << 5)
#define CiSTR_TRMSTATE		(1 << 6)
#define CiSTR_RECSTATE		(1 << 7)
#define CiSTR_STATE_RESET	(1 << 8)
#define CiSTR_STATE_LOOPBACK	(1 << 9)
#define CiSTR_STATE_BASCAN	(1 << 11)
#define CiSTR_STATE_BUSERROR	(1 << 12)
#define CiSTR_STATE_ERRPAS	(1 << 13)
#define CiSTR_STATE_BUSOFF	(1 << 14)

#define CiCONR_BRP_MSK		(7)
#define CiCONR_SAM		(1 << 4)
#define CiCONR_PTS_MSK		(7 << 5)
#define CiCONR_PBS1_MSK		(7 << 8)
#define CiCONR_PBS2_MSK		(7 << 11)
#define CiCONR_SJW_MSK		(3 << 14)


#define CiMCTLj_SENDATA		(1 << 0)
#define CiMCTLj_NEWDATA		(1 << 0)
#define CiMCTLj_TRMACTIVE	(1 << 1)
#define CiMCTLj_INVALDATA	(1 << 1)
#define CiMCTLj_MSGLOST		(1 << 2)
#define CiMCTLj_REMACTIVE	(1 << 3)
#define CiMCTLj_RSPLOCK		(1 << 4)
#define CiMCTLj_REMOTE		(1 << 5)
#define CiMCTLj_RECREQ		(1 << 6)
#define CiMCTLj_TRMREQ		(1 << 7)

#define CiSBS_SBS0_MSK		(0xf << 0)
#define CiSBS_SBS1_MSK		(0xf << 4)


typedef struct CanMsgBuf {
	uint32_t Id;
	uint8_t dlc;
	uint8_t data[8];
	uint16_t timeStamp;
} CanMsgBuf;

typedef struct R8C_Can {
	BusDevice bdev;
	Clock_t *inclk;
	CANRegisters *regSet;
	CanController *backend;
	SigNode *sigRxIrq;
	SigNode *sigTxIrq;
	int register_set;
	uint16_t regCtrl;
	uint8_t regSlpr;
	uint16_t regStr;
	uint16_t regIdr;
	uint16_t regConr;
	uint8_t regBrp;
	uint16_t regTsr;
	uint8_t regTec;
	uint8_t regRec;
	uint16_t regIcr;	
	uint8_t regEimkr;
	uint8_t regEistr;
	uint8_t regEfr;
	uint8_t regMdr;
	uint16_t regSsctlr;
	uint16_t regSsstr;
	uint8_t regGmr[5];
	uint8_t regLmar[5];
	uint8_t regLmbr[5];
	uint8_t regMctl[16];

	CanMsgBuf msgBuf[16];
	CycleTimer rx_baud_timer;
	CycleTimer tx_baud_timer;
	uint8_t regAfs;
} R8C_Can;


static void
trigger_rx_irq(R8C_Can *mcan) {
	SigNode_Set(mcan->sigRxIrq,SIG_LOW);
	SigNode_Set(mcan->sigRxIrq,SIG_HIGH);
}

static void
trigger_tx_irq(R8C_Can *mcan) {
	SigNode_Set(mcan->sigTxIrq,SIG_LOW);
	SigNode_Set(mcan->sigTxIrq,SIG_HIGH);
}
/**
 **************************************************************
 * exit sleep + reset sets register defaults
 **************************************************************
 */
static void
set_register_defaults(R8C_Can *mcan) 
{
	int i;
	mcan->regCtrl = 0x1;
	mcan->regStr = 0x100;
	mcan->regIdr = 0;
	mcan->regConr = 0;
	mcan->regBrp = 1;
	mcan->regTsr = 0;
	mcan->regTec = 0;
	mcan->regRec = 0;
	//mcan->regSistr = 0;
	mcan->regIcr = 0;
	mcan->regEimkr = 0;
	mcan->regEistr = 0;
	mcan->regEfr = 0;
	mcan->regMdr = 0;
	mcan->regSsctlr = 0;
	mcan->regSsstr = 0;	
	for(i=0;i<5;i++) {
		mcan->regGmr[i] = 0;
		mcan->regLmar[i] = 0;
		mcan->regLmbr[i] = 0;
	}
	for(i=0;i<16;i++) {
		mcan->regMctl[i] = 0;
	}
}

static void
resume_rx(void *clientData)
{
	R8C_Can *mcan = (R8C_Can *)clientData;
	CanStartRx(mcan->backend);
}

static  void
R8C_CanReceive (void *clientData,CAN_MSG *msg) 
{
	R8C_Can *mcan = (R8C_Can *)clientData;
	CanMsgBuf *msgbuf;
	uint32_t GmrSid;
	uint32_t MarSid;
	uint32_t MbrSid;
	uint32_t msg_id = CAN_ID(msg);
	int i,j;
	int basic_can_slot = -1;
	
	CanStopRx(mcan->backend);
	/* Real baud rate should be used here */
	//CycleTimer_Mod(&mcan->rx_baud_timer,9000);
	CycleTimer_Mod(&mcan->rx_baud_timer,15000);

	if(CAN_MSG_RTR(msg)) {
		fprintf(stderr,"CAN RTR message type not implemented\n");
		return;
	} 
	GmrSid = ((mcan->regGmr[0] & 0x1f) << 6) | (mcan->regGmr[1] & 0x3f);
	for(i = 0; i < 15; i++) {
		uint16_t buf_sid;
		if(!(mcan->regMctl[i] & CiMCTLj_RECREQ)) {
			continue;
		}
		if(mcan->regMctl[i] & CiMCTLj_REMOTE) {
			continue;
		}
		if((mcan->regIdr & (1 << i)) && CAN_MSG_11BIT(msg)) {
			continue;	
		}
		if(!(mcan->regIdr & (1 << i)) && CAN_MSG_29BIT(msg)) {
			continue;
		}
		msgbuf = &mcan->msgBuf[i];
		buf_sid = msgbuf->Id;
		if((i > 13) && (basic_can_slot >= 0) && (i != basic_can_slot)) {
			continue;
		} 
		if(i == 14)  {
			MarSid = ((mcan->regLmar[0] & 0x1f) << 6) | (mcan->regLmar[1] & 0x3f);
			if(((msg_id ^ buf_sid) & MarSid) != 0) {
				continue;
			} 
		} else if (i == 15) {
			MbrSid = ((mcan->regLmbr[0] & 0x1f) << 6) | (mcan->regLmbr[1] & 0x3f);
			if(((msg_id ^ buf_sid) & MbrSid) != 0) {
				continue;
			} 
		} else {
			if(((msg_id ^ buf_sid) & GmrSid) != 0) {
				continue;
			} 
		}
		msgbuf->Id = CAN_ID(msg);
		msgbuf->dlc = msg->can_dlc;
		//fprintf(stderr,"Slot %d, id %04x len %d\n",i,msgbuf->Id,msgbuf->dlc);
		for(j = 0; j < msg->can_dlc;j++) {
			msgbuf->data[j] =  msg->data[j];
		}
		// msgbuf->timeStamp = 
		if(mcan->regMctl[i] &  CiMCTLj_NEWDATA) {
			mcan->regMctl[i] |=  CiMCTLj_MSGLOST;
		} else {
			mcan->regMctl[i] |=  CiMCTLj_NEWDATA;
		}
		//mcan->regSistr |= (0x8000 >> i);
		trigger_rx_irq(mcan);	
		// set some interrupt register
	}
	
}

static void
R8C_CanTransmit(R8C_Can *mcan,int slot)
{
	CAN_MSG msg;
	int i;
	CanMsgBuf *msgbuf = &mcan->msgBuf[slot];
	memset(&msg,0,sizeof(msg));
	if(mcan->regIdr & (1 << slot)) {
		CAN_MSG_T_29(&msg);
	} else {
		CAN_MSG_T_11(&msg);
	}
	CAN_SET_ID(&msg,msgbuf->Id);
	msg.can_dlc = msgbuf->dlc;
	for(i = 0; i < 8; i++) {
		msg.data[i] = msgbuf->data[i];
	}
	CanSend(mcan->backend,&msg);
	trigger_tx_irq(mcan);	
	mcan->regMctl[slot] &= ~CiMCTLj_TRMREQ;
}
/**
 **************************************************************
 *   Bit 0:  RESET0 
 *   Bit 1:  LOOPBACK
 *   Bit 3:  BASICCAN
 *   Bit 4:  RESET1
 *   Bit 8:  TSPRE0
 *   Bit 9:  TSPRE1
 *   Bit 10: TSRESET
 *   Bit 11: ECRESET
 **************************************************************
 */
static uint32_t
cictlr_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regCtrl;
}
static void
cictlr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	if((value & 0x1) == 0x1) {
		set_register_defaults(mcan);
	} else {
		mcan->regCtrl = value;
		mcan->regStr &= ~CiSTR_STATE_RESET;
	}
}


/*
 *************************************************************
 * Bit 0 - 3 MBOX: Slot used for last Rx/Tx operation
 * Bit 4: TRMSUCC becomes 1 after successful transmission
 *	  0 after a successful rececption
 * Bit 5: RECSUCC becomes 1 after a successful reception
 ` 	  and 0 after a successful transmission
 * Bit 6: TRMSTATE: True during transmit operation.
 * Bit 7: RECSTATE: True during reception ????
 * Bit 8: STATE_RESET becomes 1 as soon as reset is completed
 * 	  and 0 as soon as RESET1 and (or ?) RESET0 are 0
 * Bit 9: STATE_LOOPBACK becomes 1 if loopback in CiCTRL0
 * 	  is set to 1, 0 else
 * Bit 11: STATE_BASICCAN 1 when Basiccan is enabled and 
 * 	  CiMCTL14 and 15 are set to receive a data frame.
 * Bit 12: STATE_BUSERROR is 1 after a communication error.
 * 	   an 0 as soon as a transmit or Receive message
 *	   is completed (no need for storing the message).
 * Bit 13: STATE_ERRPAS 1 when CiTEC or CiREC exceeds
 *	   127, the module goes to error passive state.
 *
 * Bit 14: STATE_BUSOFF when CiTEC exceeds 255 the
 *	   controller goes to busoff state, becomes
 *	   0 by a reset only.
 *************************************************************
 */
static uint32_t
cistr_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regStr;
}

static void
cistr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	//R8C_Can *mcan = (R8C_Can *) clientData;
//	fprintf(stderr,"Write cistr to %02x\n",value);
//	exit(1);
//	mcan->regStr = value;
}

/*
 *************************************************************
 * CiIDR
 * Determines if a message slot is configured for
 * Standard or for Extended format messages.
 *************************************************************
 */
static uint32_t
ciidr_read(void *clientData,uint32_t address,int rqlen)
{

	return 0;
}

static void
ciidr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

/**
 *************************************************************
 * CiCONR
 * Determine the Timing of the CAN controller
 *************************************************************
 */
static uint32_t
ciconr_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regConr;
}

static void
ciconr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regConr = value;
}



/**
 ***************************************************************
 * Can Transmit Error Count Register
 ***************************************************************
 */
static uint32_t
citecr_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regTec;
}

static void
citecr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Writing to readonly CiTEC register\n");
}

/**
 *************************************************************
 * Can Receive Error Count Register
 *************************************************************
 */
static uint32_t
cirecr_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regRec;
}

static void
cirecr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Writing to readonly CiREC register\n");
}


/*
 *****************************************************
 * Interrupt mask register, One Bit per slot.
 *****************************************************
 */
static uint32_t
ciicr_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regIcr;
}

static void
ciicr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regIcr = value;
	// update_interrupt(mcan);
}

/*
 ***************************************************************
 * Global mask register: Filtering mask for
 * Slot 0-13
 ***************************************************************
 */
static uint32_t
cigmr0_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regGmr[0];
}

static void
cigmr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regGmr[0] = value;
}

/*
 ***************************************************************
 * Local mask register: Filtering mask for Slot 14
 ***************************************************************
 */
static uint32_t
cilmar0_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regLmar[0];
}


static void
cilmar0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regLmar[0] = value;
}

static uint8_t
cimctlk_read(void *clientData,unsigned int n) {
	R8C_Can *mcan = (R8C_Can *)clientData;
	//fprintf(stderr,"Mctl %d is %02x: %06x\n",n,mcan->regMctl[n],R8C_REG_PC);
	return mcan->regMctl[n];
}

static void
cimctlk_write(void *clientData,uint8_t value,int n) 
{
	R8C_Can *mcan = (R8C_Can *)clientData;
	mcan->regMctl[n] = 
		(mcan->regMctl[n] & (CiMCTLj_REMACTIVE | CiMCTLj_TRMACTIVE))
		| (value & ~(CiMCTLj_REMACTIVE | CiMCTLj_TRMACTIVE));
	if(value & CiMCTLj_TRMREQ) {
		R8C_CanTransmit(mcan,n);
	}
	if(value & CiMCTLj_RECREQ) {
		if(!CycleTimer_IsActive(&mcan->rx_baud_timer)) {
			CanStartRx(mcan->backend);
		}
	}
}
/**
 ******************************************************************* 
 * Message Slot control register.
 *******************************************************************
 */
static uint32_t
cimctl0_read(void *clientData,uint32_t address,int rqlen)
{
	return cimctlk_read(clientData,0);
}

static void
cimctl0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,0);
}



static uint32_t
cimctl1_read(void *clientData,uint32_t address,int rqlen)
{

	return cimctlk_read(clientData,1);
}

static void
cimctl1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,1);
}

static uint32_t
cimctl2_read(void *clientData,uint32_t address,int rqlen)
{

	return cimctlk_read(clientData,2);
}

static void
cimctl2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,2);
}

static uint32_t
cimctl3_read(void *clientData,uint32_t address,int rqlen)
{

	return cimctlk_read(clientData,3);
}

static void
cimctl3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,3);
}

static uint32_t
cimctl4_read(void *clientData,uint32_t address,int rqlen)
{

	return cimctlk_read(clientData,4);
}

static void
cimctl4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,4);
}


static uint32_t
cimctl5_read(void *clientData,uint32_t address,int rqlen)
{
	return cimctlk_read(clientData,5);
}

static void
cimctl5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,4);
}
static uint32_t
cimctl6_read(void *clientData,uint32_t address,int rqlen)
{
	return cimctlk_read(clientData,6);
}

static void
cimctl6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,6);
}
static uint32_t
cimctl7_read(void *clientData,uint32_t address,int rqlen)
{
	return cimctlk_read(clientData,7);
}

static void
cimctl7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,7);
}

/*
 ***************************************************************
 * CiLMBR0
 * Local mask register: Filtering mask for Slot 15
 ***************************************************************
 */
static uint32_t
cilmbr0_read(void *clientData,uint32_t address,int rqlen)
{

	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regLmbr[0];
}

static void
cilmbr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regLmbr[0] = value;
}

static uint32_t
cimctl8_read(void *clientData,uint32_t address,int rqlen)
{
	return cimctlk_read(clientData,8);
}

static void
cimctl8_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,8);
}



static uint32_t
cimctl9_read(void *clientData,uint32_t address,int rqlen)
{
	return cimctlk_read(clientData,9);
}

static void
cimctl9_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,9);
}

static uint32_t
cimctl10_read(void *clientData,uint32_t address,int rqlen)
{
	return cimctlk_read(clientData,10);
}

static void
cimctl10_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,10);
}

static uint32_t
cimctl11_read(void *clientData,uint32_t address,int rqlen)
{

	return cimctlk_read(clientData,11);
}

static void
cimctl11_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,11);
}

static uint32_t
cimctl12_read(void *clientData,uint32_t address,int rqlen)
{
	return cimctlk_read(clientData,12);
}

static void
cimctl12_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,12);
}

static uint32_t
cimctl13_read(void *clientData,uint32_t address,int rqlen)
{

	return cimctlk_read(clientData,13);
}

static void
cimctl13_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,13);
}

static uint32_t
cimctl14_read(void *clientData,uint32_t address,int rqlen)
{

	return cimctlk_read(clientData,14);
}

static void
cimctl14_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,14);
}

static uint32_t
cimctl15_read(void *clientData,uint32_t address,int rqlen)
{

	return cimctlk_read(clientData,15);
}

static void
cimctl15_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	cimctlk_write(clientData,value,15);
}

/**
 ***********************************************************
 * Global mask register
 * Defines the filter for Slot 0-13
 ***********************************************************
 */
static uint32_t
cigmr1_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regGmr[1];
}

static void
cigmr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regGmr[1] = value;
}

/**
 ***********************************************************
 * Local mask register
 * Defines the filter for Slot 14 
 ***********************************************************
 */
static uint32_t
cilmar1_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regLmar[1];
}

static void
cilmar1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regLmar[1] = value;
}


/**
 ***********************************************************
 * Local mask register
 * Defines the filter for Slot 15 
 ***********************************************************
 */
static uint32_t
cilmbr1_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regLmbr[1];
}

static void
cilmbr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regLmbr[1] = value;
}


/**
 ***********************************************************
 * Global mask register
 * Defines the filter for Slot 0-13 
 ***********************************************************
 */
static uint32_t
cigmr2_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regGmr[2];
}

static void
cigmr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regGmr[2] = value;
}

/**
 ***********************************************************
 * Local mask register
 * Defines the filter for Slot 14 
 ***********************************************************
 */
static uint32_t
cilmar2_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regLmar[2];
}

static void
cilmar2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regLmar[2] = value;
}

/**
 ***********************************************************
 * Local mask register
 * Defines the filter for Slot 15 
 ***********************************************************
 */

static uint32_t
cilmbr2_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regLmbr[2];
}

static void
cilmbr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regLmbr[2] = value;
}


/**
 ***********************************************************
 * Global mask register
 * Defines the filter for Slot 0-13 
 ***********************************************************
 */
static uint32_t
cigmr3_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regGmr[3];
}

static void
cigmr3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regGmr[3] = value;
}


/**
 ***********************************************************
 * Local mask register
 * Defines the filter for Slot 14 
 ***********************************************************
 */
static uint32_t
cilmar3_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regLmar[3];

}

static void
cilmar3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regLmar[3] = value;
}


/**
 ***********************************************************
 * Local mask register
 * Defines the filter for Slot 15
 ***********************************************************
 */
static uint32_t
cilmbr3_read(void *clientData,uint32_t address,int rqlen)
{

	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regLmbr[3];
}

static void
cilmbr3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regLmbr[3] = value;
}


/**
 ***********************************************************
 * Global mask register
 * Defines the filter for Slot 0-13 
 ***********************************************************
 */
static uint32_t
cigmr4_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regGmr[4];
}

static void
cigmr4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regGmr[4] = value;
}

/**
 ***********************************************************
 * Local mask register
 * Defines the filter for Slot 14 
 ***********************************************************
 */
static uint32_t
cilmar4_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regLmar[4];
}

static void
cilmar4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regLmar[4] = value;
}


/**
 ***********************************************************
 * Local mask register
 * Defines the filter for Slot 15 
 ***********************************************************
 */
static uint32_t
cilmbr4_read(void *clientData,uint32_t address,int rqlen)
{

	R8C_Can *mcan = (R8C_Can *) clientData;
	return mcan->regLmbr[4];
}

static void
cilmbr4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData;
	mcan->regLmbr[4] = value;
}

static uint32_t
cislot0_0_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> 6) & 0x1f;
}

static void
cislot0_0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0x1f << 6)) | ((value & 0x1f) << 6);
}

static uint32_t
cislot0_1_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->Id & 0x3f;
}

static void
cislot0_1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0x3f << 0)) | ((value & 0x3f) << 0);
}

static uint32_t
cislot0_2_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> (14+11)) & 0xf;
}

static void
cislot0_2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0xf << 25)) | ((value & 0xf) << 25);
}

static uint32_t
cislot0_3_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> (6+11)) & 0xff;
}

static void
cislot0_3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0xff << 17)) | ((value & 0xff) << 17);
}

static uint32_t
cislot0_4_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> (0+11)) & 0x3f;
}

static void
cislot0_4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0x3f << 11)) | ((value & 0x3f) << 11);
}

static uint32_t
cislot0_5_read(void *clientData,uint32_t address,int rqlen)
{

	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->dlc;
}

static void
cislot0_5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->dlc = value & 0xf;
}

static uint32_t
cislot0_6_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[0];
}

static void
cislot0_6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[0] = value;
}

static uint32_t
cislot0_7_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[1];
}

static void
cislot0_7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[1] = value;
}

static uint32_t
cislot0_8_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[2];
}

static void
cislot0_8_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[2] = value;
}

static uint32_t
cislot0_9_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[3];
}

static void
cislot0_9_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[3] = value;
}

static uint32_t
cislot0_10_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[4];
}

static void
cislot0_10_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[4] = value;
}

static uint32_t
cislot0_11_read(void *clientData,uint32_t address,int rqlen)
{

	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[5];
}

static void
cislot0_11_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[5] = value;
}

static uint32_t
cislot0_12_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[6];
}

static void
cislot0_12_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[6] = value;
}

static uint32_t
cislot0_13_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[7];
}

static void
cislot0_13_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[7] = value;
}

static uint32_t
cislot0_14_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->timeStamp >> 8;
}

static void
cislot0_14_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->timeStamp = (msgbuf->timeStamp & 0xff) | ((value & 0xff) << 8);
}

static uint32_t
cislot0_15_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->timeStamp & 0xff;
}

static void
cislot0_15_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *can = (R8C_Can *) clientData; 
	CANRegisters *cr = can->regSet;
	int slot = ((address - cr->aCiSLOT0_0) >> 4) & 0xf; 
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->timeStamp = (msgbuf->timeStamp & 0xff00) | (value & 0xff);
}

static uint32_t
ciafs_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData; 
	return mcan->regAfs;
}

static void
ciafs_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Can *mcan = (R8C_Can *) clientData; 
	mcan->regAfs = value;
}

static void
R8CCan_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	int j,i;
	R8C_Can *mcan = (R8C_Can *) owner;
	CANRegisters *cr = mcan->regSet;
	IOH_Delete16(cr->aCiCTLR);
	IOH_Delete16(cr->aCiSTR);
	IOH_Delete16(cr->aCiIDR);
	IOH_Delete16(cr->aCiCONR);
	//IOH_Delete8(cr->aCiBRP);
	//IOH_Delete16(cr->aCiTSR);
	IOH_Delete8(cr->aCiTECR);
	IOH_Delete8(cr->aCiRECR);
	IOH_Delete8(cr->aCiGMR0);	
	IOH_Delete8(cr->aCiLMAR0);
	IOH_Delete8(cr->aCiLMBR0);
	IOH_Delete8(cr->aCiGMR1);	
	IOH_Delete8(cr->aCiLMAR1);
	IOH_Delete8(cr->aCiLMBR1);
	IOH_Delete8(cr->aCiGMR2);	
	IOH_Delete8(cr->aCiLMAR2);
	IOH_Delete8(cr->aCiLMBR2);
	IOH_Delete8(cr->aCiGMR3);	
	IOH_Delete8(cr->aCiLMAR3);
	IOH_Delete8(cr->aCiLMBR3);
	IOH_Delete8(cr->aCiGMR4);
	IOH_Delete8(cr->aCiLMAR4);
	IOH_Delete8(cr->aCiLMBR4);

	for(j=0;j<=15;j++) {
		IOH_Delete8(cr->aCiMCTL + j);
		for(i = 0;i < 15;i++) {
			IOH_Delete8(cr->aCiSLOT0_0 + (j << 4) + i);	
		}
	}
	IOH_Delete16(cr->aCiAFS);
	//R8C_Can *can = (R8C_Can *) owner;
}

static void
R8CCan_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	R8C_Can *mcan = (R8C_Can *) owner;
	uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_PRD_RARP | IOH_FLG_PA_CBSE;
	int j;
	CANRegisters *cr = mcan->regSet;
	//IOH_uint32_t aCiMCTL;
	IOH_New16(cr->aCiCTLR,cictlr_read,cictlr_write,mcan);
	IOH_New16f(cr->aCiSTR,cistr_read,cistr_write,mcan,flags);
	IOH_New16f(cr->aCiIDR,ciidr_read,ciidr_write,mcan,flags);
	IOH_New16f(cr->aCiCONR,ciconr_read,ciconr_write,mcan,flags);
	//IOH_New16(cr->aCiTSR,citsr_read,citsr_write,mcan);
	IOH_New8(cr->aCiRECR,cirecr_read,cirecr_write,mcan);
	IOH_New8(cr->aCiTECR,citecr_read,citecr_write,mcan);
	IOH_New16f(cr->aCiICR,ciicr_read,ciicr_write,mcan,flags);
	IOH_New8(cr->aCiGMR0,cigmr0_read,cigmr0_write,mcan);	
	IOH_New8(cr->aCiLMAR0,cilmar0_read,cilmar0_write,mcan);
	IOH_New8(cr->aCiLMBR0,cilmbr0_read,cilmbr0_write,mcan);
	IOH_New8(cr->aCiGMR1,cigmr1_read,cigmr1_write,mcan);	
	IOH_New8(cr->aCiLMAR1,cilmar1_read,cilmar1_write,mcan);
	IOH_New8(cr->aCiLMBR1,cilmbr1_read,cilmbr1_write,mcan);
	IOH_New8(cr->aCiGMR2,cigmr2_read,cigmr2_write,mcan);	
	IOH_New8(cr->aCiLMAR2,cilmar2_read,cilmar2_write,mcan);
	IOH_New8(cr->aCiLMBR2,cilmbr2_read,cilmbr2_write,mcan);
	IOH_New8(cr->aCiGMR3,cigmr3_read,cigmr3_write,mcan);	
	IOH_New8(cr->aCiLMAR3,cilmar3_read,cilmar3_write,mcan);
	IOH_New8(cr->aCiLMBR3,cilmbr3_read,cilmbr3_write,mcan);
	IOH_New8(cr->aCiGMR4,cigmr4_read,cigmr4_write,mcan);
	IOH_New8(cr->aCiLMAR4,cilmar4_read,cilmar4_write,mcan);
	IOH_New8(cr->aCiLMBR4,cilmbr4_read,cilmbr4_write,mcan);
	IOH_New8(cr->aCiMCTL + 0,cimctl0_read,cimctl0_write,mcan);
	IOH_New8(cr->aCiMCTL + 1,cimctl1_read,cimctl1_write,mcan);
	IOH_New8(cr->aCiMCTL + 2,cimctl2_read,cimctl2_write,mcan);
	IOH_New8(cr->aCiMCTL + 3,cimctl3_read,cimctl3_write,mcan);
	IOH_New8(cr->aCiMCTL + 4,cimctl4_read,cimctl4_write,mcan);
	IOH_New8(cr->aCiMCTL + 5,cimctl5_read,cimctl5_write,mcan);
	IOH_New8(cr->aCiMCTL + 6,cimctl6_read,cimctl6_write,mcan);
	IOH_New8(cr->aCiMCTL + 7,cimctl7_read,cimctl7_write,mcan);
	IOH_New8(cr->aCiMCTL + 8,cimctl8_read,cimctl8_write,mcan);
	IOH_New8(cr->aCiMCTL + 9,cimctl9_read,cimctl9_write,mcan);
	IOH_New8(cr->aCiMCTL + 10,cimctl10_read,cimctl10_write,mcan);
	IOH_New8(cr->aCiMCTL + 11,cimctl11_read,cimctl11_write,mcan);
	IOH_New8(cr->aCiMCTL + 12,cimctl12_read,cimctl12_write,mcan);
	IOH_New8(cr->aCiMCTL + 13,cimctl13_read,cimctl13_write,mcan);
	IOH_New8(cr->aCiMCTL + 14,cimctl14_read,cimctl14_write,mcan);
	IOH_New8(cr->aCiMCTL + 15,cimctl15_read,cimctl15_write,mcan);
	for(j = 0; j < 16;j++) {
		uint32_t slot_base = cr->aCiSLOT0_0 + (j << 4);
		//fprintf(stderr,"Slot base %04x\n",slot_base);
		IOH_New8f(slot_base + 0,cislot0_0_read,cislot0_0_write,mcan,flags);
		IOH_New8f(slot_base + 1,cislot0_1_read,cislot0_1_write,mcan,flags);
		IOH_New8f(slot_base + 2,cislot0_2_read,cislot0_2_write,mcan,flags);
		IOH_New8f(slot_base + 3,cislot0_3_read,cislot0_3_write,mcan,flags);
		IOH_New8f(slot_base + 4,cislot0_4_read,cislot0_4_write,mcan,flags);
		IOH_New8f(slot_base + 5,cislot0_5_read,cislot0_5_write,mcan,flags);
		IOH_New8f(slot_base + 6,cislot0_6_read,cislot0_6_write,mcan,flags);
		IOH_New8f(slot_base + 7,cislot0_7_read,cislot0_7_write,mcan,flags);
		IOH_New8f(slot_base + 8,cislot0_8_read,cislot0_8_write,mcan,flags);
		IOH_New8f(slot_base + 9,cislot0_9_read,cislot0_9_write,mcan,flags);
		IOH_New8f(slot_base + 10,cislot0_10_read,cislot0_10_write,mcan,flags);
		IOH_New8f(slot_base + 11,cislot0_11_read,cislot0_11_write,mcan,flags);
		IOH_New8f(slot_base + 12,cislot0_12_read,cislot0_12_write,mcan,flags);
		IOH_New8f(slot_base + 13,cislot0_13_read,cislot0_13_write,mcan,flags);
		IOH_New8f(slot_base + 14,cislot0_14_read,cislot0_14_write,mcan,flags);
		IOH_New8f(slot_base + 15,cislot0_15_read,cislot0_15_write,mcan,flags);
	}
	IOH_New16f(cr->aCiAFS,ciafs_read,ciafs_write,mcan,flags);
}

static CanChipOperations canOps =  {
	.receive = R8C_CanReceive,
};

BusDevice *
R8C23_CanNew(const char *name,unsigned int register_set) 
{
	R8C_Can *mcan = sg_new(R8C_Can);
	if(register_set >= array_size(canRegs)) {
		fprintf(stderr,"Illegal register set selection %d\n",register_set);
		exit(1);
	}
	mcan->register_set = register_set;
	mcan->regSet = &canRegs[mcan->register_set]; 
	mcan->bdev.first_mapping=NULL;
        mcan->bdev.Map=R8CCan_Map;
        mcan->bdev.UnMap=R8CCan_Unmap;
        mcan->bdev.owner=mcan;
        mcan->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        mcan->inclk = Clock_New("%s.inclk",name);
	mcan->sigRxIrq = SigNode_New("%s.rxirq",name);
	mcan->sigTxIrq = SigNode_New("%s.txirq",name);
        Clock_SetFreq(mcan->inclk,32000000); /* Should be connected instead */
	mcan->backend = CanSocketInterface_New(&canOps,name,mcan);
	//CycleTimer_Init(&mcan->tx_baud_timer,tx_done,usart);
        CycleTimer_Init(&mcan->rx_baud_timer,resume_rx,mcan);
        fprintf(stderr,"R8C CAN controller \"%s\" created\n",name);
        return &mcan->bdev;
}
