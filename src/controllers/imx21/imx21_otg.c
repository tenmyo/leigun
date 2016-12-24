/*
 *************************************************************************************************
 * Emulation of Freescale iMX21 USB OTG module 
 *
 * state: Host mode is working 
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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
#include "bus.h"
#include "fio.h"
#include "signode.h"
#include "imx21_otg.h"
#include "configfile.h"
#include "i2c.h"
#include "isp1301.h"
#include "usbproto.h"
#include "usbdevice.h"
#include "cycletimer.h"
#include "sgstring.h"

#if 0
#define dbgprintf(...) fprintf(stderr,__VA_ARGS__)
#else
#define dbgprintf(...)
#endif
/* otg is at  0x10024000 and 0x10025000 */

#define OTG_HWMODE(base) 	((base) + 0x00)
#define		HWMODE_FUNCREV_MASK	(0xff<<24)
#define		HWMODE_FUNCREF_SHIFT	(24)
#define		HWMODE_HSTREF_MASK	(0xff<<16)
#define		HWMODE_HSTREF_SHIFT	(16)
#define		HWMODE_ANASDBEN		(1<<14)
#define		HWMODE_OTGXVCR_MASK	(3<<6)
#define		HWMODE_OTGXVCR_SHIFT	(6)
#define		HWMODE_HOSTXVCR_MASK	(3<<4)
#define		HWMODE_HOSTXVCR_SHIFT	(4)
#define		HWMODE_CRECFG_MASK	(3<<0)
#define		HWMODE_CRECFG_SHIFT	(0)
#define		HWMODE_CRECFG_HOST_ONLY	(1<<0)
#define		HWMODE_CRECFG_FUNC_ONLY	(2<<0)
#define		HWMODE_CRECFG_HNP	(3<<0)

#define	OTG_CINT_STAT(base)	((base) + 0x04)
#define		CINT_STAT_ASHNPINT	(1<<5)
#define		CINT_STAT_ASFCINT	(1<<4)
#define		CINT_STAT_ASHCINT	(1<<3)
#define		CINT_STAT_HNPINT	(1<<2)
#define		CINT_STAT_FCINT		(1<<1)
#define		CINT_STAT_HCINT		(1<<0)

#define	OTG_CINT_STEN(base)	((base) + 0x08)
#define		CINT_STEN_ASHNPINTEN	(1<<5)
#define		CINT_STEN_ASFCINTEN	(1<<4)
#define		CINT_STEN_ASHCINTEN	(1<<3)
#define		CINT_STEN_HNPINTEN	(1<<2)
#define		CINT_STEN_FCINTEN	(1<<1)
#define		CINT_STEN_HCINTEN	(1<<0)

#define OTG_CLK_CTRL(base)	((base) + 0x0c)
#define		CLK_CTRL_FUNCCLK	(1<<2)
#define		CLK_CTRL_HSTCLK		(1<<1)
#define		CLK_CTRL_MAINCLK	(1<<0)

#define OTG_RST_CTRL(base)	((base) + 0x10)
#define		RST_CTRL_RSTI2C		(1<<15)
#define		RST_CTRL_RSTCTRL	(1<<5)
#define		RST_CTRL_RSTFC		(1<<4)
#define		RST_CTRL_RSTFSKE	(1<<3)
#define		RST_CTRL_RSTRH		(1<<2)
#define		RST_CTRL_RSTHSIE	(1<<1)
#define		RST_CTRL_RSTHC		(1<<0)

#define OTG_FRM_INVTL(base)	((base) + 0x14)
#define		FRM_INVTL_FRMINTPER_MASK	(0x3fff<<16)
#define		FRM_INVTL_FRMINTPER_SHIFT	(16)
#define		FRM_INVTL_RSTFRM		(1<<15)
#define		FRM_INVTL_FRMINT_MASK		(0x3fff)
#define		FRM_INVTL_FRMINT_SHIFT		(0)

#define OTG_FRM_REMAIN(base)	((base) + 0x18)
#define		FRM_REMAIN_FRMREMN_MASK		(0x3fff<16)
#define		FRM_REMAIN_FRMREMN_SHIFT	(16)

#define OTG_HNP_CTRL(base)	((base) + 0x1c)
#define		HNP_CTRL_HNPDAT		(1<<30)
#define		HNP_CTRL_VBUSBSE	(1<<29)
#define		HNP_CTRL_VBUSABSV	(1<<28)
#define		HNP_CTRL_VBUSGTAVV	(1<<27)
#define		HNP_CTRL_SLAVE		(1<<22)
#define		HNP_CTRL_MASTER		(1<<21)
#define		HNP_CTRL_BGEN		(1<<20)
#define		HNP_CTRL_CMPEN		(1<<19)
#define		HNP_CTRL_ISBDEV		(1<<18)
#define		HNP_CTRL_ISADEV		(1<<17)
#define		HNP_CTRL_SWVBUSPUL	(1<<15)
#define		HNP_CTRL_SWAUTORST	(1<<12)
#define		HNP_CTRL_SWPUDP		(1<<11)
#define		HNP_CTRL_SWPDDM		(1<<9)
#define		HNP_CTRL_CLRERROR	(1<<3)
#define		HNP_CTRL_ADROPBUS	(1<<2)
#define		HNP_CTRL_ABBUSREQ	(1<<1)

#define	OTG_HNP_INT_STAT(base)  ((base) + 0x2c)
#define		HNP_INT_STAT_I2COTGINT	(1<<15)
#define		HNP_INT_STAT_SRPINT	(1<<5)
#define		HNP_INT_STAT_ABSESVALID	(1<<3)
#define		HNP_INT_STAT_AVBUSVALID	(1<<2)
#define		HNP_INT_STAT_IDCHANGE	(1<<0)

#define	OTG_HNP_INT_EN(base) 	((base) + 0x30)
#define		HNP_INT_EN_I2COTGINTEN	(1<<15)
#define		HNP_INT_EN_SRPINTEN	(1<<5)
#define		HNP_INT_EN_ABSESVALIDEN (1<<3)
#define		HNP_INT_EN_AVBUSVALIDEN	(1<<2)
#define		HNP_INT_EN_IDCHANGEEN	(1<<0)

#define OTG_USBCTRL(base)	((base) + 0x600)
#define		USBCTRL_I2CWUINTSTAT	(1<<27)
#define		USBCTRL_OTGWUINTSTAT	(1<<26)
#define		USBCTRL_HOSTWUINTSTAT	(1<<25)
#define		USBCTRL_FNTWUINTSTAT	(1<<24)
#define		USBCTRL_I2CWUINTEN	(1<<19)
#define		USBCTRL_OTGWUINTEN	(1<<18)
#define		USBCTRL_HOSTWUINTEN	(1<<17)
#define		USBCTRL_FNTWUINTEN	(1<<16)
#define		USBCTRL_OTGRCVRXDP	(1<<15)
#define		USBCTRL_HOST1BYPTLL	(1<<14)
#define		USBCTRL_OTGBYPVAL_MASK		(3<<10)
#define		USBCTRL_OTGBYPVAL_SHIFT		(10)
#define		USBCTRL_HOST1BYPVAL_MASK 	(3<<8)
#define		USBCTRL_HOST1BYPVAL_SHIFT 	(8)
#define		USBCTRL_OTGPWRMASK		(1<<6)
#define		USBCTRL_HOST1PWRMASK		(1<<5)
#define		USBCTRL_HOST2PWRMASK		(1<<4)
#define		USBCTRL_USBBYP			(1<<2)
#define		USBCTRL_HOST1TXENOE		(1<<1)

#define FUNC_COMSTAT(base)	((base) + 0x40)
#define		COMSTAT_SOFTRESET	(1<<7)
#define		COMSTAT_BADISOAP	(1<<3)
#define		COMSTAT_SUSPDET		(1<<2)
#define		COMSTAT_RSMINPROC	(1<<1)
#define		COMSTAT_RESETDET	(1<<0)
#define	FUNC_DEVADDR(base)	((base) + 0x44)
#define		DEVADDR_MASK	(0x7f)

#define	FUNC_SYSINTSTAT(base)	((base) + 0x48)
#define		SYSINTSTAT_SOFDETINT		(1<<4)
#define		SYSINTSTAT_DONEREGINT		(1<<3)
#define		SYSINTSTAT_SUSPDETINT		(1<<2)
#define		SYSINTSTAT_RSMFININT		(1<<1)
#define		SYSINTSTAT_RESETINT		(1<<0)

#define	FUNC_SYSINTEN(base)	((base) + 0x4c)
#define		SYSINTEN_SOFDETIEN		(1<<4)
#define		SYSINTEN_DONEREGIEN		(1<<3)
#define		SYSINTEN_SUSPDETIEN		(1<<2)
#define		SYSINTEN_RSMFINIEN		(1<<1)
#define		SYSINTEN_RESETIEN		(1<<0)

#define FUNC_XBUFINTSTAT(base)	((base) + 0x50)
#define FUNC_YBUFINTSTAT(base)  ((base) + 0x54)
#define FUNC_XYINTEN(base)	((base) + 0x58)
#define FUNC_XFILLSTAT(base)	((base) + 0x5c)
#define FUNC_YFILLSTAT(base)	((base) + 0x60)
#define	FUNC_ENDPNTEN(base)	((base) + 0x64)
#define FUNC_ENDPNRDY(base)		((base) + 0x68)
#define FUNC_IMMEDINT(base)	((base) + 0x6c)
#define FUNC_EPNTDONESTAT(base)	((base) + 0x70)
#define FUNC_EPNTDONEEN(base)	((base) + 0x74)
#define FUNC_EPNTTOGBITS(base)	((base) + 0x78)
#define FUNC_FNEPRDYCLR(base)	((base) + 0x7c)

#define HOST_CTRL(base)		((base) + 0x80)
#define		CTRL_HCRESET		(1<<31)
#define		CTRL_SCHEDOVR_MASK	(3<<16)
#define		CTRL_SCHEDOVR_SHIFT	(16)
#define		CTRL_RMTWUEN		(1<<4)
#define		CTRL_HCUSBSTE_MASK	(3<<2)
#define		CTRL_HCUSBSTE_SHIFT	(2)
#define         CTRL_HCUSBSTE_RESET  (0<<2)
#define         CTRL_HCUSBSTE_RESUME (1<<2)
#define         CTRL_HCUSBSTE_OPER      (2<<2)
#define         CTRL_HCUSBSTE_SUSP      (3<<2)
#define		CTRL_CTLBLKSR_MASK	(3)
#define		CTRL_CTLBLKSR_SHIFT	(0)

#define HOST_SYSISR(base)	((base) + 0x88)
#define		SYSISR_PSCINT		(1<<6)
#define		SYSISR_FMOFINT		(1<<5)
#define		SYSISR_HERRINT		(1<<4)
#define		SYSISR_RESDETINT	(1<<3)
#define		SYSISR_SOFINT		(1<<2)
#define		SYSISR_DONEINT		(1<<1)
#define		SYSISR_SORINT		(1<<0)

#define HOST_SYSIEN(base)	((base) + 0x8c)
#define		SYSIEN_PSCINT		(1<<6)
#define		SYSIEN_FMOFINT		(1<<5)
#define		SYSIEN_HERRINT		(1<<4)
#define		SYSIEN_RESDETINT	(1<<3)
#define		SYSIEN_SOFINT		(1<<2)
#define		SYSIEN_DONEINT		(1<<1)
#define		SYSIEN_SORINT		(1<<0)

#define	HOST_XBUFSTAT(base)	((base) + 0x98)
#define HOST_YBUFSTAT(base)	((base) + 0x9c)
#define HOST_XYINTEN(base) 	((base) + 0xa0)
#define HOST_XFILLSTAT(base)	((base) + 0xa8)
#define HOST_YFILLSTAT(base)	((base) + 0xac)
#define HOST_ETDENSET(base)	((base) + 0xc0)
#define HOST_ETDENCLR(base) 	((base) + 0xc4)
#define HOST_IMMEDINT(base)	((base) + 0xcc)
#define	HOST_ETDDONESTAT(base)	((base) + 0xd0)
#define HOST_ETDDONEN(base)	((base) + 0xd4)
#define HOST_FRMNUMB(base)	((base) + 0xe0)
#define	HOST_LSTHRESH(base)	((base) + 0xe4)
#define	HOST_ROOTHUBA(base)	((base) + 0xe8)
#define		ROOTHUBA_PWRTOGOOD_MASK		(0xff<<24)
#define		ROOTHUBA_PWRTOGOOD_SHIFT	(24)
#define		ROOTHUBA_NOOVRCURP		(1<<12)
#define		ROOTHUBA_OVRCURPM		(1<<11)
#define		ROOTHUBA_DEVTYPE		(1<<10)
#define		ROOTHUBA_PWRSWTMD		(1<<9)
#define		ROOTHUBA_NOPWRSWT		(1<<8)
#define		ROOTHUBA_NDNSTMPRT_MASK		(0xff)
#define		ROOTHUBA_NDNSTMPRT_SHIFT	(0)
#define HOST_ROOTHUBB(base)	((base) + 0xec)
#define		ROOTHUBB_PRTPWRCM_MASK		(0xff<<16)
#define		ROOTHUBB_PRTPWRCM_SHIFT		(16)
#define		ROOTHUBB_DEVREMOVE_MASK		(0xff)
#define		ROOTHUBB_DEVREMOVE_SHIFT	(0)

#define HOST_ROOTSTAT(base)	((base) + 0xf0)
#define		ROOTSTAT_CLRRMTWUE	(1<<31)
#define		ROOTSTAT_OVRCURCHG	(1<<17)
#define		ROOTSTAT_DEVCONWUE	(1<<15)
#define		ROOTSTAT_OVRCURI	(1<<1)
#define		ROOTSTAT_LOCPWRS	(1<<0)

#define HOST_PORTSTAT1(base)	((base) + 0xf4)
#define HOST_PORTSTAT2(base)	((base) + 0xf8)
#define HOST_PORTSTAT3(base)	((base) + 0xfc)
#define		PORTSTAT_PRTRSTSC	(1<<20)
#define		PORTSTAT_OVRCURIC	(1<<19)
#define		PORTSTAT_PRTSTATSC	(1<<18)
#define		PORTSTAT_PRTENBLSC	(1<<17)
#define		PORTSTAT_CONNECTSC	(1<<16)
#define		PORTSTAT_LSDEVCON	(1<<9)
#define		PORTSTAT_PRTPWRST	(1<<8)
#define		PORTSTAT_PRTRSTST	(1<<4)
#define		PORTSTAT_PRTOVRCURI	(1<<3)
#define		PORTSTAT_PRTSUSPST	(1<<2)
#define		PORTSTAT_PRTENABST	(1<<1)
#define		PORTSTAT_CURCONST	(1<<0)
/* Write port status */
#define		PORTSTAT_CLRPWRST	(1<<9)
#define		PORTSTAT_SETPWRST	(1<<8)
#define		PORTSTAT_SETPRTRST	(1<<4)
#define		PORTSTAT_CLRSUSP	(1<<3)
#define		PORTSTAT_SETSUSP	(1<<2)
#define		PORTSTAT_SETPRTENAB	(1<<1)
#define		PORTSTAT_CLRPRTENAB	(1<<0)

/* 
 * --------------------------------------------------------------------
 *   I2C Block: All registers are 8 Bit wide only 
 *   The manual seems to be totally wrong , better use
 *   Philips ISP 1301 documentation 
 * --------------------------------------------------------------------
 */
#define TRANS_VENODR_ID_LOW(base)		((base) + 0x100)
#define TRANS_VENDOR_ID_HIGH(base)		((base) + 0x101)
#define TRANS_PRODUCT_ID_LOW(base)		((base) + 0x102)
#define	TRANS_PRODUCT_ID_HIGH(base)		((base) + 0x103)
#define	TRANS_MODE_CONTROL_SET(base)		((base) + 0x104)
#define TRANS_MODE_CONTROL_CLR(base)		((base) + 0x105)
#define	TRANS_OTG_CONTROL_SET(base)		((base) + 0x106)
#define	TRANS_OTG_CONTROL_CLR(base)		((base) + 0x107)
#define TRANS_INTERRUPT_SRC(base)		((base) + 0x108)
#define	TRANS_INTLAT_SET(base)			((base) + 0x10a)
#define	TRANS_INTLAT_CLR(base)			((base) + 0x10b)
#define	TRANS_INTMSK_FALSE_SET(base)		((base) + 0x10c)
#define TRANS_INTMSK_FALSE_CLR(base)		((base) + 0x10d)
#define TRANS_INTMSK_TRUE_SET(base)		((base) + 0x10e)
#define TRANS_INTMSK_TRUE_CLR(base)		((base) + 0x10f)
#define	TRANS_INTLATCH_SET(base)		((base) + 0x112)
#define	TRANS_INTLATCH_CLR(base)		((base) + 0x113)

