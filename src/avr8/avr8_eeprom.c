#include <stdint.h>
#include "sgstring.h"
#include "configfile.h"
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "diskimage.h"
#include "avr8_eeprom.h"

/* EECR - EEPROM Control Register */
#define EECR_EEPM1   (1 << 5)
#define EECR_EEPM0   (1 << 4)
#define EECR_EERIE   (1 << 3)
#define EECR_EEMPE   (1 << 2)
#define EECR_EEPE    (1 << 1)
#define EECR_EERE    (1 << 0)

#if 0
#define dbgprintf(x...) fprintf(stderr,x)
#else
#define dbgprintf(x...)
#endif

typedef struct AVR8_EEProm {
	uint8_t eecr;
	uint8_t eedr;
	uint16_t eear;
	DiskImage *diskimage;
	uint8_t *data;
	uint16_t size;
} AVR8_EEProm;

static uint8_t
eecr_read(void *clientData,uint32_t address)
{
        AVR8_EEProm *ee = (AVR8_EEProm *) clientData;
	dbgprintf("EECR read %02x\n",ee->eecr);
        return ee->eecr; 
}

static void
eecr_write(void *clientData,uint8_t value,uint32_t address)
{
        AVR8_EEProm *ee = (AVR8_EEProm *) clientData;
	ee->eecr = value & ~(EECR_EEPE | EECR_EERE);
	if(value & EECR_EERE) {
		if(ee->eear < ee->size) {
			ee->eedr = ee->data[ee->eear];
			dbgprintf("EEPROM: read %02x from %04x\n",ee->eedr,ee->eear);
			ee->eecr &= ~EECR_EERE;
		}		
	}
	if((value & EECR_EEPE)) {
		if(value & EECR_EEMPE) {
			ee->data[ee->eear] = ee->eedr;
			ee->eecr &= ~EECR_EEPE;
		} else {
			fprintf(stderr,"Shit, not EEMPE\n");
		}
	}
}

static uint8_t
eedr_read(void *clientData,uint32_t address)
{
        AVR8_EEProm *ee = (AVR8_EEProm *) clientData;
        return ee->eedr; 
}
static void
eedr_write(void *clientData,uint8_t value,uint32_t address)
{
        AVR8_EEProm *ee = (AVR8_EEProm *) clientData;
	dbgprintf("EEDR write %02x\n",value);
	ee->eedr = value;
}

static uint8_t
eearl_read(void *clientData,uint32_t address)
{
        AVR8_EEProm *ee = (AVR8_EEProm *) clientData;
        return ee->eear & 0xff; 
}
static void
eearl_write(void *clientData,uint8_t value,uint32_t address)
{
        AVR8_EEProm *ee = (AVR8_EEProm *) clientData;
	ee->eear =  (ee->eear & 0xff00) | value;
}

static uint8_t
eearh_read(void *clientData,uint32_t address)
{
        AVR8_EEProm *ee = (AVR8_EEProm *) clientData;
        return ee->eear >> 8; 
}

static void
eearh_write(void *clientData,uint8_t value,uint32_t address)
{
        AVR8_EEProm *ee = (AVR8_EEProm *) clientData;
	ee->eear =  (ee->eear & 0xff) | (value << 8);
}


void
AVR8_EEPromNew(const char *name,char *register_locations,uint16_t size)
{
	/* Addresses should be parsed from config string */
	unsigned int eecr_addr;
	unsigned int eedr_addr;
	unsigned int eear_addr;
	char *imagedir,*filename;
	AVR8_EEProm *ee = sg_calloc(sizeof(AVR8_EEProm));
	if(sscanf(register_locations,"%02x%02x%02x",&eecr_addr,&eedr_addr,&eear_addr) != 3) {
		fprintf(stderr,"Bad register locations in EEPROM constructor call\n"); 
		exit(1);
	}
	imagedir = Config_ReadVar("global","imagedir");
        if(!imagedir) {
                fprintf(stderr,"No directory given for AVR8 eeprom\n");
                exit(1);
        }
        filename = alloca(strlen(name) + strlen(imagedir) +20);
        sprintf(filename,"%s/%s.img",imagedir,name);

	ee->diskimage =	DiskImage_Open(filename,size,DI_RDWR | DI_CREAT_FF);
	if(!ee->diskimage) {
		fprintf(stderr,"Can not open diskimage for EEProm\n");
		exit(1);
	}
	ee->data = DiskImage_Mmap(ee->diskimage);
	if(!ee->data) {
		fprintf(stderr,"Can not mmap diskimage for EEProm\n");
		exit(1);
	}
	ee->size = size;
        AVR8_RegisterIOHandler(eecr_addr,eecr_read,eecr_write,ee);
        AVR8_RegisterIOHandler(eedr_addr,eedr_read,eedr_write,ee);
        AVR8_RegisterIOHandler(eear_addr,eearl_read,eearl_write,ee);
        AVR8_RegisterIOHandler(eear_addr + 1,eearh_read,eearh_write,ee);
}
