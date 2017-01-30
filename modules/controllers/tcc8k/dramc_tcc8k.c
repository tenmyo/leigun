#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "dramc_tcc8k.h"
#include "serial.h"
#include "cycletimer.h"

#define REG0_CONTROL(base) 	((base) + 0x000)
#define REG1_STATUS(base)	((base) + 0x004)
#define REG2_REFTIMER(base)  	((base) + 0x008)
#define REG3_WRBFTMOUT(base)	((base) + 0x00C)
#define REG4_PDEMR(base)	((base) + 0x010)
#define REG5_MRS(base)		((base) + 0x014)
#define	REG6_EMRS(base)		((base) + 0x018)
#define REG7_TRCD(base)		((base) + 0x100)
#define REG8_TRP(base)		((base) + 0x104)
#define REG9_TRC(base)		((base) + 0x108)
#define REG10_TRAS(base)	((base) + 0x10c)
#define REG11_CAS(base)		((base) + 0x110)
#define REG12_TRFC(base)	((base) + 0x114)
#define REG13_TMRD(base)	((base) + 0x118)
#define REG14_TWR(base)		((base) + 0x11c)
#define REG15_TWTR(base)	((base) + 0x120)
#define REG16_TXSNR(base)	((base) + 0x124)
#define REG17_TXSRD(base)	((base) + 0x128)
#define REG18_TXP(base)		((base) + 0x12c)
#define REG19_MEMCFG(base)	((base) + 0x200)
#define REG20_BCR0(base)	((base) + 0x204)
#define REG21_BCR1(base)	((base) + 0x208)
#define REG24_PHYCR(base)	((base) + 0x300)
#define REG25_PHYPDCR(base)	((base) + 0x304)
#define 	PHYPDCR_PDFL		(1 << 3)
#define		PHYPDCR_PDCL		(1 << 2)
#define		PHYPDCR_PDSTART		(1 << 1)
#define		PHYPDCR_PDEN		(1 << 0)
#define REG26_PHYPDCFGR(base)	((base) + 0x308)
#define REG27_PHYGCR(base)	((base) + 0x30c)
#define REG28_PHYRSLC0CR(base)	((base) + 0x310)
#define REG29_PHYRSLC1CR(base)	((base) + 0x314)
#define REG36_PHYCLKDELCR(base)	((base) + 0x330)
#define REG37_PHYDLLLCKVFCR(base)	((base) + (0x334))

typedef struct TCC_Dramc {
	BusDevice bdev;
	uint32_t regControl;
	uint32_t regStatus;
	uint32_t regReftimer;
	uint32_t regWrbftmout;
	uint32_t regPdemr;
	uint32_t regMrs;
	uint32_t regEmrs;
	uint32_t regTrcd;
	uint32_t regTrp;
	uint32_t regTrc;
	uint32_t regTras;
	uint32_t regCas;
	uint32_t regTrfc;
	uint32_t regTmrd;
	uint32_t regTwr;
	uint32_t regTwtr;
	uint32_t regTxsnr;
	uint32_t regTxsrd;
	uint32_t regTxp;
	uint32_t regMemcfg;
	uint32_t regBcr0;
	uint32_t regBcr1;
	uint32_t regPhycr;
	uint32_t regPhypdcr;
	uint32_t regPhypdcfgr;
	uint32_t regPhygcr;
	uint32_t regPhyrslc0cr;
	uint32_t regPhyrslc1cr;
	uint32_t regPhyclkdelcr;
	uint32_t regPhydlllckvfcr;
} TCC_Dramc;

static uint32_t
control_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
control_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
status_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
status_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
reftimer_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
reftimer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
wrbftmout_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
wrbftmout_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
pdemr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
pdemr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
mrs_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Dramc *dramc = clientData;
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return dramc->regMrs;
}

static void
mrs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Dramc *dramc = clientData;
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
	dramc->regMrs = value & 0x3FF7;
}

static uint32_t
emrs_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
emrs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
trcd_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
trcd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
trp_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
trp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
trc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
trc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
tras_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
tras_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
cas_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Dramc *dramc = clientData;
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return dramc->regCas;
}

static void
cas_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Dramc *dramc = clientData;
	dramc->regCas = value & 0xf;
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
trfc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
trfc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
tmrd_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
tmrd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
twr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
twr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
twtr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
twtr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
txsnr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
txsnr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
txsrd_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
txsrd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
txp_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
txp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
memcfg_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
memcfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
bcr0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
bcr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
bcr1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
bcr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
phycr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
phycr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
phypdcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return PHYPDCR_PDFL | PHYPDCR_PDFL;
}

static void
phypdcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
phypdcfgr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
phypdcfgr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
phygcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
phygcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
phyrslc0cr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
phyrslc0cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
phyrslc1cr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
phyrslc1cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
phyclkdelcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
phyclkdelcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static uint32_t
phydlllckvfcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Register not complete\n", __func__);
	return 0;
}

static void
phydlllckvfcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K DRAMC: %s: Write 0x%08x \n", __func__, value);
}

