/**
 *************************************************************
 *
 *************************************************************
 */

#include "pio_at89c51.h"
#include "signode.h"
#include "cpu_mcs51.h"
#include "sgstring.h"

#define REG_P0	0x80
#define REG_P1	0x90
#define	REG_P2	0xa0
#define REG_P3	0xb0
#define REG_P4	0xc0

typedef struct Pio {
	SigNode *sigP0[8];
	SigNode *sigP1[8];
	SigNode *sigP2[8];
	SigNode *sigP3[8];
	SigNode *sigP4[8];
	uint8_t latchP0;
	uint8_t latchP1;
	uint8_t latchP2;
	uint8_t latchP3;
	uint8_t latchP4;
} Pio;

static uint8_t
p0_read(void *eventData, uint8_t addr)
{
	Pio *pio = eventData;
	int i;
	uint8_t value = 0;
	for (i = 0; i < 8; i++) {
		if (SigNode_Val(pio->sigP0[i]) == SIG_HIGH) {
			value |= (1 << i);
		}
	}
	return value;
}

static uint8_t
p0_read_latched(void *eventData, uint8_t addr)
{
	Pio *pio = eventData;
	return pio->latchP0;
}

static void
p0_write(void *eventData, uint8_t addr, uint8_t value)
{
	Pio *pio = eventData;
	int i;
	for (i = 0; i < 8; i++) {
		if ((value >> i) & 1) {
			SigNode_Set(pio->sigP0[i], SIG_PULLUP);
		} else {
			SigNode_Set(pio->sigP0[i], SIG_LOW);
		}
	}
	pio->latchP0 = value;
}

static uint8_t
p1_read(void *eventData, uint8_t addr)
{
	Pio *pio = eventData;
	int i;
	uint8_t value = 0;
	for (i = 0; i < 8; i++) {
		if (SigNode_Val(pio->sigP1[i]) == SIG_HIGH) {
			value |= (1 << i);
		}
	}
	return value;
}

static uint8_t
p1_read_latched(void *eventData, uint8_t addr)
{
	Pio *pio = eventData;
	return pio->latchP1;
}

static void
p1_write(void *eventData, uint8_t addr, uint8_t value)
{
	Pio *pio = eventData;
	int i;
	//fprintf(stderr,"P1: %02x\n",value);
	//usleep(100000);
	for (i = 0; i < 8; i++) {
		if ((value >> i) & 1) {
			SigNode_Set(pio->sigP1[i], SIG_PULLUP);
		} else {
			SigNode_Set(pio->sigP1[i], SIG_LOW);
		}
	}
	pio->latchP1 = value;
}

static uint8_t
p2_read(void *eventData, uint8_t addr)
{
	Pio *pio = eventData;
	int i;
	uint8_t value = 0;
	for (i = 0; i < 8; i++) {
		if (SigNode_Val(pio->sigP2[i]) == SIG_HIGH) {
			value |= (1 << i);
		}
	}
	return value;
}

static uint8_t
p2_read_latched(void *eventData, uint8_t addr)
{
	Pio *pio = eventData;
	return pio->latchP2;
}

static void
p2_write(void *eventData, uint8_t addr, uint8_t value)
{
	Pio *pio = eventData;
	int i;
	for (i = 0; i < 8; i++) {
		if ((value >> i) & 1) {
			SigNode_Set(pio->sigP2[i], SIG_PULLUP);
		} else {
			SigNode_Set(pio->sigP2[i], SIG_LOW);
		}
	}
	pio->latchP2 = value;
}

static uint8_t
p3_read(void *eventData, uint8_t addr)
{
	Pio *pio = eventData;
	int i;
	uint8_t value = 0;
	for (i = 0; i < 8; i++) {
		if (SigNode_Val(pio->sigP3[i]) == SIG_HIGH) {
			value |= (1 << i);
		}
	}
	return value;
}

static uint8_t
p3_read_latched(void *eventData, uint8_t addr)
{
	Pio *pio = eventData;
	return pio->latchP3;
}

static void
p3_write(void *eventData, uint8_t addr, uint8_t value)
{
	Pio *pio = eventData;
	int i;
	if(pio->latchP3 != value) {
		fprintf(stderr, "P3: %02x\n", value);
	}
	//usleep(100000);
	for (i = 0; i < 8; i++) {
		if ((value >> i) & 1) {
			SigNode_Set(pio->sigP3[i], SIG_PULLUP);
		} else {
			SigNode_Set(pio->sigP3[i], SIG_LOW);
		}
	}
	pio->latchP3 = value;
}

static uint8_t
p4_read(void *eventData, uint8_t addr)
{
	Pio *pio = eventData;
	int i;
	uint8_t value = 0;
	for (i = 0; i < 4; i++) {
		if (SigNode_Val(pio->sigP4[i]) == SIG_HIGH) {
			value |= (1 << i);
		}
	}
	return value;
}

static uint8_t
p4_read_latched(void *eventData, uint8_t addr)
{
	Pio *pio = eventData;
	return pio->latchP4;
}

static void
p4_write(void *eventData, uint8_t addr, uint8_t value)
{
	Pio *pio = eventData;
	int i;
	for (i = 0; i < 4; i++) {
		if ((value >> i) & 1) {
			SigNode_Set(pio->sigP4[i], SIG_PULLUP);
		} else {
			SigNode_Set(pio->sigP4[i], SIG_LOW);
		}
	}
	pio->latchP4 = value;
}

void
AT89C51Pio_New(const char *name)
{
	int i;
	Pio *pio = sg_new(Pio);
	pio->latchP0 = 0xff;
	pio->latchP1 = 0xff;
	pio->latchP2 = 0xff;
	pio->latchP3 = 0xff;
	pio->latchP4 = 0xff;
	for (i = 0; i < 8; i++) {
		pio->sigP0[i] = SigNode_New("%s.P0.%u", name, i);
		pio->sigP1[i] = SigNode_New("%s.P1.%u", name, i);
		pio->sigP2[i] = SigNode_New("%s.P2.%u", name, i);
		pio->sigP3[i] = SigNode_New("%s.P3.%u", name, i);
		if (i < 4) {
			pio->sigP4[i] = SigNode_New("%s.P4.%u", name, i);
		} else {
			pio->sigP4[i] = SigNode_New("%s.P4.dummy%u", name, i);
		}
		if (!pio->sigP0[i] || !pio->sigP1[i] || !pio->sigP2[i] || !pio->sigP3[i]) {
			fprintf(stderr, "Failed to create Signal for PIO\n");
			exit(1);
		}
		if (!pio->sigP4[i]) {
			fprintf(stderr, "Failed to create Signal for PIO\n");
			exit(1);
		}
		SigNode_Set(pio->sigP0[i], SIG_OPEN);
		SigNode_Set(pio->sigP1[i], SIG_PULLUP);
		SigNode_Set(pio->sigP2[i], SIG_OPEN);
		SigNode_Set(pio->sigP3[i], SIG_PULLUP);
		if (i < 4) {
			SigNode_Set(pio->sigP4[i], SIG_PULLUP);
		}
	}
	MCS51_RegisterSFR(REG_P0, p0_read, p0_read_latched, p0_write, pio);
	MCS51_RegisterSFR(REG_P1, p1_read, p1_read_latched, p1_write, pio);
	MCS51_RegisterSFR(REG_P2, p2_read, p2_read_latched, p2_write, pio);
	MCS51_RegisterSFR(REG_P3, p3_read, p3_read_latched, p3_write, pio);
	MCS51_RegisterSFR(REG_P4, p4_read, p4_read_latched, p4_write, pio);
}
