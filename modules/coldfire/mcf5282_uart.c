/*
 ***************************************************************************************************
 * Emulation of MFC5282 UART 
 *
 * state:  Not implemented
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <bus.h>
#include <signode.h>
#include <serial.h>
#include <configfile.h>
#include <mcf5282_uart.h>
#include <clock.h>
#include <senseless.h>
#include <sgstring.h>

#define CFU_UMR1(base)		((base) + 0x0)
#define CFU_USR(base)		((base) + 0x4)
#define CFU_UCSR(base)		((base) + 0x4)
#define CFU_UCR(base)		((base) + 0x8)
#define CFU_URB(base)		((base) + 0xc)
#define CFU_UTB(base)		((base) + 0xc)
#define CFU_UIPCR(base)		((base) + 0x10)
#define CFU_UACR(base)		((base) + 0x10)
#define CFU_UISR(base)		((base) + 0x14)
#define CFU_UIMR(base)		((base) + 0x14)
#define CFU_UBG1(base)		((base) + 0x18)
#define CFU_UBG2(base)		((base) + 0x1c)
#define CFU_UIP(base)		((base) + 0x34)
#define CFU_UOP1(base)		((base) + 0x38)
#define CFU_UOP0(base)		((base) + 0x3c)

typedef struct CFUart {
	BusDevice bdev;
} CFUart;

static uint32_t
umr1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: umr1 not implemented\n");
	return 0;
}

static void
umr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: umr1 not implemented\n");
}

static uint32_t
usr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: usr not implemented\n");
	return 0;
}

static void
ucsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: ucsr not implemented\n");
}

static uint32_t
ucr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: UCR is not readable\n");
	return 0;
}

static void
ucr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: ucr not implemented\n");
}

static uint32_t
urb_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: urb not implemented\n");
	return 0;
}

static void
utb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: utb not implemented\n");
}

static uint32_t
uipcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: uipcr not implemented\n");
	return 0;
}

static void
uacr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: uacr not implemented\n");
}

static uint32_t
uisr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: uisr not implemented\n");
	return 0;
}

static void
uimr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: uimr not implemented\n");
}

static uint32_t
ubg1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: ubg1 not readable\n");
	return 0;
}

static void
ubg1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: ubg1 not implemented\n");
}

static uint32_t
ubg2_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: ubg2 not readable\n");
	return 0;
}

static void
ubg2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: ubg2 not implemented\n");
}

static uint32_t
uip_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: uip not implemented\n");
	return 0;
}

static void
uip_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: uip is not writable\n");
}

static uint32_t
uop1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: uop1 is not readable\n");
	return 0;
}

static void
uop1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: uop1 is not implemented\n");
}

static uint32_t
uop0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: uop0 is not readable\n");
	return 0;
}

static void
uop0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CFUart: uop0 is not implemented\n");
}

static void
CFU_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete16(CFU_UMR1(base));
	IOH_Delete16(CFU_USR(base));
	IOH_Delete16(CFU_UCR(base));
	IOH_Delete16(CFU_UCR(base));
	IOH_Delete16(CFU_UIPCR(base));
	IOH_Delete16(CFU_UISR(base));
	IOH_Delete16(CFU_UBG1(base));
	IOH_Delete16(CFU_UBG2(base));
	IOH_Delete16(CFU_UIP(base));
	IOH_Delete16(CFU_UOP1(base));
	IOH_Delete16(CFU_UOP0(base));
}

static void
CFU_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	CFUart *cfu = (CFUart *) owner;
	IOH_New16(CFU_UMR1(base), umr1_read, umr1_write, cfu);
	IOH_New16(CFU_USR(base), usr_read, ucsr_write, cfu);
	IOH_New16(CFU_UCR(base), ucr_read, ucr_write, cfu);
	IOH_New16(CFU_UCR(base), urb_read, utb_write, cfu);
	IOH_New16(CFU_UIPCR(base), uipcr_read, uacr_write, cfu);
	IOH_New16(CFU_UISR(base), uisr_read, uimr_write, cfu);
	IOH_New16(CFU_UBG1(base), ubg1_read, ubg1_write, cfu);
	IOH_New16(CFU_UBG2(base), ubg2_read, ubg2_write, cfu);
	IOH_New16(CFU_UIP(base), uip_read, uip_write, cfu);
	IOH_New16(CFU_UOP1(base), uop1_read, uop1_write, cfu);
	IOH_New16(CFU_UOP0(base), uop0_read, uop0_write, cfu);
}

BusDevice *
MCF5282_UartNew(const char *name)
{
	CFUart *cfu = sg_calloc(sizeof(CFUart));
	cfu->bdev.first_mapping = NULL;
	cfu->bdev.Map = CFU_Map;
	cfu->bdev.UnMap = CFU_Unmap;
	cfu->bdev.owner = cfu;
	cfu->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &cfu->bdev;

}
