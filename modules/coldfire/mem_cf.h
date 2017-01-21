#ifndef _MEM_CF_H
#define _MEM_CF_H
#include <bus.h>
#include <stdint.h>

static inline uint8_t
CF_MemRead8(uint32_t addr)
{
	return Bus_Read8(addr);
}

static inline uint16_t
CF_MemRead16(uint32_t addr)
{
	return Bus_Read16(addr);
}

static inline uint32_t
CF_MemRead32(uint32_t addr)
{
	return Bus_Read32(addr);
}

static inline void
CF_MemWrite8(uint8_t value, uint32_t addr)
{
	Bus_Write8(value, addr);
}

static inline void
CF_MemWrite16(uint16_t value, uint32_t addr)
{
	Bus_Write16(value, addr);
}

static inline void
CF_MemWrite32(uint32_t value, uint32_t addr)
{
	fprintf(stderr, "Write %08x to %08x\n", value, addr);
	Bus_Write32(value, addr);
}
#endif
