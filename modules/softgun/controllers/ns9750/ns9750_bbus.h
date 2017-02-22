#include <bus.h>
BusDevice *NS9xxx_BBusNew(char *mode, char *devname);
void BBus_PostIRQ(int subint);
void BBus_UnPostIRQ(int subint);
void BBus_PostDmaIRQ(int subint);
void BBus_UnPostDmaIRQ(int subint);

#define IRQ_BBUS_AGGREGATE      2

#define __REG(x) (x)

#define BB_MSTRRST      __REG(0x90600000)
#define BB_GPIOCFG_1    __REG(0x90600010)
#define BB_GPIOCFG_2    __REG(0x90600014)
#define BB_GPIOCFG_3    __REG(0x90600018)
#define BB_GPIOCFG_4    __REG(0x9060001c)
#define BB_GPIOCFG_5    __REG(0x90600020)
#define BB_GPIOCFG_6    __REG(0x90600024)
#define BB_GPIOCFG_7    __REG(0x90600028)
#define BB_GPIOCFG_8    __REG(0x90600100)	/* Only NS9360 */
#define BB_GPIOCFG_9    __REG(0x90600104)	/* Only NS9360 */
#define BB_GPIOCFG_10   __REG(0x90600108)	/* Only NS9360 */

#define BB_GPIOCTRL_1   __REG(0x90600030)
#define BB_GPIOCTRL_2   __REG(0x90600034)
#define BB_GPIOCTRL_3   __REG(0x90600120)	/* Only NS9360 */

#define BB_GPIOSTAT_1   __REG(0x90600040)
#define BB_GPIOSTAT_2   __REG(0x90600044)
#define BB_GPIOSTAT_3   __REG(0x90600130)	/* Only NS9360 */

#define BB_MONITOR      __REG(0x90600050)
#define BB_DMA_ISR      __REG(0x90600060)
#define BB_DMA_IER      __REG(0x90600064)
#define BB_USB_CFG      __REG(0x90600070)
#define BB_ENDIAN       __REG(0x90600080)
#define BB_WAKEUP       __REG(0x90600090)

#define BB_BRIDGE_BASE		__REG(0xA0400000)
#define BB_BRIDGE_MAPSIZE	(0x2000)
#define BB_IS           __REG(0xA0401000)
#define BB_IEN          __REG(0xA0401004)
#define BB_IS_MASK      0x03001fff
/* BBus Sub-Interrupts */
#define BB_IRQ_DMA	(0)
#define BB_IRQ_USB	(1)
#define BB_IRQ_SBRX	(2)
#define BB_IRQ_SBTX	(3)
#define BB_IRQ_SARX	(4)
#define BB_IRQ_SATX	(5)
#define BB_IRQ_SCRX	(6)
#define BB_IRQ_SCTX	(7)
#define BB_IRQ_SDRX	(8)
#define BB_IRQ_SDTX	(9)
#define BB_IRQ_I2C	(10)
#define BB_IRQ_1284	(11)
#define BB_IRQ_AHBDMA1	(24)
#define BB_IRQ_AHBDMA2	(25)
#define BB_IRQ_GLBL	(31)
