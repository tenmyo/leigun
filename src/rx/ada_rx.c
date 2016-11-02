/**
 **************************************************************
 * A/D Converter module type A (like usesd in RX62N)
 **************************************************************
 */

#include <stdint.h>
#include "bus.h"
#include "sgstring.h"

/* n = 0 to 3 is mapped to Channel A to D */
#define REG_ADDR(base,n)    ((base) + 0x0 + (n << 1)) 
#define REG_ADCSR(base)     ((base) + 0x10)
#define REG_ADCR(base)      ((base) + 0x11)
#define REG_ADDPR(base)     ((base) + 0x12)
#define REG_ADDIAGR(base)   ((base) + 0x1f)


typedef struct ADa ADa;

typedef struct ADChan {
    ADa *ad;
    uint32_t channel;
    uint16_t regADDR;
} ADChan;

struct ADa {
    BusDevice bdev;
    ADChan adChan[4];
    uint8_t regADCSR;
    uint8_t regADCR;
    uint8_t regADDPR;
    uint8_t regADDIAGR;
};

static uint32_t
addr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0x300;
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

}

static uint32_t
addpr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
addpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static uint32_t
addiagr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
addiagr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static void
ADa_Unmap(void *owner, uint32_t base, uint32_t mask)
{
    ADa *ad = owner;
    int i;
    for(i = 0; i < array_size(ad->adChan); i++) {
        IOH_Delete16(REG_ADDR(base,i));
    }
    IOH_Delete8(REG_ADCSR(base));
    IOH_Delete8(REG_ADCR(base));
    IOH_Delete8(REG_ADDPR(base));
    IOH_Delete8(REG_ADDIAGR(base));
}

static void
ADa_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
    ADa *ad = owner;
    int i;
    for(i = 0; i < array_size(ad->adChan); i++) {
        ADChan *adch = &ad->adChan[i];
        IOH_New16(REG_ADDR(base,i),addr_read,addr_write,adch);
    }
    IOH_New8(REG_ADCSR(base),adcsr_read,adcsr_write,ad);
    IOH_New8(REG_ADCR(base), adcr_read, adcr_write, ad);
    IOH_New8(REG_ADDPR(base), addpr_read, addpr_write, ad);
    IOH_New8(REG_ADDIAGR(base), addiagr_read, addiagr_write, ad);
}


BusDevice *
RxADa_New(const char *name)
{
    
    ADa *ad = sg_new(ADa);
    int i;
    ad->bdev.first_mapping = NULL;
    ad->bdev.Map = ADa_Map;
    ad->bdev.UnMap = ADa_Unmap;
    ad->bdev.owner = ad;
    ad->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
    for(i = 0; i < array_size(ad->adChan); i++) {
        ADChan *adch = &ad->adChan[i];
        adch->channel = i;
        adch->ad = ad;
    }
    //ad->clkIn = Clock_New("%s.clk", name);
     //   ad->clkConv = Clock_New("%s.clkConv", name);
    //Clock_MakeDerived(ad->clkConv,ad->clkIn,1,8);
    return &ad->bdev;
}
