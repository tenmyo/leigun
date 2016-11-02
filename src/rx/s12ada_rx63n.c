/**
 ******************************************************************
 * Renesas RX63N 12 Bit A/D Converter simulation
 ******************************************************************
 */
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include "bus.h"
#include "sglib.h"
#include "sgstring.h"
#include "cycletimer.h"
#include "clock.h"
#include "s12ada_rx63n.h"

#define REG_ADCSR(base)		((base) + 0x00)
#define		ADCSR_ADST	(1 << 7)
#define		ADCSR_ADCS	(1 << 6)
#define		ADCSR_ADIE	(1 << 4)
#define		ADCSR_CKS_MSK	(3 << 2)
#define		ADCSR_CKS_SHIFT	(2)
#define		ADCSR_TRGE	(1 << 1)
#define		ADCSR_EXTRG	(1 << 0)
#define REG_ADANS0(base)	((base) + 0x04)
#define REG_ADANS1(base)	((base) + 0x06)
#define REG_ADADS0(base)	((base) + 0x08)
#define REG_ADADS1(base)	((base) + 0x0a)
#define REG_ADADC(base)		((base) + 0x0c)
#define REG_ADCER(base)		((base) + 0x0e)
#define REG_ADSTRGR(base)	((base) + 0x10)
#define REG_ADEXICR(base)	((base) + 0x12)
#define REG_ADTSDR(base)	((base) + 0x1a)
#define REG_ADOCDR(base)	((base) + 0x1c)
#define REG_ADDR(base,ch)	((base) + 0x20 + ((ch) << 1))
#define REG_ADSSTR01(base)	((base) + 0x60)
#define REG_ADSSTR23(base)	((base) + 0x70)

typedef struct ADChan {
	uint32_t channel;
	uint16_t regADDR;
} ADChan;

typedef struct Adc12 {
	BusDevice bdev;
	Clock_t *clkIn;
	Clock_t *clkConv;
	uint8_t regADCSR;
	uint16_t regADANS0;
	uint16_t regADANS1;
	uint16_t regADADS0;
	uint16_t regADADS1;
	uint16_t regADADC;
	uint16_t regADCER;
	uint8_t regADSTRGR;
	uint16_t regADEXICR;
	uint16_t regADTSDR;
	uint16_t regADOCDR;
	uint16_t regADSSTR01;
	uint16_t regADSSTR23;
	ADChan adChan[21];
} Adc12;

static void
update_conv_clock(Adc12 *ad,uint8_t cks) 
{
	unsigned int div;
	switch(cks) {
		case 0:
			div = 8;
			break;
		case 1:
			div = 4;
			break;
		case 2:
			div = 2;
			break;
		default:
		case 3:
			div = 1;
			break;
	
	}
	Clock_MakeDerived(ad->clkConv,ad->clkIn,1,div);
}

static uint32_t
adcsr_read(void *clientData, uint32_t address, int rqlen)
{
	Adc12 *ad = clientData;
	return ad->regADCSR & ~(ADCSR_ADST);
}

static void
adcsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Adc12 *ad = clientData;
	uint8_t cks = (value & ADCSR_CKS_MSK) >> ADCSR_CKS_SHIFT;
	update_conv_clock(ad,cks);
	ad->regADCSR = value & ~(1 << 5);
}

static uint32_t
adans0_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adans0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adans1_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adans1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adads0_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adads0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adads1_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adads1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adadc_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adadc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adcer_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adcer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adstrgr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adstrgr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adexicr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adexicr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adtsdr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adtsdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adocdr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adocdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
addr_read(void *clientData, uint32_t address, int rqlen)
{
	ADChan *adch = clientData;
	//Adc12 *ad = container_of(adch,Adc12,adChan[adch->channel]);
	//return adch->regADDR + (lrand48() & 0xf);
    return 0x300;
}

static void
addr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
adsstr01_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adsstr01_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adsstr23_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adsstr23_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}


static void
Adc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
        Adc12 *ad = owner;
	int i;
	IOH_New8(REG_ADCSR(base),adcsr_read,adcsr_write,ad);
	IOH_New16(REG_ADANS0(base),adans0_read,adans0_write,ad);
	IOH_New16(REG_ADANS1(base),adans1_read,adans1_write,ad);
	IOH_New16(REG_ADADS0(base),adads0_read,adads0_write,ad);
	IOH_New16(REG_ADADS1(base),adads1_read,adads1_write,ad);
	IOH_New16(REG_ADADC(base),adadc_read,adadc_write,ad);
	IOH_New16(REG_ADCER(base),adcer_read,adcer_write,ad);
	IOH_New8(REG_ADSTRGR(base),adstrgr_read,adstrgr_write,ad);
	IOH_New16(REG_ADEXICR(base),adexicr_read,adexicr_write,ad);
	IOH_New16(REG_ADTSDR(base),adtsdr_read,adtsdr_write,ad);
	IOH_New16(REG_ADOCDR(base),adocdr_read,adocdr_write,ad);
	for(i = 0; i < array_size(ad->adChan); i++) {
		ADChan *adch = &ad->adChan[i];
		IOH_New16(REG_ADDR(base,i),addr_read,addr_write,adch);
	}
	IOH_New16(REG_ADSSTR01(base),adsstr01_read,adsstr01_write,ad);
	IOH_New16(REG_ADSSTR23(base),adsstr23_read,adsstr23_write,ad);
}

static void
Adc_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
        Adc12 *ad = owner;
	IOH_Delete8(REG_ADCSR(base));
	IOH_Delete16(REG_ADANS0(base));
	IOH_Delete16(REG_ADANS1(base));
	IOH_Delete16(REG_ADADS0(base));
	IOH_Delete16(REG_ADADS1(base));
	IOH_Delete16(REG_ADADC(base));
	IOH_Delete16(REG_ADCER(base));
	IOH_Delete8(REG_ADSTRGR(base));
	IOH_Delete16(REG_ADEXICR(base));
	IOH_Delete16(REG_ADTSDR(base));
	IOH_Delete16(REG_ADOCDR(base));
	for(i = 0; i < array_size(ad->adChan); i++) {
		IOH_Delete16(REG_ADDR(base,i));
	}
	IOH_Delete16(REG_ADSSTR01(base));
	IOH_Delete16(REG_ADSSTR23(base));
}

BusDevice *
Rx63nS12ADa_New(const char *name)
{
        Adc12 *ad = sg_new(Adc12);
	int i;
        ad->bdev.first_mapping = NULL;
        ad->bdev.Map = Adc_Map;
        ad->bdev.UnMap = Adc_Unmap;
       	ad->bdev.owner = ad;
        ad->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	for(i = 0; i < array_size(ad->adChan); i++) {
		ADChan *adch = &ad->adChan[i];
		adch->channel = i;
	}
        ad->clkIn = Clock_New("%s.clk", name);
        ad->clkConv = Clock_New("%s.clkConv", name);
	Clock_MakeDerived(ad->clkConv,ad->clkIn,1,8);
        return &ad->bdev;
}

