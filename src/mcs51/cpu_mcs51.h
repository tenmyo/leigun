#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "diskimage.h"

#define PSW_CY	(1<<7)
#define PSW_AC	(1<<6)
#define PSW_F0	(1<<5)
#define PSW_RS1	(1<<4)
#define PSW_RS0 (1<<3)
#define PSW_OV	(1<<2)
#define PSW_P	(1<<0)

typedef struct MCS51Cpu {
	uint16_t pc;
	uint16_t dptr;
	uint16_t sp;
	uint8_t iram[256];
        uint8_t icode;
	uint8_t *r0p;
	uint8_t psw;
	uint8_t regAcc;
	uint8_t regB;
	uint8_t *approm;
	int approm_size;
	DiskImage *flash_di;
} MCS51Cpu;

#define ICODE   (g_mcs51.icode)
#define GET_REG_PC 	(g_mcs51.pc)
#define SET_REG_PC(val) ((g_mcs51.pc) = (val))
#define PSW (g_mcs51.psw)

MCS51Cpu g_mcs51;

static inline void
MCS51_SetPSW(uint8_t val) {
	g_mcs51.psw = val;
	switch(val & (PSW_RS0 | PSW_RS1)) {
		case 0:
			g_mcs51.r0p = &g_mcs51.iram[0];
			break;
		case PSW_RS0:
			g_mcs51.r0p = &g_mcs51.iram[8];
			break;
		case PSW_RS1:
			g_mcs51.r0p = &g_mcs51.iram[16];
			break;
		case PSW_RS0 | PSW_RS1:
			g_mcs51.r0p = &g_mcs51.iram[24];
			break;
		default:
			break;
	}
}

static inline uint8_t
MCS51_ReadPgmMem(uint16_t addr)
{
//        return gavr8.appmem[word_addr & gavr8.appmem_word_mask];
	if(addr < g_mcs51.approm_size) {
		return g_mcs51.approm[addr];
	} else {
		return 0;
	}
}
static inline void 
MCS51_WritePgmMem(uint8_t val,uint16_t addr)
{
//        return gavr8.appmem[word_addr & gavr8.appmem_word_mask];
	return;
}
static inline uint8_t
MCS51_ReadMem(uint16_t word_addr)
{
//        return gavr8.appmem[word_addr & gavr8.appmem_word_mask];
	return 0;
}
static inline void 
MCS51_WriteMem(uint8_t val,uint16_t word_addr)
{
//        return gavr8.appmem[word_addr & gavr8.appmem_word_mask];
	return;
}
static inline uint8_t
MCS51_ReadBit(uint8_t bitaddr) 
{
	uint8_t addr = 0x20 + (bitaddr >> 3);
	uint8_t bit = bitaddr & 0x7;	
	return (MCS51_ReadMem(addr) >> bit) & 1;
}
static inline void 
MCS51_WriteBit(uint8_t value,uint8_t bitaddr) 
{
	uint8_t addr = 0x20 + (bitaddr >> 3);
	uint8_t bit = bitaddr & 0x7;	
	uint8_t data;
	data = MCS51_ReadMem(addr);
	if(value) {
		data |= (1<<bit);
	} else {
		data &= ~(1<<bit);
	}
	MCS51_WriteMem(data,addr);
}
static inline uint8_t
MCS51_ReadExmem(uint16_t word_addr)
{
//        return gavr8.appmem[word_addr & gavr8.appmem_word_mask];
	return 0;
}
static inline void 
MCS51_WriteExmem(uint8_t val,uint16_t word_addr)
{
//        return gavr8.appmem[word_addr & gavr8.appmem_word_mask];
	return;
}



static inline void
MCS51_SetAcc(uint8_t val) 
{
	g_mcs51.regAcc = val;
}

static inline uint8_t 
MCS51_GetAcc(void) 
{
	return g_mcs51.regAcc;
}

static inline uint8_t 
MCS51_GetRegB(void) 
{
	return g_mcs51.regB;
}

static inline void
MCS51_SetRegB(uint8_t value)
{
	g_mcs51.regB = value;
}

static inline uint16_t 
MCS51_GetRegDptr(void) {
	return g_mcs51.dptr;
}

static inline void 
MCS51_SetRegDptr(uint16_t val) {
	g_mcs51.dptr = val;
}

static inline uint16_t 
MCS51_GetRegSP(void) {
	return g_mcs51.sp;
}

static inline void 
MCS51_SetRegSP(uint16_t val) {
	g_mcs51.sp = val;
}

static inline void
MCS51_SetRegR(uint8_t val,int reg)
{
	g_mcs51.r0p[reg] = val;	
}

static inline uint8_t
MCS51_GetRegR(int reg)
{
	return g_mcs51.r0p[reg];
}

void MCS51_Run(); 
void MCS51_Init(const char *instancename);
