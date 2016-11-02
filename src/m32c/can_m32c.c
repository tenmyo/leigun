/* 
 ************************************************************************************************ 
 *
 * M32C CAN controller 
 *
 * State: Working but clocks and timing checks are missing.
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

typedef struct CANRegisters {
	uint32_t aCiCTLR0;
	uint32_t aCiCTLR1;
	uint32_t aCiSLPR;
	uint32_t aCiSTR;
	uint32_t aCiIDR;
	uint32_t aCICONR;
	uint32_t aCiBRP;
	uint32_t aCiTSR;
	uint32_t aCiTEC;
	uint32_t aCiREC;
	uint32_t aCiSISTR;
	uint32_t aCiSIMKR;
	uint32_t aCiEIMKR;
	uint32_t aCiEISTR; 
	uint32_t aCiEFR;
	uint32_t aCiMDR;	
	uint32_t aCiSSCTLR;
	uint32_t aCiSSSTR;
	uint32_t aCiGMR0;	
	uint32_t aCiLMAR0;
	uint32_t aCilMBR0;
	uint32_t aCiGMR1;	
	uint32_t aCiLMAR1;
	uint32_t aCilMBR1;
	uint32_t aCiGMR2;	
	uint32_t aCiLMAR2;
	uint32_t aCilMBR2;
	uint32_t aCiGMR3;	
	uint32_t aCiLMAR3;
	uint32_t aCilMBR3;
	uint32_t aCiGMR4;	
	uint32_t aCiLMAR4;
	uint32_t aCiLMBR4;
	uint32_t aCiMCTL0;
	uint32_t aCiSBS;	
	uint32_t aCiSLOT0_0;	
	uint32_t aCiSLOT1_0;	
	uint32_t aCiSLOT0_1;
	uint32_t aCiSLOT1_1;
	uint32_t aCiSLOT0_2;
	uint32_t aCiSLOT1_2;
	uint32_t aCiSLOT0_3;
	uint32_t aCiSLOT1_3;
	uint32_t aCiSLOT0_4;
	uint32_t aCiSLOT1_4;
	uint32_t aCiSLOT0_5;
	uint32_t aCiSLOT1_5;
	uint32_t aCiSLOT0_6;
	uint32_t aCiSLOT1_6;
	uint32_t aCiSLOT0_14;
	uint32_t aCiSLOT1_14;
	uint32_t aCiSLOT0_15;
	uint32_t aCiSLOT1_15;
	uint32_t aCiAFS;
} CANRegisters;

static CANRegisters canRegs[] = {
	{
		.aCiCTLR0 = 0x200,
		.aCiCTLR1 = 0x241,
		.aCiSLPR = 0x242,
		.aCiSTR = 0x202,
		.aCiIDR = 0x204,
		.aCICONR = 0x206,
		.aCiBRP = 0x217,
		.aCiTSR = 0x209,
		.aCiTEC = 0x20A,
		.aCiREC = 0x20B,
		.aCiSISTR = 0x20C,
		.aCiSIMKR = 0x0210,
		.aCiEIMKR = 0x0214,
		.aCiEISTR = 0x0215, 
		.aCiEFR = 0x0216,
		.aCiMDR = 0x0219,	
		.aCiSSCTLR = 0x220,
		.aCiSSSTR = 0x224,
		.aCiGMR0 = 0x228,
		.aCiLMAR0 = 0x230,
		.aCilMBR0 = 0x238,
		.aCiGMR1 = 0x229,
		.aCiLMAR1 = 0x231,
		.aCilMBR1 = 0x239,
		.aCiGMR2 = 0x22a,
		.aCiLMAR2 = 0x232,
		.aCilMBR2 = 0x23a,
		.aCiGMR3 = 0x22b,
		.aCiLMAR3 = 0x233,
		.aCilMBR3 = 0x23b, 
		.aCiGMR4 = 0x22c,	
		.aCiLMAR4 = 0x234,
		.aCiLMBR4 = 0x23c,
		.aCiMCTL0 = 0x230,
		.aCiSBS = 0x240,
		.aCiSLOT0_0 = 0x1e0,	
		.aCiSLOT1_0 = 0x1f0,	
		.aCiSLOT0_1 = 0x1e1,
		.aCiSLOT1_1 = 0x1f1,
		.aCiSLOT0_2 = 0x1e2,
		.aCiSLOT1_2 = 0x1f2,
		.aCiSLOT0_3 = 0x1e3,
		.aCiSLOT1_3 = 0x1f3,
		.aCiSLOT0_4 = 0x1e4,
		.aCiSLOT1_4 = 0x1f4,
		.aCiSLOT0_5 = 0x1e5,
		.aCiSLOT1_5 = 0x1f5,
		.aCiSLOT0_6 = 0x1e6,
		.aCiSLOT1_6 = 0x1f6,
		.aCiSLOT0_14 = 0x1ee,
		.aCiSLOT1_14 = 0x1fe,
		.aCiSLOT0_15 = 0x1ef,
		.aCiSLOT1_15 = 0x1ff,
		.aCiAFS = 0x244,
	}, {

		.aCiCTLR0 = 0x280,
		.aCiCTLR1 = 0x251,
		.aCiSLPR = 0x252,
		.aCiSTR = 0x282,
		.aCiIDR = 0x284,
		.aCICONR = 0x286,
		.aCiBRP = 0x297,
		.aCiTSR = 0x288,
		.aCiTEC = 0x28A,
		.aCiREC = 0x28B,
		.aCiSISTR = 0x28C,
		.aCiSIMKR = 0x0290,
		.aCiEIMKR = 0x0294,
		.aCiEISTR = 0x0295, 
		.aCiEFR = 0x0296,
		.aCiMDR = 0x0299,	
		.aCiSSCTLR = 0x2A0,
		.aCiSSSTR = 0x2A4,
		.aCiGMR0 = 0x2a8,
		.aCiLMAR0 = 0x2b0,
		.aCilMBR0 = 0x2b8,
		.aCiGMR1 = 0x2a9,
		.aCiLMAR1 = 0x2b1,
		.aCilMBR1 = 0x2b9,
		.aCiGMR2 = 0x2aa,
		.aCiLMAR2 = 0x2b2,
		.aCilMBR2 = 0x2ba,
		.aCiGMR3 = 0x2ab,	
		.aCiLMAR3 = 0x2b3,
		.aCilMBR3 = 0x2bb,
		.aCiGMR4 = 0x2ac,
		.aCiLMAR4 = 0x2b4,
		.aCiLMBR4 = 0x2bc,
		.aCiMCTL0 = 0x2b0,
		.aCiSBS = 0x250,
		.aCiSLOT0_0 = 0x260,
		.aCiSLOT1_0 = 0x270,
		.aCiSLOT0_1 = 0x261,
		.aCiSLOT1_1 = 0x271,
		.aCiSLOT0_2 = 0x262,
		.aCiSLOT1_2 = 0x272,
		.aCiSLOT0_3 = 0x263,
		.aCiSLOT1_3 = 0x273,
		.aCiSLOT0_4 = 0x264,
		.aCiSLOT1_4 = 0x274,
		.aCiSLOT0_5 = 0x265, 
		.aCiSLOT1_5 = 0x275,
		.aCiSLOT0_6 = 0x266,
		.aCiSLOT1_6 = 0x276, 
		.aCiSLOT0_14 = 0x26e,
		.aCiSLOT1_14 = 0x27e,
		.aCiSLOT0_15 = 0x26f,
		.aCiSLOT1_15 = 0x27f,
		.aCiAFS = 0x254,
	}
};

#define CiCTRL0_RESET0		(1 << 0)
#define CiCTRL0_LOOPBACK	(1 << 1)
#define CiCTRL0_BASICCAN	(1 << 3)
#define CiCTRL0_RESET1		(1 << 4)
#define CiCTRL0_TSPRE0		(1 << 8)
#define CiCTRL0_TSPRE1		(1 << 9)
#define CiCTRL0_TSRESET		(1 << 10)
#define CiCTRL0_ECRESET		(1 << 11)

#define CiCTLR1_BANKSEL		(1 << 3)
#define CiCTLR1_INTSEL		(1 << 6)

#define CiSLPR_SLEEP		(1 << 0)
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

#define CiCONR_SAM		(1 << 4)
#define CiCONR_PTS_MSK		(7 << 5)
#define CiCONR_PBS1_MSK		(7 << 8)
#define CiCONR_PBS2_MSK		(7 << 11)
#define CiCONR_SJW_MSK		(3 << 14)

#define CiEIMKR_BOIM		(1 << 0)
#define CiEIMKR_EPIM		(1 << 1)
#define CiEIMKR_BEIM		(1 << 2)

#define CiEISTR_BOIS		(1 << 0)
#define CiEISTR_EPIS		(1 << 1)
#define CiEISTR_BEIS		(1 << 2)

#define CiEFR_ACKE		(1 << 0)
#define CiEFR_CRCE		(1 << 1)
#define CiEFR_FORME		(1 << 2)
#define CiEFR_STFE		(1 << 3)
#define CiEFR_BITE0		(1 << 4)
#define CiEFR_BITE1		(1 << 5)
#define CiEFR_RCVE		(1 << 6)
#define CiEFR_TRE		(1 << 7)

#define CiMDR_CMOD_MSK		(3 << 0)

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

typedef struct M32C_Can {
	BusDevice bdev;
	Clock_t *in_clk;
	Clock_t *baud_clk;
	CANRegisters *regSet;
	CanController *backend;
	SigNode *sigRxIrq;
	SigNode *sigTxIrq;
	int register_set;
	uint16_t regCtrl0;
	uint8_t regCtrl1;	
	uint8_t regSlpr;
	uint16_t regStr;
	uint16_t regIdr;
	uint16_t regConr;
	uint8_t regBrp;
	uint16_t regTsr;
	uint8_t regTec;
	uint8_t regRec;
	uint16_t regSistr;
	uint16_t regSimkr;	
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
	/* The two windows selected by SBS */
	uint8_t regSbs;
	CanMsgBuf msgBuf[16];
	CycleTimer rx_baud_timer;
	CycleTimer tx_baud_timer;
	uint8_t regAfs;
} M32C_Can;


