#include <bus.h>

void PPCMMU_InvalidateTlb();
uint64_t PPCMMU_Read64(uint32_t addr);
uint32_t PPCMMU_Read32(uint32_t addr);
uint16_t PPCMMU_Read16(uint32_t addr);
uint8_t PPCMMU_Read8(uint32_t addr);
void PPCMMU_Write64(uint64_t value, uint32_t addr);
void PPCMMU_Write32(uint32_t value, uint32_t addr);
void PPCMMU_Write16(uint16_t value, uint32_t addr);
void PPCMMU_Write8(uint8_t value, uint32_t addr);
uint32_t PPCMMU_translate_ifetch(uint32_t va);
static inline uint32_t
PPCMMU_IFetch(uint32_t va)
{
	uint32_t pa;
	pa = PPCMMU_translate_ifetch(va);
	return Bus_Read32(pa);
}
