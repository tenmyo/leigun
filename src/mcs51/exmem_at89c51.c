/**
 **********************************************************************
 * External memory (SRAM + eeprom) 
 **********************************************************************
 */

#include <stdint.h>
#include <stdbool.h>
#include "sglib.h"
#include "sgstring.h"
#include "cpu_mcs51.h"
#include "exmem_at89c51.h"
#include "diskimage.h"
#include "configfile.h"

#define REG_EECON	0xd2
#define		EECON_EEBUSY	(1 << 0)
#define		EECON_EEE		(1 << 1)

typedef struct AT89C51_Exmem {
	MCS51Cpu *mcs51;
	uint8_t regEECON;
	uint8_t *eepromData;
	uint16_t eepromSize;
	uint8_t *eram;
	uint32_t eramSize;
	bool eepromWbValid;
	bool eepromWbWarnEnabled;
#if 1
/* New */
	uint16_t eepromColLatchBase;
    uint8_t eepromColLatch[128]; 
    bool eepromColLatchValid[128]; 
    bool eepromColBaseValid;
#endif
	DiskImage *eeprom_di;
} AT89C51_Exmem;

static void
Eeprom_Write(void *dev, uint16_t addr, uint8_t value)
{
	AT89C51_Exmem *em = dev;
	fprintf(stderr,"eewrite 0x%02x to 0x%04x\n",value,addr);
	//sleep(1);
	if (addr < em->eepromSize) {
        if (em->eepromColBaseValid == true) {
            if(em->eepromColLatchBase != (addr & ~0x7f))  {
                fprintf(stderr,"EEPROM PGM error: Programming crosses 128 Byte page\n");
            }  
        }
        em->eepromColLatchBase = addr & ~0x7f;   
        em->eepromColBaseValid = true;
        em->eepromColLatch[addr & 0x7f] = value;
        em->eepromColLatchValid[addr & 0x7f] = true;
	}
}

/**
 * \fn static uint8_t Eeprom_Read(void *dev,uint16_t addr)
 */
static uint8_t
Eeprom_Read(void *dev, uint16_t addr)
{
	AT89C51_Exmem *em = dev;
	if (addr < em->eepromSize) {
	    fprintf(stderr,"Read 0x%02x from eeprom at 0x%04x\n",em->eepromData[addr], addr);
		return em->eepromData[addr];
	} else {
		fprintf(stderr, "Read outside of eeprom\n");
		return 0;
	}
}

/**
 ***************************************************************
 * Write to expanded memory internal 2kB RAM
 ***************************************************************
 */
static void
Eram_Write(void *dev, uint16_t addr, uint8_t value)
{
	AT89C51_Exmem *em = dev;
	if (addr < em->eramSize) {
		em->eram[addr] = value;
		//fprintf(stderr,"ERAM write %04x: %02x\n",addr,value);
	} else {
		fprintf(stderr, "Bug in %s\n", __func__);
		exit(1);
	}
}

static uint8_t
Eram_Read(void *dev, uint16_t addr)
{
	AT89C51_Exmem *em = dev;
	if (addr < em->eramSize) {
		//fprintf(stderr,"ERAM read %04x\n",addr);
		return em->eram[addr];
	} else {
		fprintf(stderr, "Bug in %s\n", __func__);
		exit(1);
	}
}

static uint8_t
eecon_read(void *eventData, uint8_t addr)
{
	AT89C51_Exmem *em = eventData;
	return em->regEECON;
}

static void
eecon_write(void *eventData, uint8_t addr, uint8_t value)
{
	AT89C51_Exmem *em = eventData;
	uint8_t diff = value ^ em->regEECON;
    unsigned int i;
	if (((value & 0xf0) == 0xa0) && ((em->regEECON & 0xf0) == 0x50)) {
        for (i = 0; i < 128; i++) {
            if (em->eepromColLatchValid[i]) {
                em->eepromColLatchValid[i] = false;
                em->eepromData[em->eepromColLatchBase | i] =  em->eepromColLatch[i];
                fprintf(stderr, "PGM %02x\n", em->eepromColLatch[i]);
            }
            em->eepromColBaseValid = false;
        }
	}
	em->regEECON = value & ~EECON_EEBUSY;
	fprintf(stderr,"EECON 0x%02x\n",em->regEECON);
	if (diff & EECON_EEE) {
		if (value & EECON_EEE) {
#if 0
			if(em->eepromWbWarnEnabled == true) {
				em->eepromWbWarnEnabled = false;
				fprintf(stderr, "AT89C51 EEPROM: Lost data, Prev. incomplete PGM sequence\n");
			}
#endif
			MCS51_UnmapExmem(em->mcs51, 0, em->eramSize);
			MCS51_MapExmem(em->mcs51, 0, em->eepromSize, Eeprom_Read, Eeprom_Write, em);
			fprintf(stderr, "Mapped EEPROM\n");
		} else {
			MCS51_UnmapExmem(em->mcs51, 0, em->eepromSize);
			MCS51_MapExmem(em->mcs51, 0, em->eramSize, Eram_Read, Eram_Write, em);
			fprintf(stderr, "Unmapped EEPROM\n");
		}
	}
}

void
AT89C51_ExmemInit(MCS51Cpu * mcs51, const char *name)
{
	AT89C51_Exmem *em = sg_new(AT89C51_Exmem);
	uint32_t eramSize = 2048;
	char *eeprom_name;
	char *imagedir;
	imagedir = Config_ReadVar("global", "imagedir");
	if (!imagedir) {
		fprintf(stderr, "No directory given for MCS51 ROM diskimage\n");
		exit(1);
	}
	eeprom_name = alloca(strlen(imagedir) + strlen(name) + 15);

	em->mcs51 = mcs51;
	em->eramSize = eramSize;
	em->eram = sg_calloc(eramSize);
	sprintf(eeprom_name, "%s/%s.eeprom", imagedir, name);
	em->eepromSize = 2048;
	em->eeprom_di = DiskImage_Open(eeprom_name, em->eepromSize, DI_RDWR | DI_CREAT_FF);
	if (!em->eeprom_di) {
		fprintf(stderr, "Can not create or open the EEPROM image\"%s\"\n", eeprom_name);
		exit(1);
	}
	em->eepromData = DiskImage_Mmap(em->eeprom_di);
	em->eepromWbValid = false;
	em->eepromWbWarnEnabled = false;
	MCS51_MapExmem(mcs51, 0, eramSize, Eram_Read, Eram_Write, em);
	MCS51_RegisterSFR(REG_EECON, eecon_read, NULL, eecon_write, em);
}