#define I2C_OTGDEVADDR(base)			((base) + 0x118)
#define I2C_NUMSEQOPS(base)			((base) + 0x119)
#define	I2C_SEQREADSTRT(base)			((base) + 0x11a)
#define I2C_OPCTRL(base)			((base) + 0x11b)
#define		OPCTRL_I2CBUSY		(1<<7)
#define		OPCTRL_HWSWMODE		(1<<1)
#define		OPCTRL_I2COE		(1<<0)
#define I2C_DIVFACTOR(base)			((base) + 0x11e)
#define	I2C_INTANDCTRL(base)			((base) + 0x11f)

#define ETD_BASE(base)	((base) + 0x200)
#define ETD_ADDR(base,n)	((base) + 0x200 + (n) * 16)

#define EP_BASE(base)	((base) + 0x400)

/* The AHB DMA block */
#define DMA_REV(base)		((base) + 0x800)
#define DMA_INTSTAT(base)	((base) + 0x804)
#define		DMAINTSTAT_EPERR	(1<<1)
#define		DMAINTSTAT_ETDERR	(1<<0)
#define DMA_INTEN(base)		((base) + 0x808)
#define		DMAINTEN_EPERRINTEN	(1<<1)
#define		DMAINTEN_ETDERRINTEN	(1<<0)
#define	DMA_ETDDMAERSTAT(base) 	((base) + 0x80c)
#define DMA_EPDMAERSTAT(base)	((base) + 0x810)
#define DMA_ETDDMAEN(base)	((base) + 0x820)
#define DMA_EPDMAEN(base)	((base) + 0x824)
#define DMA_ETDDMAXTEN(base)	((base) + 0x828)
#define DMA_EPDMAXTEN(base)	((base) + 0x82c)
#define DMA_ETDDMAENXYT(base)	((base) + 0x830)
#define	DMA_EPDMAENXYT(base)	((base) + 0x834)
#define DMA_ETCDMABST4EN(base)  ((base) + 0x838)
#define DMA_EPDMABST4EN(base)	((base) + 0x83c)
#define	DMA_MISCCONTROL(base)	((base) + 0x840)
#define		MISCCONTROL_ISOPREVFRM		(1<<3)
#define		MISCCONTROL_SKPRTRY		(1<<2)
#define		MISCCONTROL_ARBMODE		(1<<1)
#define		MISCCONTROL_FILTCC		(1<<0)
#define DMA_ETDDMACHANCLR(base)	((base) + 0x848)
#define DMA_EPDMACHANCLR(base)	((base) + 0x84c)
#define DMA_ETDSMSA(base,n)		((base) + 0x900 + ((n)<<2))
#define DMA_EPSMSA(base,n)		((base) + 0x980 + ((n)<<2))
#define DMA_ETDDMABUFPTR(base,n) 	((base) + 0xa00 + ((n)<<2))
#define	DMA_EPDMABUFPTR(base,n)		((base) + 0xa80 + ((n)<<2))

#define DATA_MEM(base)		((base) + 0x1000)

/* 
 * -----------------------------------------------------------------
 * Completion codes from reference manual table 36-2
 * -----------------------------------------------------------------
 */

#define CC_NOERROR     (0)
#define CC_CRC          (1)
#define CC_BITSTUFF     (2)
#define CC_TGLMISMATCH  (3)
#define CC_STALL        (4)
#define CC_DEVNOTRESP   (5)
#define CC_PIDFAIL      (6)
#define CC_RESERVED1    (7)
#define CC_DATAOVER     (8)
#define CC_DATAUNDER    (9)
#define CC_ACK          (10)
#define CC_NAK         (11)
#define CC_BUFOVER      (12)
#define CC_BUFUNDER     (13)
#define CC_SCHEDOVER    (14)
#define CC_NOTACC       (15)	/* This is set by software before submission */

/* Endpoint Transfer descriptors */

#define DMARQ_ACTIVE 	(SIG_LOW)
#define DMARQ_PASSIVE 	(SIG_HIGH)

/* 
 * ------------------------------------------------------------------------
 * The Endpoint and Transfer descriptor
 * The first 4 dwords are defined by hardware and can not be
 * changed. 
 * ------------------------------------------------------------------------
 */
typedef struct Etd {
	uint32_t hwControl;
	uint32_t bufsrtad;
	uint32_t bufstat;
	uint32_t dword3;
	int nr;
	int xfill_level;	/* for in direction */
	int yfill_level;	/* for in direction */
	int buf_toggle;		/* decides if  X or Y-Buf  is used  as source / destination */
	uint32_t dmabufptr;	/* the current possition in dma buffer */
	uint32_t dma_len;	/* The totallen in the beginning when dmabufptr is reset to 0 */
} Etd;

/* The following fields are the same for Control/Bulk ISO and Interrupt Transfer descriptors */
#define HETD_SNDNAK 		(1<<30)
/* TOGCRY is only used when DATATOGL high bit is 0 , it is always written back */
#define HETD_TOGCRY		(1<<28)
#define	HETD_HALTED		(1<<27)
#define HETD_MAXPKTSIZ_MASK	(0x3ff << 16)
#define HETD_MAXPKTSIZ_SHIFT	(16)
#define HETD_FORMAT_MASK	(3<<14)
#define	HETD_FORMAT_SHIFT	(14)
#define         HETD_FORMAT_CONTROL     (0<<14)
#define         HETD_FORMAT_ISO         (1<<14)
#define         HETD_FORMAT_BULK        (2<<14)
#define         HETD_FORMAT_INTERRUPT   (3<<14)
#define HETD_SPEED		(1<<13)
#define         HETD_SPEED_LOW          (1<<13)
#define         HETD_SPEED_FULL         (0<<13)
#define HETD_DIRECT_MASK	(3<<11)
#define HETD_DIRECT_SHIFT	(11)
#define         HETD_DIRECT_FROM_TD     (0<<11)
#define         HETD_DIRECT_OUT         (1<<11)
#define         HETD_DIRECT_IN          (2<<11)
#define         HETD_DIRECT_FROM_TD_2   (3<<11)
#define HETD_ENDPNT_MASK	(0xf<<8)
#define HETD_ENDPNT_SHIFT	(8)
#define HETD_ADDRESS_MASK	(0x7f << 0)
#define HETD_ADDRESS_SHIFT	(0)

#define CBTD_YBUFSRTAD_MASK	(0xffff<<16)
#define CBTD_YBUFSRTAD_SHIFT	(16)
#define CBTD_XBUFSRTAD_MASK	(0xffff)
#define CBTD_XBUFSRTAB_SHIFT	(0)

#define CBITD_COMPCODE_MASK	(0xf << 28)
#define CBITD_COMPCODE_SHIFT	(28)
#define CBITD_ERRORCNT_MASK	(0xf << 24)
#define CBITD_ERRORCNT_SHIFT	(24)
#define CBITD_DATAOG_MASK	(3<<22)
#define CBITD_DATAOG_SHIFT	(22)
#define CBITD_DELAYINT_MASK	(7<<19)
#define CBITD_DELAYINT_SHIFT	(19)
#define CBITD_BUFROUND		(1<<18)
/* direction for the token */
#define CBITD_DIRPID_MASK	(3<<16)
#define CBITD_DIRPID_SHIFT	(16)
#define 	CBITD_DIRPID_SETUP         (0<<16)
#define 	CBITD_DIRPID_OUT           (1<<16)
#define 	CBITD_DIRPID_IN            (2<<16)

#define CBTD_RTRYDELAY_MASK	(0xff)
#define	CBTD_RTRYDELAY_SHIFT	(0)

#define CBITD_BUFSIZE_MASK	(0x7ff<<21)
#define	CBITD_BUFSIZE_SHIFT	(21)
#define CBITD_TOTBYECNT_MASK   (0x1fffff)
#define	CBITD_TOTBYECNT_SHIFT	(0)

/* Interrupt transfer descriptor */
#define ITD_YBUFSRTAD_MASK	(0xfffc<<16)
#define ITD_YBUFSRTAD_SHIFT	(16)
#define ITD_XBUFSRTAD_MASK	(0xfffc)
#define ITD_XBUFSRTAD_SHIFT	(0)

#define ITD_COMPCODE_MASK	(0xf << 28)
#define ITD_COMPCODE_SHIFT	(28)
#define ITD_ERRORCNT_MASK	(0xf << 24)
#define ITD_ERRORCNT_SHIFT	(24)
#define ITD_DATAOG_MASK		(3<<22)
#define ITD_DATAOG_SHIFT	(22)
#define ITD_DELAYINT_MASK	(7<<19)
#define ITD_DELAYINT_SHIFT	(19)
#define ITD_BUFROUND		(1<<18)
#define ITD_RELPOLPOS_MASK	(0xff<<8)
#define	ITD_RELPOLPOS_SHIFT	(8)
#define ITD_POLINTERV_MASK	(0xff)
#define ITD_POLINTERV_SHIFT	(0)

/* DWORD 1 */
#define ISOTD_YBUFSRTAD_MASK	(0xffff<<16)
#define ISOTD_YBUFSRTAD_SHIFT	(16)
#define ISOTD_XBUFSRTAD_MASK	(0xffff)
#define ISOTD_XBUFSRTAB_SHIFT	(0)

/* DWORD 2 */
#define ISOTD_COMPCODE_MASK	(0xf << 28)
#define ISOTD_COMPCODE_SHIFT	(28)
#define ISOTD_FRAMECNT		(1<<24)
#define ISOTD_DELAYINT_MASK	(7<<19)
#define ISOTD_DELAYINT_SHIFT	(19)
#define ISOTD_STARTFRM_MASK	(0xffff)
#define	ISOTD_STARTFRM_SHIFT	(0)

#define ISOTD_COMPCODE1_MASK	(0xf << 28)
#define ISOTD_COMPCODE1_SHIFT	(28)
#define	ISOTD_PKTLEN1_MASK	(0x3ff<<16)
#define	ISOTD_PKTLEN1_SHIFT	(16)
#define ISOTD_COMPCODE0_MASK	(0xf << 12)
#define ISOTD_COMPCODE0_SHIFT	(28)
#define	ISOTD_PKTLEN0_MASK	(0x3ff<<0)
#define	ISOTD_PKTLEN0_SHIFT	(0)

typedef struct IMXOtg IMXOtg;

typedef struct RootHubPort {
	IMXOtg *otg;		/* These roothub ports belongs to an otg */
	UsbDevice *usbdev;
	uint32_t portstat;
	CycleTimer port_reset_timer;
} RootHubPort;

#define RCVR_IDLE		(0)
#define RCVR_WAIT_ACK		(1)
#define RCVR_WAIT_DATA		(2)	/* Don't care about toggle */
#define RCVR_WAIT_DATA0		(3)
#define RCVR_WAIT_DATA1		(4)

/* The host microengine */
#define MAX_HCODE_LEN 		(128)
#define HSTACK_SIZE 	(8)
#define	HCMD_SEND_TOKEN			(0x01000000)
#define HCMD_WAIT_DATA			(0x03000000)
#define	HCMD_WAIT_HANDSHAKE		(0x04000000)
#define	HCMD_SETUP_HANDSHAKE		(0x05000000)
#define	HCMD_END			(0x06000000)
#define HCMD_NDELAY			(0x07000000)
#define HCMD_START_DATA			(0x08000000)
#define HCMD_DO_DATA			(0x09000000)
#define HCMD_SET_ADDR			(0x0a000000)
#define HCMD_SET_EPNT			(0x0b000000)
#define HCMD_SETUP_DATA_IN		(0x0c000000)
#define HCMD_WAIT_DATA_IN		(0x0d000000)
#define	HCMD_DO				(0x0e000000)
#define	HCMD_WHILE			(0x0f000000)
#define 	COND_ETD_NOT_DONE	(0x07)

typedef struct IMXUsbHost {
	CycleTimer frame_timer;
	IMXOtg *otg;
	uint32_t ctrl;
	uint32_t sysisr;
	uint32_t sysien;
	uint32_t xbufstat;
	uint32_t ybufstat;
	uint32_t xyinten;
	uint32_t xfillstat;
	uint32_t yfillstat;
	uint32_t etden;
	uint32_t immedint;
	uint32_t etddonestat;
	uint32_t etddoneen;
	uint32_t frmnumb;
	uint32_t lsthresh;
	uint32_t roothuba;
	uint32_t roothubb;
	uint32_t rootstat;

	/* 
	 * ----------------------------------------------------
	 * The microengine  for scheduling the USB packets 
	 * ----------------------------------------------------
	 */

	CycleTimer ndelayTimer;
	CycleTimer pktDelayTimer;
	uint32_t code[MAX_HCODE_LEN];
	int ip_stack[HSTACK_SIZE];
	int stackptr;
	Etd *currentEtd;	/* The ETD which is currently used by this microengine */
	int interp_running;
	int code_ip;		/* The instruction pointer */
	int code_wp;
	UsbPacket usbpkt;	/* Assembly and receive buffer */

	int token;
	RootHubPort port[3];
	SigNode *intUsbHost;
	Etd etd[32];
	uint8_t *data_mem;	/* share the data memory with function */
} IMXUsbHost;

struct IMXOtg {
	BusDevice bdev;
	ISP1301 *isp1301;
	/* level sensitive interrupts */
	SigNode *intUsbWkup;
	SigNode *intUsbMnp;
	SigNode *intUsbDma;
	SigNode *intUsbFunc;
	SigNode *intUsbCtrl;

	IMXUsbHost host;
	void (*pkt_receiver) (IMXOtg * otg, const UsbPacket * pkt);

	uint32_t otg_hwmode;
	uint32_t otg_cint_stat;
	uint32_t otg_cint_sten;
	uint32_t otg_clk_ctrl;
	uint32_t otg_rst_ctrl;
	uint32_t otg_frm_invtl;
	uint32_t otg_frm_remain;
	uint32_t otg_hnp_ctrl;
	uint32_t otg_hnp_int_stat;
	uint32_t otg_hnp_int_en;
	uint32_t otg_usbctrl;

	uint32_t func_comstat;
	uint32_t func_devaddr;
	uint32_t func_sysintstat;
	uint32_t func_sysinten;
	uint32_t func_xbufintstat;
	uint32_t func_ybufintstat;
	uint32_t func_xyinten;
	uint32_t func_xfillstat;
	uint32_t func_yfillstat;
	uint32_t func_endpnten;
	uint32_t func_endpnrdy;
	uint32_t func_immedint;
	uint32_t func_epntdonestat;
	uint32_t func_epntdoneen;
	uint32_t func_epnttogbits;
	uint32_t func_fneprdyclr;

	uint32_t dma_rev;
	uint32_t dma_intstat;
	uint32_t dma_inten;
	uint32_t dma_etddmaerstat;
	uint32_t dma_epdmaerstat;
	uint32_t dma_etddmaen;
	uint32_t dma_epdmaen;
	uint32_t dma_etddmaxten;
	uint32_t dma_epdmaxten;
	uint32_t dma_etddmaenxyt;
	uint32_t dma_epdmaenxyt;
	uint32_t dma_etcdmabst4en;
	uint32_t dma_epdmabst4en;
	uint32_t dma_misccontrol;
	uint32_t dma_etddmachanclr;
	uint32_t dma_epdmachanclr;
	uint32_t dma_etdsmsa[32];
	uint32_t dma_epsmsa[16];
	SigNode *etdXDmaNode[32];
	SigNode *etdYDmaNode[32];
	uint32_t dma_epdmabufptr_out[16];
	uint32_t dma_epdmabufptr_in[16];

	uint8_t i2c_otgdevaddr;
	uint8_t i2c_numseqops;
	uint8_t i2c_seqreadstrt;
	uint8_t i2c_opctrl;
	uint8_t i2c_divfactor;
	uint8_t i2c_intandctrl;
	uint8_t ep_mem[512];
	uint8_t data_mem[4096];
};

/*
 * ------------------------------------------------------------------------
 * Write the completion code to dword 2 of an Etd 
 * ------------------------------------------------------------------------
 */
static inline void
etd_set_compcode(Etd * etd, int compcode)
{
	etd->bufstat =
	    (etd->bufstat & ~CBITD_COMPCODE_MASK) | ((compcode & 0xf) << CBITD_COMPCODE_SHIFT);
}

static void
update_host_interrupt(IMXOtg * otg)
{
	IMXUsbHost *host = &otg->host;
	if ((otg->otg_cint_sten & (CINT_STAT_HCINT | CINT_STAT_ASHCINT))
	    && (host->sysisr & host->sysien)) {
		SigNode_Set(host->intUsbHost, SIG_LOW);
		if (!(otg->otg_clk_ctrl & CLK_CTRL_HSTCLK)) {
			otg->otg_cint_stat |= CINT_STAT_ASHCINT;
		} else {
			otg->otg_cint_stat &= ~CINT_STAT_ASHCINT;
			otg->otg_cint_stat |= CINT_STAT_HCINT;
		}
	} else {
		SigNode_Set(host->intUsbHost, SIG_HIGH);
	}
}

