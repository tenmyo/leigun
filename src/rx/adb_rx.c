/*
 *********************************************************************
 * 10 Bit ADb A/D converter from RX63n
 *********************************************************************
 */

#include <stdint.h>
#include "bus.h"
#include "sgstring.h"
#include "clock.h"
#include "adb_rx.h"

#define REG_ADDR(base,n)    ((base) + 0x0 + (n << 1))
#define REG_ADCSR(base)     ((base) + 0x10)
#define REG_ADCR(base)      ((base) + 0x11)
#define REG_ADCR2(base)     ((base) + 0x12)
#define REG_ADSSTR(base)    ((base) + 0x13)

typedef struct ADb ADb;

typedef struct ADChan {
    ADb *ad;
    uint32_t channel;
    uint16_t regADDR;
} ADChan;

struct ADb {
    BusDevice bdev;
    ADChan adChan[8];
    Clock_t *clkIn;
    Clock_t *clkConv;
    uint8_t regADCSR;
    uint8_t regADCR;
    uint8_t regADCR2;
    uint8_t regADSSTR;
};

static uint32_t
addr_read(void *clientData, uint32_t address, int rqlen)
{
        return 123;
}

static void
addr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}
static uint32_t
adcsr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adcsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adcr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    ADb *ad = clientData; 
    uint32_t cks = (value >> 2) & 3;
    uint32_t div;
    switch (cks) {
        case 0:
            div = 8;
            break;
        case 1:
            div = 4;
            break;
        case 2:
            div = 2;
            break;
        default: /* Make the compiler happy */
        case 3:
            div = 1;
            break;
    }
    Clock_MakeDerived(ad->clkConv, ad->clkIn, 1, div); 
}

static uint32_t
adcr2_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adcr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
adsstr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
adsstr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static void
ADb_Unmap(void *owner, uint32_t base, uint32_t mask)
{
    ADb *ad = owner;
    int i;
    for(i = 0; i < array_size(ad->adChan); i++) {
        IOH_Delete16(REG_ADDR(base,i));
    }
    IOH_Delete8(REG_ADCSR(base));
    IOH_Delete8(REG_ADCR(base));
    IOH_Delete8(REG_ADCR2(base));
    IOH_Delete8(REG_ADSSTR(base));
}

static void
ADb_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
    ADb *ad = owner;
    int i;
    for(i = 0; i < array_size(ad->adChan); i++) {
        ADChan *adch = &ad->adChan[i];
        IOH_New16(REG_ADDR(base,i),addr_read,addr_write,adch);
    }
    IOH_New8(REG_ADCSR(base),adcsr_read,adcsr_write,ad);
    IOH_New8(REG_ADCR(base), adcr_read, adcr_write, ad);
    IOH_New8(REG_ADCR2(base), adcr2_read, adcr2_write, ad);
    IOH_New8(REG_ADSSTR(base), adsstr_read, adsstr_write, ad);
}

BusDevice *
RxADb_New(const char *name)
{

    ADb *ad = sg_new(ADb);
    int i;
    ad->bdev.first_mapping = NULL;
    ad->bdev.Map = ADb_Map;
    ad->bdev.UnMap = ADb_Unmap;
    ad->bdev.owner = ad;
    ad->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
    for(i = 0; i < array_size(ad->adChan); i++) {
        ADChan *adch = &ad->adChan[i];
        adch->channel = i;
        adch->ad = ad;
    }
    ad->clkIn = Clock_New("%s.clk", name);
    ad->clkConv = Clock_New("%s.clkConv", name);
    Clock_MakeDerived(ad->clkConv,ad->clkIn,1,8); /* Default value of adcr */

    return &ad->bdev;
}