static void
trigger_rx_irq(M32C_Can *mcan) {
	SigNode_Set(mcan->sigRxIrq,SIG_LOW);
	SigNode_Set(mcan->sigRxIrq,SIG_HIGH);
}

static void
trigger_tx_irq(M32C_Can *mcan) {
	SigNode_Set(mcan->sigTxIrq,SIG_LOW);
	SigNode_Set(mcan->sigTxIrq,SIG_HIGH);
}
/**
 **************************************************************
 * exit sleep + reset sets register defaults
 **************************************************************
 */
static void
set_register_defaults(M32C_Can *mcan) 
{
	int i;
	mcan->regCtrl0 = 0x11;
	mcan->regCtrl1 = 0;
	mcan->regStr = 0x100;
	mcan->regIdr = 0;
	mcan->regConr = 0;
	mcan->regBrp = 1;
	mcan->regTsr = 0;
	mcan->regTec = 0;
	mcan->regRec = 0;
	mcan->regSistr = 0;
	mcan->regSimkr = 0;
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
	/* The two windows selected by SBS */
	mcan->regSbs = 0;
	// update_interrupt(mcan);
}

static void
resume_rx(void *clientData)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	CanStartRx(mcan->backend);
}

static  void
M32C_CanReceive (void *clientData,CAN_MSG *msg) 
{
	M32C_Can *mcan = (M32C_Can *)clientData;
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
		mcan->regSistr |= (0x8000 >> i);
		trigger_rx_irq(mcan);	
		// set some interrupt register
	}
	
}

