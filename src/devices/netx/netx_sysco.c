/*
 **************************************************************************************************
 * Emulation of Hilscher NetX system controller 
 *
 * state: Not implemented 
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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

#include "bus.h"
#include "sgstring.h"
#include "fio.h"
#include "signode.h"
#include "clock.h"

#define SYS_BOO_SR(base)		((base) + 0x00)
#define SYS_IOC_CR(base)		((base) + 0x04)
#define		IOC_IF_SELECT_N		(1<<31)
#define		IOC_SEL_E_PWM2_ECLK	(1<<28)
#define		IOC_SEL_F1_PWM3_ECLK	(1<<26)
#define		IOC_SEL_F0_PWM3_ECLK	(1<<25)
#define		IOC_SEL_WDG		(1<<24)
#define		IOC_SEL_ETM		(1<<23)
#define		IOC_LED_MII3		(1<<22)
#define		IOC_LED_MII2		(1<<21)
#define		IOC_SEL_MP		(1<<20)
#define		IOC_SEL_ENC1		(1<<19)
#define		IOC_SEL_ENC0		(1<<18)
#define		IOC_SEL_E_RPWM2		(1<<17)
#define		IOC_SEL_E_FAILURE2	(1<<16)
#define		IOC_SEL_E_PWM2		(1<<15)
#define		IOC_SEL_F3_PWM3		(1<<14)
#define		IOC_SEL_F2_RPWM3	(1<<13)
#define		IOC_SEL_F2_FAILURE3	(1<<12)
#define		IOC_SEL_F1_RPWM3	(1<<11)
#define		IOC_SEL_F1_PWM3		(1<<10)
#define		IOC_SEL_F0_FAILURE3	(1<<9)
#define		IOC_SEL_F0_PWM3		(1<<8)
#define		IOC_SEL_FO1		(1<<7)
#define		IOC_SEL_FO0		(1<<6)
#define		IOC_SEL_MII3PWM		(1<<5)
#define		IOC_SEL_MII23		(1<<4)
#define		IOC_SEL_MII3		(1<<3)
#define		IOC_SEL_MII2		(1<<2)
#define		IOC_SEL_LCD_COL		(1<<1)
#define		IOC_SEL_LCD_BW		(1<<0)

#define SYS_IOC_MR(base)		((base) + 0x08)
#define SYS_RES_CR(base)		((base) + 0x0c)
#define 	RES_CR_RSTIN         (1<<0)
#define 	RES_CR_WDG_RES       (1<<1)
#define 	RES_CR_HOST_RES      (1<<2)
#define 	RES_CR_FIRMW_RES     (1<<3)
#define 	RES_CR_XPEC0_RES     (1<<4)
#define 	RES_CR_XPEC1_RES     (1<<5)
#define 	RES_CR_XPEC2_RES     (1<<6)
#define 	RES_CR_XPEC3_RES     (1<<7)
#define 	RES_CR_DIS_XPEC0_RES (1<<16)
#define 	RES_CR_DIS_XPEC1_RES (1<<17)
#define 	RES_CR_DIS_XPEC2_RES (1<<18)
#define 	RES_CR_DIS_XPEC3_RES (1<<19)
#define 	RES_CR_FIRMW_FLG0    (1<<20)
#define 	RES_CR_FIRMW_FLG1    (1<<21)
#define 	RES_CR_FIRMW_FLG2    (1<<22)
#define 	RES_CR_FIRMW_FLG3    (1<<23)
#define 	RES_CR_FIRMW_RES_EN  (1<<24)
#define 	RES_CR_RSTOUT        (1<<25)
#define 	RES_CR_EN_RSTOUT     (1<<26)

#define SYS_PHY_CONTROL(base)		((base) + 0x10)
#define SYS_REV(base)             	((base) + 0x34)
#define SYS_IOC_ACCESS_KEY(base)  	((base) + 0x70)
#define SYS_WDG_TR(base)          	((base) + 0x200)
#define SYS_WDG_CTR(base)         	((base) + 0x204)
#define SYS_WDG_IRQ_TIMEOUT(base) 	((base) + 0x208)
#define SYS_WDG_RES_TIMEOUT(base) 	((base) + 0x20c)

typedef struct NetXSysco {
	BusDevice bdev;
	uint32_t reg_boo_sr;
	uint32_t reg_ioc_cr;
	uint32_t reg_ioc_mr;
	uint32_t reg_res_cr;
	uint32_t reg_phy_control;
	uint32_t reg_sys_rev;
	uint32_t reg_ioc_access_key;
	uint32_t reg_wdg_tr;
	uint32_t reg_wdg_ctr;
	uint32_t reg_wdg_irq_timeout;
	uint32_t reg_wdg_res_timeout;
} NetXSysco;

static uint32_t
boo_sr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
        return 0;
}

static void
boo_sr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
}

static uint32_t
ioc_cr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
        return 0;
}

static void
ioc_cr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
}
static uint32_t
ioc_mr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
        return 0;
}

static void
ioc_mr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
}

/* 
 *****************************************************************
 * Reset Control Register
 *****************************************************************
 */
