/**
 **************************************************************
 * A/D Converter module type A (like usesd in RX62N)
 **************************************************************
 */

#include <stdint.h>
#include "bus.h"
#include "sgstring.h"
#include "da_rx63.h"

#define REG_DADR0(base)     ((base) + 0x00)
#define REG_DADR1(base)     ((base) + 0x02) 
#define REG_DACR(base)      ((base) + 0x04)
#define REG_DADPR(base)     ((base) + 0x05)
#define     DADPR_DPSEL (1 << 7)
#define REG_DAADSCR(base)   ((base) + 0x06)

typedef struct DA {
    BusDevice bdev;
    uint16_t regDADR0; /* Stores the value always in right aligned format ! */ 
    uint16_t regDADR1;
    uint8_t  regDACR;
    uint8_t  regDADPR;
    uint8_t  regDAADSCR;
} DA;

static uint32_t
dadr0_read(void *clientData, uint32_t address, int rqlen)
{
    DA *da = clientData;
    if (da->regDADPR & DADPR_DPSEL) {
        return da->regDADR0 << 6;
    } else {
        return da->regDADR0;
    }
}

static void
dadr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    DA *da = clientData; 
    if (da->regDADPR & DADPR_DPSEL) {
        da->regDADR0 = (value >> 6) & 0x3ff;
    } else {
        da->regDADR0 = value & 0x3ff;
    }
}

static uint32_t
dadr1_read(void *clientData, uint32_t address, int rqlen)
{
    DA *da = clientData;
    if (da->regDADPR & DADPR_DPSEL) {
        return (da->regDADR1 << 6);
    } else {
        return da->regDADR1;
    }
}

static void
dadr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    DA *da = clientData; 
    if (da->regDADPR & DADPR_DPSEL) {
        da->regDADR1 = (value >> 6) & 0x3ff;
    } else {
        da->regDADR1 = value;
    }
}

static uint32_t
dacr_read(void *clientData, uint32_t address, int rqlen)
{
    DA *da = clientData;
    return da->regDACR;
}

static void
dacr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    DA *da = clientData;
    da->regDACR = value | 0x1f;
}

static uint32_t
dadpr_read(void *clientData, uint32_t address, int rqlen)
{
    DA *da = clientData;
    return da->regDADPR;
}

static void
dadpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    DA *da = clientData;
    da->regDADPR = value & 0x80;
}

static uint32_t
daadscr_read(void *clientData, uint32_t address, int rqlen)
{
        return 0;
}

static void
daadscr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

/**
 *******************************************************************************
 * \fn static void DA_Unmap(void *owner, uint32_t base, uint32_t mask)
 * Unmap the DA Controller from the memory space
 *******************************************************************************
 */
static void
DA_Unmap(void *owner, uint32_t base, uint32_t mask)
{
    IOH_Delete16(REG_DADR0(base));
    IOH_Delete16(REG_DADR1(base));
    IOH_Delete8(REG_DACR(base));
    IOH_Delete8(REG_DADPR(base));
    IOH_Delete8(REG_DAADSCR(base));
}

static void
DA_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
    DA *da = owner;
    IOH_New16(REG_DADR0(base), dadr0_read, dadr0_write, da);
    IOH_New16(REG_DADR1(base), dadr1_read, dadr1_write, da);
    IOH_New8(REG_DACR(base), dacr_read, dacr_write, da);
    IOH_New8(REG_DADPR(base), dadpr_read, dadpr_write, da);
    IOH_New8(REG_DAADSCR(base), daadscr_read, daadscr_write, da);
}

BusDevice *
Rx63DA_New(const char *name) 
{
    DA *da = sg_new(DA);
    da->bdev.first_mapping = NULL;
    da->bdev.Map = DA_Map;
    da->bdev.UnMap = DA_Unmap;
    da->bdev.owner = da;
    da->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
    da->regDACR = 0x1f;
    da->regDADPR = 0;
    return &da->bdev;
}
