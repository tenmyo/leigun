#include <stdint.h>

uint64_t MIPS64MMU_Translate(uint64_t addr);

static inline uint32_t 
MIPS64MMU_FetchIcode(uint64_t vaddr) 
{
	//uint64_t paddr = MIPS64MMU_Translate(vaddr);	
	//Translate to hva
	//Bus64_Read32(vaddr);
	return 0;
}

static inline uint8_t
MIPS64MMU_Read8(uint64_t vaddr) 
{
	return 0;
}

static inline uint16_t
MIPS64MMU_Read16(uint64_t vaddr) 
{

	return 0;
}
static inline uint32_t
MIPS64MMU_Read32(uint64_t vaddr) 
{
	return 0;
}
static inline uint64_t
MIPS64MMU_Read64(uint64_t vaddr) 
{
	return 0;
}
static inline void 
MIPS64MMU_Write8(uint8_t value,uint64_t vaddr) 
{

}

static inline void
MIPS64MMU_Write16(uint16_t value,uint64_t vaddr) 
{

}

static inline void
MIPS64MMU_Write32(uint32_t value,uint64_t vaddr) 
{

}
static inline void 
MIPS64MMU_Write64(uint64_t value,uint64_t vaddr) 
{

}
