/*
 * ---------------------------------------------------------------------------
 * Emulation of FBDD-02-02 specific functions
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "signode.h"
#include "fbdd_02_02.h"
#include "sgstring.h"

#define CPLD_LINKMODE   (0)
#define CPLD_FANFAIL1_4 (1)
#define CPLD_FANFAIL5_7 (2)
#define CPLD_LOSLOL     (3)
#define CPLD_TXON       (4)

#define EOSC_LINK_MODE_CONT             (0)
#define EOSC_LINK_MODE_TEST_PULSE       (1)
#define EOSC_LINK_MODE_TEST_PULSE2      (2)
#define EOSC_LINK_MODE_OFF              (3)

#define TX2_ON          (1<<1)
#define TX1_ON          (1<<0)

typedef struct FbddCpld {
	SigNode *addatNode[4];
	SigNode *dirNode;
	SigNode *clkNode;
	SigTrace *clkTrace;
	SigTrace *dirTrace;
	int clk_oldval;
	uint8_t data;
	uint8_t address;

	/* Registers in Altera */
	uint8_t linkmode;
	uint8_t fanfail1_4;
	uint8_t fanfail5_7;
	uint8_t loslol;
	uint8_t txon;
} FbddCpld;

uint8_t
read_cpld(FbddCpld * fcpl, int address)
{
	switch (address) {
	    case CPLD_LINKMODE:
		    return fcpl->linkmode;
		    break;
	    case CPLD_TXON:
		    return fcpl->txon;
		    break;

	    case CPLD_FANFAIL1_4:
		    return fcpl->fanfail1_4;

	    case CPLD_FANFAIL5_7:
		    return fcpl->fanfail5_7;

	    case CPLD_LOSLOL:
		    return fcpl->loslol;

	    default:
		    return 0;
		    //fprintf(stderr,"reg %d not implemented in fbdd_02_02\n",address);
	}
	return 0;
}

void
write_cpld(FbddCpld * fcpl, uint8_t value, int address)
{
	switch (address) {
	    case CPLD_LINKMODE:
		    {
			    uint8_t linkmode1 = (value & 3);
			    uint8_t linkmode2 = (value >> 2) & 3;
			    fcpl->linkmode = value;
			    switch (linkmode1) {
				case EOSC_LINK_MODE_CONT:
				case EOSC_LINK_MODE_TEST_PULSE:
				case EOSC_LINK_MODE_TEST_PULSE2:
					fcpl->txon |= TX1_ON;
					break;
				case EOSC_LINK_MODE_OFF:
					fcpl->txon &= ~TX1_ON;
					break;
			    }
			    switch (linkmode2) {
				case EOSC_LINK_MODE_CONT:
				case EOSC_LINK_MODE_TEST_PULSE:
				case EOSC_LINK_MODE_TEST_PULSE2:
					fcpl->txon |= TX2_ON;
					break;
				case EOSC_LINK_MODE_OFF:
					fcpl->txon &= ~TX2_ON;
					break;
			    }
		    }
		    break;
	    case CPLD_TXON:
		    //fcpl->txon = value;   
		    break;

	    case CPLD_FANFAIL1_4:
	    case CPLD_FANFAIL5_7:
	    case CPLD_LOSLOL:
		    fprintf(stderr, "reg %d in fbdd_02_02 cpld not writable\n", address);
		    break;
	}
}

uint8_t
read_addat(FbddCpld * fcpl)
{
	int i, val;
	uint8_t addat = 0;
	for (i = 0; i < 4; i++) {
		val = SigNode_Val(fcpl->addatNode[i]);
		if (val == SIG_HIGH || val == SIG_PULLUP) {
			addat |= (1 << i);
		}
	}
	return addat;
}

void
write_addat(FbddCpld * fcpl, uint8_t data)
{
	int i;
	for (i = 0; i < 4; i++) {
		if (data & (1 << i)) {
			SigNode_Set(fcpl->addatNode[i], SIG_HIGH);
		} else {
			SigNode_Set(fcpl->addatNode[i], SIG_LOW);
		}
	}
}

/*
 * ------------------------------------------------------------
 * ------------------------------------------------------------
 */
