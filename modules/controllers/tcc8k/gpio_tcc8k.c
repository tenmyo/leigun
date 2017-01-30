#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "gpio_tcc8k.h"
#include "serial.h"
#include "cycletimer.h"

#define REG_GPIO_DAT(base,ofs)		((base) + (ofs) + 0x00)
#define REG_GPIO_DOE(base,ofs)		((base) + (ofs) + 0x04)
#define REG_GPIO_FS0(base,ofs)		((base) + (ofs) + 0x08)
#define REG_GPIO_FS1(base,ofs)		((base) + (ofs) + 0x0c)
#define REG_GPIO_RPU(base,ofs)		((base) + (ofs) + 0x30)
#define REG_GPIO_RPD(base,ofs)		((base) + (ofs) + 0x34)
#define REG_GPIO_DV0(base,ofs)		((base) + (ofs) + 0x38)
#define REG_GPIO_DV1(base,ofs)		((base) + (ofs) + 0x3c)

#define PORT_PD_OFS	(0x00)
#define PORT_PS_OFS	(0x40)
#define PORT_PU_OFS	(0x80)
#define PORT_FC_OFS	(0xc0)
#define PORT_FD_OFS	(0x100)
#define PORT_LC_OFS	(0x140)
#define PORT_LD_OFS	(0x180)
#define PORT_AD_OFS	(0x1c0)
#define PORT_XC_OFS	(0x200)
#define PORT_XD_OFS	(0x240)

typedef struct PortInfo {
	char *name;
	int nr_pins;
} PortInfo;

static PortInfo portInfo[] = {
	{
	 .name = "pd",
	 .nr_pins = 13,

	 },
	{
	 .name = "ps",
	 .nr_pins = 24,
	 },
	{
	 .name = "pu",
	 .nr_pins = 20,
	 },
	{
	 .name = "fc",
	 .nr_pins = 8,
	 },
	{
	 .name = "fd",
	 .nr_pins = 20,
	 },
	{
	 .name = "lc",
	 .nr_pins = 6,
	 },
	{
	 .name = "ld",
	 .nr_pins = 24,
	 },
	{
	 .name = "ad",
	 .nr_pins = 8,
	 },
	{
	 .name = "xc",
	 .nr_pins = 29,
	 },
	{
	 .name = "xd",
	 .nr_pins = 11,
	 }
};

typedef struct TCC_Gpio TCC_Gpio;

typedef struct GpioPort {
	TCC_Gpio *gpio;
	int nr_pins;
	char *name;
	SigNode *ioPin[32];
	SigNode *sigOE[32];	/* Internal Output enable signal */
	uint32_t regDat;
	uint32_t dataIn;
	uint32_t regDoe;
	uint32_t regFs0;
	uint32_t regFs1;
	uint32_t regRpu;
	uint32_t regRpd;
	uint32_t regDv0;
	uint32_t regDv1;
} GpioPort;

struct TCC_Gpio {
	BusDevice bdev;
	GpioPort ports[10];
};

static void
read_inputs(GpioPort * port)
{
	int i;
	uint32_t val = 0;
	for (i = 0; i < port->nr_pins; i++) {
		if (SigNode_Val(port->ioPin[i]) == SIG_HIGH) {
			val |= (1 << i);
		}
	}
	port->dataIn = val;
}

static void
update_outports(GpioPort * port)
{
	uint32_t out_mask = port->regDoe;
	int i;
	for (i = 0; i < port->nr_pins; i++) {
		if (!(out_mask & (1 << i))) {
			if (port->regRpd & (1 << i)) {
				SigNode_Set(port->ioPin[i], SIG_WEAK_PULLDOWN);
			} else if (port->regRpu & (1 << i)) {
				SigNode_Set(port->ioPin[i], SIG_WEAK_PULLUP);
			} else {
				SigNode_Set(port->ioPin[i], SIG_OPEN);
			}
		}
		if (port->regDat & (1 << i)) {
			SigNode_Set(port->ioPin[i], SIG_HIGH);
		} else {
			SigNode_Set(port->ioPin[i], SIG_LOW);
		}
	}
}

/**
 */
static uint32_t
dat_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	uint32_t value;
	read_inputs(port);
	uint32_t out_mask = port->regDoe;
	value = (port->regDat & out_mask) | (port->dataIn & ~out_mask);
	fprintf(stderr, "TCC8K GPIO: %s: Register not tested\n", __func__);
	return value;
}

