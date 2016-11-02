#ifndef _MMU_SH4_H
#define _MMU_SH4_H
#include <stdint.h>
#include <bus.h>

uint32_t SH4_TranslateUtlb(uint32_t va,unsigned int op_write);
uint32_t SH4_TranslateItlb(uint32_t va);

BusDevice * SH4MMU_New(const char *name);

static inline uint8_t 
SH4_MMURead8(uint32_t va) 
{
	uint32_t pa = SH4_TranslateUtlb(va,0);
	return Bus_Read8(pa); 
} 

static inline uint16_t 
SH4_MMURead16(uint32_t va) 
{
	uint32_t pa = SH4_TranslateUtlb(va,0);
	return Bus_Read16(pa); 
} 

static inline uint16_t 
SH4_MMUIFetch(uint32_t addr) 
{
	return Bus_Read16(SH4_TranslateItlb(addr)); 
} 

static inline uint32_t 
SH4_MMURead32(uint32_t va) 
{
	uint32_t pa = SH4_TranslateUtlb(va,0);
	return Bus_Read32(pa); 
} 

static inline uint64_t 
SH4_MMURead64(uint32_t va) 
{
	uint32_t pa = SH4_TranslateUtlb(va,0);
	return Bus_Read64(pa); 
} 

static inline void
SH4_MMUWrite8(uint8_t value,uint32_t va)
{
	uint32_t pa = SH4_TranslateUtlb(va,1);
	Bus_Write8(value,pa); 
}

static inline void
SH4_MMUWrite16(uint16_t value,uint32_t va)
{
	uint32_t pa = SH4_TranslateUtlb(va,1);
	Bus_Write16(value,pa); 
}

static inline void
SH4_MMUWrite32(uint32_t value,uint32_t va)
{
	uint32_t pa = SH4_TranslateUtlb(va,1);
	Bus_Write32(value,pa); 
}
static inline void
SH4_MMUWrite64(uint64_t value,uint32_t va)
{
	uint32_t pa = SH4_TranslateUtlb(va,1);
	Bus_Write64(value,pa); 
}
#endif
