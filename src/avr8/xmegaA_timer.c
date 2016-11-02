/**
 ************************************************************
 ************************************************************
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sgstring.h"
#include "avr8_cpu.h"
#include "serial.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "xmegaA_timer.h"
#include "fio.h"

#define TMR_CTRLA(base)		((base) + 0x00)
#define TMR_CTRLB(base)		((base) + 0x01)
#define TMR_CTRLC(base)		((base) + 0x02)
#define TMR_CTRLD(base)		((base) + 0x03)
#define TMR_INTCTRLA(base)	((base) + 0x06)
#define TMR_INTCTRLB(base)	((base) + 0x07)
#define TMR_CTRLFCLR(base)	((base) + 0x08)
#define TMR_CTRLFSET(base)	((base) + 0x09)
#define TMR_CTRLGCLR(base)	((base) + 0x0a)
#define TMR_CTRLGSET(base)	((base) + 0x0b)
#define TMR_INTFLAGS(base)	((base) + 0x0c)
#define TMR_TEMP(base)		((base) + 0x0f)
#define TMR_CNTH(base)		((base) + 0x21)
#define TMR_CNTL(base)		((base) + 0x20)
#define TMR_PERH(base)		((base) + 0x27)
#define TMR_PERL(base)		((base) + 0x26)
#define TMR_CCAL(base)		((base) + 0x28)
#define TMR_CCAH(base)		((base) + 0x29)
#define TMR_CCBL(base)		((base) + 0x2a)
#define TMR_CCBH(base)		((base) + 0x2b)
#define TMR_CCCL(base)		((base) + 0x2c)
#define TMR_CCCH(base)		((base) + 0x2d)
#define TMR_CCDL(base)		((base) + 0x2e)
#define TMR_CCDH(base)		((base) + 0x2f)
#define TMR_PERBUFL(base)	((base) + 0x36)
#define TMR_PERBUFH(base)	((base) + 0x37)
#define TMR_CCABUFL(base)	((base) + 0x38)
#define TMR_CCABUFH(base)	((base) + 0x39)
#define TMR_CCBBUFL(base)	((base) + 0x3a)
#define TMR_CCBBUFH(base)	((base) + 0x3b)
#define TMR_CCCBUFL(base)	((base) + 0x3c)
#define TMR_CCCBUFH(base)	((base) + 0x3d)
#define TMR_CCDBUFL(base)	((base) + 0x3e)
#define TMR_CCDBUFH(base)	((base) + 0x3f)

typedef struct XMegaTimer {
	uint32_t dummy;
} XMegaTimer;

static uint8_t
ctrla_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ctrla_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ctrlb_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ctrlb_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ctrlc_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ctrlc_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ctrld_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ctrld_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
intctrla_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
intctrla_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
intctrlb_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
intctrlb_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ctrlfclr_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ctrlfclr_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ctrlfset_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ctrlfset_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ctrlgclr_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ctrlgclr_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ctrlgset_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ctrlgset_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
intflags_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
intflags_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
temp_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
temp_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
cnth_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
cnth_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
cntl_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
cntl_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
perh_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
perh_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
perl_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
perl_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccal_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccal_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccah_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccah_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccbl_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccbl_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccbh_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccbh_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
cccl_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
cccl_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccch_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccch_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccdl_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccdl_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccdh_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccdh_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
perbufl_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
perbufl_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
perbufh_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
perbufh_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccabufl_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccabufl_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccabufh_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccabufh_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccbbufl_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccbbufl_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccbbufh_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccbbufh_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
cccbufl_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
cccbufl_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
cccbufh_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
cccbufh_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccdbufl_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccdbufl_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

static uint8_t
ccdbufh_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
	return 0;
}

static void
ccdbufh_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega Timer: %s not implemented\n", __func__);
}

void
XMegaA_TimerNew(const char *name, uint32_t base)
{
	XMegaTimer *tmr = sg_new(XMegaTimer);

	AVR8_RegisterIOHandler(TMR_CTRLA(base), ctrla_read, ctrla_write, tmr);
	AVR8_RegisterIOHandler(TMR_CTRLB(base), ctrlb_read, ctrlb_write, tmr);
	AVR8_RegisterIOHandler(TMR_CTRLC(base), ctrlc_read, ctrlc_write, tmr);
	AVR8_RegisterIOHandler(TMR_CTRLD(base), ctrld_read, ctrld_write, tmr);
	AVR8_RegisterIOHandler(TMR_INTCTRLA(base), intctrla_read, intctrla_write, tmr);
	AVR8_RegisterIOHandler(TMR_INTCTRLB(base), intctrlb_read, intctrlb_write, tmr);
	AVR8_RegisterIOHandler(TMR_CTRLFCLR(base), ctrlfclr_read, ctrlfclr_write, tmr);
	AVR8_RegisterIOHandler(TMR_CTRLFSET(base), ctrlfset_read, ctrlfset_write, tmr);
	AVR8_RegisterIOHandler(TMR_CTRLGCLR(base), ctrlgclr_read, ctrlgclr_write, tmr);
	AVR8_RegisterIOHandler(TMR_CTRLGSET(base), ctrlgset_read, ctrlgset_write, tmr);
	AVR8_RegisterIOHandler(TMR_INTFLAGS(base), intflags_read, intflags_write, tmr);
	AVR8_RegisterIOHandler(TMR_TEMP(base), temp_read, temp_write, tmr);
	AVR8_RegisterIOHandler(TMR_CNTH(base), cnth_read, cnth_write, tmr);
	AVR8_RegisterIOHandler(TMR_CNTL(base), cntl_read, cntl_write, tmr);
	AVR8_RegisterIOHandler(TMR_PERH(base), perh_read, perh_write, tmr);
	AVR8_RegisterIOHandler(TMR_PERL(base), perl_read, perl_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCAL(base), ccal_read, ccal_write, tmr),
	    AVR8_RegisterIOHandler(TMR_CCAH(base), ccah_read, ccah_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCBL(base), ccbl_read, ccbl_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCBH(base), ccbh_read, ccbh_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCCL(base), cccl_read, cccl_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCCH(base), ccch_read, ccch_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCDL(base), ccdl_read, ccdl_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCDH(base), ccdh_read, ccdh_write, tmr);
	AVR8_RegisterIOHandler(TMR_PERBUFL(base), perbufl_read, perbufl_write, tmr);
	AVR8_RegisterIOHandler(TMR_PERBUFH(base), perbufh_read, perbufh_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCABUFL(base), ccabufl_read, ccabufl_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCABUFH(base), ccabufh_read, ccabufh_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCBBUFL(base), ccbbufl_read, ccbbufl_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCBBUFH(base), ccbbufh_read, ccbbufh_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCCBUFL(base), cccbufl_read, cccbufl_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCCBUFH(base), cccbufh_read, cccbufh_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCDBUFL(base), ccdbufl_read, ccdbufl_write, tmr);
	AVR8_RegisterIOHandler(TMR_CCDBUFH(base), ccdbufh_read, ccdbufh_write, tmr);
}
