#include <bus.h>
BusDevice * AT91Mc_New(const char *name); 
void AT91Mc_RegisterDevice(BusDevice *mcdev,BusDevice *bdev,unsigned int area_id);

#define AT91_AREA_EXMEM0	(0)
#define AT91_AREA_IROM		(1)
#define AT91_AREA_SRAM		(2)
#define AT91_AREA_CS0		(3)
#define AT91_AREA_CS1		(4)
#define AT91_AREA_CS2		(5)
#define AT91_AREA_CS3		(6)
#define AT91_AREA_CS4		(7)
#define AT91_AREA_CS5		(8)
#define AT91_AREA_CS6		(9)
#define AT91_AREA_CS7		(10)
