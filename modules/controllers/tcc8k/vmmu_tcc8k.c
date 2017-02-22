#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include "bus.h"
#include "sgstring.h"
#include "compiler_extensions.h"
#include "vmmu_tcc8k.h"
#include "byteorder.h"

#define	REG_REGION(base,region)	((base) + ((region) << 2))
#define		REGION_SA_MSK 	(0xfff << 20)
#define 	REGION_SZ_MSK 	(0x1f << 12)
#define 	REGION_AP 	(3 << 10)
#define 	REGION_EN 	(1 << 9)
#define 	REGION_D_MSK	(0xf << 4)
#define 	REGION_CA 	(1 << 3)
#define 	REGION_BU 	(1 << 2)
#define	REG_PTE_ROM(base)	((base) + 0x10000)

typedef struct TccVmmu {
	BusDevice bdev;
	uint32_t regRegion[8];
	uint8_t PteVRom[16384];
} TccVmmu;

static __UNUSED__ void
dump_pte_vrom(TccVmmu * vm)
{
	int i;
	for (i = 0; i < 4096; i++) {
		uint32_t *pteP = (uint32_t *) (vm->PteVRom + (i << 2));
		uint32_t pte = BYTE_LeToH32(*pteP);
		if ((i & 7) == 0) {
			fprintf(stdout, "%03x: ", i);
		}
		fprintf(stdout, "%08x ", pte);
		if ((i & 7) == 7) {
			fprintf(stdout, "\n");
		}
		usleep(100);
	}
}

static void
update_pte_vrom(TccVmmu * vm)
{
	uint64_t i;
	uint32_t r;
	memset(vm->PteVRom, 0, sizeof(vm->PteVRom));
	for (r = 0; r < 8; r++) {
		uint32_t region = vm->regRegion[r];
		uint64_t size = UINT64_C(2) << (((region & REGION_SZ_MSK) >> 12));
		uint64_t addr = region & REGION_SA_MSK;

		fprintf(stderr, "size %08x at %08x, \n", (uint32_t) size, (uint32_t) addr);
		for (i = addr; i < (addr + size); i += (UINT64_C(1) << 20)) {
			uint32_t pte = i & REGION_SA_MSK;
			uint32_t *pteP = (uint32_t *) (vm->PteVRom + (i >> 18));
			pte |= (region & 0x0dff) | 0x12;
			*pteP = BYTE_HToLe32(pte);
		}
	}
}

static uint32_t
region_read(void *clientData, uint32_t address, int rqlen)
{
	TccVmmu *vm = clientData;
	int region = (address >> 2) & 7;
	return vm->regRegion[region];
}

static void
region_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccVmmu *vm = clientData;
	int region = (address >> 2) & 7;
	vm->regRegion[region] = value;
	update_pte_vrom(vm);
#if 0
	if (region == 7) {
		dump_pte_vrom(vm);
	}
#endif
}

static void
TccVmmu_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	TccVmmu *vm = owner;
	int i;
	uint32_t mapsize = 16384;
	flags &= MEM_FLAG_READABLE;
	Mem_MapRange(REG_PTE_ROM(base), vm->PteVRom, 16384, mapsize, flags);
	for (i = 0; i < 8; i++) {
		IOH_New32(REG_REGION(base, i), region_read, region_write, vm);
	}
}

static void
TccVmmu_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	uint32_t mapsize = 16384;
	int i;
	Mem_UnMapRange(REG_PTE_ROM(base), mapsize);
	for (i = 0; i < 8; i++) {
		IOH_Delete32(REG_REGION(base, i));
	}
}

BusDevice *
TCC8K_VmmuNew(const char *name)
{
	TccVmmu *vm = sg_new(TccVmmu);
	vm->bdev.first_mapping = NULL;
	vm->bdev.Map = TccVmmu_Map;
	vm->bdev.UnMap = TccVmmu_UnMap;
	vm->bdev.owner = vm;
	vm->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &vm->bdev;
}