static void
TDramc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	TCC_Dramc *dramc = owner;
	IOH_New32(REG0_CONTROL(base), control_read, control_write, dramc);
	IOH_New32(REG1_STATUS(base), status_read, status_write, dramc);
	IOH_New32(REG2_REFTIMER(base), reftimer_read, reftimer_write, dramc);
	IOH_New32(REG3_WRBFTMOUT(base), wrbftmout_read, wrbftmout_write, dramc);
	IOH_New32(REG4_PDEMR(base), pdemr_read, pdemr_write, dramc);
	IOH_New32(REG5_MRS(base), mrs_read, mrs_write, dramc);
	IOH_New32(REG6_EMRS(base), emrs_read, emrs_write, dramc);
	IOH_New32(REG7_TRCD(base), trcd_read, trcd_write, dramc);
	IOH_New32(REG8_TRP(base), trp_read, trp_write, dramc);
	IOH_New32(REG9_TRC(base), trc_read, trc_write, dramc);
	IOH_New32(REG10_TRAS(base), tras_read, tras_write, dramc);
	IOH_New32(REG11_CAS(base), cas_read, cas_write, dramc);
	IOH_New32(REG12_TRFC(base), trfc_read, trfc_write, dramc);
	IOH_New32(REG13_TMRD(base), tmrd_read, tmrd_write, dramc);
	IOH_New32(REG14_TWR(base), twr_read, twr_write, dramc);
	IOH_New32(REG15_TWTR(base), twtr_read, twtr_write, dramc);
	IOH_New32(REG16_TXSNR(base), txsnr_read, txsnr_write, dramc);
	IOH_New32(REG17_TXSRD(base), txsrd_read, txsrd_write, dramc);
	IOH_New32(REG18_TXP(base), txp_read, txp_write, dramc);
	IOH_New32(REG19_MEMCFG(base), memcfg_read, memcfg_write, dramc);
	IOH_New32(REG20_BCR0(base), bcr0_read, bcr0_write, dramc);
	IOH_New32(REG21_BCR1(base), bcr1_read, bcr1_write, dramc);
	IOH_New32(REG24_PHYCR(base), phycr_read, phycr_write, dramc);
	IOH_New32(REG25_PHYPDCR(base), phypdcr_read, phypdcr_write, dramc);
	IOH_New32(REG26_PHYPDCFGR(base), phypdcfgr_read, phypdcfgr_write, dramc);
	IOH_New32(REG27_PHYGCR(base), phygcr_read, phygcr_write, dramc);
	IOH_New32(REG28_PHYRSLC0CR(base), phyrslc0cr_read, phyrslc0cr_write, dramc);
	IOH_New32(REG29_PHYRSLC1CR(base), phyrslc1cr_read, phyrslc1cr_write, dramc);
	IOH_New32(REG36_PHYCLKDELCR(base), phyclkdelcr_read, phyclkdelcr_write, dramc);
	IOH_New32(REG37_PHYDLLLCKVFCR(base), phydlllckvfcr_read, phydlllckvfcr_write, dramc);
}

static void
TDramc_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(REG0_CONTROL(base));
	IOH_Delete32(REG1_STATUS(base));
	IOH_Delete32(REG2_REFTIMER(base));
	IOH_Delete32(REG3_WRBFTMOUT(base));
	IOH_Delete32(REG4_PDEMR(base));
	IOH_Delete32(REG5_MRS(base));
	IOH_Delete32(REG6_EMRS(base));
	IOH_Delete32(REG7_TRCD(base));
	IOH_Delete32(REG8_TRP(base));
	IOH_Delete32(REG9_TRC(base));
	IOH_Delete32(REG10_TRAS(base));
	IOH_Delete32(REG11_CAS(base));
	IOH_Delete32(REG12_TRFC(base));
	IOH_Delete32(REG13_TMRD(base));
	IOH_Delete32(REG14_TWR(base));
	IOH_Delete32(REG15_TWTR(base));
	IOH_Delete32(REG16_TXSNR(base));
	IOH_Delete32(REG17_TXSRD(base));
	IOH_Delete32(REG18_TXP(base));
	IOH_Delete32(REG19_MEMCFG(base));
	IOH_Delete32(REG20_BCR0(base));
	IOH_Delete32(REG21_BCR1(base));
	IOH_Delete32(REG24_PHYCR(base));
	IOH_Delete32(REG25_PHYPDCR(base));
	IOH_Delete32(REG26_PHYPDCFGR(base));
	IOH_Delete32(REG27_PHYGCR(base));
	IOH_Delete32(REG28_PHYRSLC0CR(base));
	IOH_Delete32(REG29_PHYRSLC1CR(base));
	IOH_Delete32(REG36_PHYCLKDELCR(base));
	IOH_Delete32(REG37_PHYDLLLCKVFCR(base));
}

BusDevice *
TCC8K_DramcNew(const char *name)
{
	TCC_Dramc *dramc = sg_new(TCC_Dramc);
	dramc->bdev.first_mapping = NULL;
	dramc->bdev.Map = TDramc_Map;
	dramc->bdev.UnMap = TDramc_UnMap;
	dramc->bdev.owner = dramc;
	dramc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	dramc->regCas = 3;
	return &dramc->bdev;
}