/*
 * ------------------------------------------------------------------------
 *
 * Read memory and write it to the x/y buffes of an ETD
 *
 * ------------------------------------------------------------------------
 */
static void
etd_dma_mem_to_etdbuf(IMXOtg * otg, Etd * etd)
{
	uint32_t dmabufptr;
	uint32_t addr;
	uint32_t bufsize = ((etd->dword3 & CBITD_BUFSIZE_MASK) >> CBITD_BUFSIZE_SHIFT) + 1;
	int use_xbuf;
	uint32_t etdmask = (1 << etd->nr);
	int transfer_count = 0;
	while (etd->dmabufptr < etd->dma_len) {
		int len;
		dmabufptr = etd->dmabufptr;
		addr = otg->dma_etdsmsa[etd->nr] + dmabufptr;
		len = etd->dma_len - dmabufptr;
		if (len > bufsize) {
			len = bufsize;
		}
		if ((dmabufptr / bufsize) & 1) {
			use_xbuf = 0;
		} else {
			use_xbuf = 1;
		}
		if (use_xbuf) {
			uint32_t xbufsrtad = etd->bufsrtad & 0xffff;
			if ((xbufsrtad + len) > 4096) {
				dbgprintf("xbuf behind end of data ram: %04x\n", xbufsrtad + len);
				return;
			}
			if (otg->host.xfillstat & etdmask) {
				dbgprintf("Already xfilled\n");
				return;
			} else {
				Bus_Read(otg->data_mem + xbufsrtad, addr, len);
				otg->host.xfillstat |= etdmask;
				if (otg->host.xyinten & etdmask) {
					otg->host.xbufstat |= etdmask;
				}
				dbgprintf("Read X-Data\n");
				etd->dmabufptr += len;
				transfer_count += len;
			}
		} else {
			uint32_t ybufsrtad = etd->bufsrtad >> 16;
			if ((ybufsrtad + len) > 4096) {
				dbgprintf("ybuf behind end of data ram: %04x\n", ybufsrtad + len);
				return;
			}
			if (otg->host.yfillstat & etdmask) {
				dbgprintf("Already yfilled\n");
				return;
			} else {
				Bus_Read(otg->data_mem + ybufsrtad, addr, len);
				otg->host.yfillstat |= etdmask;
				if (otg->host.xyinten & etdmask) {
					otg->host.ybufstat |= etdmask;
				}
				dbgprintf("Read Y-Data\n");
				etd->dmabufptr += len;
				transfer_count += len;
			}
		}
	}
}

/*
 * ------------------------------------------------------------------------
 * ETD buffer to memory 
 * Do an dma from the etd to main memory as long as the buffer is filled
 * ------------------------------------------------------------------------
 */
static void
etd_dma_etdbuf_to_mem(IMXOtg * otg, Etd * etd)
{
	uint32_t dmabufptr;
	uint32_t addr;
	uint32_t bufsize = ((etd->dword3 & CBITD_BUFSIZE_MASK) >> CBITD_BUFSIZE_SHIFT) + 1;
	int use_xbuf;
	uint32_t etdmask = (1 << etd->nr);
	while (etd->dmabufptr < etd->dma_len) {
		int len;
		dmabufptr = etd->dmabufptr;
		addr = otg->dma_etdsmsa[etd->nr] + dmabufptr;
		len = etd->dma_len;
		if (len > bufsize) {
			len = bufsize;
		}
		if ((dmabufptr / bufsize) & 1) {
			use_xbuf = 0;
		} else {
			use_xbuf = 1;
		}
		if (use_xbuf) {
			int i;
			uint32_t xbufsrtad = etd->bufsrtad & 0xffff;
			if (!(otg->host.xfillstat & etdmask)) {
				break;
			}
			if (etd->xfill_level < len) {
				len = etd->xfill_level;
			}
			dbgprintf("* xbuf %04x cplen %d, dmaaddr %08x, dmabufptr %08x\n", xbufsrtad,
				  len, addr, etd->dmabufptr);
			if ((xbufsrtad + len) > 4096) {
				fprintf(stderr, "xbuf behind end of data ram: %04x\n",
					xbufsrtad + len);
				return;
			}
			for (i = 0; i < len; i++) {
				dbgprintf(stderr, "%02x ", *(otg->data_mem + xbufsrtad + i));
			}
			dbgprintf(stderr, "\n");
			Bus_Write(addr, otg->data_mem + xbufsrtad, len);
			otg->host.xfillstat &= ~etdmask;
			if (otg->host.xyinten & etdmask) {
				otg->host.xbufstat |= etdmask;
			}
			/* now next receive is allowed */
			etd->dmabufptr += len;
		} else {
			uint32_t ybufsrtad = etd->bufsrtad >> 16;
			if (!(otg->host.yfillstat & etdmask)) {
				break;
			}
			if (etd->yfill_level < len) {
				len = etd->yfill_level;
			}
			dbgprintf("* ybuf %04x cplen %d, dmaaddr %08x, dmabufptr %08x\n", ybufsrtad,
				  len, addr, etd->dmabufptr);
			if ((ybufsrtad + len) > 4096) {
				fprintf(stderr, "ybuf behind end of data ram: %04x\n",
					ybufsrtad + len);
				return;
			}
			Bus_Write(addr, otg->data_mem + ybufsrtad, len);
			otg->host.yfillstat &= ~etdmask;
			/* now next receive is allowed */
			if (otg->host.xyinten & etdmask) {
				otg->host.ybufstat |= (1 << etd->nr);
				// update_host_interrupt();
			}
			etd->dmabufptr += len;
		}
	}
}

static void
etd_do_dma(IMXOtg * otg, Etd * etd)
{
	int dir_out = 0;
	uint32_t direct, dirpid;
	if (!(otg->dma_etddmaen & (1 << etd->nr))) {
		dbgprintf("DMA is not enabled\n");
		return;
	}
	direct = (etd->hwControl & HETD_DIRECT_MASK);
	dirpid = (etd->bufstat & CBITD_DIRPID_MASK);
	if (direct == HETD_DIRECT_OUT) {
		dir_out = 1;
	} else if (direct == HETD_DIRECT_IN) {
		dir_out = 0;
	} else if (dirpid == CBITD_DIRPID_SETUP) {
		dir_out = 1;
	} else if (dirpid == CBITD_DIRPID_OUT) {
		dir_out = 1;
	} else if (dirpid == CBITD_DIRPID_IN) {
		dir_out = 0;
	} else {
		fprintf(stderr, "Can not determine direction of transfer\n");
		exit(1);
		return;
	}
	if (dir_out) {
		etd_dma_mem_to_etdbuf(otg, etd);
	} else {
		etd_dma_etdbuf_to_mem(otg, etd);
	}
}

/*
 * ---------------------------------------------------------------------------------------
 * etd_abort_dma
 *	Does currently nothing because DMA is atomic in first version of emulation
 * ---------------------------------------------------------------------------------------
 */
static void
etd_abort_dma(IMXOtg * otg, Etd * etd)
{
	return;
}

static void
update_func_interrupt(IMXOtg * otg)
{
	if ((otg->otg_cint_sten & (CINT_STAT_FCINT | CINT_STAT_ASFCINT))
	    && (otg->func_sysintstat & otg->func_sysinten)) {
		SigNode_Set(otg->intUsbFunc, SIG_LOW);
		if (!(otg->otg_clk_ctrl & CLK_CTRL_FUNCCLK)) {
			otg->otg_cint_stat |= CINT_STAT_ASFCINT;
		} else {
			otg->otg_cint_stat &= ~CINT_STAT_ASFCINT;
			otg->otg_cint_stat |= CINT_STAT_FCINT;
		}
	} else {
		SigNode_Set(otg->intUsbFunc, SIG_HIGH);
	}
}

static void
update_hnp_interrupt(IMXOtg * otg)
{
	if ((otg->otg_cint_sten & (CINT_STAT_HNPINT | CINT_STAT_ASHNPINT))
	    && (otg->otg_hnp_int_stat & otg->otg_hnp_int_en)) {
		SigNode_Set(otg->intUsbMnp, SIG_LOW);
		if (!(otg->otg_clk_ctrl & CLK_CTRL_MAINCLK)) {
			otg->otg_cint_stat |= CINT_STAT_ASHNPINT;
		} else {
			otg->otg_cint_stat &= ~CINT_STAT_ASHNPINT;
			otg->otg_cint_stat |= CINT_STAT_HNPINT;
		}
	} else {
		SigNode_Set(otg->intUsbMnp, SIG_HIGH);
	}
}

static void
update_dma_interrupt(IMXOtg * otg)
{
	if (otg->dma_intstat & otg->dma_inten) {
		SigNode_Set(otg->intUsbDma, SIG_LOW);
	} else {
		SigNode_Set(otg->intUsbDma, SIG_HIGH);
	}
}

static void
reset_i2c(IMXOtg * otg)
{
	otg->i2c_otgdevaddr = 0xac;
	otg->i2c_numseqops = 0;
	otg->i2c_seqreadstrt = 8;
	otg->i2c_opctrl = 0;
	otg->i2c_divfactor = 0x78;	/* 48 MHz / 0x78 = 400 kHz */
	otg->i2c_intandctrl = 0;
}

/*
 * --------------------------------------------------------------------------
 * reset_control
 *	reset the control block. The reset values are verified with
 *	real device
 * --------------------------------------------------------------------------
 */
static void
reset_control(IMXOtg * otg)
{
	otg->otg_hwmode = 0x202000a3;
	otg->otg_cint_stat = 0;
	otg->otg_cint_sten = 0;
	otg->otg_clk_ctrl = 0;
	otg->otg_frm_invtl = 0x2a2f2edf;
	otg->otg_frm_remain = 0x21c40000;	/* Manual seems to be wrong here */
	otg->otg_hnp_ctrl = 0x20040200;
	otg->otg_hnp_int_stat = 0;
	otg->otg_hnp_int_en = 0;
	otg->otg_usbctrl = 0x000f1000;	/* Manual seems to be wrong */
	update_hnp_interrupt(otg);
	// update_clocks
}

/*
 * -------------------------------------------------------------------------
 * USB Function reset
 *	All reset values are 0 except first register 
 * 	(verified with real device 0x40-0x7f)
 * -------------------------------------------------------------------------
 */
static void
reset_func(IMXOtg * otg)
{
	otg->func_comstat = 1;
	otg->func_devaddr = 0;
	otg->func_sysintstat = 0;
	otg->func_sysinten = 0;
	otg->func_xbufintstat = 0;
	otg->func_ybufintstat = 0;
	otg->func_xyinten = 0;
	otg->func_xfillstat = 0;
	otg->func_yfillstat = 0;
	otg->func_endpnten = 0;
	otg->func_endpnrdy = 0;
	otg->func_immedint = 0;
	otg->func_epntdonestat = 0;
	otg->func_epntdoneen = 0;
	otg->func_epnttogbits = 0;
	otg->func_fneprdyclr = 0;
	update_func_interrupt(otg);
}

static void
reset_funcser(IMXOtg * otg)
{
	fprintf(stderr, "IMX21 OTG: Function serial reset not implemented\n");
}

static void
reset_roothub(IMXOtg * otg)
{
	int i;
	for (i = 0; i < 3; i++) {
		RootHubPort *rhp = &otg->host.port[i];
		CycleTimer_Remove(&rhp->port_reset_timer);
	}
	fprintf(stderr, "IMX21 OTG: roothub reset not implemented\n");
}

static void
reset_hostser(IMXOtg * otg)
{
	fprintf(stderr, "IMX21 OTG: hostser reset not implemented\n");
}

static void
reset_hc(IMXOtg * otg)
{
	int i;
	otg->host.ctrl = 0;
	otg->host.sysisr = 0;
	otg->host.sysien = 0;
	otg->host.xbufstat = 0;
	otg->host.ybufstat = 0;
	otg->host.xyinten = 0;
	otg->host.xfillstat = 0;
	otg->host.yfillstat = 0;
	otg->host.etden = 0;
	otg->host.immedint = 0;
	otg->host.etddonestat = 0;
	otg->host.etddoneen = 0;
	otg->host.frmnumb = 0;
	otg->host.lsthresh = 0x628;
//warning for debugging reduced to 1 roothub port
	otg->host.roothuba = 0x01000103;	/* 3 Roothub ports */
	//otg->host.roothuba = 0x01000101; 

	otg->host.roothubb = 0x00070000;
	otg->host.rootstat = 0x00070000;
	for (i = 0; i < 3; i++) {
		RootHubPort *rhp = &otg->host.port[i];
		rhp->portstat &= PORTSTAT_CURCONST;
	}
	update_host_interrupt(otg);
}

static void
reset_dma(IMXOtg * otg)
{
	int i;
	otg->dma_rev = 0x10;
	otg->dma_intstat = 0;
	otg->dma_inten = 0;
	otg->dma_etddmaerstat = 0;
	otg->dma_epdmaerstat = 0;
	otg->dma_etddmaen = 0;
	otg->dma_epdmaen = 0;
	otg->dma_etddmaxten = 0;
	otg->dma_epdmaxten = 0;
	otg->dma_etddmaenxyt = 0;
	otg->dma_epdmaenxyt = 0;
	otg->dma_etcdmabst4en = 0xffffffff;	/* ???? contradictions ! doku writer was drunken */
	otg->dma_epdmabst4en = 0xffffffff;	/* ???? contradictions ! doku writer was drunken */
	otg->dma_misccontrol = 0;
	otg->dma_etddmachanclr = 0;
	otg->dma_epdmachanclr = 0;	/* doku: cut copy paste bug from etddmachanclr */
	for (i = 0; i < 32; i++) {
		Etd *etd = &otg->host.etd[i];
		otg->dma_etdsmsa[i] = 0;
		etd->dmabufptr = 0;
	}
	for (i = 0; i < 16; i++) {
		otg->dma_epsmsa[i] = 0;
		otg->dma_epdmabufptr_out[i] = 0;
		otg->dma_epdmabufptr_in[i] = 0;
	}
	update_dma_interrupt(otg);
}

/*
 * ----------------------------------------------------------------------------------------------
 * HWMODE
 *
 *  Bit 24-31 FUNCREV Function revision. fixed to 0x20
 *  Bit 16-23 HSTREF  Host revision. fixed to 0x20
 *  Bit 14    ANASDBEN Analog signal short debounce enable
 *  Bit 6     OTGXVCR  OTG transceiver properties
 *		00: differential TX/RX
 *		01: single ended TX, differential RX
 *		10: differential TX, single ended RX
 *		11: single ended TX/RX
 *
 *  Bit 4-5   HOSTXVCR 	Host transceiver properties, values like OTGXVCR
 *  Bit 0-1   CRECFG	USB OTG module configuration	
 *		00: reserved
 *		01: host only operation
 *		10: function operation (no HNP)
 *		11: Software HNP
 *
 * ----------------------------------------------------------------------------------------------
 */

static uint32_t
otg_hwmode_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->otg_hwmode;
}

static void
otg_hwmode_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG HWMODE: no effect implemented\n");
	otg->otg_hwmode = (otg->otg_hwmode & ~0xf3) | (value & 0xf3);
	return;
}

/*
 * ------------------------------------------------------------------------------
 * OTG_CINT_STAT USB Interrupt status register
 *
 *   Bit 5: ASHNPINT	Asynchronous HNP Interrupt
 *   Bit 4: ASFCINT	Asynchronous Function Interrupt
 *   Bit 3: ASHCINT	Asynchronous Host Interrupt
 *   Bit 2: HNPINT	HNP Interrupt
 *   Bit 1: FCINT	Function Interrupt
 *   Bit 0: HCINT	Host Interrupt
 * ------------------------------------------------------------------------------
 */

static uint32_t
otg_cint_stat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->otg_cint_stat;
}

static void
otg_cint_stat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	//fprintf(stderr,"IMX21 OTG: 0x%08x not implemented\n",address);
	return;
}

/*
 * ------------------------------------------------------------------
 * OTG_CINT_STEN USB Interrupt enable registers
 *
 *  Bit 5: ASHNPINTEN	Asynchronous HNP interrupt enable
 *  Bit 4: ASFCINTEN	Asynchronous Function interrupt enable
 *  Bit 3: ASHCINTEN 	Asynchronous Host interrupt enable
 *  Bit 2: HNPINTEN 	HNP interrupt enable
 *  Bit 1: FCINTEN	Function interrupt enable
 *  Bit 0: HCINTEN	Host interrupt enable
 * ------------------------------------------------------------------
 */

static uint32_t
otg_cint_sten_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->otg_cint_sten;
}

