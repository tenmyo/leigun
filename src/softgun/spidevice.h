#include <stdint.h>

/*
 ***************************************************************
 * Do not change state of CLK pin when changing configuration.
 * this is the behaviour of M32C. It needs a dummy SPI write 
 * at initialization.
 ***************************************************************
 */
#define SPIDEV_KEEP_IDLE_STATE	(0x0)
#define SPIDEV_SET_IDLE_STATE	(0x100)

#define SPIDEV_CPHA0	(0x00)
#define SPIDEV_CPHA1	(0x200)

#define SPIDEV_CPOL0	(0x00)
#define SPIDEV_CPOL1	(0x400)

#define SPIDEV_MSBFIRST	(0x00)
#define SPIDEV_LSBFIRST	(0x800)

#define SPIDEV_CSLOW	(0x0)
#define SPIDEV_CSHIGH	(0x2000)

#define SPIDEV_MS_MSK	(0xc000)
#define SPIDEV_DISA	(0x0000)
#define SPIDEV_SLAVE	(0x4000)
#define SPIDEV_MASTER	(0x8000)

#define SPIDEV_BITS(x)  ((x) & 0xff)

typedef struct Spi_Device Spi_Device;

typedef void SpiDev_XmitEventProc(void *dev, uint8_t * data, int bits);

#if 0
typedef struct SpiDev_Operations {
	SpiDev_XmitEventProc *spiXmitEvent;
} SpiDev_Operations;
#endif

Spi_Device *SpiDev_New(const char *name, SpiDev_XmitEventProc * proc, void *owner);
void SpiDev_Configure(Spi_Device * spidev, uint32_t config);
void SpiDev_StartXmit(Spi_Device * spi, uint8_t * firstdata, int bits);
