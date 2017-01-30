/**
 ******************************************************************************************
 *
 ******************************************************************************************
 */
#include <unistd.h>
#include <stdint.h>
#include "bus.h"
#include "sgstring.h"
#include "mmcard.h"
#include "mmcdev.h"
#include "signode.h"
#include "clock.h"
#include "cycletimer.h"

#define REG_BMI(base) 		((base) + 0x00)
#define REG_AHBCON0(base) 	((base) + 0x04)
#define REG_APBPWE(base) 	((base) + 0x08)
#define REG_DTCMWAIT(base) 	((base) + 0x0C)
#define REG_ECCSEL(base) 	((base) + 0x10)
#define REG_AHBCON1(base) 	((base) + 0x14)
#define REG_SDHCFG(base) 	((base) + 0x18)
#define REG_REMAP(base) 	((base) + 0x20)
#define REG_LCDSIAE(base) 	((base) + 0x24)
#define REG_VERSION_EN(base)	((base) + 0x40)
#define REG_VERSION(base)	((base) + 0x4c)
#define REG_XMCCFG(base) 	((base) + 0xE0)
#define REG_IMCCFG(base) 	((base) + 0xE4)

typedef struct TCC_Sysco {
	BusDevice bdev;
	uint32_t regVersionEn;
} TCC_Sysco;

/**
 *************************************************************************************
 * REG_BMI - Boot Mode Status Register
 *************************************************************************************
 */
static uint32_t
bmi_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0x0a;
}

static void
bmi_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************************
 * REG_AHBCON0(base) 	R/W 0x00000000 AHB Control Register 0
 **********************************************************************************************
 */
static uint32_t
ahbcon0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ahbcon0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************************
 * REG_APBPWE(base) 	 R/W 0x00000000 Reserved. (APB Posted Write Enable Register)
 **********************************************************************************************
 */
static uint32_t
apbpwe_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0;
}

static void
apbpwe_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************************
 * REG_DTCMWAIT(base) 	 R/W 0x00000000 DTCM Interface Wait Control Register
 **********************************************************************************************
 */
static uint32_t
dtcmwait_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0;
}

static void
dtcmwait_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************************
 * REG_ECCSEL(base) 	 R/W 0x00000000 ECC Block Monitoring Bus Selection Register
 **********************************************************************************************
 */
static uint32_t
eccsel_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0;
}

static void
eccsel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************************
 * REG_AHBCON1(base) 	 R/W 0x000F0000 AHB Control Register 1
 **********************************************************************************************
 */
static uint32_t
ahbcon1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ahbcon1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************************
 * REG_SDHCFG(base) 	 R/W 0x00000000 SD/MMC Controller Configuration Register
 **********************************************************************************************
 */
static uint32_t
sdhcfg_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0;
}

static void
sdhcfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************************
 * REG_REMAP(base) 	 R/W - Address REMAP Register
 **********************************************************************************************
 */
static uint32_t
remap_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0;
}

static void
remap_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************************
 * REG_LCDSIAE(base) 	 R/W 0x00000000 LCDSI Access Enable Register
 **********************************************************************************************
 */
static uint32_t
lcdsiae_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0;
}

static void
lcdsiae_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

static uint32_t
version_en_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Sysco *sc = clientData;
	return sc->regVersionEn;
}

static void
version_en_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Sysco *sc = clientData;
	sc->regVersionEn = value;
}

/**
 ********************************************************************
 * \fn static uint32_t version_read(void *clientData,uint32_t address,int rqlen)
 * This is a find first nonzero bit function.
 * The register is undocumented in the manual.
 ********************************************************************
 */
static uint32_t
version_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Sysco *sc = clientData;
	int i;
	for (i = 31; i >= 0; i--) {
		if (sc->regVersionEn & (1 << i)) {
			return i + 1;
		}
	}
	return 0;
}

static void
version_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************************
 * REG_XMCCFG(base) 	 R/W - External Memory Control Configuration Register
 **********************************************************************************************
 */
static uint32_t
xmccfg_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0;
}

static void
xmccfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************************
 * REG_IMCCFG(base) 	 R/W 0x0000007F Internal Memory Control Configuration Register
 **********************************************************************************************
 */
static uint32_t
imccfg_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
	return 0;
}

static void
imccfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K Sysconfig: %s: Register not implemented\n", __func__);
}

static void
TccSysco_Map(void *owner, uint32_t base, uint32_t mask, uint32_t _flags)
{
	TCC_Sysco *sc = owner;
	IOH_New32(REG_BMI(base), bmi_read, bmi_write, sc);
	IOH_New32(REG_AHBCON0(base), ahbcon0_read, ahbcon0_write, sc);
	IOH_New32(REG_APBPWE(base), apbpwe_read, apbpwe_write, sc);
	IOH_New32(REG_DTCMWAIT(base), dtcmwait_read, dtcmwait_write, sc);
	IOH_New32(REG_ECCSEL(base), eccsel_read, eccsel_write, sc);
	IOH_New32(REG_AHBCON1(base), ahbcon1_read, ahbcon1_write, sc);
	IOH_New32(REG_SDHCFG(base), sdhcfg_read, sdhcfg_write, sc);
	IOH_New32(REG_REMAP(base), remap_read, remap_write, sc);
	IOH_New32(REG_LCDSIAE(base), lcdsiae_read, lcdsiae_write, sc);
	IOH_New32(REG_VERSION_EN(base), version_en_read, version_en_write, sc);
	IOH_New32(REG_VERSION(base), version_read, version_write, sc);
	IOH_New32(REG_XMCCFG(base), xmccfg_read, xmccfg_write, sc);
	IOH_New32(REG_IMCCFG(base), imccfg_read, imccfg_write, sc);
}

static void
TccSysco_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(REG_BMI(base));
	IOH_Delete32(REG_AHBCON0(base));
	IOH_Delete32(REG_APBPWE(base));
	IOH_Delete32(REG_DTCMWAIT(base));
	IOH_Delete32(REG_ECCSEL(base));
	IOH_Delete32(REG_AHBCON1(base));
	IOH_Delete32(REG_SDHCFG(base));
	IOH_Delete32(REG_REMAP(base));
	IOH_Delete32(REG_LCDSIAE(base));
	IOH_Delete32(REG_XMCCFG(base));
	IOH_Delete32(REG_IMCCFG(base));
}

/**
 ********************************************************************************
 * Create system configuration module.
 ********************************************************************************
 */
BusDevice *
TCC8K_SyscoNew(const char *name)
{
	TCC_Sysco *sc = sg_new(TCC_Sysco);
	sc->bdev.first_mapping = NULL;
	sc->bdev.Map = TccSysco_Map;
	sc->bdev.UnMap = TccSysco_UnMap;
	sc->bdev.owner = sc;
	sc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &sc->bdev;
}
