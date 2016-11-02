/**
 *****************************************************************
 * Bus Matrix
 *****************************************************************
 */

#include <stdint.h>
#include "sglib.h"
#include "sgstring.h"
#include "bus.h"
#include "at91_matrix.h"

#define MATRIX_MCFG0(base)	((base) + 0)
#define MATRIX_MCFG1(base) 	((base) + 4)
#define MATRIX_MCFG2(base)	((base) + 8)
#define MATRIX_MCFG3(base)	((base) + 0xc)
#define MATRIX_MCFG4(base)	((base) + 0x10)
#define MATRIX_MCFG5(base)	((base) + 0x14)
#define MATRIX_MCFG6(base)	((base) + 0x18)
#define MATRIX_MCFG7(base)	((base) + 0x1c)
#define MATRIX_MCFG8(base)	((base) + 0x20)
#define MATRIX_SCFG0(base)	((base) + 0x40)
#define MATRIX_SCFG1(base)	((base) + 0x44)
#define MATRIX_SCFG2(base)	((base) + 0x48)
#define MATRIX_SCFG3(base)	((base) + 0x4c)
#define MATRIX_SCFG4(base)	((base) + 0x50)
#define MATRIX_SCFG5(base)	((base) + 0x54)
#define MATRIX_SCFG6(base)	((base) + 0x58)
#define MATRIX_PRAS0(base)	((base) + 0x80)
#define MATRIX_PRBS0(base)	((base) + 0x84)
#define MATRIX_PRAS1(base)	((base) + 0x88)
#define MATRIX_PRBS1(base)	((base) + 0x8c)
#define MATRIX_PRAS2(base)	((base) + 0x90)
#define MATRIX_PRBS2(base)	((base) + 0x94)
#define MATRIX_PRAS3(base)	((base) + 0x98)
#define MATRIX_PRBS3(base)	((base) + 0x9c)
#define MATRIX_PRAS4(base)	((base) + 0xa0)
#define MATRIX_PRBS4(base)	((base) + 0xa4)
#define MATRIX_PRAS5(base)	((base) + 0xa8)
#define MATRIX_PRBS5(base)	((base) + 0xac)
#define MATRIX_PRAS6(base)	((base) + 0xb0)
#define MATRIX_PRBS6(base)	((base) + 0xb4)
#define MATRIX_MRCR(base)	((base) + 0x100)

typedef struct AT91Matrix {
	BusDevice bdev;
	BusDevice *sram;
	BusDevice *irom;
	char *name;
	uint32_t regMRCR;
} AT91Matrix;

static void
update_mapping(AT91Matrix * ma)
{
	switch (ma->regMRCR & 3) {
	    case 0:
		    Mem_AreaDeleteMappings(ma->sram);
		    Mem_AreaDeleteMappings(ma->irom);
		    Mem_AreaAddMapping(ma->irom, 0x00000000, 0x00020000,
				       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		    Mem_AreaAddMapping(ma->sram, 0x00300000, 0x00014000,
				       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		    Mem_AreaAddMapping(ma->irom, 0x00400000, 0x00020000,
				       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		    break;

	    case 3:
		    Mem_AreaDeleteMappings(ma->sram);
		    Mem_AreaDeleteMappings(ma->irom);
		    Mem_AreaAddMapping(ma->sram, 0x00000000, 0x00020000,
				       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		    Mem_AreaAddMapping(ma->sram, 0x00300000, 0x00014000,
				       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		    Mem_AreaAddMapping(ma->irom, 0x00400000, 0x00020000,
				       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		    break;
	    case 1:
	    case 2:
		    fprintf(stderr, "Mapping not supported\n");
		    break;
	}
}

static uint32_t
mcfg_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%sX is not implemented\n", __func__);
	return 0;
}

static void
mcfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Memcfg write %08x\n", value);
	//exit(1);
	fprintf(stderr, "%sX is not implemented\n", __func__);
}

static uint32_t
scfg_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%sX is not implemented\n", __func__);
	return 0;
}

static void
scfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%sX is not implemented\n", __func__);
}

static uint32_t
prxsy_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%sX is not implemented\n", __func__);
	return 0;
}

static void
prxsy_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%sX is not implemented\n", __func__);
}

static uint32_t
mrcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s is not implemented\n", __func__);
	return 0;
}

static void
mrcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Matrix *ma = clientData;
	fprintf(stderr, "%s is not implemented\n", __func__);
	fprintf(stderr, "value %08x\n", value);
	ma->regMRCR = value & 0xff;
	update_mapping(ma);

//      exit(1);
}