static void
M32C_CanTransmit(M32C_Can *mcan,int slot)
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
cictlr0_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regCtrl0;
}

static void
cictlr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	if(rqlen == 1) {
		if(address & 1) {
			value = (mcan->regCtrl0 & 0xff) | (value << 8);
		} else {
			value = (mcan->regCtrl0 & 0xff00) | (value & 0xff);
		}
	}
	if((value & 0x11) == 0x11) {
		set_register_defaults(mcan);
	} else {
		mcan->regCtrl0 = value;
		mcan->regStr &= ~CiSTR_STATE_RESET;
	}
}

/**
 *******************************************************************
 *  Bit 3: BANKSEL	
 *  Bit 6: INTSEL
 *******************************************************************
 */
static uint32_t
cictlr1_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regCtrl1;	
}

static void
cictlr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regCtrl1 = value & (CiCTLR1_BANKSEL | CiCTLR1_INTSEL);	
}

/**
 **********************************************************
 * Sleep mode register: enable/disable CAN bus
 **********************************************************
 */
static uint32_t
cislpr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regSlpr;
}

static void
cislpr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	uint8_t diff = value ^ mcan->regSlpr;
	mcan->regSlpr = value & CiSLPR_SLEEP;
	if(value & diff & CiSLPR_SLEEP) {
		// map all registers 
	} else if(~value & diff & CiSLPR_SLEEP) {
		// make all registers but slpr inaccessible
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regStr;
}

#include "cpu_m32c.h"
static void
cistr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"CAN: Writing readonly CiSTR register at %08x\n",M32C_REG_PC);
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

static void
update_clock(M32C_Can *mcan) 
{
	uint16_t brp;
	uint8_t pts = (mcan->regConr >> 5) & 7;
	uint8_t pts_tq = pts + 1;
	uint8_t pbs1 = (mcan->regConr >> 8) & 7;
	uint8_t pbs1_tq = pbs1 + 1;
	uint8_t pbs2 = (mcan->regConr >> 11) & 7;
	uint8_t pbs2_tq = pbs2 + 1;
	//uint8_t sjw = (mcan->regConr >> 14) & 3;
	//uint8_t sjw_tq = sjw + 1;
	uint16_t segs = pts_tq + pbs1_tq + pbs2_tq + 1;
	brp = mcan->regBrp;
	
	Clock_MakeDerived(mcan->baud_clk,mcan->in_clk,1,segs * (brp + 1));
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regConr;
}

static void
ciconr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regConr = value;
	update_clock(mcan);
}

/*
 *******************************************************
 * CiBRP
 * Baud Rate Prescaler
 * Divide the CAN-clock by BRP+1. a BRP value of 0 
 * (division by one) is not allowed.
 *******************************************************
 */
static uint32_t
cibrp_read(void *clientData,uint32_t address,int rqlen)
{

	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regBrp;
}

static void
cibrp_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regBrp = value;
	update_clock(mcan);
}

/**
 *****************************************************************
 * CiTSR is a 16 Bit counter used for timestamping incoming CAN
 * messages.
 *****************************************************************
 */