static void
dat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	port->regDat = value;
	update_outports(port);
	fprintf(stderr, "TCC8K GPIO: %s: Register not tested\n", __func__);
}

static uint32_t
doe_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	return port->regDoe;
}

static void
doe_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	port->regDoe = value;
	update_outports(port);
	fprintf(stderr, "TCC8K GPIO: %s: Register not tested\n", __func__);
}

static uint32_t
fs0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K GPIO: %s: Register not implemented\n", __func__);
	return 0;
}

static void
fs0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K GPIO: %s: Register not implemented\n", __func__);
}

static uint32_t
fs1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K GPIO: %s: Register not implemented\n", __func__);
	return 0;
}

static void
fs1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K GPIO: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************
 * Pullup control
 **********************************************************************
 */
static uint32_t
rpu_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	return port->regRpu;
}

static void
rpu_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	port->regRpu = value;
	update_outports(port);
	fprintf(stderr, "RPU 0x%08x write 0x%08x\n", address, value);
}

/**
 **********************************************************************
 * Pulldown control
 **********************************************************************
 */
static uint32_t
rpd_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	return port->regRpd;
}

static void
rpd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	port->regRpd = value;
	update_outports(port);
	fprintf(stderr, "RPD 0x%08x write 0x%08x\n", address, value);
}

static uint32_t
dv0_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	return port->regDv0;
}

static void
dv0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	port->regDv0 = value;
}

static uint32_t
dv1_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	return port->regDv1;
}

static void
dv1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = clientData;
	port->regDv1 = value;
}

static void
TGpio_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	TCC_Gpio *gpio = owner;
	int i;
	for (i = 0; i < 10; i++) {
		uint32_t ofs = i * 0x40;
		GpioPort *port;
		port = &gpio->ports[i];
		IOH_New32(REG_GPIO_DAT(base, ofs), dat_read, dat_write, port);
		IOH_New32(REG_GPIO_DOE(base, ofs), doe_read, doe_write, port);
		IOH_New32(REG_GPIO_FS0(base, ofs), fs0_read, fs0_write, port);
		IOH_New32(REG_GPIO_FS1(base, ofs), fs1_read, fs1_write, port);
		IOH_New32(REG_GPIO_RPU(base, ofs), rpu_read, rpu_write, port);
		IOH_New32(REG_GPIO_RPD(base, ofs), rpd_read, rpd_write, port);
		IOH_New32(REG_GPIO_DV0(base, ofs), dv0_read, dv0_write, port);
		IOH_New32(REG_GPIO_DV1(base, ofs), dv1_read, dv1_write, port);
	}
}

static void
TGpio_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	for (i = 0; i < 10; i++) {
		uint32_t ofs = i * 0x40;
		IOH_Delete32(REG_GPIO_DAT(base, ofs));
		IOH_Delete32(REG_GPIO_DOE(base, ofs));
		IOH_Delete32(REG_GPIO_FS0(base, ofs));
		IOH_Delete32(REG_GPIO_FS1(base, ofs));
		IOH_Delete32(REG_GPIO_RPU(base, ofs));
		IOH_Delete32(REG_GPIO_RPD(base, ofs));
		IOH_Delete32(REG_GPIO_DV0(base, ofs));
		IOH_Delete32(REG_GPIO_DV1(base, ofs));
	}

}

/**
 ********************************************************************
 * BusDevice * TCC8K_GpioNew(const char *name)
 ********************************************************************
 */
BusDevice *
TCC8K_GpioNew(const char *name)
{
	TCC_Gpio *gpio = sg_new(TCC_Gpio);
	int i, j;
	gpio->bdev.first_mapping = NULL;
	gpio->bdev.Map = TGpio_Map;
	gpio->bdev.UnMap = TGpio_UnMap;
	gpio->bdev.owner = gpio;
	gpio->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	for (i = 0; i < array_size(portInfo); i++) {
		GpioPort *port = &gpio->ports[i];
		port->gpio = gpio;
		port->nr_pins = portInfo[i].nr_pins;
		port->name = portInfo[i].name;
		for (j = 0; j < port->nr_pins; j++) {
			port->ioPin[j] = SigNode_New("%s.%s.P%d", name, port->name, j);
			port->sigOE[j] = SigNode_New("%s.%s,OE%d", name, port->name, j);
		}
	}
#if 0
	gpio->sigIrq = SigNode_New("%s.irq", name);
	if (!gpio->sigIrq) {
		fprintf(stderr, "Can not create interrupt line for %s\n", name);
		exit(1);
	}
#endif
	return &gpio->bdev;
}