static void
AT91Matrix_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Matrix *ma = (AT91Matrix *) owner;
	IOH_New32(MATRIX_MCFG0(base), mcfg_read, mcfg_write, ma);
	IOH_New32(MATRIX_MCFG1(base), mcfg_read, mcfg_write, ma);
	IOH_New32(MATRIX_MCFG2(base), mcfg_read, mcfg_write, ma);
	IOH_New32(MATRIX_MCFG3(base), mcfg_read, mcfg_write, ma);
	IOH_New32(MATRIX_MCFG4(base), mcfg_read, mcfg_write, ma);
	IOH_New32(MATRIX_MCFG5(base), mcfg_read, mcfg_write, ma);
	IOH_New32(MATRIX_MCFG6(base), mcfg_read, mcfg_write, ma);
	IOH_New32(MATRIX_MCFG7(base), mcfg_read, mcfg_write, ma);
	IOH_New32(MATRIX_MCFG8(base), mcfg_read, mcfg_write, ma);
	IOH_New32(MATRIX_SCFG0(base), scfg_read, scfg_write, ma);
	IOH_New32(MATRIX_SCFG1(base), scfg_read, scfg_write, ma);
	IOH_New32(MATRIX_SCFG2(base), scfg_read, scfg_write, ma);
	IOH_New32(MATRIX_SCFG3(base), scfg_read, scfg_write, ma);
	IOH_New32(MATRIX_SCFG4(base), scfg_read, scfg_write, ma);
	IOH_New32(MATRIX_SCFG5(base), scfg_read, scfg_write, ma);
	IOH_New32(MATRIX_SCFG6(base), scfg_read, scfg_write, ma);
	IOH_New32(MATRIX_PRAS0(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRBS0(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRAS1(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRBS1(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRAS2(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRBS2(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRAS3(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRBS3(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRAS4(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRBS4(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRAS5(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRBS5(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRAS6(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_PRBS6(base), prxsy_read, prxsy_write, ma);
	IOH_New32(MATRIX_MRCR(base), mrcr_read, mrcr_write, ma);
}

static void
AT91Matrix_UnMap(void *owner, uint32_t base, uint32_t mapsize)
{
	IOH_Delete32(MATRIX_MCFG0(base));
	IOH_Delete32(MATRIX_MCFG1(base));
	IOH_Delete32(MATRIX_MCFG2(base));
	IOH_Delete32(MATRIX_MCFG3(base));
	IOH_Delete32(MATRIX_MCFG4(base));
	IOH_Delete32(MATRIX_MCFG5(base));
	IOH_Delete32(MATRIX_MCFG6(base));
	IOH_Delete32(MATRIX_MCFG7(base));
	IOH_Delete32(MATRIX_MCFG8(base));
	IOH_Delete32(MATRIX_SCFG0(base));
	IOH_Delete32(MATRIX_SCFG1(base));
	IOH_Delete32(MATRIX_SCFG2(base));
	IOH_Delete32(MATRIX_SCFG3(base));
	IOH_Delete32(MATRIX_SCFG4(base));
	IOH_Delete32(MATRIX_SCFG5(base));
	IOH_Delete32(MATRIX_SCFG6(base));
	IOH_Delete32(MATRIX_PRAS0(base));
	IOH_Delete32(MATRIX_PRBS0(base));
	IOH_Delete32(MATRIX_PRAS1(base));
	IOH_Delete32(MATRIX_PRBS1(base));
	IOH_Delete32(MATRIX_PRAS2(base));
	IOH_Delete32(MATRIX_PRBS2(base));
	IOH_Delete32(MATRIX_PRAS3(base));
	IOH_Delete32(MATRIX_PRBS3(base));
	IOH_Delete32(MATRIX_PRAS4(base));
	IOH_Delete32(MATRIX_PRBS4(base));
	IOH_Delete32(MATRIX_PRAS5(base));
	IOH_Delete32(MATRIX_PRBS5(base));
	IOH_Delete32(MATRIX_PRAS6(base));
	IOH_Delete32(MATRIX_PRBS6(base));
	IOH_Delete32(MATRIX_MRCR(base));
}

BusDevice *
AT91Matrix_New(const char *name, BusDevice * sram, BusDevice * irom)
{
	AT91Matrix *ma = sg_new(AT91Matrix);
	if ((sram == NULL) || (irom == NULL)) {
		fprintf(stderr, "AT91Matrix called without boot devices\n");
		exit(1);
	}
	ma->sram = sram;
	ma->irom = irom;
	ma->name = strdup(name);
	ma->bdev.first_mapping = NULL;
	ma->bdev.Map = AT91Matrix_Map;
	ma->bdev.UnMap = AT91Matrix_UnMap;
	ma->bdev.owner = ma;
	ma->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	ma->regMRCR = 0;
	update_mapping(ma);
	return &ma->bdev;
}
