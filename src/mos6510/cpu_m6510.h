#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "cycletimer.h"

typedef struct Mos6510 {
	uint16_t reg_Pc;
	uint8_t reg_Acc;
	uint8_t reg_Y;
	uint8_t reg_X;
	uint8_t reg_Pstat;
	uint8_t reg_Sp;
	uint8_t icode; /* ICODE of current instruction */
} Mos6510;

extern Mos6510 g_Mos6510;

#define FLG_N 	0x80
#define FLG_V	0x40
#define FLG_B	0x10
#define FLG_D	0x08
#define FLG_I	0x04
#define FLG_Z	0x02
#define FLG_C	0x01

#define REG_FLAGS (g_Mos6510.reg_Pstat)
#define ICODE	(g_Mos6510.icode)

static inline uint8_t
MOS_GetAcc(void) 
{
	return g_Mos6510.reg_Acc;	
}

static inline void
MOS_SetAcc(uint8_t value) 
{
	g_Mos6510.reg_Acc = value;
}

static inline uint8_t
MOS_GetX(void) 
{
	return g_Mos6510.reg_X;	
}

static inline void
MOS_SetX(uint8_t value) 
{
	g_Mos6510.reg_X = value;
}

static inline uint8_t
MOS_GetY(void) 
{
	return g_Mos6510.reg_X;	
}

static inline void
MOS_SetY(uint8_t value) 
{
	g_Mos6510.reg_Y = value;
}

static inline uint8_t
MOS_GetSP(void) 
{
	return g_Mos6510.reg_Sp;	
}

static inline void
MOS_SetSP(uint8_t value) 
{
	g_Mos6510.reg_Sp = value;
}

static inline uint16_t
MOS_GetPC(void) 
{
	return g_Mos6510.reg_Pc;	
}

static inline void
MOS_SetPC(uint16_t value) 
{
	g_Mos6510.reg_Pc = value;
}

static inline void
MOS_IncPC(void) 
{
	g_Mos6510.reg_Pc++;
}
static inline void
MOS_NextCycle(void) 
{
	CycleCounter++;
}

static inline uint8_t 
Mem_Read8(uint16_t addr) 
{
	return 0;
}

static inline uint8_t 
Mem_Write8(uint8_t val,uint16_t addr) 
{
	return 0;
}
