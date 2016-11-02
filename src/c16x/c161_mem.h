#include "bus.h"

#define C161_CS0 (0)
#define C161_CS1 (1)
#define C161_CS2 (2)
#define C161_CS3 (3)
#define C161_CS4 (4)

#define BUSCON_CSWEN 	(1<<15)
#define BUSCON_CSREN 	(1<<14)
#define BUSCON_RDYEN 	(1<<12)
#define BUSCON_BUSACT 	(1<<10)
#define BUSCON_ALECTL	(1<<9)
#define	BUSCON_EWEN	(1<<8)
#define	BUSCON_BTYP_SHIFT	(6)
#define	BUSCON_BTYP_MASK	(3<<6)
#define BUSCON_MTTC	(5)
#define BUSCON_RWDC	(4)
#define BUSCON_MCTC_SHIFT 	(0)
#define BUSCON_MCTC_MASK 	(0xf)

#define ADDRSEL_RGSAD_SHIFT	(4)
#define ADDRSEL_RGSAD_MASK	(0xfff0)
#define ADDRSEL_RGSZ_SHIFT	(0)
#define ADDRSEL_RGSZ_MASK	(0xf)

#define XBCON_RDYEN	(1<<12)
#define	XBCON_BSWC	(1<<11)
#define XBCON_BUSACT	(1<<10)
#define XBCON_ALECTL	(1<<9)
#define XBCON_EWEN	(1<<8)
#define XBCON_BTYP_SHIFT 	(6)
#define XBCON_BTYP_MASK		(3<<6)
#define XBCON_MTTC		(5)
#define XBCON_RWDC		(4)
#define XBCON_MCTC_SHIFT 	(0)
#define XBCON_MCTC_MASK 	(0xf)

#define XADRS_RGSAD_SHIFT	(4)
#define XADRS_RGSAD_MASK	(0xfff0)
#define XADRS_RGSZ_SHIFT	(0)
#define XADRS_RGSZ_MASK		(0xf)

typedef struct C161_Memco C161_Memco;

C161_Memco *C161_MemcoNew();
void C161_RegisterDevice(C161_Memco * memco, BusDevice * bdev, uint32_t cs);