static void
otg_cint_sten_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->otg_cint_sten = value;
	update_host_interrupt(otg);
	update_func_interrupt(otg);
	update_hnp_interrupt(otg);
	return;
}

/*
 * ----------------------------------------------------------------------------------------
 *
 * OTG_CLK_CTRL
 *
 *  Bit 2: FUNCCLK 	Function clock
 *  Bit 1: HSTCLK	Host clock	
 *  Bit 0: MAINCLK	Main clock. Can not be cleared when func or host clock is enabled
 *
 * -----------------------------------------------------------------------------------------
 */

static uint32_t
otg_clk_ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->otg_clk_ctrl;
}

static void
otg_clk_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	if (value & CLK_CTRL_FUNCCLK) {
		otg->otg_cint_stat &= ~CINT_STAT_ASFCINT;
	}
	if (value & CLK_CTRL_HSTCLK) {
		otg->otg_cint_stat &= ~CINT_STAT_ASHCINT;
	}
	if (value & CLK_CTRL_MAINCLK) {
		otg->otg_cint_stat &= ~CINT_STAT_ASHNPINT;
	}
	otg->otg_clk_ctrl = value;
	fprintf(stderr, "IMX21 OTG: CLK control not fully implemented\n");
	return;
}

/*
 * --------------------------------------------------------------------------------
 * OTG_RST_CTRL Reset control register
 *
 *  Bit 15: RSTI2C 	Reset the I2C controller 
 *  Bit 5:  RSTCTRL	Reset the control logic (selfclr)
 *  Bit 4:  RSTFC	Reset the function conroller (selfclr)
 *  Bit 3:  RSTFSKE	Reset the function serial engine (selfclr)
 *  Bit 2:  RSTRH	Reset roothub (selfclr)
 *  Bit 1:  RSTHSIE	Reset host serial engine
 *  Bit 0:  RSTHC	Reset host controller
 *
 * --------------------------------------------------------------------------------
 */

static uint32_t
otg_rst_ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->otg_rst_ctrl;
}

static void
otg_rst_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	if (value & RST_CTRL_RSTI2C) {
		reset_i2c(otg);
	}
	if (value & RST_CTRL_RSTCTRL) {
		reset_control(otg);
	}
	if (value & RST_CTRL_RSTFC) {
		reset_func(otg);
	}
	if (value & RST_CTRL_RSTFSKE) {
		reset_funcser(otg);
	}
	if (value & RST_CTRL_RSTRH) {
		reset_roothub(otg);
	}
	if (value & RST_CTRL_RSTHSIE) {
		reset_hostser(otg);
	}
	if (value & RST_CTRL_RSTHC) {
		reset_hc(otg);
	}
	otg->otg_rst_ctrl = 0;	/* should be delayed by some microseconds */
	return;
}

/*
 * -----------------------------------------------------------------------------------
 * FRM_INVTL	Frame Interval register 
 *
 *   Bit 16-29:	FRMINTPER Frame interval periodic
 *   Bit 15:	RSTFRM	  Reset frame
 *   Bit 0-13:  FRMINT	  frame interval (SOF spacing)
 * -----------------------------------------------------------------------------------
 */
static uint32_t
otg_frm_invtl_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
otg_frm_invtl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ------------------------------------------------------------------------
 * OTG FRM_REMAIN
 *	Bits 16-29: FRM_REMAIN Bits remaining to next frame (SOF)
 * ------------------------------------------------------------------------
 */

static uint32_t
otg_frm_remain_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
otg_frm_remain_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * -----------------------------------------------------------------------------------------
 * OTG_HNP_CTRL	HNP control/status register
 *
 *  Bit 30: HNPDAT	HNP data toggle
 *  Bit 29: VBUSBSE	V bus greater than B session end
 *  Bit 28: VBUSABSV	V bus A B session valid
 *  Bit 27: VBUSGTAVV	V bus greater than A V bus valid
 *  Bit 22: SLAVE	HNP slave state: OTG working as function
 *  Bit 21: MASTER	HNP master state: OTG working as host
 *  Bit 20: BGEN	Enables charge pump band gap
 *  Bit 19: CMPEN	Enables the charge pump comparator
 *  Bit 18: ISBDEV	device is a B device (ID pin)
 *  Bit 17: ISADEV	device is a A device (ID pin)
 *  Bit 15: SWVBUSPUL	Software V Bus pulse enable
 *  Bit 12: SWAUTORST	Software automatic reset (selfclearing on USB reset)
 *  Bit 11: SWPUDP	Software pull up 
 *  Bit  9: SWPDDM	Software pull down
 *  Bit  3: CLRERROR 	HNP clear error state (selfclr ??????)	
 *  Bit  2: ADROPBUS	A Drop V Bus: A device wants to drop V-Bus
 *  Bit  1: ABBUSREQ	A B bus request: Software requests to be an a or an b master
 *
 * -----------------------------------------------------------------------------------------
 */

static uint32_t
otg_hnp_ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
otg_hnp_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ----------------------------------------------------------------------
 * OTG_HNP_INT_STAT 
 *   Bit 15: I2COTGINT 		Interrupt from I2C OTG transceiver
 *   Bit  5: SRPINT		Session request detect interrupt
 *   Bit  3: ABSESVALID		A B session valid change interrupt
 *   Bit  2: AVBUSVALID	 	AV Bus valid change interrupt
 *   Bit  0: IDCHANGE		State of ID pin has changed
 * ----------------------------------------------------------------------
 */
static uint32_t
otg_hnp_int_stat_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
otg_hnp_int_stat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ----------------------------------------------------------------------
 * OTG_HNP_INT_EN 
 *   Bit 15: I2COTGINTEN	enable interrupt from I2C OTG transceiver
 *   Bit  5: SRPINTEN		enable session request detect interrupt
 *   Bit  3: ABSESVALIDEN	enable A B session valid change interrupt
 *   Bit  2: AVBUSVALIDEN	enable AV Bus valid change interrupt
 *   Bit  0: IDCHANGEEN		enable id-change interrupt
 * ----------------------------------------------------------------------
 */
static uint32_t
otg_hnp_int_en_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
otg_hnp_int_en_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ------------------------------------------------------------------------------
 * OTG_USBCTRL	control the wakeup interrupts and mux logic
 *
 *  Bit 27:	I2CWUINTSTAT	I2C wake up interrupt status
 *  Bit 26:	OTGWUINTSTAT	OTG wake up interrupt status
 *  Bit 25:	HOSTWUINTSTAT	Host wake up interrupt status 
 *  Bit 24: 	FNTWUINTSTAT	Function wake up interrupt status
 *  Bit 19: 	I2CWUINTEN	I2C wake up interrupt enable
 *  Bit 18: 	OTGWUINTEN	OTG wake up interrupt enable
 *  Bit 17: 	HOSTWUINTEN	Host wake up interrupt enable
 *  Bit 16: 	FNTWUINTEN	Function wake up interrupt enable
 *  Bit 15: 	OTGRCVRXDP	OTG differential receiver connected to rxdp
 *  Bit 14: 	HOST1BYPTLL	Host 1 bypass transceiver less logic
 *  Bit 10-11:	OTGBYPVAL	OTG Bypass valid
 *  Bit 8-9: 	HOST1BYPVAL	Host 1 Bypass valid
 *  Bit 6: 	OTGPWRMASK	OTG power output pin mask
 *  Bit 5: 	HOST1PWRMASK	Host 1 power output pin mask
 *  Bit 4: 	HOST2PWRMASK	Host 2 power output pin mask
 *  Bit 2: 	USBBYP		USB Bypass enable
 *  Bit 1: 	HOST1TXENOE	Host 1 transmit enable output enable
 * ------------------------------------------------------------------------------
 */

static uint32_t
otg_usbctrl_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
otg_usbctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * --------------------------------------------------------------------
 * COMSTAT
 *
 *  Bit 7: SOFTRESET 	Trigger a software reset when set, read 0
 *  Bit 3: BADISOAP  	Accept bad isochronous packets
 *  Bit 2: SUSPDET   	Detect if USB is in suspended state
 *  Bit 1: RSMINPROC	Resume in progress
 *  Bit 0: RESETDET	USB Bus reset detected
 * --------------------------------------------------------------------
 */

static uint32_t
func_comstat_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_comstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ----------------------------------------------------------------------
 * DEVADDR
 *	Device address
 * ----------------------------------------------------------------------
 */

static uint32_t
func_devaddr_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_devaddr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ---------------------------------------------------------------------------
 * SYSINTSTAT
 *
 *  Bit  4: SOFDETINT	start of frame detect interrupt
 *  Bit  3: DONEREGINT	done register interrupt (Endpoint status register) 
 *  Bit  2: SUSPDETINT	suspend detect interrupt
 *  Bit  1: RSMFININT   resume finished interrupt
 *  Bit  0: RESETINT	reset detected interrupt
 * ---------------------------------------------------------------------------
 */

static uint32_t
func_sysintstat_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_sysintstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ------------------------------------------------------------------------------
 * SYSINTEN
 *
 *  Bit  4: SOFDETIEN 	start of frame detect interrupt enable
 *  Bit  3: DONEREGIEN	done register interrupt enable
 *  Bit  2: SUSPDETIEN	suspend detect interrupt enable
 *  Bit  1: RSMFINIEN	resume finished interrupt enable
 *  Bit  0: RESETIEN	reset detect interrupt enable
 * ------------------------------------------------------------------------------
 */

static uint32_t
func_sysinten_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_sysinten_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ------------------------------------------------------------------------------
 * XBUFINTSTAT X-Buffer Interrupt status
 * 	All odd bits:	XBUFININT  X-Buffer input interrupt
 *	All even bits:  XBUFOUTINT X-Buffer output interrupt
 *	
 * ------------------------------------------------------------------------------
 */

static uint32_t
func_xbufintstat_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_xbufintstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ------------------------------------------------------------------------------
 *
 * YBUFINTSTAT Y-Buffer Interrupt status
 *
 * 	All odd bits:	YBUFININT  Y-Buffer input interrupt
 *	All even bits:  YBUFOUTINT Y-Buffer output interrupt
 *	
 * ------------------------------------------------------------------------------
 */

static uint32_t
func_ybufintstat_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_ybufintstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * --------------------------------------------------------------------------------
 * XYINTEN
 *	XY Interrupt enable register
 *  All odd bits: XYBUFININT	
 *  All even bits: XYBUFOUTINT
 * --------------------------------------------------------------------------------
 */
static uint32_t
func_xyinten_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_xyinten_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * --------------------------------------------------------------------------------
 * XFILLSTAT X-Filled status
 *	XFILLIN	 
 *	XFILLOUT
 * --------------------------------------------------------------------------------
 */

static uint32_t
func_xfillstat_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_xfillstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * --------------------------------------------------------------------------------
 * YFILLSTAT Y-Filled status
 *	YFILLIN	 
 *	YFILLOUT
 * --------------------------------------------------------------------------------
 */

static uint32_t
func_yfillstat_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_yfillstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * --------------------------------------------------------------------------------
 * ENDPNTEN
 *	Endpoint enable register
 *
 * all odd bits:  EPINEN enable input endpoint
 * all even bits: EPODEN enable output endpoint
 * --------------------------------------------------------------------------------
 */

static uint32_t
func_endpnten_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_endpnten_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ---------------------------------------------------------------------------------
 * ENDPNRDY Endpoint ready set register
 *  all odd bits:  ENDPINRDY set in endpoint to ready
 *  all even bits: ENDPOUTRDY set out endpoint to ready
 * ---------------------------------------------------------------------------------
 */
static uint32_t
func_endpnrdy_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_endpnrdy_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ---------------------------------------------------------------------------------
 * IMMEDINT	
 *	Immediate interrupt register
 *  All odd bits: IMININT
 *  All even bits: IMOUTINT
 * ---------------------------------------------------------------------------------
 */

static uint32_t
func_immedint_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_immedint_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * -----------------------------------------------------------------------------------
 * EPNTDONESTAT
 *	Endpoint done status register	
 *
 *  All odd bits: 	EPINDONE input endpoint done
 *  All even bits: 	EPOUTDONE output endpoint done
 * -----------------------------------------------------------------------------------
 */
static uint32_t
func_epntdonestat_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_epntdonestat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ---------------------------------------------------------------------
 * EPNDONEEN
 *	Endpoint done interrupt enable
 *	Enables interupt on SOF or immediately
 * All odd bits: EPINDEN
 * All even bits: EPOUTDEN
 * ---------------------------------------------------------------------
 */
static uint32_t
func_epntdoneen_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_epntdoneen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * -------------------------------------------------------------------------
 * EPNTTOGBITS 
 *	Endpoint toggle bit register
 * -------------------------------------------------------------------------
 */
static uint32_t
func_epnttogbits_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_epnttogbits_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ----------------------------------------------------------------------------
 * Frame pointer and endpoint ready clear register
 *	read: return framenumber of last received SOF	
 * 	write: clean endpoint ready 
 * ----------------------------------------------------------------------------
 */

static uint32_t
func_fneprdyclr_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
func_fneprdyclr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * --------------------------------------------------------------------
 * This sends the currently active packet of the usb host to
 * the ports
 * --------------------------------------------------------------------
 */
static void
host_send_packet(void *clientData)
{
	int i;
	IMXUsbHost *host = (IMXUsbHost *) clientData;
	UsbPacket *pkt = &host->usbpkt;
	dbgprintf("Will send an USB packet\n");
	for (i = 0; i < 3; i++) {
		UsbDevice *usbdev = host->port[i].usbdev;
		if (usbdev) {
			UsbDev_Feed(usbdev, pkt);
		}
	}
}

static inline void
host_send_packet_delayed(IMXUsbHost * host)
{
	/* Calculation of delay from pktsize will follow one year */
	dbgprintf("Send packet delayed\n");
	CycleTimer_Mod(&host->pktDelayTimer, NanosecondsToCycles(10000));
}

static inline void
hscript_add(IMXUsbHost * host, uint32_t icode)
{
	if (host->code_wp < MAX_HCODE_LEN) {
		host->code[host->code_wp] = icode;
		host->code_wp++;
	} else {
		fprintf(stderr, "IMXOtg: Instruction memory overflow\n");
		host->code[MAX_HCODE_LEN - 1] = HCMD_END;
	}
}

/*
 * -------------------------------------------------------------
 * Assemble code for sending a packet from one ETD
 * returns < 0 if failed
 * -------------------------------------------------------------
 */
static int
etd_processor_emit_code(IMXUsbHost * host, int etdnum)
{
	Etd *etd = &host->etd[etdnum];
	int toggle;
	int dir_out;
	host->currentEtd = etd;
	dbgprintf("Setting current etd to %d\n", etd->nr);
	host->code_ip = 0;
	host->code_wp = 0;
	host->stackptr = 0;

	if (etd->bufstat & (1 << 23)) {
		toggle = (etd->bufstat >> 22) & 1;
	} else {
		toggle = !!(etd->hwControl & HETD_TOGCRY);
	}
	switch (etd->hwControl & HETD_DIRECT_MASK) {
	    case HETD_DIRECT_OUT:
		    dir_out = 1;
		    break;

	    case HETD_DIRECT_IN:
		    dir_out = 0;
		    break;

	    case HETD_DIRECT_FROM_TD:
	    case HETD_DIRECT_FROM_TD_2:
		    switch (etd->bufstat & CBITD_DIRPID_MASK) {
			case CBITD_DIRPID_SETUP:
				dir_out = 1;
				break;
			case CBITD_DIRPID_OUT:
				dir_out = 1;
				break;
			case CBITD_DIRPID_IN:
				dir_out = 0;
				break;
		    }
	}
	switch (etd->bufstat & CBITD_DIRPID_MASK) {
	    case CBITD_DIRPID_SETUP:
		    hscript_add(host, HCMD_SET_ADDR + (etd->hwControl & 0x7f));
		    hscript_add(host, HCMD_SET_EPNT + ((etd->hwControl >> 7) & 0xf));
		    hscript_add(host, HCMD_SEND_TOKEN | USB_PID_SETUP);
		    hscript_add(host, HCMD_START_DATA + toggle);
		    hscript_add(host, HCMD_SETUP_HANDSHAKE);
		    hscript_add(host, HCMD_DO_DATA);
		    hscript_add(host, HCMD_WAIT_HANDSHAKE);
		    break;

	    case CBITD_DIRPID_OUT:
		    hscript_add(host, HCMD_SET_ADDR + (etd->hwControl & 0x7f));
		    hscript_add(host, HCMD_SET_EPNT + ((etd->hwControl >> 7) & 0xf));
		    hscript_add(host, HCMD_SEND_TOKEN | USB_PID_OUT);
		    hscript_add(host, HCMD_START_DATA + toggle);
		    hscript_add(host, HCMD_SETUP_HANDSHAKE);
		    hscript_add(host, HCMD_DO_DATA);
		    hscript_add(host, HCMD_WAIT_HANDSHAKE);
		    break;

	    case CBITD_DIRPID_IN:
		    hscript_add(host, HCMD_SET_ADDR + (etd->hwControl & 0x7f));
		    hscript_add(host, HCMD_SET_EPNT + ((etd->hwControl >> 7) & 0xf));
		    hscript_add(host, HCMD_SETUP_DATA_IN + toggle);
		    hscript_add(host, HCMD_SEND_TOKEN | USB_PID_IN);
		    hscript_add(host, HCMD_WAIT_DATA_IN);
		    break;

	    default:
		    fprintf(stderr, "Illegal dirpid\n");
		    return -1;
		    break;
	}
	hscript_add(host, HCMD_END);
	return 0;
}

