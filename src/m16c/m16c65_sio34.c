/**
 ********************************************************************************
 * M16C65 SIO 3 and 4 are synchronous IO ports
 ********************************************************************************
 */
#include "bus.h"
#include "m16c65_sio34.h"
#include "sgstring.h"
#include "signode.h"
#include "spidevice.h"

typedef struct SioRegs {
	uint32_t aSiTRR;
	uint32_t aSiC;
	uint32_t aSiBRG;
} SioRegs;

#define REG_S34C2	(0x278)

/** SiC register definition */
#define SiC_CLKSEL_MSK	(3)
#define SiC_DISA	(1 << 2)
#define SiC_INT_CLK	(1 << 6)
#define SiC_CKPOL	(1 << 4)
#define SiC_MSBFIRST	(1 << 5)

SioRegs sioregsets[] = {
	{
	 /* SIO 3 */
	 .aSiTRR = 0x270,
	 .aSiC = 0x272,
	 .aSiBRG = 0x273,
	 },
	{
	 /* SIO 4 */
	 .aSiTRR = 0x274,
	 .aSiC = 0x276,
	 .aSiBRG = 0x277,
	 }
};

typedef struct SIO {
	BusDevice bdev;
	SigNode *sigIrq;
	SioRegs *regset;
	Spi_Device *spidev;
	uint8_t regSiTRR;
	uint8_t regSiC;
} SIO;

static void
post_interrupt(SIO * sio)
{
	SigNode_Set(sio->sigIrq, SIG_LOW);
	SigNode_Set(sio->sigIrq, SIG_HIGH);
}

static void
spidev_xmit(void *owner, uint8_t * data, int bits)
{
	SIO *sio = owner;
	sio->regSiTRR = *data;
	post_interrupt(sio);
	return;
}

static uint32_t
sitrr_read(void *clientData, uint32_t address, int rqlen)
{
	SIO *sio = clientData;
#if 0
	if (sio->regSiTRR != 1) {
		fprintf(stderr, "SiTRR read %02x\n", sio->regSiTRR);
	}
#endif
	return sio->regSiTRR;
}

static void
sitrr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SIO *sio = clientData;
#if 0
	if (value != 0) {
		fprintf(stderr, "SiTRR write 0x%02x\n", value);
	}
#endif
	uint8_t data = value;
	sio->regSiTRR = value;
	SpiDev_StartXmit(sio->spidev, &data, 8);
	post_interrupt(sio);
}

static uint32_t
sic_read(void *clientData, uint32_t address, int rqlen)
{
	SIO *sio = clientData;
	return sio->regSiC;
}

static void
sic_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SIO *sio = clientData;
	uint32_t spi_control;
	sio->regSiC = value;
	spi_control = SPIDEV_BITS(8);
	if (value & SiC_INT_CLK) {
		spi_control |= SPIDEV_MASTER;
	} else {
		spi_control |= SPIDEV_SLAVE;
	}
	spi_control |= SPIDEV_CPHA1;
	if (value & SiC_CKPOL) {
		spi_control |= SPIDEV_CPOL0;
	} else {
		spi_control |= SPIDEV_CPOL1;
	}
	if (value & SiC_MSBFIRST) {
		spi_control |= SPIDEV_MSBFIRST;
	} else {
		spi_control |= SPIDEV_LSBFIRST;
	}
	SpiDev_Configure(sio->spidev, spi_control);
}

static uint32_t
sibrg_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
sibrg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static void
M16CSio34_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	SIO *sio = owner;
	SioRegs *regs = sio->regset;
	IOH_Delete8(regs->aSiTRR);
	IOH_Delete8(regs->aSiC);
	IOH_Delete8(regs->aSiBRG);
}

static void
M16CSio34_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	SIO *sio = owner;
	SioRegs *regs = sio->regset;
	IOH_New8(regs->aSiTRR, sitrr_read, sitrr_write, sio);
	IOH_New8(regs->aSiC, sic_read, sic_write, sio);
	IOH_New8(regs->aSiBRG, sibrg_read, sibrg_write, sio);

}

BusDevice *
M16C65_SioNew(const char *name, unsigned int register_set)
{
	SIO *sio = sg_new(SIO);
	if (register_set >= array_size(sioregsets)) {
		fprintf(stderr, "Illegal register set selection %d\n", register_set);
		exit(1);
	}
	sio->spidev = SpiDev_New(name, spidev_xmit, sio);
	sio->regset = &sioregsets[register_set];
	sio->bdev.first_mapping = NULL;
	sio->bdev.Map = M16CSio34_Map;
	sio->bdev.UnMap = M16CSio34_Unmap;
	sio->bdev.owner = sio;
	sio->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;

	sio->sigIrq = SigNode_New("%s.irq", name);
	if (!sio->sigIrq) {
		fprintf(stderr, "Can not create SIO \"%s\" IRQ line\n", name);
		exit(1);
	}
	return &sio->bdev;
}