static uint32_t
citsr_read(void *clientData,uint32_t address,int rqlen)
{

	return 0;
}

static void
citsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

/**
 ***************************************************************
 * Can Transmit Error Count Register
 ***************************************************************
 */
static uint32_t
citec_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regTec;
}

static void
citec_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Writing to readonly CiTEC register\n");
}

/**
 *************************************************************
 * Can Receive Error Count Register
 *************************************************************
 */
static uint32_t
cirec_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regRec;
}

static void
cirec_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Writing to readonly CiREC register\n");
}

/**
 *************************************************************
 * Slot Interrupt Status Register CiSISTR.
 * Shows which message slot is requesting an interrupt
 * (One Bit per slot).
 * Can be cleared by writing a 0
 *************************************************************
 */
static uint32_t
cisistr_read(void *clientData,uint32_t address,int rqlen)
{

	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regSistr;;
}

static void
cisistr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regSistr &= value;
	// update_interrupt(mcan);
}

/*
 *****************************************************
 * Interrupt mask register, One Bit per slot.
 *****************************************************
 */
static uint32_t
cisimkr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regSimkr;
}

static void
cisimkr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regSimkr = value;
	// update_interrupt(mcan);
}

/**
 *****************************************************
 * Error interrupt mask register
 *****************************************************
 */
static uint32_t
cieimkr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regEimkr;
}

static void
cieimkr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regEimkr = value;
	// update_interrupt(mcan);
}

/**
 *********************************************************
 * Error Interrupt status register
 *********************************************************
 */
static uint32_t
cieistr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regEistr;
}

static void
cieistr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regEistr &= value;
	//update_interrupt(mcan);
}

/**
 ************************************************************
 * CiEFR
 * Error source register
 ************************************************************
 */
static uint32_t
ciefr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regEfr;
}

static void
ciefr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regEfr &= value;
}

/**
 ***************************************************************
 * Operating mode register
 * Bits 0+1: 00 Normal operating mode
 *	     01 Bus monitoring mode
 *	     10 Self test mode
 ***************************************************************
 */
static uint32_t
cimdr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regMdr;
}

static void
cimdr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regMdr = value & 3;
}

/**
 *************************************************************
 * CiSSCTLR
 * Single shot control register. One bit per message slot 
 *************************************************************
 */
static uint32_t
cissctlr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		fprintf(stderr,"Accessing cissctlr while BANKSEL 0\n");
		return 0;
	}
	return mcan->regSsctlr;
}

static void
cissctlr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		fprintf(stderr,"Accessing cissctlr while BANKSEL 0\n");
		return;
	}
	mcan->regSsctlr = value;
}

/**
 *************************************************************
 * Single shot status register
 *************************************************************
 */
static uint32_t
cissstr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		fprintf(stderr,"Accessing cissstr while BANKSEL 0\n");
		return 0;
	}
	return mcan->regSsstr;;
}

static void
cissstr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		fprintf(stderr,"Accessing cissstr while BANKSEL 0\n");
		return;
	}
	mcan->regSsstr &= value;
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regGmr[0];
}

static void
cigmr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regLmar[0];
}


static void
cilmar0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regLmar[0] = value;
}

static uint8_t
cimctlk_read(void *clientData,unsigned int n) {
	M32C_Can *mcan = (M32C_Can *)clientData;
	//fprintf(stderr,"Mctl %d is %02x: %06x\n",n,mcan->regMctl[n],M32C_REG_PC);
	return mcan->regMctl[n];
}

static void
cimctlk_write(void *clientData,uint8_t value,int n) 
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	mcan->regMctl[n] = 
		(mcan->regMctl[n] & (CiMCTLj_REMACTIVE | CiMCTLj_TRMACTIVE))
		| (value & ~(CiMCTLj_REMACTIVE | CiMCTLj_TRMACTIVE));
	if(value & CiMCTLj_TRMREQ) {
		M32C_CanTransmit(mcan,n);
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
cilmar0mctl0_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		return cilmar0_read(clientData,address,rqlen);
	} else {
		return cimctl0_read(clientData,address,rqlen);
	}
}

static void
cilmar0mctl0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		cilmar0_write(clientData,value,address,rqlen);
	} else {
		cimctl0_write(clientData,value,address,rqlen);
	}
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

	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regLmbr[0];
}

static void
cilmbr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
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
cilmbr0cimctl8_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		return cilmbr0_read(clientData,address,rqlen);
	} else {
		return cimctl8_read(clientData,address,rqlen);
	}
}

static void
cilmbr0cimctl8_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		cilmbr0_write(clientData,value,address,rqlen);
	} else {
		cimctl8_write(clientData,value,address,rqlen);
	}

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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regGmr[1];
}

static void
cigmr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regLmar[1];
}

static void
cilmar1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regLmar[1] = value;
}

