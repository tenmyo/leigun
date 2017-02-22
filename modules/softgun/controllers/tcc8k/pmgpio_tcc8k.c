/**
 *************************************************************
 * GPIO Port PK with interrupt capabilities
 *************************************************************
 */

#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "pmgpio_tcc8k.h"
#include "serial.h"
#include "cycletimer.h"

#define BACKUP_RAM(base)	((base) + 0x000)
#define PMGPIO_DAT(base)	((base) + 0x800)
#define PMGPIO_DOE(base)	((base) + 0x804)
#define PMGPIO_FS0(base)	((base) + 0x808)
#define PMGPIO_RPU(base)	((base) + 0x810)
#define PMGPIO_RPD(base)	((base) + 0x814)
#define PMGPIO_DV0(base)	((base) + 0x818)
#define PMGPIO_DV1(base)	((base) + 0x81C)
#define PMGPIO_EE0(base)	((base) + 0x820)
#define PMGPIO_EE1(base)	((base) + 0x824)
#define PMGPIO_CTL(base)	((base) + 0x828)
#define PMGPIO_DI(base)		((base) + 0x82C)
#define PMGPIO_STR(base)	((base) + 0x830)
#define PMGPIO_STF(base)	((base) + 0x834)
#define PMGPIO_POL(base)	((base) + 0x838)
#define PMGPIO_APB(base)	((base) + 0x900)

typedef struct PmGpio {
	BusDevice bdev;
	uint8_t bram[1024];
	SigNode *sigPk[32];
} PmGpio;

static uint32_t
bram_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
bram_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
dat_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
dat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
doe_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
doe_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
fs0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
fs0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
rpu_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
rpu_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
rpd_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
rpd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
dv0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
dv0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
dv1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
dv1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
ee0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
ee0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
ee1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
ee1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
ctl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
ctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
di_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
di_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
str_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
str_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
stf_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
stf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
pol_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
pol_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static uint32_t
apb_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
	return 0;
}

static void
apb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PmGpio: %s not implemented\n", __func__);
}

static void
PmGpio_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	PmGpio *gpio = owner;
	IOH_NewRegion(BACKUP_RAM(base), 2048, bram_read, bram_write, IOH_FLG_HOST_ENDIAN, gpio);

	//      IOH_New32(GPIOPK_RST(base),rst_read,rst_write,gpio);
	IOH_New32(PMGPIO_DAT(base), dat_read, dat_write, gpio);
	IOH_New32(PMGPIO_DOE(base), doe_read, doe_write, gpio);
	IOH_New32(PMGPIO_FS0(base), fs0_read, fs0_write, gpio);
	IOH_New32(PMGPIO_RPU(base), rpu_read, rpu_write, gpio);
	IOH_New32(PMGPIO_RPD(base), rpd_read, rpd_write, gpio);
	IOH_New32(PMGPIO_DV0(base), dv0_read, dv0_write, gpio);
	IOH_New32(PMGPIO_DV1(base), dv1_read, dv1_write, gpio);
	IOH_New32(PMGPIO_EE0(base), ee0_read, ee0_write, gpio);
	IOH_New32(PMGPIO_EE1(base), ee1_read, ee1_write, gpio);
	IOH_New32(PMGPIO_CTL(base), ctl_read, ctl_write, gpio);
	IOH_New32(PMGPIO_DI(base), di_read, di_write, gpio);
	IOH_New32(PMGPIO_STR(base), str_read, str_write, gpio);
	IOH_New32(PMGPIO_STF(base), stf_read, stf_write, gpio);
	IOH_New32(PMGPIO_POL(base), pol_read, pol_write, gpio);
	IOH_New32(PMGPIO_APB(base), apb_read, apb_write, gpio);
}

static void
PmGpio_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	//     IOH_Delete32(GPIOPK_RST(base));
//#define BACKUP_RAM(base)      ((base) + 0x000)
	IOH_DeleteRegion(BACKUP_RAM(base), 2048);
	IOH_Delete32(PMGPIO_DAT(base));
	IOH_Delete32(PMGPIO_DOE(base));
	IOH_Delete32(PMGPIO_FS0(base));
	IOH_Delete32(PMGPIO_RPU(base));
	IOH_Delete32(PMGPIO_RPD(base));
	IOH_Delete32(PMGPIO_DV0(base));
	IOH_Delete32(PMGPIO_DV1(base));
	IOH_Delete32(PMGPIO_EE0(base));
	IOH_Delete32(PMGPIO_EE1(base));
	IOH_Delete32(PMGPIO_CTL(base));
	IOH_Delete32(PMGPIO_DI(base));
	IOH_Delete32(PMGPIO_STR(base));
	IOH_Delete32(PMGPIO_STF(base));
	IOH_Delete32(PMGPIO_POL(base));
	IOH_Delete32(PMGPIO_APB(base));
}

BusDevice *
TCC8K_PMGpioNew(const char *name)
{
	PmGpio *gpio = sg_new(PmGpio);
	int j;
	gpio->bdev.first_mapping = NULL;
	gpio->bdev.Map = PmGpio_Map;
	gpio->bdev.UnMap = PmGpio_UnMap;
	gpio->bdev.owner = gpio;
	gpio->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	for (j = 0; j < 32; j++) {
		gpio->sigPk[j] = SigNode_New("%s.pk%d", name, j);
		if (!gpio->sigPk[j]) {
			fprintf(stderr, "Can not create FS0 for \"%s\"\n", name);
			exit(1);
		}
	}
	return &gpio->bdev;

}