/* 
 * ------------------------------------------------------------------------------------
 * etd_select:
 *	select the etd which is handled next using the rules from the USB standard
 *	Takes the first found etd. This is not fair. Should be substituted 
 * 	by a better algorithm.
 * ------------------------------------------------------------------------------------
 */
static int
etd_select(IMXUsbHost * host)
{
	int i;
	/* quick hack : Use the first etd which is ready to be processed */
	if (host->etden) {
		for (i = 0; i < 32; i++) {
			if (host->etden & (1 << i)) {
				return i;
			}
		}
	}
	return -1;
}

/*
 * -----------------------------------------------------------
 * refill instructions into the microengine if there are
 * unhandled etds
 * -----------------------------------------------------------
 */
static int
etd_processor_refill_code(IMXUsbHost * host)
{
	int etdnr;
	etdnr = etd_select(host);
	if (etdnr >= 0) {
		if (etd_processor_emit_code(host, etdnr) < 0) {
			return 0;
		}
		return 1;
	} else {
		host->interp_running = 0;
		return 0;
	}
}

/*
 * ----------------------------------------------------------------------
 * Read the next buf from etd memory.  Returns the length of the data
 * returned. The algorithm which decides which buffer to use is not
 * documented so it might be wrong. 
 * I suspect it always starts with X-Buffer and switches to the
 * other buffer whenever "BUFSIZE+1" bytes are consumed.
 * ----------------------------------------------------------------------
 */

static int
fetch_next_etdbuf(IMXUsbHost * host, Etd * etd, uint8_t * buf, int maxlen)
{
	int bufsize = ((etd->dword3 & CBITD_BUFSIZE_MASK) >> CBITD_BUFSIZE_SHIFT) + 1;
	int buf_select = etd->buf_toggle;
	int totbyecnt = etd->dword3 & 0x1fffff;
	int copy_len;
	unsigned int bufsrtad;
	/* Check the direction somewhere ????? */
	if (totbyecnt > bufsize) {
		copy_len = bufsize;
	} else {
		copy_len = totbyecnt;
	}
	if (copy_len > maxlen) {
		fprintf(stderr, "USB packet buffer is to small\n");
		return -1;
	}
	if (buf_select == 0) {	/* X-Buf */
		bufsrtad = etd->bufsrtad & 0xffff;
	} else {		/* Y-Buf */
		bufsrtad = etd->bufsrtad >> 16;
	}
	if ((bufsrtad + copy_len) > 4096) {
		fprintf(stderr, "IMXUsbHost: Buffer outside of data buffer window\n");
		return -1;
	}
	etd_do_dma(host->otg, etd);
	memcpy(buf, host->data_mem + bufsrtad, copy_len);
	if (buf_select == 0) {
		host->xfillstat &= ~(1 << etd->nr);
	} else {
		host->yfillstat &= ~(1 << etd->nr);
	}
	/* 
	 * ---------------------------------------------------------------------------
	 * Modify length in TOTBYECNT. The metroworks driver for linux-2.4 
	 * uses this to calculate the number of transfered bytes.
	 * ---------------------------------------------------------------------------
	 */

	etd->dword3 = (etd->dword3 & ~0x1fffff) | ((etd->dword3 & 0x1fffff) - copy_len);
	etd->buf_toggle ^= 1;
	return copy_len;
}

/*
 * ----------------------------------------------------------------------------------
 * Put the data to the buffer in the ETD memory
 * ----------------------------------------------------------------------------------
 */
static int
put_data_to_etdbuf(IMXUsbHost * host, Etd * etd, const uint8_t * buf, int pktlen)
{
	int bufsize = ((etd->dword3 & CBITD_BUFSIZE_MASK) >> CBITD_BUFSIZE_SHIFT) + 1;
	int use_ybuf = etd->buf_toggle;
	int totbyecnt = etd->dword3 & 0x1fffff;
	int copy_len;
	int etdmask = 1 << etd->nr;
	unsigned int bufsrtad;

	if (totbyecnt > bufsize) {
		copy_len = bufsize;
	} else {
		copy_len = totbyecnt;
	}
	if (pktlen <= copy_len) {
		copy_len = pktlen;
	} else {
		fprintf(stderr,
			"* USB: One packet buffer is to small for pktlen %d, bs %d tot %d dw3 %08x\n",
			pktlen, bufsize, totbyecnt, etd->dword3);
	}
	dbgprintf("etd->bufsrtad is %08x\n", etd->bufsrtad);
	if (use_ybuf) {
		bufsrtad = etd->bufsrtad >> 16;
	} else {
		bufsrtad = etd->bufsrtad & 0xffff;
	}
	if ((bufsrtad + copy_len) > 4096) {
		fprintf(stderr, "* IMXUsbHost: DSTBuffer outside of data buffer window\n");
		return -1;
	}
	dbgprintf
	    ("Putting %d bytes of %d to the etdbuf(%d) with totbye %d to buf %d, address %04x etdnr %d\n",
	     copy_len, pktlen, use_ybuf, totbyecnt, use_ybuf, bufsrtad, etd->nr);
	if (pktlen) {
		int i;
		for (i = 0; i < pktlen; i++) {
			dbgprintf("%02x ", buf[i]);
		}
		dbgprintf("\n");
	}

	memcpy(host->data_mem + bufsrtad, buf, copy_len);
	if (use_ybuf) {
		host->yfillstat |= etdmask;
		etd->yfill_level = copy_len;
	} else {
		host->xfillstat |= etdmask;
		etd->xfill_level = copy_len;
	}
	/* modify length in TOTBYECNT */
	totbyecnt = (etd->dword3 & 0x1fffff) - copy_len;
	etd->dword3 = (etd->dword3 & ~0x1fffff) | (totbyecnt & 0x1fffff);
	etd_do_dma(host->otg, etd);
	etd->buf_toggle ^= 1;
	return copy_len;
}

/*
 * -----------------------------------------------------------
 * mark an etd as completed
 * -----------------------------------------------------------
 */
static void
host_complete_etd(IMXUsbHost * host, Etd * etd, int cc)
{
	int totallen = etd->dword3 & 0x1fffff;
	uint32_t etdmask = (1 << etd->nr);
	if ((cc != CC_ACK) || (totallen == 0)) {
		if (cc == CC_ACK) {
			cc = CC_NOERROR;
		}
		etd_set_compcode(etd, cc);
		host->etddonestat |= etdmask;
		host->etden &= ~etdmask;
		if (host->etddonestat & host->etddoneen & host->immedint) {
			host->sysisr |= SYSISR_DONEINT;
			update_host_interrupt(host->otg);
		}
	}
	/* Complete the dma */
	host->otg->dma_etddmaen &= ~etdmask;
}

static void etd_processor_do_cmds(void *cd);
/*
 * ------------------------------------------------------------------------------
 * ------------------------------------------------------------------------------
 */
static void
etd_processor_timeout(void *cd)
{
	IMXUsbHost *host = (IMXUsbHost *) cd;
	IMXOtg *otg = host->otg;
	Etd *etd = host->currentEtd;
	if (!etd) {
		fprintf(stderr, "No currentEtd in line %d\n", __LINE__);
		return;
	}
	fprintf(stderr, "USB ETD processor: Timeout waiting for Device reply\n");
	otg->pkt_receiver = NULL;
	host_complete_etd(host, etd, CC_DEVNOTRESP);
	// interpreter is now finished
	host->interp_running = 0;
}

/*
 * --------------------------------------------------------------------------
 * Called when an expected handshake packet arrives
 * --------------------------------------------------------------------------
 */
static void
etd_handshake_receiver(IMXOtg * otg, const UsbPacket * pkt)
{
	IMXUsbHost *host = &otg->host;
	Etd *etd = host->currentEtd;
	int cc;
	int totallen;
	if (!etd) {
		fprintf(stderr, "No currentEtd in line %d\n", __LINE__);
		return;
	}
	CycleTimer_Remove(&otg->host.ndelayTimer);
	/* Check if packet is expected one */
	dbgprintf("A handshake packet with pid %d was received\n", pkt->pid);
	otg->pkt_receiver = NULL;
	if (pkt->pid == USB_PID_ACK) {
		cc = CC_ACK;
	} else if (pkt->pid == USB_PID_NAK) {
		/* Maybe the processor should be stopped on NAK. Check this later */
		cc = CC_NAK;
	} else if (pkt->pid == USB_PID_STALL) {
		cc = CC_STALL;
	} else {
		cc = CC_PIDFAIL;
	}
	totallen = etd->dword3 & 0x1fffff;
	if ((cc != CC_ACK) || (totallen == 0)) {
		dbgprintf("**** Complete packet ttl %d len %d  cc %d\n", totallen, pkt->len, cc);
		host_complete_etd(host, etd, cc);
	}
	/* do not immediately start next command */
	CycleTimer_Add(&host->ndelayTimer, NanosecondsToCycles(0), etd_processor_do_cmds, host);
	return;
}

/*
 * ----------------------------------------------------------------
 * Receive the packet after PID_IN was sent by host
 * If result 
 * ----------------------------------------------------------------
 */

static void
etd_pkt_in_receiver(IMXOtg * otg, const UsbPacket * pkt)
{
	IMXUsbHost *host = &otg->host;
	Etd *etd = host->currentEtd;
	int cc;
	int totallen;
	int maxpacket = (etd->hwControl & HETD_MAXPKTSIZ_MASK) >> HETD_MAXPKTSIZ_SHIFT;
	if (!etd) {
		fprintf(stderr, "Bug: No currentEtd in line %d\n", __LINE__);
		return;
	}
	CycleTimer_Remove(&otg->host.ndelayTimer);
	/* Check if packet is expected one */
	dbgprintf("Ein Eingangspacket wurde empfangen\n");
	otg->pkt_receiver = NULL;

	/* The host only generates a response if a datapacket arrives */
	if ((pkt->pid == USB_PID_DATA0) || (pkt->pid == USB_PID_DATA1)) {
		put_data_to_etdbuf(host, etd, pkt->data, pkt->len);
		cc = CC_NOERROR;
		/* should be done delayed, but wait mechanism is missing currently */
		host->usbpkt.pid = USB_PID_ACK;
		host_send_packet(host);
	} else if (pkt->pid == USB_PID_NAK) {
		cc = CC_NAK;
	} else if (pkt->pid == USB_PID_STALL) {
		cc = CC_STALL;
	} else {
		cc = CC_PIDFAIL;
	}
	totallen = etd->dword3 & 0x1fffff;
	if ((cc != CC_NOERROR) || (totallen == 0) || (pkt->len < maxpacket)) {
		dbgprintf("**** Complete packet ttl %d len %d max %d, cc %d\n", totallen, pkt->len,
			  maxpacket, cc);
		host_complete_etd(host, etd, cc);
	}
	CycleTimer_Add(&host->ndelayTimer, NanosecondsToCycles(0), etd_processor_do_cmds, host);
	//return etd_processor_do_cmds(&otg->host);
	return;
}

int
hcond_check(IMXUsbHost * host, int cond)
{
	Etd *etd = host->currentEtd;
	switch (cond) {
	    case COND_ETD_NOT_DONE:
		    if (host->etddonestat & (1 << etd->nr)) {
			    return 0;
		    } else {
			    return 1;
		    }
	}
	return 0;
}

/*
 * ------------------------------------------------------------------------
 *
 * ------------------------------------------------------------------------
 */
static void
stop_etdprocessor(IMXUsbHost * host)
{
//      host->otg->pkt_receiver = etd_handshake_receiver; /* ????? */
	host->otg->pkt_receiver = NULL;	/* ????? */
	CycleTimer_Remove(&host->ndelayTimer);
	host->interp_running = 0;
}

/*
 * ----------------------------------------------------------------------
 * This is the interpreter for the microengine code
 * ----------------------------------------------------------------------
 */

static void
etd_processor_do_cmds(void *cd)
{
	IMXUsbHost *host = (IMXUsbHost *) cd;
	uint32_t cmd;
	int maxpacket;
	uint32_t ndelay = 0;
	int done = 0;
	int result;
	host->interp_running = 1;
	Etd *etd = host->currentEtd;
	while (1) {
		if (host->code_ip >= MAX_HCODE_LEN) {
			fprintf(stderr, "IMXUsbHost: Instruction pointer out of range\n");
			return;
		}
		cmd = host->code[host->code_ip];
		//fprintf(stderr,"Instruction %08x\n",cmd);
		switch (cmd & 0xff000000) {

		    case HCMD_START_DATA:
			    if (cmd & 1) {
				    host->usbpkt.pid = USB_PID_DATA1;
			    } else {
				    host->usbpkt.pid = USB_PID_DATA0;
			    }
			    host->usbpkt.len = 0;	/* incremented later when data is added */
			    host->code_ip++;
			    break;

		    case HCMD_DO_DATA:
			    maxpacket =
				(etd->hwControl & HETD_MAXPKTSIZ_MASK) >> HETD_MAXPKTSIZ_SHIFT;
			    maxpacket = (maxpacket > USB_MAX_PKTLEN) ? USB_MAX_PKTLEN : maxpacket;
			    result = fetch_next_etdbuf(host, etd,
						       host->usbpkt.data /* + host->usbpkt.len */ ,
						       maxpacket);
			    etd->hwControl ^= HETD_TOGCRY;
			    if (result == 0) {
				    //fprintf(stderr,"IMXUsbHost: Zero sized packet or tx fifo underflow ?????\n");
				    // set some errorcode abort_script
			    } else if (result < 0) {
				    fprintf(stderr, "IMXUsbHost: Fatal error aborting USBPacket\n");
				    stop_etdprocessor(host);
				    done = 1;
			    } else {
				    host->usbpkt.len += result;
			    }
			    dbgprintf("Data out: pkt len is now %d\n", host->usbpkt.len);
			    host->code_ip++;
			    host_send_packet_delayed(host);
			    break;

		    case HCMD_SET_ADDR:
			    host->usbpkt.addr = cmd & 0x7f;
			    host->code_ip++;
			    break;

		    case HCMD_NDELAY:
			    host->code_ip++;
			    CycleTimer_Add(&host->ndelayTimer, NanosecondsToCycles(ndelay),
					   etd_processor_do_cmds, host);
			    return;

		    case HCMD_SET_EPNT:
			    host->usbpkt.epnum = cmd & 0xf;
			    host->code_ip++;
			    break;

		    case HCMD_SEND_TOKEN:
			    host->usbpkt.pid = cmd & 0xff;
			    host->code_ip++;
			    host_send_packet(host);	/* not delayed because there is no wait */
			    break;

		    case HCMD_WAIT_HANDSHAKE:
			    host->code_ip++;
			    return;

		    case HCMD_SETUP_HANDSHAKE:
			    ndelay = 12000;	/* Should be calculated from clock */
			    if (!CycleTimer_IsActive(&host->ndelayTimer)) {
				    CycleTimer_Add(&host->ndelayTimer, NanosecondsToCycles(ndelay),
						   etd_processor_timeout, host);
			    } else {
				    fprintf(stderr, "Setup handshake: Bug, Timer is busy\n");
			    }
			    /* setup receiver handler */
			    host->otg->pkt_receiver = etd_handshake_receiver;
			    host->code_ip++;
			    break;

		    case HCMD_SETUP_DATA_IN:
			    ndelay = 12000;	/* Should be calculated from clock */
			    if (!CycleTimer_IsActive(&host->ndelayTimer)) {
				    CycleTimer_Add(&host->ndelayTimer, NanosecondsToCycles(ndelay),
						   etd_processor_timeout, host);
			    } else {
				    fprintf(stderr, "Setup data: Bug, Timer is busy\n");
			    }
			    /* setup receiver handler */
			    host->otg->pkt_receiver = etd_pkt_in_receiver;
			    host->code_ip++;
			    break;

		    case HCMD_WAIT_DATA_IN:
			    dbgprintf("Wait for data in with running packet processor\n");
			    host->code_ip++;
			    return;

		    case HCMD_END:
			    dbgprintf("Microengine script end at %d\n", host->code_ip);
			    stop_etdprocessor(host);
			    done = 1;
			    break;

		    case HCMD_DO:
			    host->code_ip++;
			    if (host->stackptr < HSTACK_SIZE) {
				    host->ip_stack[host->stackptr] = host->code_ip;
				    host->stackptr++;
			    } else {
				    fprintf(stderr, "IMXUsb: Stack underflow\n");
			    }
			    break;

		    case HCMD_WHILE:
			    host->code_ip++;
			    if (host->stackptr > 0) {
				    host->stackptr--;
				    if (hcond_check(host, cmd & 0x00ffffff)) {
					    host->code_ip = host->ip_stack[host->stackptr];
				    }
			    } else {
				    fprintf(stderr, "IMXUsb: Stack underflow\n");
				    return;
			    }
			    break;

		    default:
			    fprintf(stderr, "IMXUsbHost: Instruction %08x not implemented\n", cmd);
			    stop_etdprocessor(host);
			    done = 1;
			    break;
		}
		if (done) {
			if (etd_processor_refill_code(host) <= 0) {
				return;
			}
			done = 0;
		}
	}
}