static uint32_t
cilmar1cimctl1_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		return cilmar1_read(clientData,address,rqlen);
	} else {
		return cimctl1_read(clientData,address,rqlen);
	}
}

static void
cilmar1cimctl1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		cilmar1_write(clientData,value,address,rqlen);
	} else {
		cimctl1_write(clientData,value,address,rqlen);
	}

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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regLmbr[1];
}

static void
cilmbr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regLmbr[1] = value;
}

static uint32_t
cilmbr1cimctl9_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		return cilmbr1_read(clientData,address,rqlen);
	} else {
		return cimctl9_read(clientData,address,rqlen);
	}
}

static void
cilmbr1cimctl9_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		cilmbr1_write(clientData,value,address,rqlen);
	} else {
		cimctl9_write(clientData,value,address,rqlen);
	}
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regGmr[2];
}

static void
cigmr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regLmar[2];
}

static void
cilmar2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regLmar[2] = value;
}

static uint32_t
cilmar2cimctl2_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		return cilmar2_read(clientData,address,rqlen);
	} else {
		return cimctl2_read(clientData,address,rqlen);
	}
}

static void
cilmar2cimctl2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		cilmar2_write(clientData,value,address,rqlen);
	} else {
		cimctl2_write(clientData,value,address,rqlen);
	}
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regLmbr[2];
}

static void
cilmbr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regLmbr[2] = value;
}

static uint32_t
cilmbr2cimctl10_read(void *clientData,uint32_t address,int rqlen)
{

	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		return cilmbr2_read(clientData,address,rqlen);
	} else {
		return cimctl10_read(clientData,address,rqlen);
	}
}

static void
cilmbr2cimctl10_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		cilmbr2_write(clientData,value,address,rqlen);
	} else {
		cimctl10_write(clientData,value,address,rqlen);
	}

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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regGmr[3];
}

static void
cigmr3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regLmar[3];

}

static void
cilmar3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regLmar[3] = value;
}

static uint32_t
cilmar3cimctl3_read(void *clientData,uint32_t address,int rqlen)
{

	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		return cilmar3_read(clientData,address,rqlen);
	} else {
		return cimctl3_read(clientData,address,rqlen);
	}
}

static void
cilmar3cimctl3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		cilmar3_write(clientData,value,address,rqlen);
	} else {
		cimctl3_write(clientData,value,address,rqlen);
	}

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

	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regLmbr[3];
}

static void
cilmbr3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regLmbr[3] = value;
}

static uint32_t
cilmbr3cimctl11_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		return cilmbr3_read(clientData,address,rqlen);
	} else {
		return cimctl11_read(clientData,address,rqlen);
	}
}

static void
cilmbr3cimctl11_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		cilmbr3_write(clientData,value,address,rqlen);
	} else {
		cimctl11_write(clientData,value,address,rqlen);
	}
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regGmr[4];
}

static void
cigmr4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
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
	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regLmar[4];
}

static void
cilmar4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regLmar[4] = value;
}

static uint32_t
cilmar4cimctl4_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		return cilmar4_read(clientData,address,rqlen);
	} else {
		return cimctl4_read(clientData,address,rqlen);
	}
}

static void
cilmar4cimctl4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		cilmar4_write(clientData,value,address,rqlen);
	} else {
		cimctl4_write(clientData,value,address,rqlen);
	}

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

	M32C_Can *mcan = (M32C_Can *) clientData;
	return mcan->regLmbr[4];
}

static void
cilmbr4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData;
	mcan->regLmbr[4] = value;
}

static uint32_t
cilmbr4cimctl12_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		return cilmbr4_read(clientData,address,rqlen);
	} else {
		return cimctl12_read(clientData,address,rqlen);
	}
}

static void
cilmbr4cimctl12_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *)clientData;
	if(mcan->regCtrl1 & CiCTLR1_BANKSEL) {
		cilmbr4_write(clientData,value,address,rqlen);
	} else {
		cimctl12_write(clientData,value,address,rqlen);
	}
}

static uint32_t
cisbs_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	return can->regSbs;
}

static void
cisbs_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	can->regSbs = value;
}

static uint32_t
cislot0_0_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> 6) & 0x1f;
}

static void
cislot0_0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0x1f << 6)) | ((value & 0x1f) << 6);
}

static uint32_t
cislot0_1_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->Id & 0x3f;
}

static void
cislot0_1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0x3f << 0)) | ((value & 0x3f) << 0);
}

static uint32_t
cislot0_2_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> (14+11)) & 0xf;
}

static void
cislot0_2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0xf << 25)) | ((value & 0xf) << 25);
}

static uint32_t
cislot0_3_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> (6+11)) & 0xff;
}

static void
cislot0_3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0xff << 17)) | ((value & 0xff) << 17);
}

static uint32_t
cislot0_4_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> (0+11)) & 0x3f;
}

static void
cislot0_4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0x3f << 11)) | ((value & 0x3f) << 11);
}

static uint32_t
cislot0_5_read(void *clientData,uint32_t address,int rqlen)
{

	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->dlc;
}

