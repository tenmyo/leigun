/*
 *****************************************************************************
 * MDIO Management Interface according to clause 45 of the 802.3 specification
 *****************************************************************************
 */

#include "sglib.h"
#include "sgstring.h"
#include "signode.h"
#include "cycletimer.h"
#include "phy.h"
#include "mdio_slave.h"
#include "configfile.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x);fflush(stderr);}
#else
#define dbgprintf(x...)
#endif

#define STATE_IDLE	(0)
#define STATE_ST	(1)
#define STATE_OP1	(2)
#define STATE_OP2	(3)
#define STATE_TA1	(4)
#define STATE_TA2	(5)
#define STATE_PHYAD	(6)
#define STATE_REGAD	(7)
#define STATE_READ	(8)
#define STATE_WRITE	(9)
#define STATE_IGNORE	(10)

#define OP_ADDRESS		(0)
#define OP_WRITE		(1)
#define OP_READINC		(2)
#define OP_READ			(3)

#define MAX_CLK		2500000

struct MDIO_Slave {
	MDIO_Device *mdioDev;
	uint8_t devAddr;
	uint8_t devType;
	uint32_t maxClock;
	uint32_t posEdgeToMdioDelay;
	uint32_t minClkEdge;

	SigNode *sigMDC;
	SigNode *sigMDIO;

	int min_syncbits; 	/*< Standard says 32 Bit sync pattern */
	int sampleValMDIO;
	int state;
	int bitcount;
	int operation;
	uint16_t dataShift;
	uint16_t devAdShift;
	uint16_t devTypeShift;
	CycleCounter_t edgeTime;
	CycleTimer rdPosEdgeToMdioTimer;
};

static void 
MDIO_TraceProc(SigNode *sig, int value, void *clientData)
{
	MDIO_Slave *ms = clientData;
	ms->sampleValMDIO = value;
}

/**
 **********************************************************************
 * \fn static void MDIO_Shift(void *eventData)
 * Shifting a data bit out is done delayed by this timer handler.
 **********************************************************************
 */
static void
MDIO_Shift(void *eventData)
{
	MDIO_Slave *ms = eventData;
	if(ms->dataShift & 0x8000) {
		SigNode_Set(ms->sigMDIO,SIG_HIGH);
	} else {
		SigNode_Set(ms->sigMDIO,SIG_LOW);
	}
	ms->dataShift <<= 1;
	ms->bitcount++;
}

/**
 *************************************************************************************
 * \fn static void MDC_TraceProc(SigNode *sig, int value, void *clientData)
 * Event procedure called on change of the MDC clock signal. This is the main
 * state machine of the MDIO slave. 
 *************************************************************************************
 */