/*
 * ---------------------------------------------------------
 * Start the etd processor
 * ---------------------------------------------------------
 */
static void
etd_processor_start(IMXUsbHost * host)
{
	if (host->interp_running) {
		fprintf(stderr, "IMXUsbHost: The ETD Processor is already running\n");
		return;
	}
	if (etd_processor_refill_code(host) <= 0) {
		return;
	}
	CycleTimer_Add(&host->ndelayTimer, NanosecondsToCycles(0), etd_processor_do_cmds, host);
}

/* 
 * ---------------------------------------------------------------
 * The 1 ms Frame intervall timer calls this
 * Maybe this handler should be disabled when no ETD is pending
 * and SOF interrupt is disabled
 * ---------------------------------------------------------------
 */
static void
host_do_sof(void *clientData)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	IMXUsbHost *host = &otg->host;
	uint16_t frm_interval = otg->otg_frm_invtl & 0x3fff;
	int nanoseconds;
	host->frmnumb = (host->frmnumb + 1) & 0xffff;

	if (host->sysien & SYSIEN_SOFINT) {
		host->sysisr |= SYSISR_SOFINT;
		update_host_interrupt(otg);
	}
	if ((host->etddonestat & host->etddoneen) && !(host->sysisr & SYSISR_DONEINT)) {
		host->sysisr |= SYSISR_DONEINT;
		update_host_interrupt(otg);
	}

	nanoseconds = frm_interval * 83;	// 83.33
	if (!nanoseconds) {
		nanoseconds = 100000;
	}
	CycleTimer_Mod(&host->frame_timer, NanosecondsToCycles(nanoseconds));
}

/*
 * --------------------------------------------------------------------------
 * HOST_CTRL Host controller register
 *
 *  Bit 31:	HCRESET	 Initiate a software controlled hardware reset
 *  Bit 16-17: 	SCHEDOVR Scheduler overrun counter
 *  Bit 4:	RMTWUEN	 Remote wake up enable
 *  Bit 2-3	HCUSBSTE Hub controller USB state
 *		00: USB reset
 *		01: USB resume
 *		10: USB operational
 *		11: USB suspend
 *  Bit 0-1	CTLBLKSR Control/Bulk service ratio
 * --------------------------------------------------------------------------
 */

static uint32_t
host_ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.ctrl;
}

static void
host_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	int status;
	if (value & CTRL_HCRESET) {
		reset_hc(otg);
		return;
	}
	otg->host.ctrl = value;
	status = value & CTRL_HCUSBSTE_MASK;
	switch (status) {
	    case CTRL_HCUSBSTE_RESET:
		    CycleTimer_Remove(&otg->host.frame_timer);
		    break;
	    case CTRL_HCUSBSTE_RESUME:
		    if (!CycleTimer_IsActive(&otg->host.frame_timer)) {
			    CycleTimer_Mod(&otg->host.frame_timer, MicrosecondsToCycles(1000));
		    }
		    break;
	    case CTRL_HCUSBSTE_OPER:
		    if (!CycleTimer_IsActive(&otg->host.frame_timer)) {
			    CycleTimer_Mod(&otg->host.frame_timer, MicrosecondsToCycles(1000));
		    }
		    break;
	    case CTRL_HCUSBSTE_SUSP:
		    /* ???? */
		    CycleTimer_Remove(&otg->host.frame_timer);
		    break;
	}
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ----------------------------------------------------------------------
 * HOST_SYSISR System interrupt status register 
 *  Bit 6:  PSCINT 	Port status change interrupt
 *  Bit 5:  FMOFINT	Frame number overflow interrupt
 *  Bit 4:  HERRINT	Host error interrupt
 *  Bit 3:  RESDETINT	Resume detect interrupt
 *  Bit 2:  SOFINT	Start of frame interrupt
 *  Bit 1:  DONEINT		Done register interrupt
 *  Bit 0:  SORINT		Scheduler overrun interrupt
 * ----------------------------------------------------------------------
 */
static uint32_t
host_sysisr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.sysisr;
}

static void
host_sysisr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.sysisr &= ~(value & 0x3f);
	if (otg->host.etddonestat && otg->host.immedint) {
		otg->host.sysisr |= SYSISR_DONEINT;
	}
	update_host_interrupt(otg);
	return;
}

/*
 * ----------------------------------------------------------------------
 * HOST_SYSIEN System interrupt enable register 
 *  Bit 6:  PSCINT 	Port status change interrupt enable
 *  Bit 5:  FMOFINT	Frame number overflow interrupt enable
 *  Bit 4:  HERRINT	Host error interrupt enable
 *  Bit 3:  RESDETINT	Resume detect interrupt enable
 *  Bit 2:  SOFINT	Start of frame interrupt enable
 *  Bit 1:  DONEINT	Done register interrupt enable
 *  Bit 0:  SORINT	Scheduler overrun interrupt enable
 * ----------------------------------------------------------------------
 */
static uint32_t
host_sysien_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.sysien;
}

static void
host_sysien_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.sysien = value;
	update_host_interrupt(otg);
	return;
}

/*
 * -----------------------------------------------------------------------
 * XBUFSTAT
 * 	X Buffer interrupt status bitfield for 32 ETD's
 * 	A bit is set when XBuffer has been emptied (OUT)
 *	or if an XBuffer has been filled (IN)
 * -----------------------------------------------------------------------
 */

static uint32_t
host_xbufstat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.xbufstat;
}

static void
host_xbufstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.xbufstat &= ~value;
	return;
}

/*
 * -------------------------------------------------------------------
 * YBUFSTAT
 *	Y Buffer interrupt status bitfield for 32 ETD's
 * -------------------------------------------------------------------
 */
static uint32_t
host_ybufstat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.ybufstat;
}

static void
host_ybufstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.ybufstat &= ~value;
	return;
}

/*
 * ------------------------------------------------------------------------------
 * XYINTEN
 * 	X/Y Buffer Interrupt enable bitfield
 * ------------------------------------------------------------------------------
 */
static uint32_t
host_xyinten_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.xyinten;
}

static void
host_xyinten_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ----------------------------------------------------------
 * XFILLSTAT
 *	X Buffer filled status. One toggling bit per ETD
 * Out endpoint: application indicates that something sendable is in y
 * In endpoint: host indicates that something was received in y-buf
 * No documentation about interaction with DMA controller
 * ----------------------------------------------------------
 */
static uint32_t
host_xfillstat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.xfillstat;
}

static void
host_xfillstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.xfillstat ^= value;
	return;
}

/*
 * --------------------------------------------------------------------
 * YFILLSTAT
 *	Y Buffer filled status
 * Out endpoint: application indicates that something sendable is in y
 * In endpoint: host indicates that something was received in y-buf
 * --------------------------------------------------------------------
 */
static uint32_t
host_yfillstat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.yfillstat;
}

static void
host_yfillstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.yfillstat ^= value;
	return;
}

/*
 * ------------------------------------------------------------------------
 * ETDENSET ETD enable set register
 *	0: no change
 *	1: enable etd
 * ------------------------------------------------------------------------
 */

static uint32_t
host_etdenset_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return otg->host.etden;
}

/*
 * --------------------------------------------------------------------------
 * Enable a host endpoint.
 * Does this clear automatically when done ????
 * Its not clear who initializes the buf toggle. Maybe its done here
 * --------------------------------------------------------------------------
 */
static void
host_etdenset_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	IMXUsbHost *host = &otg->host;
	uint32_t oldvalue = otg->host.etden;
	int i;
	uint32_t diff;
	host->etden |= value;
	diff = oldvalue ^ host->etden;
	for (i = 0; i < 32; i++) {
		if (diff & (1 << i)) {
			Etd *etd = &host->etd[i];
			etd->buf_toggle = 0;
			etd->dmabufptr = 0;
			etd->dma_len = etd->dword3 & 0x1fffff;
		}
	}
	if (!oldvalue && value) {
		etd_processor_start(host);
	}
	return;
}

/*
 * ------------------------------------------------------------------------
 * ETDENCLR ETD enable clear register
 *	0: no change
 *	1: disable etd
 * When does this take effect
 * ------------------------------------------------------------------------
 */

static uint32_t
host_etdenclr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "ETDENCLR: Register is writeonly\n");
	return 0;
}

static void
host_etdenclr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.etden &= ~value;
	// stop immediately ?
	/* action is taken automatically on next frame */
	return;
}

/*
 * ----------------------------------------------------------------------------------
 * IMMEDINT 
 * 	Immediate interrupt enable bitfield. An interrupt is triggered 
 *	when ETD is done. Normaly interrupts are delayed until the following SOF 
 * ----------------------------------------------------------------------------------
 */

static uint32_t
host_immedint_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.immedint;
}

static void
host_immedint_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	/* When will this take effect ? */
	otg->host.immedint = value;
	return;
}

/*
 * ------------------------------------------------------------------------
 * ETDDONESTAT
 *	Endpointer descriptor done Bitfield, updated immediately but
 *	does not trigger interrupt before next SOF when immediate interrupt
 *	bit is not set
 * ------------------------------------------------------------------------
 */

static uint32_t
host_etddonestat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.etddonestat;
}

static void
host_etddonestat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.etddonestat &= ~value;	/* ??? Write 1 to clear ??? */
	return;
}

/*
 * --------------------------------------------------------------------------
 * ETDDONEEN
 *	Endpointer done interrupt enable (immediately or on SOF)
 * --------------------------------------------------------------------------
 */
static uint32_t
host_etddoneen_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	//otg->host.etddoneen; 
	return otg->host.etddoneen;
}

static void
host_etddoneen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.etddoneen = value;
	update_host_interrupt(otg);
	return;
}

/*
 * --------------------------------------------------------------------------
 * FRMNUMB 
 *	Framenumber read. 
 * --------------------------------------------------------------------------
 */
static uint32_t
host_frmnumb_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.frmnumb;
}

static void
host_frmnumb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.frmnumb = value & 0xffff;
	return;
}

/*
 * -------------------------------------------------------------------------------------
 * LSTRHRESH
 *	Low speed threshold. Bit times remaining in frame required to allow a low
 *	speed packet to be started
 * --------------------------------------------------------------------------------------
 */

static uint32_t
host_lsthresh_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.lsthresh;
}

static void
host_lsthresh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->host.lsthresh = value & 0x7ff;
	return;
}

/*
 * ----------------------------------------------------------------------------------------
 * ROOTHUBA
 *	Root hub descriptor register A 
 *
 *  Bit 24-31:	PWRTOGOOD	Power on to power good time (unit ?) 
 *  Bit 12: 	NOOVRCURP	No overcurrent protection
 *  Bit 11:	OVRCURPM	Overcurrent protection mode
 *  Bit 10:	DEVTYPE		Device type (always 0)
 *  Bit  9:	PWRSWTMD	Power switching mode
 *  Bit  8:	NOPWRSWT	No power switching (always 1)
 *  Bit 0-7:    NDNSTMPRT	Number of downstream ports (always 3)
 * ----------------------------------------------------------------------------------------
 */
static uint32_t
host_roothuba_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.roothuba;
}

static void
host_roothuba_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMX21 OTG: roothuba is not writable\n");
	return;
}

/*
 * --------------------------------------------------------------------------------------
 * ROOTHUBB
 * 	Root hub descriptor register B
 *
 *  Bit 16-23:	PRTPWRCM: 	Port power control mask 
 *  Bit 0-7:	DEVREMOVE:	Device removable bitfield
 * --------------------------------------------------------------------------------------
 */

static uint32_t
host_roothubb_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.roothubb;
}

static void
host_roothubb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMX21 OTG: Roothubb is not writable\n");
	return;
}

/*
 * --------------------------------------------------------------------------------
 * ROOTSTAT
 *	Root Hub status register
 *
 *  Bit 31: CLRRMTWUE	Clear remote wake up enable
 *  Bit 17: OVRCURCHG	Overcurrent indicator change
 *  Bit 15: DEVCONWUE	Device connect wakeup enable
 *  Bit 1:  OVRCURI	Overcurrent indicator
 *  Bit 0:  LOCPWRS	Local power status (always 0)
 * --------------------------------------------------------------------------------
 */

static uint32_t
host_rootstat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->host.rootstat;
}

/*
 * -----------------------
 * Currently ignored
 * -----------------------
 */
static void
host_rootstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	if (value & ROOTSTAT_OVRCURCHG) {
		otg->host.rootstat &= ~ROOTSTAT_OVRCURCHG;
	}
	return;
}

/*
 * ----------------------------------------------------------------------
 * Timer handler called 50ms after port reset was triggered
 * ----------------------------------------------------------------------
 */
static void
port_reset_done(void *clientData)
{
	RootHubPort *rhp = (RootHubPort *) clientData;
	rhp->portstat &= ~PORTSTAT_PRTRSTST;
	rhp->portstat |= PORTSTAT_PRTRSTSC;	/* Reset status change */
	if (rhp->portstat & PORTSTAT_CURCONST) {
		rhp->portstat |= PORTSTAT_PRTENABST;
	}
}

/*
 * --------------------------------------------------------------------------------
 * PORSTAT1-3 Port status register
 *
 *  Bit  20: PRTRSTSC	Port reset status change 
 *  Bit  19: OVRCURIC	Overcurrent indicator change
 *  Bit  18: PRTSTATSC	Port suspend status change
 *  Bit  17: PRTENBLSC	Port enable status change
 *  Bit  16: CONNECTSC	Connect status change
 *  Bit   9: LSDEVCON	Lowspeed device attachement / Clear port power
 *  Bit   8: PRTPWRST	Port power status
 *  Bit   4: PRTRSTST	Port reset status
 *  Bit   3: PRTOVRCURI	Port overcurrent indicator
 *  Bit   2: PRTSUSPST	Port suspend status
 *  Bit   1: PRTENABST	Port enable status
 *  Bit   0: CURCONST	Current connect status
 * --------------------------------------------------------------------------------
 */

static uint32_t
host_portstat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	unsigned int index = ((address & 0xff) - 0xf4) >> 2;
	if (index > 2) {
		fprintf(stderr, "Illegal portstat index %d\n", index);
		return 0;
	}
	return otg->host.port[index].portstat;
}

static void
host_portstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	unsigned int index = ((address & 0xff) - 0xf4) >> 2;
	uint32_t clearmask = ~(value & 0x001f0000);
	RootHubPort *rhp;
	if (index > 2) {
		fprintf(stderr, "Illegal portstat index %d\n", index);
		return;
	}
	rhp = &otg->host.port[index];
	fprintf(stderr, "portstat%d  before %08x not implemented\n", index, rhp->portstat);
	rhp->portstat = rhp->portstat & clearmask;
	if (value & PORTSTAT_SETPRTRST) {
		if (rhp->portstat & PORTSTAT_CURCONST) {
			UsbDevice *usbdev = rhp->usbdev;
			UsbPacket pkt;
			pkt.pid = USB_CTRLPID_RESET;
			rhp->portstat |= PORTSTAT_PRTRSTST;
			/* clear delayed */
			UsbDev_Feed(usbdev, &pkt);
			CycleTimer_Mod(&rhp->port_reset_timer, MillisecondsToCycles(50));
		} else {
			rhp->portstat |= PORTSTAT_CONNECTSC;
		}
	}
	if (value & PORTSTAT_CLRSUSP) {
		rhp->portstat &= ~PORTSTAT_PRTSUSPST;
	}
	if (value & PORTSTAT_SETSUSP) {
		rhp->portstat |= PORTSTAT_PRTSUSPST;
	}
	if (value & PORTSTAT_SETPRTENAB) {
		if (rhp->portstat & PORTSTAT_CURCONST) {
			rhp->portstat |= PORTSTAT_PRTENABST;
		} else {
			rhp->portstat |= PORTSTAT_CONNECTSC;
			// update_interrupts();
		}
		printf("Port was enabled\n");
		exit(1);
	}
	if (value & PORTSTAT_CLRPWRST) {
		rhp->portstat &= ~PORTSTAT_PRTPWRST;
	}
	if (value & PORTSTAT_SETPWRST) {
		rhp->portstat |= PORTSTAT_PRTPWRST;
	}
	if (value & PORTSTAT_CLRPRTENAB) {
		rhp->portstat &= ~PORTSTAT_PRTENABST;
	}
	fprintf(stderr, "Set portstat%d x%08x : result %08x\n", index, value, rhp->portstat);
	return;
}