static uint32_t
res_cr_read(void *clientData,uint32_t address,int rqlen)
{
	NetXSysco *sc = (NetXSysco *) clientData;	
        return sc->reg_res_cr;
}

static void
res_cr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	NetXSysco *sc = (NetXSysco *) clientData;	
	sc->reg_res_cr = value; 
	if(value & RES_CR_FIRMW_RES && (value & RES_CR_FIRMW_RES_EN)) {
		fprintf(stderr,"Reset by System controller\n");
		exit(0);
	}
}

static uint32_t
phy_control_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
        return 0;
}

static void
phy_control_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
}

static uint32_t
rev_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
        return 0;
}

static void
rev_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
}

static uint32_t
ioc_access_key_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
        return 0;
}

static void
ioc_access_key_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
}

static uint32_t
wdg_tr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
        return 0;
}

static void
wdg_tr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
}

static uint32_t
wdg_ctr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
        return 0;
}

static void
wdg_ctr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
}

static uint32_t
wdg_irq_timeout_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
        return 0;
}

static void
wdg_irq_timeout_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
}

static uint32_t
wdg_res_timeout_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
        return 0;
}

static void
wdg_res_timeout_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Sysco register %08x not implemented\n",address);
}


static void
NetXSysco_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
        NetXSysco *sc = (NetXSysco *) owner;
	IOH_New32(SYS_BOO_SR(base),boo_sr_read,boo_sr_write,sc);
	IOH_New32(SYS_IOC_CR(base),ioc_cr_read,ioc_cr_write,sc);
	IOH_New32(SYS_IOC_MR(base),ioc_mr_read,ioc_mr_write,sc);
	IOH_New32(SYS_RES_CR(base),res_cr_read,res_cr_write,sc);
	IOH_New32(SYS_PHY_CONTROL(base),phy_control_read,phy_control_write,sc);
	IOH_New32(SYS_REV(base),rev_read,rev_write,sc);
	IOH_New32(SYS_IOC_ACCESS_KEY(base),ioc_access_key_read,ioc_access_key_write,sc);
	IOH_New32(SYS_WDG_TR(base),wdg_tr_read,wdg_tr_write,sc);
	IOH_New32(SYS_WDG_CTR(base),wdg_ctr_read,wdg_ctr_write,sc);
	IOH_New32(SYS_WDG_IRQ_TIMEOUT(base),wdg_irq_timeout_read,wdg_irq_timeout_write,sc);
	IOH_New32(SYS_WDG_RES_TIMEOUT(base),wdg_res_timeout_read,wdg_res_timeout_write,sc);
}

static void
NetXSysco_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(SYS_BOO_SR(base));
	IOH_Delete32(SYS_IOC_CR(base));
	IOH_Delete32(SYS_IOC_MR(base));
	IOH_Delete32(SYS_RES_CR(base));
	IOH_Delete32(SYS_PHY_CONTROL(base));
	IOH_Delete32(SYS_REV(base));
	IOH_Delete32(SYS_IOC_ACCESS_KEY(base));
	IOH_Delete32(SYS_WDG_TR(base));
	IOH_Delete32(SYS_WDG_CTR(base));
	IOH_Delete32(SYS_WDG_IRQ_TIMEOUT(base));
	IOH_Delete32(SYS_WDG_RES_TIMEOUT(base));
}


BusDevice *
NetXSysco_New(const char *devname) {
        NetXSysco *sc = sg_new(NetXSysco);
        sc->bdev.first_mapping = NULL;
        sc->bdev.Map = NetXSysco_Map;
        sc->bdev.UnMap = NetXSysco_UnMap;
        sc->bdev.owner = sc;
        sc->bdev.hw_flags = MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;

        fprintf(stderr,"Created NetX System Controller \"%s\"\n",devname);
        return &sc->bdev;
}

