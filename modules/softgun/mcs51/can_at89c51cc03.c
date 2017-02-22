#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "cpu_mcs51.h"
#include "signode.h"
#include "clock.h"
#include "can_at89c51cc03.h"
#include "cycletimer.h"
#include "inttypes.h"

#define REG_CANGCON		0xAB
#define REG_CANGSTA		0xaa
#define REG_CANGIT		0x9b
#define REG_CANTEC		0x9c
#define REG_CANREC		0x9d
#define REG_CANGIE		0xc1
#define	REG_CANEN1		0xce
#define REG_CANEN2		0xcf
#define REG_CANSIT1		0xba
#define REG_CANSIG2		0xbb
#define REG_CANIE1		0xc2
#define REG_CANIE2		0xc3
#define REG_CANBT1		0xb4
#define REG_CANBT2		0xb5
#define	REG_CANBT3		0xb6
#define REG_CANPAGE		0xb1
#define REG_CANCONCH	0xb3
#define REG_CANSTCH		0xb2
#define REG_CANIDT1		0xbc
#define REG_CANIDT2		0xbd
#define REG_CANIDT3		0xbe
#define REG_CANIDT4		0xbf
#define REG_CANIDM1		0xc4
#define REG_CANIDM2		0xc5
#define REG_CANIDM3		0xc6
#define REG_CANIDM4		0xc7
#define REG_CANMSG		0xa3
#define REG_CANTCON		0xa1
#define REG_CANTIMH		0xad
#define REG_CANTIML		0xac
#define REG_CANSTMPH	0xaf
#define REG_CANSTMPL	0xae
#define REG_CANTTCH		0xa5
#define REG_CANTTCL		0xa4

static uint8_t
canttcl_read(void *eventData, uint8_t addr)
{
	return 0;
}

static void
canttcl_write(void *eventData, uint8_t addr, uint8_t value)
{
	static CycleCounter_t last;
	fprintf(stderr, "CANTTCL 0x%02x %" PRId64 "\n", value,CyclesToMilliseconds(CycleCounter_Get() - last));
	last = CycleCounter_Get();
}

void
AT89C51CAN_New(const char *name)
{
	MCS51_RegisterSFR(REG_CANTTCL, canttcl_read, NULL, canttcl_write, NULL);
}