static uint32_t
etd_mem_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	IMXUsbHost *host = &otg->host;
	uint32_t data32;
	uint32_t value = 0;
	int index = (address & 0x1ff) >> 4;
	if (((address & 3) + rqlen) > 4) {
		fprintf(stderr, "Unaligned ETD mem access\n");
		return 0;
	}
	switch (address & 0xc) {
	    case 0:
		    data32 = host->etd[index].hwControl;
		    break;
	    case 4:
		    data32 = host->etd[index].bufsrtad;
		    break;
	    case 8:
		    data32 = host->etd[index].bufstat;
		    break;
	    case 0xc:
		    data32 = host->etd[index].dword3;
		    break;
	    default:
		    data32 = 0;	/* Make the compiler quit */
		    break;
	}
	value = data32 >> ((address & 3) * 8);
	//fprintf(stderr,"ETD mem read addr %08x %08x\n",address,value);
	return value;

}

/*
 * Write to etd mem. This gets the value in host byteorder 
 * and it writes it into the memory 32Bit wise in host byteorder
 */
static void
etd_mem_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	IMXOtg *otg = (IMXOtg *) clientData;
	IMXUsbHost *host = &otg->host;
	//int i;
	//uint8_t data[4];
	uint8_t *addr;
	int index = (address & 0x1ff) >> 4;
	if (((address & 3) + rqlen) > 4) {
		fprintf(stderr, "Unaligned ETD mem access\n");
		return;
	}
	addr = ((uint8_t *) & host->etd[index]) + (address & 0xc);
#if 0
	*(uint32_t *) data = host32_to_le(*(uint32_t *) (addr));
	for (i = 0; i < rqlen; i++) {
		data[i + (address & 3)] = value;
		value = value >> 8;
	}
	*(uint32_t *) (addr) = le32_to_host(*(uint32_t *) data);
#endif
	*(uint32_t *) addr = value;
	//fprintf(stderr,"ETDMEM write: index %d ,ofs %d, value %08x(%08x)\n",index,address & 0xc,*(uint32_t*)addr,value);
}

static uint32_t
ep_mem_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	uint32_t value = 0;
	uint32_t index = address & 0x1ff;
	int i;
	for (i = 0; i < rqlen; i++) {
		value = value | otg->ep_mem[(index + i) & 0x1ff] << (i * 8);
	}
	return value;

}

static void
ep_mem_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	uint32_t index = address & 0x1ff;
	int i;
	for (i = 0; i < rqlen; i++) {
		otg->ep_mem[(index + i) & 0x1ff] = value;
		value = value >> 8;
	}
}

/* The AHB DMA block */

/*
 * ------------------------------------------------------------------------------
 * DMA_REV_READ
 *	Revision code for DMAIP block design, always 0x10
 * ------------------------------------------------------------------------------
 */

static uint32_t
dma_rev_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->dma_rev;
}

static void
dma_rev_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMX21 OTG: DMA revision is not writable\n");
	return;
}

/*
 * ----------------------------------------------------------------------------
 * DMA_INTSTAT
 *   Bit 1:	EPERR   Endpoint error interrupt
 *   Bit 0: 	ETDERR	ETD error interrupt	
 *
 *	Cleared by writing 1 to the Bit of the bad ETD/EP in 
 *	ETDDMAERSTAT/EPDMAERSTAT 
 * ----------------------------------------------------------------------------
 */
static uint32_t
dma_intstat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->dma_intstat;
}

static void
dma_intstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMX21 OTG: DMA_INTSTAT is not writable\n");
	return;
}

/*
 * ----------------------------------------------------------------------------
 * DMA_INTEN
 *   Bit 1:	EPERRINTEN	Endpoint error interrupt
 *   Bit 0: 	ETDERRINTEN	ETD error interrupt	
 * ----------------------------------------------------------------------------
 */

static uint32_t
dma_inten_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->dma_inten;
}

static void
dma_inten_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->dma_inten = value & 3;
	update_dma_interrupt(otg);
	return;
}

/*
 * --------------------------------------------------------------------------
 * DMA_ETDDMAERSTAT
 *	ETD dma error status bitfield
 *	Cleared on writing a 1 to the corresponding bit
 * --------------------------------------------------------------------------
 */
static uint32_t
dma_etddmaerstat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->dma_etddmaerstat;
}

static void
dma_etddmaerstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	uint32_t clearmask = ~value;
	otg->dma_etddmaerstat &= clearmask;
	if (!otg->dma_etddmaerstat) {
		otg->dma_intstat &= ~DMAINTSTAT_ETDERR;
		update_dma_interrupt(otg);
	}
	return;
}

/*
 * --------------------------------------------------------------------------
 * DMA_EPDMAERSTAT
 *	EPD dma error status bitfield
 * --------------------------------------------------------------------------
 */

static uint32_t
dma_epdmaerstat_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->dma_epdmaerstat;
}

static void
dma_epdmaerstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	uint32_t clearmask = ~value;
	otg->dma_epdmaerstat &= clearmask;
	if (!otg->dma_epdmaerstat) {
		otg->dma_intstat &= ~DMAINTSTAT_EPERR;
		update_dma_interrupt(otg);
	}
	return;
}

/*
 * --------------------------------------------------------------------------
 * DMA_ETDDMAEN
 *	ETD dma enable bitfield 
 * 	Enable the DMA for the enpoints specified in a mask
 *	Will auto disable at end of transfer;
 * --------------------------------------------------------------------------
 */

static uint32_t
dma_etddmaen_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->dma_etddmaen;
}

static void
dma_etddmaen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	//uint32_t diff = otg->dma_etddmaen ^ value;
	/* Sets the etddmaen, but its cleared on channel clear or completion */
	otg->dma_etddmaen |= value;
	return;
}

/*
 * --------------------------------------------------------------------------
 * DMA_EPDMAEN
 *	Endpoint dma enable register 
 * --------------------------------------------------------------------------
 */
static uint32_t
dma_epdmaen_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->dma_epdmaen;
}

static void
dma_epdmaen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * --------------------------------------------------------------------------
 * DMA_ETDDMAXTEN
 *	ETD dma enable X trigger request
 * --------------------------------------------------------------------------
 */

static uint32_t
dma_etddmaxten_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMX21 OTG: ETDDMAXTEN is a writeonly register\n");
	return 0;
}

static void
dma_etddmaxten_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * --------------------------------------------------------------------------
 * DMA_EPDDMAXTEN
 *	Endpoint dma enable X trigger request
 * --------------------------------------------------------------------------
 */

static uint32_t
dma_epdmaxten_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
dma_epdmaxten_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ---------------------------------------------------------------------------
 * ETDDMAENXYT
 * 	ETD dma enable XY Trigger request
 * ---------------------------------------------------------------------------
 */

static uint32_t
dma_etddmaenxyt_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMX21 OTG: ETDDMAENXYT is a writeonly register\n");
	return 0;
}

static void
dma_etddmaenxyt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	int i;
	for (i = 0; i < 32; i++) {
		if (value & (1 << i)) {
			//SigNode_Set(etd->xDmaReqNode,DMARQ_ACTIVE);   
			//SigNode_Set(etd->yDmaReqNode,DMARQ_ACTIVE);   
		}
	}
	// enable dma now
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ---------------------------------------------------------------------------
 * EPDMAENXYT
 * 	EPD dma enable XY Trigger request
 * ---------------------------------------------------------------------------
 */

static uint32_t
dma_epdmaenxyt_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
dma_epdmaenxyt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ---------------------------------------------------------------------------
 * ETCDMABST4EN 
 *	ETD DMA Burst4 enable
 * ---------------------------------------------------------------------------
 */
static uint32_t
dma_etcdmabst4en_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
dma_etcdmabst4en_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ---------------------------------------------------------------------------
 * EPDMABST4EN 
 *	EPD DMA Burst4 enable
 * ---------------------------------------------------------------------------
 */

static uint32_t
dma_epdmabst4en_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
dma_epdmabst4en_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ------------------------------------------------------------------------
 * DMA_MISCCONTROL
 *  Bit 3: ISOPREVFRM	ISO out previous frame mode
 *  Bit 2: SKPRTRY	Skip on retry mode
 *  Bit 1: ARBMODE	Arbiter mode select
 *  Bit 0: FILTCC	Filter on completion code
 * ------------------------------------------------------------------------
 */
static uint32_t
dma_misccontrol_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "iMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
dma_misccontrol_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "iMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ---------------------------------------------------------------------------
 * ETDDMACHANCLR
 *	ETD dma channel clear register: disable and abort dma
 * ---------------------------------------------------------------------------
 */

static uint32_t
dma_etddmachanclr_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
dma_etddmachanclr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	int i;
	for (i = 0; i < 32; i++) {
		if (value & otg->dma_etddmaen & (1 << i)) {
			etd_abort_dma(otg, &otg->host.etd[i]);
		}
	}
	otg->dma_etddmaen &= ~value;
	return;
}

/*
 * ---------------------------------------------------------------------------
 * EPDMACHANCLR
 *	EP dma channel clear register: disable and abort dma
 * ---------------------------------------------------------------------------
 */

static uint32_t
dma_epdmachanclr_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
dma_epdmachanclr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * ----------------------------------------------------------------
 * ETDSMSA
 * 	Etd system memory start address register
 * ----------------------------------------------------------------
 */
static uint32_t
dma_etdsmsa_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	unsigned int index = (address & 0x7f) >> 2;
	return otg->dma_etdsmsa[index];
}

static void
dma_etdsmsa_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	unsigned int index = (address & 0x7f) >> 2;
	otg->dma_etdsmsa[index] = value;
	return;
}

/*
 * --------------------------------------------------------------------
 * EPSMSA
 *	Endpoint system memory start address
 * --------------------------------------------------------------------
 */

static uint32_t
dma_epsmsa_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
dma_epsmsa_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

/*
 * -------------------------------------------------------------------------
 * ETDDMABUFPTR
 *	etd dma buffer pointer. accessible only for debug purposes
 * -------------------------------------------------------------------------
 */
static uint32_t
dma_etddmabufptr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	IMXUsbHost *host = &otg->host;
	unsigned int index = (address >> 2) & 0x1f;
	Etd *etd = &host->etd[index];
	return etd->dmabufptr;
}

static void
dma_etddmabufptr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	IMXUsbHost *host = &otg->host;
	unsigned int index = (address >> 2) & 0x1f;
	Etd *etd = &host->etd[index];
	etd->dmabufptr = value;
	return;
}

/*
 * -------------------------------------------------------------------------
 * EPDMABUFPTR
 *	EP dma buffer pointer. accessible only for debug purposes
 * -------------------------------------------------------------------------
 */

static uint32_t
dma_epdmabufptr_read(void *clientData, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return 0;
}

static void
dma_epdmabufptr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//IMXOtg *otg = (IMXOtg *) clientData;
	fprintf(stderr, "IMX21 OTG: 0x%08x not implemented\n", address);
	return;
}

static uint32_t
isp1301_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	int reg = address & 0x1f;
	return ISP1301_Read(otg->isp1301, reg);
}

static void
isp1301_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	int reg = address & 0x1f;
	ISP1301_Write(otg->isp1301, value, reg);
	return;
}

static uint32_t
i2c_otgdevaddr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->i2c_otgdevaddr;
}

static void
i2c_otgdevaddr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->i2c_otgdevaddr = value;
	return;
}

static uint32_t
i2c_numseqops_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->i2c_numseqops;
}

static void
i2c_numseqops_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->i2c_numseqops = value;
	return;
}

static uint32_t
i2c_seqreadstrt_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->i2c_seqreadstrt;
}

static void
i2c_seqreadstrt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->i2c_seqreadstrt = value;
	return;
}

static uint32_t
i2c_opctrl_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	/* Bit 7 is Busy for i2c transaction */
	return otg->i2c_opctrl;
}

static void
i2c_opctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	otg->i2c_opctrl = value;
	return;
}

static uint32_t
i2c_divfactor_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->i2c_divfactor;
}

static void
i2c_divfactor_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	IMXOtg *otg = (IMXOtg *) clientData;
	otg->i2c_divfactor = value;
	return;
}

static uint32_t
i2c_intandctrl_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	return otg->i2c_intandctrl;
}

static void
i2c_intandctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	uint8_t clearmask = ~((value & 6) | 0x70);
	otg->i2c_intandctrl = (otg->i2c_intandctrl & clearmask) | (value & 0x70);
	return;
}

static uint32_t
data_mem_read(void *clientData, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	uint32_t value = 0;
	uint32_t index = address & 0xfff;
	int i;
	for (i = 0; i < rqlen; i++) {
		value = value | (otg->data_mem[(index + i) & 0xfff] << (i * 8));
	}
	return value;
}

static void
data_mem_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXOtg *otg = (IMXOtg *) clientData;
	uint32_t index = address & 0xfff;
	int i;
	for (i = 0; i < rqlen; i++) {
		otg->data_mem[(index + i) & 0xfff] = value;
		value = value >> 8;
	}
	return;
}

static void
IMXOtg_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	IOH_Delete32(OTG_HWMODE(base));
	IOH_Delete32(OTG_CINT_STAT(base));
	IOH_Delete32(OTG_CINT_STEN(base));
	IOH_Delete32(OTG_CLK_CTRL(base));
	IOH_Delete32(OTG_RST_CTRL(base));
	IOH_Delete32(OTG_FRM_INVTL(base));
	IOH_Delete32(OTG_FRM_REMAIN(base));
	IOH_Delete32(OTG_HNP_CTRL(base));
	IOH_Delete32(OTG_HNP_INT_STAT(base));
	IOH_Delete32(OTG_HNP_INT_EN(base));
	IOH_Delete32(OTG_USBCTRL(base));

	IOH_Delete32(FUNC_COMSTAT(base));
	IOH_Delete32(FUNC_DEVADDR(base));
	IOH_Delete32(FUNC_SYSINTSTAT(base));
	IOH_Delete32(FUNC_SYSINTEN(base));
	IOH_Delete32(FUNC_XBUFINTSTAT(base));
	IOH_Delete32(FUNC_YBUFINTSTAT(base));
	IOH_Delete32(FUNC_XYINTEN(base));
	IOH_Delete32(FUNC_XFILLSTAT(base));
	IOH_Delete32(FUNC_YFILLSTAT(base));
	IOH_Delete32(FUNC_ENDPNTEN(base));
	IOH_Delete32(FUNC_ENDPNRDY(base));
	IOH_Delete32(FUNC_IMMEDINT(base));
	IOH_Delete32(FUNC_EPNTDONESTAT(base));
	IOH_Delete32(FUNC_EPNTDONEEN(base));
	IOH_Delete32(FUNC_EPNTTOGBITS(base));
	IOH_Delete32(FUNC_FNEPRDYCLR(base));

	IOH_Delete32(HOST_CTRL(base));
	IOH_Delete32(HOST_SYSISR(base));
	IOH_Delete32(HOST_SYSIEN(base));
	IOH_Delete32(HOST_XBUFSTAT(base));
	IOH_Delete32(HOST_YBUFSTAT(base));
	IOH_Delete32(HOST_XYINTEN(base));
	IOH_Delete32(HOST_XFILLSTAT(base));
	IOH_Delete32(HOST_YFILLSTAT(base));
	IOH_Delete32(HOST_ETDENSET(base));
	IOH_Delete32(HOST_ETDENCLR(base));
	IOH_Delete32(HOST_IMMEDINT(base));
	IOH_Delete32(HOST_ETDDONESTAT(base));
	IOH_Delete32(HOST_ETDDONEN(base));
	IOH_Delete32(HOST_FRMNUMB(base));
	IOH_Delete32(HOST_LSTHRESH(base));
	IOH_Delete32(HOST_ROOTHUBA(base));
	IOH_Delete32(HOST_ROOTHUBB(base));
	IOH_Delete32(HOST_ROOTSTAT(base));
	IOH_Delete32(HOST_PORTSTAT1(base));
	IOH_Delete32(HOST_PORTSTAT2(base));
	IOH_Delete32(HOST_PORTSTAT3(base));
	IOH_DeleteRegion(ETD_BASE(base), 0x200);
	IOH_DeleteRegion(EP_BASE(base), 0x200);
