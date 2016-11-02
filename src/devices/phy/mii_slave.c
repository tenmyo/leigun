/*
 *****************************************************************************
 * MII Management Interface (MDIO) PHY device side state machine.
 * This is the IEEE802.3 Clause 22 managenent interface using Startcode "01". 
 * The Clause 45 * MDIO using StartCode "00" is found in a separate directory
 *****************************************************************************
 */

#include "sglib.h"
#include "sgstring.h"
#include "signode.h"
#include "cycletimer.h"
#include "phy.h"
#include "mii_slave.h"

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

#define OP_READ		(0)
#define OP_WRITE	(1)

struct MII_Slave {
	PHY_Device *phy;
	uint8_t phyAddr;
	SigNode *sigMDC;
	SigNode *sigMDIO;
	int sampleValMDIO;
	int state;
	int bitcount;
	int operation;
	uint16_t dataShift;
	uint16_t phyAdShift;
	uint16_t regAdShift;
	CycleCounter_t edgeTime;
	CycleTimer rdPosEdgeToMdioTimer;
};

static void 
MDIO_TraceProc(SigNode *sig, int value, void *clientData)
{
	MII_Slave *ms = clientData;
	ms->sampleValMDIO = value;
}

static void
MDIOSet(void *eventData)
{
	MII_Slave *ms = eventData;
	if(ms->dataShift & 0x8000) {
		SigNode_Set(ms->sigMDIO,SIG_HIGH);
	} else {
		SigNode_Set(ms->sigMDIO,SIG_LOW);
	}
	ms->dataShift <<= 1;
	ms->bitcount++;
}

static void 
MDC_TraceProc(SigNode *sig, int value, void *clientData)
{
	MII_Slave *ms = clientData;
	int mdio = ms->sampleValMDIO;
	CycleCounter_t now = CycleCounter_Get();
	if(now - ms->edgeTime < NanosecondsToCycles(200)) {
		fprintf(stderr,"MII timing violation %u\n",(int)(now - ms->edgeTime));
		return;
	}
	ms->edgeTime = now;
	if(value == SIG_LOW) {
		/* Ignore negative edge */
		return;
	}
	switch(ms->state) {
		case STATE_IDLE:
			if(mdio == SIG_LOW) {
				if(ms->bitcount < 32) {
					fprintf(stderr,"MII frame: sync to short (%d bits)\n",
						ms->bitcount);
				}
				ms->state = STATE_ST;
			} else {
				ms->bitcount++;
			}
			break;
		case STATE_ST:
			if(mdio == SIG_HIGH) {
				ms->state = STATE_OP1;
			} else {
				ms->state = STATE_IDLE;
			}
			break;
		case STATE_OP1:
			if(mdio == SIG_HIGH) {
				ms->operation = OP_READ;
			} else {
				ms->operation = OP_WRITE;
			}
			ms->state = STATE_OP2;
			break;

		case STATE_OP2:
			if(mdio == SIG_LOW) {
				if(ms->operation != OP_READ) {
					fprintf(stderr,"MII illegal operation 00\n");
					ms->state = STATE_IDLE;
				} else {
					ms->state = STATE_PHYAD;
					ms->phyAdShift = 0;
					ms->bitcount = 0;
				}
			} else {
				if(ms->operation != OP_WRITE) {
					fprintf(stderr,"MII illegal operation 11\n");
					ms->state = STATE_IDLE;
				} else {
					ms->state = STATE_PHYAD;
					ms->phyAdShift = 0;
					ms->bitcount = 0;
				}
			}	
			break;

		case STATE_PHYAD:
			if(mdio == SIG_HIGH) {
				ms->phyAdShift = (ms->phyAdShift << 1) | 1;
			} else {
				ms->phyAdShift = (ms->phyAdShift << 1);
			}
			ms->bitcount++;
			if(ms->bitcount >= 5) {
				ms->bitcount = 0;
				ms->regAdShift = 0;
				ms->state = STATE_REGAD;
			} 
			break;

		case STATE_REGAD:
			if(mdio == SIG_HIGH) {
				ms->regAdShift = (ms->regAdShift << 1) | 1;
			} else {
				ms->regAdShift = (ms->regAdShift << 1);
			}
			ms->bitcount++;
			if(ms->bitcount >= 5) {
				ms->state = STATE_TA1;
			} 
			break;

		case STATE_TA1:
			if(ms->operation == OP_WRITE) {
				if(mdio != SIG_HIGH) {
					fprintf(stderr,"Error: MDIO not high in TA1 state\n");
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
			if(ms->operation == OP_WRITE) {
				if(mdio != SIG_LOW) {
					fprintf(stderr,"Error: MDIO not high in TA2 state\n");
					ms->state = STATE_IDLE;
				} else {
					ms->state = STATE_WRITE;
					ms->bitcount = 0;
				}
			} else {
				PHY_Device *phy;
				ms->bitcount = 0;
				phy = ms->phy;
				if(phy && (ms->phyAdShift == ms->phyAddr)) {
					PHY_ReadRegister(phy,&ms->dataShift, ms->regAdShift);
				} else {
					fprintf(stderr,"READ for nonexistent phy 0x%02x\n",ms->phyAdShift);
				}
				dbgprintf("State READ %u phy %u val 0x%04x\n",ms->regAdShift,ms->phyAdShift, ms->dataShift);
				CycleTimer_Mod(&ms->rdPosEdgeToMdioTimer,NanosecondsToCycles(300));	
				ms->state = STATE_READ;
			}
			break;

		case STATE_READ:
			if(ms->bitcount >= 16) {
				SigNode_Set(ms->sigMDIO,SIG_OPEN);
				ms->state = STATE_IDLE;
				CycleTimer_Remove(&ms->rdPosEdgeToMdioTimer);	
			} else {
				CycleTimer_Mod(&ms->rdPosEdgeToMdioTimer,NanosecondsToCycles(300));	
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
				PHY_Device *phy;
				phy = ms->phy;
				if(phy && (ms->phyAdShift == ms->phyAddr)) {
					PHY_WriteRegister(phy,ms->dataShift, ms->regAdShift);
				} else {
					fprintf(stderr,"WRITE for nonexistent phy 0x%02x\n",ms->phyAdShift);
				}
				dbgprintf("State Write %u phy %u value %04x\n",ms->regAdShift,ms->phyAdShift,ms->dataShift);
				ms->state = STATE_IDLE;
			}
			break;
	}
}

void
MIISlave_RegisterPhy(MII_Slave *ms,PHY_Device *phy,unsigned int addr) 
{
	if(addr < 32) {
		ms->phyAddr = addr;
		ms->phy = phy;
	} else {
		fprintf(stderr,"Illegal PHY address %u\n",addr);
		exit(1);
	}
}

/**
 ****************************************************************************
 * Create a state machine for eating MII signals. (PHY side)
 ****************************************************************************
 */
MII_Slave *
MIISlave_New(const char *name)
{
	MII_Slave *ms;
	ms = sg_new(MII_Slave);
	ms->sigMDC = SigNode_New("%s.mdc",name);
	ms->sigMDIO = SigNode_New("%s.mdio",name);
	ms->sampleValMDIO = SigNode_Val(ms->sigMDIO);
	ms->state = STATE_IDLE;
	SigNode_Trace(ms->sigMDC,MDC_TraceProc,ms);
	SigNode_Trace(ms->sigMDIO,MDIO_TraceProc,ms);
	CycleTimer_Init(&ms->rdPosEdgeToMdioTimer,MDIOSet,ms);
	return ms;
}