static void
cislot0_5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->dlc = value & 0xf;
}

static uint32_t
cislot0_6_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[0];
}

static void
cislot0_6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[0] = value;
}

static uint32_t
cislot0_7_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[1];
}

static void
cislot0_7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[1] = value;
}

static uint32_t
cislot0_8_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[2];
}

static void
cislot0_8_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[2] = value;
}

static uint32_t
cislot0_9_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[3];
}

static void
cislot0_9_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[3] = value;
}

static uint32_t
cislot0_10_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[4];
}

static void
cislot0_10_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[4] = value;
}

static uint32_t
cislot0_11_read(void *clientData,uint32_t address,int rqlen)
{

	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[5];
}

static void
cislot0_11_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[5] = value;
}

static uint32_t
cislot0_12_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[6];
}

static void
cislot0_12_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[6] = value;
}

static uint32_t
cislot0_13_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[7];
}

static void
cislot0_13_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[7] = value;
}

static uint32_t
cislot0_14_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->timeStamp >> 8;
}

static void
cislot0_14_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->timeStamp = (msgbuf->timeStamp & 0xff) | ((value & 0xff) << 8);
}

static uint32_t
cislot0_15_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->timeStamp & 0xff;
}

static void
cislot0_15_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = can->regSbs & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->timeStamp = (msgbuf->timeStamp & 0xff00) | (value & 0xff);
}

static uint32_t
cislot1_0_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> 6) & 0x1f;
}

static void
cislot1_0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0x1f << 6)) | ((value & 0x1f) << 6);
}

static uint32_t
cislot1_1_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->Id & 0x3f;
}

static void
cislot1_1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0x3f << 0)) | ((value & 0x3f) << 0);
}

static uint32_t
cislot1_2_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> (14+11)) & 0xf;
}

static void
cislot1_2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0xf << 25)) | ((value & 0xf) << 25);
}

static uint32_t
cislot1_3_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> (6+11)) & 0xff;
}

static void
cislot1_3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs  >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0xff << 17)) | ((value & 0xff) << 17);
}

static uint32_t
cislot1_4_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return (msgbuf->Id >> (0+11)) & 0x3f;
}

static void
cislot1_4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->Id = (msgbuf->Id & ~(0x3f << 11)) | ((value & 0x3f) << 11);
}

static uint32_t
cislot1_5_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->dlc;
}

static void
cislot1_5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->dlc = value & 0xf;
}

static uint32_t
cislot1_6_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[0];
}

static void
cislot1_6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[0] = value;

}
static uint32_t
cislot1_7_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[1];
}

static void
cislot1_7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[1] = value;
}

static uint32_t
cislot1_8_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[2];
}

static void
cislot1_8_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[2] = value;
}

static uint32_t
cislot1_9_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[3];
}

static void
cislot1_9_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[3] = value;
}

static uint32_t
cislot1_10_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[4];
}

static void
cislot1_10_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[4] = value;
}

static uint32_t
cislot1_11_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[5];
}

static void
cislot1_11_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[5] = value;
}

static uint32_t
cislot1_12_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[6];
}

static void
cislot1_12_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[6] = value;
}

static uint32_t
cislot1_13_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->data[7];
}

static void
cislot1_13_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->data[7] = value;
}

static uint32_t
cislot1_14_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->timeStamp >> 8;
}

static void
cislot1_14_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->timeStamp = (msgbuf->timeStamp & 0xff) | ((value & 0xff) << 8);
}

static uint32_t
cislot1_15_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	return msgbuf->timeStamp & 0xff;
}

static void
cislot1_15_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *can = (M32C_Can *) clientData; 
	int slot = (can->regSbs >> 4) & 0xf;
	CanMsgBuf *msgbuf = &can->msgBuf[slot];
	msgbuf->timeStamp = (msgbuf->timeStamp & 0xff00) | (value & 0xff);
}

static uint32_t
ciafs_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData; 
	return mcan->regAfs;
}

static void
ciafs_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Can *mcan = (M32C_Can *) clientData; 
	mcan->regAfs = value;
}