/* The AHB DMA block */
	IOH_Delete32(DMA_REV(base));
	IOH_Delete32(DMA_INTSTAT(base));
	IOH_Delete32(DMA_INTEN(base));
	IOH_Delete32(DMA_ETDDMAERSTAT(base));
	IOH_Delete32(DMA_EPDMAERSTAT(base));
	IOH_Delete32(DMA_ETDDMAEN(base));
	IOH_Delete32(DMA_EPDMAEN(base));
	IOH_Delete32(DMA_ETDDMAXTEN(base));
	IOH_Delete32(DMA_EPDMAXTEN(base));
	IOH_Delete32(DMA_ETDDMAENXYT(base));
	IOH_Delete32(DMA_EPDMAENXYT(base));
	IOH_Delete32(DMA_ETCDMABST4EN(base));
	IOH_Delete32(DMA_EPDMABST4EN(base));
	IOH_Delete32(DMA_MISCCONTROL(base));
	IOH_Delete32(DMA_ETDDMACHANCLR(base));
	IOH_Delete32(DMA_EPDMACHANCLR(base));

	for (i = 0; i < 0x20; i++) {
		IOH_Delete32(DMA_ETDSMSA(base, i));
		IOH_Delete32(DMA_EPSMSA(base, i));
		IOH_Delete32(DMA_ETDDMABUFPTR(base, i));
		IOH_Delete32(DMA_EPDMABUFPTR(base, i));
	}
	for (i = 0; i < 0x14; i++) {
		IOH_Delete8(TRANS_VENODR_ID_LOW(base) + i);
	}
	IOH_Delete8(I2C_OTGDEVADDR(base));
	IOH_Delete8(I2C_NUMSEQOPS(base));
	IOH_Delete8(I2C_OPCTRL(base));
	IOH_Delete8(I2C_SEQREADSTRT(base));
	IOH_Delete8(I2C_DIVFACTOR(base));
	IOH_Delete8(I2C_INTANDCTRL(base));
	IOH_DeleteRegion(DATA_MEM(base), 0x1000);
}

static void
IMXOtg_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	IMXOtg *otg = (IMXOtg *) owner;
	uint32_t flags;
	int i;
	IOH_New32(OTG_HWMODE(base), otg_hwmode_read, otg_hwmode_write, otg);
	IOH_New32(OTG_CINT_STAT(base), otg_cint_stat_read, otg_cint_stat_write, otg);
	IOH_New32(OTG_CINT_STEN(base), otg_cint_sten_read, otg_cint_sten_write, otg);
	IOH_New32(OTG_CLK_CTRL(base), otg_clk_ctrl_read, otg_clk_ctrl_write, otg);
	IOH_New32(OTG_RST_CTRL(base), otg_rst_ctrl_read, otg_rst_ctrl_write, otg);
	IOH_New32(OTG_FRM_INVTL(base), otg_frm_invtl_read, otg_frm_invtl_write, otg);
	IOH_New32(OTG_FRM_REMAIN(base), otg_frm_remain_read, otg_frm_remain_write, otg);
	IOH_New32(OTG_HNP_CTRL(base), otg_hnp_ctrl_read, otg_hnp_ctrl_write, otg);
	IOH_New32(OTG_HNP_INT_STAT(base), otg_hnp_int_stat_read, otg_hnp_int_stat_write, otg);
	IOH_New32(OTG_HNP_INT_EN(base), otg_hnp_int_en_read, otg_hnp_int_en_write, otg);
	IOH_New32(OTG_USBCTRL(base), otg_usbctrl_read, otg_usbctrl_write, otg);

	IOH_New32(FUNC_COMSTAT(base), func_comstat_read, func_comstat_write, otg);
	IOH_New32(FUNC_DEVADDR(base), func_devaddr_read, func_devaddr_write, otg);
	IOH_New32(FUNC_SYSINTSTAT(base), func_sysintstat_read, func_sysintstat_write, otg);
	IOH_New32(FUNC_SYSINTEN(base), func_sysinten_read, func_sysinten_write, otg);
	IOH_New32(FUNC_XBUFINTSTAT(base), func_xbufintstat_read, func_xbufintstat_write, otg);
	IOH_New32(FUNC_YBUFINTSTAT(base), func_ybufintstat_read, func_ybufintstat_write, otg);
	IOH_New32(FUNC_XYINTEN(base), func_xyinten_read, func_xyinten_write, otg);
	IOH_New32(FUNC_XFILLSTAT(base), func_xfillstat_read, func_xfillstat_write, otg);
	IOH_New32(FUNC_YFILLSTAT(base), func_yfillstat_read, func_yfillstat_write, otg);
	IOH_New32(FUNC_ENDPNTEN(base), func_endpnten_read, func_endpnten_write, otg);
	IOH_New32(FUNC_ENDPNRDY(base), func_endpnrdy_read, func_endpnrdy_write, otg);
	IOH_New32(FUNC_IMMEDINT(base), func_immedint_read, func_immedint_write, otg);
	IOH_New32(FUNC_EPNTDONESTAT(base), func_epntdonestat_read, func_epntdonestat_write, otg);
	IOH_New32(FUNC_EPNTDONEEN(base), func_epntdoneen_read, func_epntdoneen_write, otg);
	IOH_New32(FUNC_EPNTTOGBITS(base), func_epnttogbits_read, func_epnttogbits_write, otg);
	IOH_New32(FUNC_FNEPRDYCLR(base), func_fneprdyclr_read, func_fneprdyclr_write, otg);

	IOH_New32(HOST_CTRL(base), host_ctrl_read, host_ctrl_write, otg);
	IOH_New32(HOST_SYSISR(base), host_sysisr_read, host_sysisr_write, otg);
	IOH_New32(HOST_SYSIEN(base), host_sysien_read, host_sysien_write, otg);
	IOH_New32(HOST_XBUFSTAT(base), host_xbufstat_read, host_xbufstat_write, otg);
	IOH_New32(HOST_YBUFSTAT(base), host_ybufstat_read, host_ybufstat_write, otg);
	IOH_New32(HOST_XYINTEN(base), host_xyinten_read, host_xyinten_write, otg);
	IOH_New32(HOST_XFILLSTAT(base), host_xfillstat_read, host_xfillstat_write, otg);
	IOH_New32(HOST_YFILLSTAT(base), host_yfillstat_read, host_yfillstat_write, otg);
	IOH_New32(HOST_ETDENSET(base), host_etdenset_read, host_etdenset_write, otg);
	IOH_New32(HOST_ETDENCLR(base), host_etdenclr_read, host_etdenclr_write, otg);
	IOH_New32(HOST_IMMEDINT(base), host_immedint_read, host_immedint_write, otg);
	IOH_New32(HOST_ETDDONESTAT(base), host_etddonestat_read, host_etddonestat_write, otg);
	IOH_New32(HOST_ETDDONEN(base), host_etddoneen_read, host_etddoneen_write, otg);
	IOH_New32(HOST_FRMNUMB(base), host_frmnumb_read, host_frmnumb_write, otg);
	IOH_New32(HOST_LSTHRESH(base), host_lsthresh_read, host_lsthresh_write, otg);
	IOH_New32(HOST_ROOTHUBA(base), host_roothuba_read, host_roothuba_write, otg);
	IOH_New32(HOST_ROOTHUBB(base), host_roothubb_read, host_roothubb_write, otg);
	IOH_New32(HOST_ROOTSTAT(base), host_rootstat_read, host_rootstat_write, otg);
	IOH_New32(HOST_PORTSTAT1(base), host_portstat_read, host_portstat_write, otg);
	IOH_New32(HOST_PORTSTAT2(base), host_portstat_read, host_portstat_write, otg);
	IOH_New32(HOST_PORTSTAT3(base), host_portstat_read, host_portstat_write, otg);

	IOH_NewRegion(ETD_BASE(base), 0x200, etd_mem_read, etd_mem_write, IOH_FLG_LITTLE_ENDIAN,
		      otg);
	IOH_NewRegion(EP_BASE(base), 0x200, ep_mem_read, ep_mem_write, IOH_FLG_LITTLE_ENDIAN, otg);
/* The AHB DMA block */
	IOH_New32(DMA_REV(base), dma_rev_read, dma_rev_write, otg);
	IOH_New32(DMA_INTSTAT(base), dma_intstat_read, dma_intstat_write, otg);
	IOH_New32(DMA_INTEN(base), dma_inten_read, dma_inten_write, otg);
	IOH_New32(DMA_ETDDMAERSTAT(base), dma_etddmaerstat_read, dma_etddmaerstat_write, otg);
	IOH_New32(DMA_EPDMAERSTAT(base), dma_epdmaerstat_read, dma_epdmaerstat_write, otg);
	IOH_New32(DMA_ETDDMAEN(base), dma_etddmaen_read, dma_etddmaen_write, otg);
	IOH_New32(DMA_EPDMAEN(base), dma_epdmaen_read, dma_epdmaen_write, otg);
	IOH_New32(DMA_ETDDMAXTEN(base), dma_etddmaxten_read, dma_etddmaxten_write, otg);
	IOH_New32(DMA_EPDMAXTEN(base), dma_epdmaxten_read, dma_epdmaxten_write, otg);
	IOH_New32(DMA_ETDDMAENXYT(base), dma_etddmaenxyt_read, dma_etddmaenxyt_write, otg);
	IOH_New32(DMA_EPDMAENXYT(base), dma_epdmaenxyt_read, dma_epdmaenxyt_write, otg);
	IOH_New32(DMA_ETCDMABST4EN(base), dma_etcdmabst4en_read, dma_etcdmabst4en_write, otg);
	IOH_New32(DMA_EPDMABST4EN(base), dma_epdmabst4en_read, dma_epdmabst4en_write, otg);
	IOH_New32(DMA_MISCCONTROL(base), dma_misccontrol_read, dma_misccontrol_write, otg);
	IOH_New32(DMA_ETDDMACHANCLR(base), dma_etddmachanclr_read, dma_etddmachanclr_write, otg);
	IOH_New32(DMA_EPDMACHANCLR(base), dma_epdmachanclr_read, dma_epdmachanclr_write, otg);
	for (i = 0; i < 0x20; i++) {
		IOH_New32(DMA_ETDSMSA(base, i), dma_etdsmsa_read, dma_etdsmsa_write, otg);
		IOH_New32(DMA_EPSMSA(base, i), dma_epsmsa_read, dma_epsmsa_write, otg);
		IOH_New32(DMA_ETDDMABUFPTR(base, i), dma_etddmabufptr_read, dma_etddmabufptr_write,
			  otg);
		IOH_New32(DMA_EPDMABUFPTR(base, i), dma_epdmabufptr_read, dma_epdmabufptr_write,
			  otg);
	}
	flags = IOH_FLG_OSZR_NEXT | IOH_FLG_OSZW_NEXT | IOH_FLG_HOST_ENDIAN;
	for (i = 0; i < 0x14; i++) {
		IOH_New8f(TRANS_VENODR_ID_LOW(base) + i, isp1301_read, isp1301_write, otg, flags);
	}
	flags = IOH_FLG_OSZR_NEXT | IOH_FLG_OSZW_NEXT | IOH_FLG_HOST_ENDIAN;
	IOH_New8f(I2C_OTGDEVADDR(base), i2c_otgdevaddr_read, i2c_otgdevaddr_write, otg, flags);
	IOH_New8f(I2C_NUMSEQOPS(base), i2c_numseqops_read, i2c_numseqops_write, otg, flags);
	IOH_New8f(I2C_SEQREADSTRT(base), i2c_seqreadstrt_read, i2c_seqreadstrt_write, otg, flags);
	IOH_New8f(I2C_OPCTRL(base), i2c_opctrl_read, i2c_opctrl_write, otg, flags);
	IOH_New8f(I2C_DIVFACTOR(base), i2c_divfactor_read, i2c_divfactor_write, otg, flags);
	IOH_New8f(I2C_INTANDCTRL(base), i2c_intandctrl_read, i2c_intandctrl_write, otg, flags);

	IOH_NewRegion(DATA_MEM(base), 0x1000, data_mem_read, data_mem_write, IOH_FLG_LITTLE_ENDIAN,
		      otg);
}

/*
 * --------------------------------------------------------------------
 * Handle incoming packets at root hub port
 * Don't care from which port the packets come
 * --------------------------------------------------------------------
 */
static void
IMX21Otg_PktSink(void *dev, const UsbPacket * pkt)
{
	IMXOtg *otg = (IMXOtg *) dev;
	if (otg->pkt_receiver) {
		otg->pkt_receiver(otg, pkt);
	} else {
		fprintf(stderr, "No receiver for Usb Packet\n");
	}
}

void
IMX21Otg_Plug(BusDevice * bdev, UsbDevice * usbdev, unsigned int port)
{
	IMXOtg *otg = (IMXOtg *) bdev->owner;
	RootHubPort *rhp;
	if (port > 2) {
		fprintf(stderr, "Trying to plug into noxexisting RootHubPort %d\n", port);
		return;
	}
	rhp = &otg->host.port[port];
	UsbDev_RegisterPacketSink(usbdev, otg, IMX21Otg_PktSink);
	rhp->usbdev = usbdev;
	if (!(rhp->portstat & PORTSTAT_CURCONST)) {
		rhp->portstat |= PORTSTAT_CURCONST;
		rhp->portstat |= PORTSTAT_CONNECTSC;
	}
	if (!(rhp->portstat & PORTSTAT_PRTENABST)) {
		rhp->portstat |= PORTSTAT_PRTENABST;
		rhp->portstat |= PORTSTAT_PRTENBLSC;
	}
	/* Send some attach message here */
}

void
IMX21Otg_Unplug(BusDevice * bdev, unsigned int port)
{
	IMXOtg *otg = (IMXOtg *) bdev->owner;
	RootHubPort *rhp;
	if (port > 2) {
		fprintf(stderr, "Trying to unplug from noxexisting RootHubPort %d\n", port);
		return;
	}
	rhp = &otg->host.port[port];
	if (!rhp->usbdev) {
		fprintf(stderr, "IMXOtg: Trying to disconnect unconnected port %d\n", port);
		return;

	}
	/* Send detach message is missing here */
	UsbDev_UnregisterPacketSink(rhp->usbdev, otg, IMX21Otg_PktSink);
	rhp->usbdev = 0;
	if (rhp->portstat & PORTSTAT_CURCONST) {
		rhp->portstat &= ~PORTSTAT_CURCONST;
		rhp->portstat |= PORTSTAT_CONNECTSC;
	}
	if (rhp->portstat & PORTSTAT_PRTENABST) {
		rhp->portstat &= ~PORTSTAT_PRTENABST;
		rhp->portstat |= PORTSTAT_PRTENBLSC;
	}

}

BusDevice *
IMX21Otg_New(const char *name)
{
	IMXOtg *otg = sg_new(IMXOtg);
	IMXUsbHost *host;
	I2C_Slave *i2c_slave;
	int i;
	char *trans_name = alloca(strlen(name) + 50);
	host = &otg->host;
	// init etdmem and epmem with random values is missing here
	for (i = 0; i < 3; i++) {
		RootHubPort *rhp = &host->port[i];
		rhp->otg = otg;
		CycleTimer_Init(&rhp->port_reset_timer, port_reset_done, rhp);
	}
	host->data_mem = otg->data_mem;	/* memory is shared with Function */
	host->otg = otg;
	CycleTimer_Init(&host->pktDelayTimer, host_send_packet, host);
	for (i = 0; i < 32; i++) {
		host->etd[i].nr = i;
	}
	otg->intUsbWkup = SigNode_New("%s.intWkup", name);
	otg->intUsbMnp = SigNode_New("%s.intMnp", name);
	otg->intUsbDma = SigNode_New("%s.intDma", name);
	otg->intUsbFunc = SigNode_New("%s.intFunc", name);
	otg->host.intUsbHost = SigNode_New("%s.intHost", name);
	otg->intUsbCtrl = SigNode_New("%s.intCtrl", name);
	if (!otg->intUsbWkup || !otg->intUsbMnp || !otg->intUsbDma || !otg->intUsbFunc
	    || !otg->host.intUsbHost || !otg->intUsbCtrl) {

		fprintf(stderr, "Can not create interrupt nodes for IMX21 OTG\n");
		exit(7);
	}
	otg->bdev.first_mapping = NULL;
	otg->bdev.Map = IMXOtg_Map;
	otg->bdev.UnMap = IMXOtg_Unmap;
	otg->bdev.owner = otg;
	otg->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;

	sprintf(trans_name, "%s.isp1301", name);
	i2c_slave = ISP1301_New(trans_name);
	otg->isp1301 = ISP1301_GetPtr(i2c_slave);
	CycleTimer_Init(&otg->host.frame_timer, host_do_sof, otg);
	reset_i2c(otg);
	reset_control(otg);
	reset_func(otg);
	reset_funcser(otg);
	reset_roothub(otg);
	reset_hostser(otg);
	reset_hc(otg);
	reset_dma(otg);
	fprintf(stderr, "iMX21 USB OTG controller \"%s\" created\n", name);
	return &otg->bdev;
}