static void
clk_change(SigNode * node, int value, void *clientData)
{
	FbddCpld *fcpl = (FbddCpld *) clientData;
	int old = SIG_OPEN, new = SIG_OPEN;
	if ((fcpl->clk_oldval == SIG_LOW) || (fcpl->clk_oldval == SIG_PULLDOWN)) {
		old = SIG_LOW;
	} else if ((fcpl->clk_oldval == SIG_HIGH) || (fcpl->clk_oldval == SIG_PULLUP)) {
		old = SIG_HIGH;
	}
	if ((value == SIG_LOW) || (value == SIG_PULLDOWN)) {
		new = SIG_LOW;
	} else if ((value == SIG_HIGH) || (value == SIG_PULLUP)) {
		new = SIG_HIGH;
	}
	if ((new == SIG_HIGH) && (old == SIG_LOW)) {
		int dir = SigNode_Val(fcpl->dirNode);
		if ((dir == SIG_LOW) || (dir == SIG_PULLDOWN)) {
			fcpl->data = read_addat(fcpl);
			write_cpld(fcpl, fcpl->data, fcpl->address);
			//fprintf(stdout,"Altin:  %02x\n",fcpl->data);
		} else if ((dir == SIG_HIGH) || (dir == SIG_PULLUP)) {
			fcpl->data = read_cpld(fcpl, fcpl->address);
			write_addat(fcpl, fcpl->data);
			//fprintf(stdout,"Altout:  %02x\n",fcpl->data);
		}
	} else if ((new == SIG_LOW) && (old == SIG_HIGH)) {
		fcpl->address = read_addat(fcpl);
		//fprintf(stdout,"Emu: addr %02x\n",fcpl->address);
	}
	fcpl->clk_oldval = value;
}

static void
dir_change(SigNode * node, int value, void *clientData)
{
	FbddCpld *fcpl = (FbddCpld *) clientData;
	int i;
	if ((value == SIG_LOW) || (value == SIG_PULLDOWN)) {
		//fprintf(stdout,"Emu: Open addat\n"); // jk
		for (i = 0; i < 4; i++) {
			SigNode_Set(fcpl->addatNode[i], SIG_OPEN);
		}
	} else if ((value == SIG_HIGH) || (value == SIG_PULLUP)) {
		//fprintf(stdout,"Emu: UNOpen addat\n"); // jk
		write_addat(fcpl, fcpl->data);
	} else {
		//fprintf(stdout,"Emu: Shit DIR signal is not high and not low\n"); // jk
	}
}

/*
 * -----------------------------------------------
 * Void until i have handle
 * -----------------------------------------------
 */
void
FbddCpld_New(const char *name)
{
	int i;
	FbddCpld *fcpl;
	fcpl = sg_new(FbddCpld);
	for (i = 0; i < 4; i++) {
		fcpl->addatNode[i] = SigNode_New("%s.addat%d", name, i);
		if (!fcpl->addatNode[i]) {
			fprintf(stderr, "Can not Address/Data node for FBDD cpld\n");
			exit(41);
		}
	}
	fcpl->clkNode = SigNode_New("%s.clk", name);
	if (!fcpl->clkNode) {
		fprintf(stderr, "Can not create clock node for FBDD cpld\n");
		exit(432);
	}
	fcpl->clkTrace = SigNode_Trace(fcpl->clkNode, clk_change, fcpl);
	fcpl->dirNode = SigNode_New("%s.dir", name);
	if (!fcpl->dirNode) {
		fprintf(stderr, "Can not create direction node for FBDD cpld\n");
		exit(43);
	}
	fcpl->dirTrace = SigNode_Trace(fcpl->dirNode, dir_change, fcpl);
	fcpl->fanfail1_4 = 5;
	fcpl->fanfail5_7 = 0xa;
	fcpl->linkmode = EOSC_LINK_MODE_TEST_PULSE | (EOSC_LINK_MODE_TEST_PULSE << 2);
	return;
}

#ifdef _SHARED_
void
_init()
{
	fprintf(stderr, "FBDD_02_02 emulation module loaded, built %s %s\n", __DATE__, __TIME__);
}
#endif
