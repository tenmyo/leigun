/*
 *************************************************************************************************
 *
 * Emulation of the MCF5282 FlexCAN module
 *
 * State:
 *      Not implemented
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

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fio.h>
#include <bus.h>
#include <signode.h>
#include <cycletimer.h>
#include <sgstring.h>
#include <mcf5282_flexcan.h>

#define FC_MCB(base) 		((base) + 0x1c0000)
#define FC_CANCTRL0(base)	((base) + 0x1c0006)
#define FC_CANCTRL1(base)	((base) + 0x1c0007)
#define FC_PRESDIV(base)	((base) + 0x1c0008)
#define FC_CANCTRL2(base)	((base) + 0x1c0009)
#define FC_TIMER(base)		((base) + 0x1c000a)
#define FC_RXGMASK(base)	((base) + 0x1c0010)
#define FC_RX14MASK(base)	((base) + 0x1c0014)
#define FC_RX15MASK(base)	((base) + 0x1c0018)
#define FC_ESTAT(base)		((base) + 0x1c0020)
#define FC_IMASK(base)		((base) + 0x1c0022)
#define FC_IFLAG(base)		((base) + 0x1c0024)
#define FC_RXECTR(base)		((base) + 0x1c0026)
#define FC_TXECTR(base)		((base) + 0x1c0027)

/* Offsets in messagebuffers */
#define EXMSGB_CTRL_STAT(base)	((base)+0)
#define EXMSGB_ID_HIGH(base)	((base)+2)
#define EXMSGB_ID_LOW(base)	((base)+4)
#define EXMSGB_DATABYTE0(base)	((base)+6)
#define EXMSGB_DATABYTE1(base)	((base)+7)
#define EXMSGB_DATABYTE2(base)	((base)+8)
#define EXMSGB_DATABYTE3(base)	((base)+9)
#define EXMSGB_DATABYTE4(base)	((base)+0xa)
#define EXMSGB_DATABYTE5(base)	((base)+0xb)
#define EXMSGB_DATABYTE6(base)	((base)+0xc)
#define EXMSGB_DATABYTE7(base)	((base)+0xd)

typedef struct FlexCAN {
	BusDevice bdev;
} FlexCAN;

static void
FlexCAN_Unmap(void *owner, uint32_t base, uint32_t mask)
{
//        IOH_Delete32(SCM_IPSBAR(base));
}

static void
FlexCAN_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
//       FlexCAN *can = (FlexCAN *) owner;
//        IOH_New32(SCM_IPSBAR(base),ipsbar_read,ipsbar_write,scm);
}

BusDevice *
MCF5282_FlexCANNew(const char *name)
{
	FlexCAN *can = sg_calloc(sizeof(FlexCAN));
	can->bdev.first_mapping = NULL;
	can->bdev.Map = FlexCAN_Map;
	can->bdev.UnMap = FlexCAN_Unmap;
	can->bdev.owner = can;
	can->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &can->bdev;

}
