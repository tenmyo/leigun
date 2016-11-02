/* 
 ************************************************************************************************ 
 *
 * M16C CAN controller 
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 ************************************************************************************************ 
 */


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include "m16c_can.h"
#include "configfile.h"
#include "cycletimer.h"
#include "bus.h"
#include "signode.h"
#include "sgstring.h"

#define CxMCTL(bus,slot) 	(0x200 + (bus) * 0x20 + (slot))
#define		MCTL_NEWDATA	(1<<0)
#define		MCTL_SENT_DATA	(1<<0)
#define		MCTL_INVAL_DATA	(1<<1)
#define		MCTL_TRM_ACT	(1<<1)
#define		MCTL_MSG_LOST   (1<<2)
#define		MCTL_REM_ACTIVE (1<<3)
#define		MCTL_RSP_LOCK	(1<<4)
#define		MCTL_REMOTE	(1<<5)
#define		MCTL_REC_REQ	(1<<6)
#define		MCTL_TRM_REQ	(1<<7)

#define CxCTLR(bus) 		(0x210 + (bus) * 0x20)
#define		CTLR_RESET	(1<<0)
#define		CTLR_LOOPBACK	(1<<1)
#define		CTLR_MSG_ORDER  (1<<2)	
#define		CTLR_BASIC_CAN	(1<<3)
#define		CTLR_BUS_ERR_EN (1<<4)
#define		CTLR_SLEEP 	(1<<5)
#define		CTLR_PORTEN	(1<<6)
#define		CTLR_TSPRESCALE_SHIFT	(8)
#define		CTLR_TSPRESCALE_MASK	(3<<8)
#define		CTLR_TSRESET		(1<<10)
#define		CTLR_RET_BUS_OFF	(1<<11)	
#define		CTLR_RXONLY		(1<<13)

#define CxSTR(bus)		(0x212 + (bus) * 0x20)
#define		STR_MBOX_SHIFT	(0)
#define		STR_MBOX_MASK	(0xf)
#define		STR_TRM_SUCC		(1<<4)
#define		STR_REC_SUCC		(1<<5)
#define		STR_TRM_STATE		(1<<6)
#define		STR_REC_STATE		(1<<7)
#define		STR_STATE_RST		(1<<8)
#define		STR_STATE_LOOPBACK	(1<<9)
#define		STR_STATE_MSGORDER	(1<<10)
#define		STR_STATE_BASICCAN	(1<<11)
#define		STR_STATE_BUS_ERROR	(1<<12)
#define		STR_STATE_ERR_PASS	(1<<13)
#define		STR_STATE_BUS_OFF	(1<<14)

#define CxSSTR(bus)		(0x214 + (bus) * 0x20)
#define CxICR(bus)		(0x216 + (bus) * 0x20)
#define CxIDR(bus)		(0x218 + (bus) * 0x20)
#define CxCONR(bus)		(0x21a + (bus) * 0x20)
#define CxRECR(bus)		(0x21c + (bus) * 0x20)
#define CxTECR(bus)		(0x21d + (bus) * 0x20)
#define CxTSR(bus)		(0x21e + (bus) * 0x20)
#define CxAFS(bus)		(0x242 + (bus) * 2)
#define CxMSGBOX(bus,slot)	(0x60 + (bus)*0x200 + (slot) * 0x10)

/* Global mask register */
#define	CxGMR0(bus)		(0x160 + 0x200 * bus)
#define	CxGMR1(bus)		(0x161 + 0x200 * bus)
#define	CxGMR2(bus)		(0x162 + 0x200 * bus)
#define	CxGMR3(bus)		(0x163 + 0x200 * bus)
#define	CxGMR4(bus)		(0x164 + 0x200 * bus)

#define	CxMAR0(bus)		(0x166 + 0x200 * bus)
#define	CxMAR1(bus)		(0x167 + 0x200 * bus)
#define	CxMAR2(bus)		(0x168 + 0x200 * bus)
#define	CxMAR3(bus)		(0x169 + 0x200 * bus)
#define	CxMAR4(bus)		(0x16a + 0x200 * bus)

#define	CxMBR0(bus)		(0x16c + 0x200 * bus)
#define	CxMBR1(bus)		(0x16d + 0x200 * bus)
#define	CxMBR2(bus)		(0x16e + 0x200 * bus)
#define	CxMBR3(bus)		(0x16f + 0x200 * bus)
#define	CxMBR4(bus)		(0x170 + 0x200 * bus)


typedef struct M16C_CAN {
	int bus_nr;
	BusDevice bdev;
} M16C_CAN;

/*
 * ------------------------------------------------------
 * CAN Control register
 * ------------------------------------------------------
 */
static uint32_t
ctlr_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
ctlr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}
/*
 * ------------------------------------------------------
 *  CAN Status register
 * ------------------------------------------------------
 */
static uint32_t
str_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
str_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}
/*
 * -------------------------------------------------------
 * CAN Slot Status register
 * -------------------------------------------------------
 */
static uint32_t
sstr_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
sstr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * -------------------------------------------------------
 * CAN Interrupt control register
 * -------------------------------------------------------
 */
static uint32_t
icr_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
icr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * -------------------------------------------------------
 * CAN Extended ID register 
 * -------------------------------------------------------
 */ 
