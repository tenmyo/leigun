#ifndef _CPU_MCS51_H
#define _CPU_MCS51_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "diskimage.h"
#include "throttle.h"
#include "signode.h"
#include "clock.h"

#define PSW_CY	(1<<7)
#define PSW_AC	(1<<6)
#define PSW_F0	(1<<5)
#define PSW_RS1	(1<<4)
#define PSW_RS0 (1<<3)
#define PSW_OV	(1<<2)
#define PSW_P	(1<<0)

#define MCS51_SIG_IRQ             (1)
#define MCS51_SIG_DBG             (8)
#define MCS51_SIG_RESTART_IDEC    (0x10)

#define EXMEM_MAP_ENTRY_SHIFT	(8)
#define EXMEM_MAP_ENTRIES		(256)
#define EXMEM_MAP_ENTRY_SIZE	(65536/EXMEM_MAP_ENTRIES)
#define EXMEM_MAP_ENTRY(addr)	((addr >> 8))

typedef void C51_SfrWriteProc(void *dev, uint8_t addr, uint8_t value);
typedef uint8_t C51_SfrReadProc(void *dev, uint8_t addr);

typedef void Exmem_WriteProc(void *dev, uint16_t addr, uint8_t value);
typedef uint8_t Exmem_ReadProc(void *dev, uint16_t addr);

typedef struct MCS51Cpu {
	unsigned int iplStack[10];
	unsigned int iplStackP;
	int currentIpl;
	int maxPendingIpl;
	uint16_t pendingVectAddr;
	SigNode *sigAckIntOut;

	uint32_t signals;
	uint32_t signals_raw;
	uint32_t signals_mask;
	Throttle *throttle;

	uint16_t pc;
	uint16_t dptr;
	uint8_t sp;
	uint8_t iram[256];

	C51_SfrWriteProc *sfrWrite[128];
	C51_SfrReadProc *sfrRead[128];
	C51_SfrReadProc *sfrLatchedRead[128];
	void *sfrDev[128];
	Exmem_WriteProc *exmemWriteProc[EXMEM_MAP_ENTRIES];
	Exmem_ReadProc *exmemReadProc[EXMEM_MAP_ENTRIES];
	void *exmemDev[EXMEM_MAP_ENTRIES];

	uint8_t icode;
	uint8_t *r0p;
	uint8_t psw;
	uint8_t regAcc;
	uint8_t regB;
	uint8_t *approm;
	uint32_t approm_size;
	DiskImage *flash_di;
	Clock_t *clock1;
	Clock_t *clock6;
	Clock_t *clock12;
} MCS51Cpu;

#define ICODE   (g_mcs51.icode)
#define GET_REG_PC 	(g_mcs51.pc)
#define SET_REG_PC(val) ((g_mcs51.pc) = (val))
#define PSW (g_mcs51.psw)

extern MCS51Cpu g_mcs51;