static void
M32CCan_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	int j;
	M32C_Can *mcan = (M32C_Can *) owner;
	CANRegisters *cr = mcan->regSet;
	IOH_Delete16(cr->aCiCTLR0);
	IOH_Delete8(cr->aCiCTLR1);
	IOH_Delete8(cr->aCiSLPR);
	IOH_Delete16(cr->aCiSTR);
	IOH_Delete16(cr->aCiIDR);
	IOH_Delete16(cr->aCICONR);
	IOH_Delete8(cr->aCiBRP);
	IOH_Delete16(cr->aCiTSR);
	IOH_Delete8(cr->aCiTEC);
	IOH_Delete8(cr->aCiREC);
	IOH_Delete16(cr->aCiSISTR);
	IOH_Delete16(cr->aCiSIMKR);
	IOH_Delete8(cr->aCiEIMKR);
	IOH_Delete8(cr->aCiEISTR); 
	IOH_Delete8(cr->aCiEFR);
	IOH_Delete8(cr->aCiMDR);	
	IOH_Delete16(cr->aCiSSCTLR);
	IOH_Delete16(cr->aCiSSSTR);
	IOH_Delete8(cr->aCiGMR0);	
	IOH_Delete8(cr->aCiLMAR0);
	IOH_Delete8(cr->aCilMBR0);
	IOH_Delete8(cr->aCiGMR1);	
	IOH_Delete8(cr->aCiLMAR1);
	IOH_Delete8(cr->aCilMBR1);
	IOH_Delete8(cr->aCiGMR2);	
	IOH_Delete8(cr->aCiLMAR2);
	IOH_Delete8(cr->aCilMBR2);
	IOH_Delete8(cr->aCiGMR3);	
	IOH_Delete8(cr->aCiLMAR3);
	IOH_Delete8(cr->aCilMBR3);
	IOH_Delete8(cr->aCiGMR4);
	IOH_Delete8(cr->aCiLMAR4);
	IOH_Delete8(cr->aCiLMBR4);
	IOH_Delete8(cr->aCiSBS);	
	IOH_Delete8(cr->aCiMCTL0+5);
	IOH_Delete8(cr->aCiMCTL0+6);
	IOH_Delete8(cr->aCiMCTL0+7);
	IOH_Delete8(cr->aCiMCTL0+13);
	IOH_Delete8(cr->aCiMCTL0+14);
	IOH_Delete8(cr->aCiMCTL0+15);
	
	for(j=0;j<=15;j++) {
		IOH_Delete8(cr->aCiSLOT0_0 + j);	
		IOH_Delete8(cr->aCiSLOT1_0 + j);	
	}
	IOH_Delete16(cr->aCiAFS);
	//M32C_Can *can = (M32C_Can *) owner;
}