static void 
MDC_TraceProc(SigNode *sig, int value, void *clientData)
{
	MDIO_Slave *ms = clientData;
	int mdio = ms->sampleValMDIO;
	CycleCounter_t now = CycleCounter_Get();
	if((now - ms->edgeTime) < NanosecondsToCycles(ms->minClkEdge)) {
		fprintf(stderr,"MDIO timing violation %u\n",(int)(now - ms->edgeTime));
		return;
	}
	ms->edgeTime = now;
	if(value == SIG_LOW) {
		/* Ignore negative edge */
		return;
	}
	//fprintf(stderr,"State %u mdio %u\n",ms->state,mdio);
	switch(ms->state) {
		case STATE_IDLE:
			if(mdio == SIG_LOW) {
				if(ms->bitcount >= ms->min_syncbits) {
					ms->state = STATE_ST;
				}
			} else {
				ms->bitcount++;
			}
			break;

		case STATE_ST:
			if(mdio == SIG_LOW) {
				ms->state = STATE_OP1;
			}  else {
				ms->bitcount = 0;
				ms->state = STATE_IDLE;
			}
			break;

		case STATE_OP1:
			if(mdio == SIG_HIGH) {
				ms->operation = 2;
			} else {
				ms->operation = 0;
			}
			ms->state = STATE_OP2;
			break;

		case STATE_OP2:
			if(mdio == SIG_HIGH) {
				ms->operation |= 1;
			}	
			ms->state = STATE_PHYAD;
			ms->devAdShift = 0;
			ms->bitcount = 0;
			//fprintf(stderr,"The operation is %u\n",ms->operation);
			break;

		case STATE_PHYAD:
			if(mdio == SIG_HIGH) {
				ms->devAdShift = (ms->devAdShift << 1) | 1;
			} else {
				ms->devAdShift = (ms->devAdShift << 1);
			}
			ms->bitcount++;
			if(ms->bitcount >= 5) {
				ms->bitcount = 0;
				ms->devTypeShift = 0;
				ms->state = STATE_REGAD;
			} 
			break;

		case STATE_REGAD:
			if(mdio == SIG_HIGH) {
				ms->devTypeShift = (ms->devTypeShift << 1) | 1;
			} else {
				ms->devTypeShift = (ms->devTypeShift << 1);
			}
			ms->bitcount++;
			if(ms->bitcount >= 5) {
				MDIO_Device *mmd;
				mmd = ms->mdioDev;
				if(mmd && (ms->devAdShift == ms->devAddr) && (ms->devTypeShift == ms->devType)) {
					ms->state = STATE_TA1;
				} else {
					ms->bitcount = 0;
					ms->state = STATE_IGNORE;
				}
			} 
			break;

		case STATE_TA1:
			if((ms->operation == OP_WRITE) || (ms->operation == OP_ADDRESS)) {
				if(mdio != SIG_HIGH) {
					fprintf(stderr,"Error: MDIO not high in TA1 state\n");
					fflush(stderr);
					exit(1);
					ms->state = STATE_IDLE;
				} else {
					ms->state = STATE_TA2;
				}	
			} else {
				SigNode_Set(ms->sigMDIO,SIG_LOW);
				ms->state = STATE_TA2;
			}
			break;

		case STATE_TA2:
			if((ms->operation == OP_WRITE) || (ms->operation == OP_ADDRESS)) {
				if(mdio != SIG_LOW) {
					fprintf(stderr,"Error: MDIO not high in TA2 state\n");
					ms->state = STATE_IDLE;
				} else {
					ms->state = STATE_WRITE;
					ms->bitcount = 0;
				}
			} else {
				MDIO_Device *mmd;
				ms->bitcount = 0;
				/* Shift address comparision missing here */
				mmd = ms->mdioDev;
				if(ms->operation == OP_READ) {
					MDIODev_Read(mmd,&ms->dataShift,0);
				} else if(ms->operation == OP_READINC) {
					MDIODev_Read(mmd,&ms->dataShift,1);
				} 
				dbgprintf("State READ %u phy %u val 0x%04x\n",
					ms->devTypeShift,ms->devAdShift, ms->dataShift);
				CycleTimer_Mod(&ms->rdPosEdgeToMdioTimer,
					NanosecondsToCycles(ms->posEdgeToMdioDelay));	
				ms->state = STATE_READ;
			}
			break;

		case STATE_READ:
			if(ms->bitcount >= 16) {
				SigNode_Set(ms->sigMDIO,SIG_OPEN);
				ms->state = STATE_IDLE;
				CycleTimer_Remove(&ms->rdPosEdgeToMdioTimer);
			} else {
				CycleTimer_Mod(&ms->rdPosEdgeToMdioTimer,NanosecondsToCycles(ms->posEdgeToMdioDelay));	
			}
			break;

		case STATE_WRITE:
			if(mdio == SIG_HIGH) {
				ms->dataShift = (ms->dataShift << 1) | 1;
			} else {
				ms->dataShift = (ms->dataShift << 1);
			}
			ms->bitcount++;
			if(ms->bitcount >= 16) {
				MDIO_Device *mmd;
				mmd = ms->mdioDev;
				if(ms->operation == OP_ADDRESS) {
					MDIODev_Address(mmd,ms->dataShift);
				} else if(ms->operation == OP_WRITE) {
					MDIODev_Write(mmd,ms->dataShift);
				}
				dbgprintf("State Write %u mdio %u value %04x\n",ms->devTypeShift,ms->devAdShift,ms->dataShift);
				ms->state = STATE_IDLE;
			}
			break;

		case STATE_IGNORE: /* Ignore two TA bits and 16 data bits */
			ms->bitcount++;
			if(ms->bitcount >= 18) {
				ms->state = STATE_IDLE;
			}
			break;
	}
}

void
MDIOSlave_RegisterDev(MDIO_Slave *ms,MDIO_Device *mdioDev,uint8_t devAddr,uint8_t devType) 
{
	if(devAddr < 32) {
		ms->devAddr = devAddr;
		ms->devType = devType;
		ms->mdioDev = mdioDev;
		//fprintf(stderr,"Registered MDIO dev %u, type %u\n",devAddr,devType);
		//sleep(1);
	} else {
		fprintf(stderr,"Illegal MDIO device address %u\n",devAddr);
		exit(1);
	}
}

/**
 ****************************************************************************
 * \fn MDIO_Slave * MDIOSlave_New(const char *name)
 * Create a state machine for eating MDIO signals. (MDIO device side)
 ****************************************************************************
 */
MDIO_Slave *
MDIOSlave_New(const char *name)
{
	MDIO_Slave *ms;
	ms = sg_new(MDIO_Slave);
	ms->maxClock = 2500000;
	Config_ReadUInt32(&ms->maxClock, name, "mdio_clk");

	ms->sigMDC = SigNode_New("%s.mdc",name);
	ms->sigMDIO = SigNode_New("%s.mdio",name);
	ms->sampleValMDIO = SigNode_Val(ms->sigMDIO);
	ms->state = STATE_IDLE;
	ms->min_syncbits = 16;	/* Should be configurable by MDIO device */
	ms->posEdgeToMdioDelay = 1000000000 / ms->maxClock * 3 / 4;
	ms->minClkEdge = 1000000000 / ms->maxClock  * 40 / 100;
	SigNode_Trace(ms->sigMDC,MDC_TraceProc,ms);
	SigNode_Trace(ms->sigMDIO,MDIO_TraceProc,ms);
	CycleTimer_Init(&ms->rdPosEdgeToMdioTimer,MDIO_Shift,ms);
	return ms;
}