static uint32_t
idr_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
idr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * -------------------------------------------
 * CAN Configuration register
 * -------------------------------------------
 */
static uint32_t
conr_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
conr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * ----------------------------------------------
 * CAN receive error count register
 * ----------------------------------------------
 */
static uint32_t
recr_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
recr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * ----------------------------------------------------
 * CAN transmit error count register
 * ----------------------------------------------------
 */

static uint32_t
tecr_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
tecr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * ----------------------------------------------------------
 * CAN Time stamp register
 * ----------------------------------------------------------
 */
static uint32_t
tsr_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
tsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * ---------------------------------------------------------------
 * CAN acceptance filter support register
 * ---------------------------------------------------------------
 */ 
static uint32_t
afs_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
afs_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * -----------------------------------------------
 * CAN Message Box
 * -----------------------------------------------
 */

static uint32_t
msgbox_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}
static void
msgbox_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * -------------------------------------------------------------
 * Message control register
 * -------------------------------------------------------------
 */
static uint32_t
mctl_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}

static void
mctl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * --------------------------------------------------------------
 * CAN global mask register
 * --------------------------------------------------------------
 */
static uint32_t
gmr_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}

static void
gmr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * ----------------------------------------------------------------
 * CAN local mask A register
 * ----------------------------------------------------------------
 */
static uint32_t
mar_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}

static void
mar_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * ----------------------------------------------------------------
 * CAN local mask B register
 * ----------------------------------------------------------------
 */
static uint32_t
mbr_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
        return 0;
}

static void
mbr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"M16C_CAN register 0x%08x not implemented\n",address);
}

/*
 * -----------------------------------------------------------
 * CAN Map
 *	map the device to bus
 * -----------------------------------------------------------
 */
static void
CAN_Map(void *module_owner,uint32_t base,uint32_t mapsize,uint32_t flags)
{
	M16C_CAN *mcan = (M16C_CAN *)module_owner;
	int bus = mcan->bus_nr;
	int slot;
	
	IOH_New8(CxCTLR(bus),ctlr_read,ctlr_write,mcan); 
	IOH_New8(CxSTR(bus),str_read,str_write,mcan);
	IOH_New8(CxSSTR(bus),sstr_read,sstr_write,mcan);
	IOH_New8(CxICR(bus),icr_read,icr_write,mcan);
	IOH_New8(CxIDR(bus),idr_read,idr_write,mcan);
	IOH_New8(CxCONR(bus),conr_read,conr_write,mcan);
	IOH_New8(CxRECR(bus),recr_read,recr_write,mcan);
	IOH_New8(CxTECR(bus),tecr_read,tecr_write,mcan);
	IOH_New8(CxTSR(bus),tsr_read,tsr_write,mcan);
	IOH_New8(CxAFS(bus),afs_read,afs_write,mcan);
	for(slot=0;slot<16;slot++) {
		int i;
		for(i=0;i<16;i++) {
			IOH_New8(CxMSGBOX(bus,slot)+i,msgbox_read,msgbox_write,mcan);
		}
		IOH_New8(CxMCTL(bus,slot),mctl_read,mctl_write,mcan); 
	}
	IOH_New8(CxGMR0(bus),gmr_read,gmr_write,mcan);
	IOH_New8(CxGMR1(bus),gmr_read,gmr_write,mcan);
	IOH_New8(CxGMR2(bus),gmr_read,gmr_write,mcan);
	IOH_New8(CxGMR3(bus),gmr_read,gmr_write,mcan);
	IOH_New8(CxGMR4(bus),gmr_read,gmr_write,mcan);

	IOH_New8(CxMAR0(bus),mar_read,mar_write,mcan);
	IOH_New8(CxMAR1(bus),mar_read,mar_write,mcan);
	IOH_New8(CxMAR2(bus),mar_read,mar_write,mcan);
	IOH_New8(CxMAR3(bus),mar_read,mar_write,mcan);
	IOH_New8(CxMAR4(bus),mar_read,mar_write,mcan);

	IOH_New8(CxMBR0(bus),mbr_read,mbr_write,mcan);
	IOH_New8(CxMBR1(bus),mbr_read,mbr_write,mcan);
	IOH_New8(CxMBR2(bus),mbr_read,mbr_write,mcan);
	IOH_New8(CxMBR3(bus),mbr_read,mbr_write,mcan);
	IOH_New8(CxMBR4(bus),mbr_read,mbr_write,mcan);
}

static void
CAN_UnMap(void *module_owner,uint32_t base,uint32_t mapsize)
{
}

BusDevice *
M16CCAN_New(const char *can_name,int bus_nr)
{
        M16C_CAN *mcan = sg_new(M16C_CAN);

	mcan->bus_nr = bus_nr;
        mcan->bdev.first_mapping=NULL;
        mcan->bdev.Map=CAN_Map;
        mcan->bdev.UnMap=CAN_UnMap;
        mcan->bdev.owner=mcan;
        mcan->bdev.hw_flags=MEM_FLAG_READABLE | MEM_FLAG_WRITABLE;
        fprintf(stderr,"Created M16C CAN controller \"%s\"\n",can_name);
        return &mcan->bdev;
}