static void
M32CCan_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	M32C_Can *mcan = (M32C_Can *) owner;
	uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_PRD_RARP | IOH_FLG_PA_CBSE;
	CANRegisters *cr = mcan->regSet;
	IOH_New16f(cr->aCiCTLR0,cictlr0_read,cictlr0_write,mcan,flags);
	IOH_New8(cr->aCiCTLR1,cictlr1_read,cictlr1_write,mcan);
	IOH_New8(cr->aCiSLPR,cislpr_read,cislpr_write,mcan);;
	IOH_New16f(cr->aCiSTR,cistr_read,cistr_write,mcan,flags);
	IOH_New16f(cr->aCiIDR,ciidr_read,ciidr_write,mcan,flags);
	IOH_New16f(cr->aCICONR,ciconr_read,ciconr_write,mcan,flags);
	IOH_New8(cr->aCiBRP,cibrp_read,cibrp_write,mcan);
	IOH_New16(cr->aCiTSR,citsr_read,citsr_write,mcan);
	IOH_New8(cr->aCiTEC,citec_read,citec_write,mcan);
	IOH_New8(cr->aCiREC,cirec_read,cirec_write,mcan);
	IOH_New16f(cr->aCiSISTR,cisistr_read,cisistr_write,mcan,flags);
	IOH_New16f(cr->aCiSIMKR,cisimkr_read,cisimkr_write,mcan,flags);
	IOH_New8(cr->aCiEIMKR,cieimkr_read,cieimkr_write,mcan);
	IOH_New8(cr->aCiEISTR,cieistr_read,cieistr_write,mcan); 
	IOH_New8(cr->aCiEFR,ciefr_read,ciefr_write,mcan);
	IOH_New8(cr->aCiMDR,cimdr_read,cimdr_write,mcan);	
	IOH_New16f(cr->aCiSSCTLR,cissctlr_read,cissctlr_write,mcan,flags);
	IOH_New16f(cr->aCiSSSTR,cissstr_read,cissstr_write,mcan,flags);
	IOH_New8(cr->aCiGMR0,cigmr0_read,cigmr0_write,mcan);	
	IOH_New8(cr->aCiLMAR0,cilmar0mctl0_read,cilmar0mctl0_write,mcan);
	IOH_New8(cr->aCilMBR0,cilmbr0cimctl8_read,cilmbr0cimctl8_write,mcan);
	IOH_New8(cr->aCiGMR1,cigmr1_read,cigmr1_write,mcan);	
	IOH_New8(cr->aCiLMAR1,cilmar1cimctl1_read,cilmar1cimctl1_write,mcan);
	IOH_New8(cr->aCilMBR1,cilmbr1cimctl9_read,cilmbr1cimctl9_write,mcan);
	IOH_New8(cr->aCiGMR2,cigmr2_read,cigmr2_write,mcan);	
	IOH_New8(cr->aCiLMAR2,cilmar2cimctl2_read,cilmar2cimctl2_write,mcan);
	IOH_New8(cr->aCilMBR2,cilmbr2cimctl10_read,cilmbr2cimctl10_write,mcan);
	IOH_New8(cr->aCiGMR3,cigmr3_read,cigmr3_write,mcan);	
	IOH_New8(cr->aCiLMAR3,cilmar3cimctl3_read,cilmar3cimctl3_write,mcan);
	IOH_New8(cr->aCilMBR3,cilmbr3cimctl11_read,cilmbr3cimctl11_write,mcan);
	IOH_New8(cr->aCiGMR4,cigmr4_read,cigmr4_write,mcan);
	IOH_New8(cr->aCiLMAR4,cilmar4cimctl4_read,cilmar4cimctl4_write,mcan);
	IOH_New8(cr->aCiLMBR4,cilmbr4cimctl12_read,cilmbr4cimctl12_write,mcan);
	IOH_New8(cr->aCiMCTL0+5,cimctl5_read,cimctl5_write,mcan);
	IOH_New8(cr->aCiMCTL0+6,cimctl6_read,cimctl6_write,mcan);
	IOH_New8(cr->aCiMCTL0+7,cimctl7_read,cimctl7_write,mcan);
	IOH_New8(cr->aCiMCTL0+13,cimctl13_read,cimctl13_write,mcan);
	IOH_New8(cr->aCiMCTL0+14,cimctl14_read,cimctl14_write,mcan);
	IOH_New8(cr->aCiMCTL0+15,cimctl15_read,cimctl15_write,mcan);

	IOH_New8(cr->aCiSBS,cisbs_read,cisbs_write,mcan);	
	IOH_New8f(cr->aCiSLOT0_0 + 0,cislot0_0_read,cislot0_0_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 0,cislot1_0_read,cislot1_0_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 1,cislot0_1_read,cislot0_1_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 1,cislot1_1_read,cislot1_1_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 2,cislot0_2_read,cislot0_2_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 2,cislot1_2_read,cislot1_2_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 3,cislot0_3_read,cislot0_3_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 3,cislot1_3_read,cislot1_3_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 4,cislot0_4_read,cislot0_4_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 4,cislot1_4_read,cislot1_4_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 5,cislot0_5_read,cislot0_5_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 5,cislot1_5_read,cislot1_5_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 6,cislot0_6_read,cislot0_6_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 6,cislot1_6_read,cislot1_6_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 7,cislot0_7_read,cislot0_7_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 7,cislot1_7_read,cislot1_7_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 8,cislot0_8_read,cislot0_8_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 8,cislot1_8_read,cislot1_8_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 9,cislot0_9_read,cislot0_9_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 9,cislot1_9_read,cislot1_9_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 10,cislot0_10_read,cislot0_10_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 10,cislot1_10_read,cislot1_10_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 11,cislot0_11_read,cislot0_11_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 11,cislot1_11_read,cislot1_11_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 12,cislot0_12_read,cislot0_12_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 12,cislot1_12_read,cislot1_12_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 13,cislot0_13_read,cislot0_13_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 13,cislot1_13_read,cislot1_13_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 14,cislot0_14_read,cislot0_14_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 14,cislot1_14_read,cislot1_14_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT0_0 + 15,cislot0_15_read,cislot0_15_write,mcan,flags);
	IOH_New8f(cr->aCiSLOT1_0 + 15,cislot1_15_read,cislot1_15_write,mcan,flags);
	IOH_New16f(cr->aCiAFS,ciafs_read,ciafs_write,mcan,flags);
}

static CanChipOperations canOps =  {
	.receive = M32C_CanReceive,
};

BusDevice *
M32C_CanNew(const char *name,unsigned int register_set) 
{
	M32C_Can *mcan = sg_new(M32C_Can);
	if(register_set >= array_size(canRegs)) {
		fprintf(stderr,"Illegal register set selection %d\n",register_set);
		exit(1);
	}
	mcan->register_set = register_set;
	mcan->regSet = &canRegs[mcan->register_set]; 
	mcan->bdev.first_mapping=NULL;
        mcan->bdev.Map=M32CCan_Map;
        mcan->bdev.UnMap=M32CCan_Unmap;
        mcan->bdev.owner=mcan;
        mcan->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        mcan->in_clk = Clock_New("%s.clk",name);
        mcan->baud_clk = Clock_New("%s.baud_clk",name);
	mcan->sigRxIrq = SigNode_New("%s.rxirq",name);
	mcan->sigTxIrq = SigNode_New("%s.txirq",name);
	SigNode_Set(mcan->sigRxIrq,SIG_HIGH);
	SigNode_Set(mcan->sigTxIrq,SIG_HIGH);
        Clock_SetFreq(mcan->in_clk,24000000); /* Should be connected instead */
	mcan->backend = CanSocketInterface_New(&canOps,name,mcan);
	//CycleTimer_Init(&mcan->tx_baud_timer,tx_done,usart);
        CycleTimer_Init(&mcan->rx_baud_timer,resume_rx,mcan);
        fprintf(stderr,"M32C CAN controller \"%s\" created\n",name);
        return &mcan->bdev;
}