static inline void
MCS51_SetPSW(uint8_t val)
{
	g_mcs51.psw = val;
	switch (val & (PSW_RS0 | PSW_RS1)) {
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

void MCS51_RegisterSFR(uint8_t byte_addr, C51_SfrReadProc * readProc, C51_SfrReadProc * latchedRead,
		       C51_SfrWriteProc * writeProc, void *cbData);

static inline uint8_t
MCS51_ReadPgmMem(uint16_t addr)
{
	if (addr < g_mcs51.approm_size) {
		return g_mcs51.approm[addr];
	} else {
		return 0;
	}
}

static inline void
MCS51_WritePgmMem(uint8_t val, uint16_t addr)
{
	return;
}

static inline uint8_t
MCS51_ReadMemDirect(uint8_t byte_addr)
{
	if (byte_addr < 128) {
		return g_mcs51.iram[byte_addr];
	} else {
		C51_SfrReadProc *sfrRead;
		void *dev;
		dev = g_mcs51.sfrDev[byte_addr & 0x7f];
		sfrRead = g_mcs51.sfrRead[byte_addr & 0x7f];
		if (sfrRead) {
			return sfrRead(dev, byte_addr);
		} else {
			fprintf(stderr, "No handler for SFR 0x%02x\n", byte_addr);
			return 0;
		}
	}
}

static inline uint8_t
MCS51_ReadLatchedMemDirect(uint8_t byte_addr)
{
	if (byte_addr < 128) {
		return g_mcs51.iram[byte_addr];
	} else {
		C51_SfrReadProc *sfrRead;
		void *dev;
		dev = g_mcs51.sfrDev[byte_addr & 0x7f];
		sfrRead = g_mcs51.sfrLatchedRead[byte_addr & 0x7f];
		if (!sfrRead) {
			sfrRead = g_mcs51.sfrRead[byte_addr & 0x7f];
		}
		if (sfrRead) {
			return sfrRead(dev, byte_addr);
		} else {
			fprintf(stderr, "No handler for SFR 0x%02x\n", byte_addr);
			return 0;
		}
	}
}

static inline uint8_t
MCS51_ReadMemIndirect(uint8_t byte_addr)
{
	return g_mcs51.iram[byte_addr];
}

static inline void
MCS51_WriteMemDirect(uint8_t val, uint8_t byte_addr)
{
	if (byte_addr < 128) {
		g_mcs51.iram[byte_addr] = val;
	} else {
		C51_SfrWriteProc *writeProc;
		void *dev;
		writeProc = g_mcs51.sfrWrite[byte_addr & 0x7f];
		dev = g_mcs51.sfrDev[byte_addr & 0x7f];
		if (writeProc) {
			writeProc(dev, byte_addr, val);
		} else {
			fprintf(stderr, "No handler for SFR 0x%02x\n", byte_addr);
		}
	}
}

static inline void
MCS51_WriteMemIndirect(uint8_t val, uint8_t byte_addr)
{
	g_mcs51.iram[byte_addr] = val;
}

static inline uint8_t
MCS51_ReadBit(uint8_t bitaddr)
{
	uint8_t addr;
	uint8_t bit = bitaddr & 0x7;
	addr = bitaddr & ~7;
	if (addr < 128) {
		addr = (addr >> 3) + 0x20;
	}
	return (MCS51_ReadMemDirect(addr) >> bit) & 1;
}

static inline uint8_t
MCS51_ReadBitLatched(uint8_t bitaddr)
{
	uint8_t addr;
	uint8_t bit = bitaddr & 0x7;
	addr = bitaddr & ~7;
	if (addr < 128) {
		addr = (addr >> 3) + 0x20;
	}
	return (MCS51_ReadLatchedMemDirect(addr) >> bit) & 1;
}

static inline void
MCS51_WriteBit(uint8_t value, uint8_t bitaddr)
{
	uint8_t addr;
	uint8_t bit = bitaddr & 0x7;
	uint8_t data;
	addr = bitaddr & ~7;
	if (addr < 128) {
		addr = (addr >> 3) + 0x20;
	}
	data = MCS51_ReadMemDirect(addr);
	if (value) {
		data |= (1 << bit);
	} else {
		data &= ~(1 << bit);
	}
	MCS51_WriteMemDirect(data, addr);
}

static inline void
MCS51_WriteBitLatched(uint8_t value, uint8_t bitaddr)
{
	uint8_t addr;
	uint8_t bit = bitaddr & 0x7;
	uint8_t data;
	addr = bitaddr & ~7;
	if (addr < 128) {
		addr = (addr >> 3) + 0x20;
	}
	data = MCS51_ReadLatchedMemDirect(addr);
	if (value) {
		data |= (1 << bit);
	} else {
		data &= ~(1 << bit);
	}
	MCS51_WriteMemDirect(data, addr);
}

static inline uint8_t
MCS51_ReadExmem(uint16_t word_addr)
{
	unsigned int entryNr = word_addr >> EXMEM_MAP_ENTRY_SHIFT;
	if (g_mcs51.exmemReadProc[entryNr]) {
		return g_mcs51.exmemReadProc[entryNr] (g_mcs51.exmemDev[entryNr], word_addr);
	} else {
		fprintf(stderr, "Read outside of exmem: %04x\n", word_addr);
		return 0;
	}
}

static inline void
MCS51_WriteExmem(uint8_t val, uint16_t word_addr)
{
	unsigned int entryNr = word_addr >> EXMEM_MAP_ENTRY_SHIFT;
	if (g_mcs51.exmemWriteProc[entryNr]) {
		return g_mcs51.exmemWriteProc[entryNr] (g_mcs51.exmemDev[entryNr], word_addr, val);
	} else {
		fprintf(stderr, "Write outside of exmem: %04x\n", word_addr);
	}
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
MCS51_GetRegDptr(void)
{
	return g_mcs51.dptr;
}

static inline void
MCS51_SetRegDptr(uint16_t val)
{
	g_mcs51.dptr = val;
}

static inline uint8_t
MCS51_GetRegSP(void)
{
	return g_mcs51.sp;
}

static inline void
MCS51_SetRegSP(uint8_t val)
{
	g_mcs51.sp = val;
}

static inline void
MCS51_SetRegR(uint8_t val, int reg)
{
	g_mcs51.r0p[reg] = val;
}

static inline uint8_t
MCS51_GetRegR(int reg)
{
	return g_mcs51.r0p[reg];
}

static inline void
MCS51_UpdateSignals(void)
{
	//g_mcs51.signals = g_mcs51.signals_raw & g_mcs51.signals_mask;
	g_mcs51.signals = g_mcs51.signals_raw;
}

static inline void
MCS51_PostSignal(uint32_t signal)
{
	g_mcs51.signals_raw |= signal;
	MCS51_UpdateSignals();
}

static inline void
MCS51_UnpostSignal(uint32_t signal)
{
	g_mcs51.signals_raw &= ~signal;
	g_mcs51.signals = g_mcs51.signals_raw & g_mcs51.signals_mask;
}

void

MCS51_MapExmem(MCS51Cpu * mcs51, uint16_t addr, uint32_t size,
	       Exmem_ReadProc * rproc, Exmem_WriteProc * wproc, void *dev);
void MCS51_UnmapExmem(MCS51Cpu * mcs51, uint16_t addr, uint32_t size);

void MCS51_PopIpl(void);
void MCS51_PostILvl(int ilvl, uint16_t vectAddr);
void MCS51_Run();
MCS51Cpu *MCS51_Init(const char *instancename);
#endif
