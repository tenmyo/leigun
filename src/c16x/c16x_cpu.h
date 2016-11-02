#include <stdint.h>
#include <bus.h>

#define C16X_FLAG_IRQ (1)
#define C16X_FLAG_NMI (2)

#define SFR_BASE	(0xfe00)
#define ESFR_BASE	(0xf000)
#define SFR_ADDR(reg) (((reg)<<1)+SFR_BASE)
#define ESFR_ADDR(reg) (((reg)<<1)+ESFR_BASE)
/*
 * ESFR register numbers 
 */
#define ESFR_XADRS1 	(0xa)
#define ESFR_XADRS2 	(0xb)
#define ESFR_XADRS3 	(0xc)
#define ESFR_XADRS4 	(0xd)
#define ESFR_XADRS5 	(0xe)
#define ESFR_XADRS6 	(0xf)
#define ESFR_XPERCON 	(0x12)
#define ESFR_IDMEM2	(0x3b)
#define ESFR_IDPROC	(0x3c)
#define ESFR_IDMEM	(0x3d)
#define ESFR_IDCHIP	(0x3e)
#define ESFR_IDMANUF	(0x3f)
#define ESFR_SSCTB	(0x58)
#define ESFR_SSCRB	(0x59)
#define ESFR_SSCBR	(0x5a)
#define ESFR_SSCCLC	(0x5b)
#define ESFR_SCUSLC	(0x60)
#define ESFR_SCUSLS	(0x61)
#define ESFR_RTCCLC	(0x64)
#define ESFR_RTCRELL	(0x66)
#define ESFR_RTCRELH	(0x67)
#define ESFR_T14REL	(0x68)
#define ESFR_T14	(0x69)
#define ESFR_RTCL	(0x6a)
#define ESFR_RTCH	(0x6b)
#define ESFR_DTIDR	(0x6c)
#define ESFR_DP0L	(0x80)
#define ESFR_DP0H	(0x81)
#define ESFR_DP1L	(0x82)
#define ESFR_DP1H	(0x83)
#define ESFR_RP0H	(0x84)
#define ESFR_XBCON1	(0x8a)
#define ESFR_XBCON2	(0x8b)
#define ESFR_XBCON3	(0x8c)
#define ESFR_XBCON4	(0x8d)
#define ESFR_XBCON5	(0x8e)
#define ESFR_XBCON6	(0x8f)
#define ESFR_UTD3IC	(0xb0)
#define ESFR_UTD4IC	(0xb1)
#define ESFR_UTD5IC	(0xb2)
#define ESFR_UTD6IC	(0xb3)
#define ESFR_UTD7IC	(0xb4)
#define ESFR_URXRIC	(0xb5)
#define ESFR_UTXRIC	(0xb6)
#define ESFR_UCFGVIC	(0xb7)
#define ESFR_USOFIC	(0xb8)
#define ESFR_USSOIC	(0xb9)
#define ESFR_USSIC	(0xba)
#define ESFR_ULCDIC	(0xbb)
#define ESFR_USETIC	(0xbc)
#define ESFR_URD0IC	(0xbd)
#define ESFR_EPECIC	(0xbe)
#define ESFR_PECCLIC	(0xc0)
#define ESFR_RTC_INTIC	(0xc2)
#define ESFR_XP0IC	(0xc3)
#define ESFR_ABENDIC	(0xc6)
#define ESFR_XP1IC	(0xc7)
#define ESFR_ABSTIC	(0xca)
#define ESFR_RES6IC	(0xcd)
#define ESFR_S0TBIC	(0xce)
#define ESFR_XP3IC	(0xcf)
#define ESFR_EXICON	(0xe0)
#define ESFR_ODP2	(0xe1)
#define ESFR_ODP3	(0xe3)
#define ESFR_RTCISNC	(0xe4)
#define ESFR_ODP4	(0xe5)
#define ESFR_RTCCON	(0xe6)
#define ESFR_ODP6	(0xe7)
#define ESFR_SYSCON2	(0xe8)
#define ESFR_SYSCON3	(0xea)
#define ESFR_EXISEL	(0xed)
#define ESFR_SYSCON1	(0xee)
#define ESFR_ISNC	(0xef)

/* 
 * SFR Area
 */
#define SFR_DPP0	(0)
#define SFR_DPP1	(1)
#define SFR_DPP2	(2)
#define SFR_DPP3	(3)
#define SFR_CSP		(4)
#define SFR_EMUCON	(5)
#define SFR_MDH		(6)
#define SFR_MDL		(7)
#define SFR_CP		(8)
#define SFR_SP		(9)
#define SFR_STKOV	(0xa)
#define SFR_STKUN	(0xb)
#define SFR_ADDRSEL1	(0xc)
#define SFR_ADDRSEL2	(0xd)
#define SFR_ADDRSEL3	(0xe)
#define SFR_ADDRSEL4	(0xf)
#define SFR_ODP0H	(0x11)
#define SFR_ODP1L	(0x12)
#define SFR_ODP1H	(0x13)
#define SFR_T2		(0x20)
#define SFR_T3		(0x21)
#define SFR_T4		(0x22)
#define SFR_T5		(0x23)
#define SFR_T6		(0x24)
#define SFR_CAPREL	(0x25)
#define SFR_GPTCLC	(0x26)
#define SFR_P0LPUDSEL	(0x30)
#define SFR_P0HPUDSEL	(0x31)
#define SFR_P0LPUDEN	(0x32)
#define SFR_P0HPUDEN	(0x33)
#define SFR_P0LPHEN	(0x34)
#define SFR_P0HPHEN	(0x35)
#define SFR_P1LPUDSEL	(0x36)
#define SFR_P1HPUDSEL	(0x37)
#define SFR_P1LPUDEN	(0x38)
#define SFR_P1HPUDEN	(0x39)
#define SFR_P1LPHEN	(0x3a)
#define SFR_P1HPHEN	(0x3b)
#define SFR_P2PUDSEL	(0x3c)
#define SFR_P2PUDEN	(0x3d)
#define SFR_P2PHEN	(0x3e)

#define SFR_P3PUDSEL	(0x3f)
#define SFR_P3PUDEN	(0x40)
#define SFR_P3PHEN	(0x41)

#define SFR_P4PUDSEL	(0x42)
#define SFR_P4PUDEN	(0x43)
#define SFR_P4PHEN	(0x44)

#define SFR_P6PUDSEL	(0x48)
#define SFR_P6PUDEN	(0x49)
#define SFR_P6PHEN	(0x4a)
#define SFR_S0PMW	(0x55)
#define SFR_WDT		(0x57)
#define SFR_S0TBUF	(0x58)
#define SFR_S0RBUF	(0x59)
#define SFR_S0BG	(0x5a)
#define SFR_S0FDV	(0x5b)
#define SFR_PECC0	(0x60)
#define SFR_PECC1	(0x61)
#define SFR_PECC2	(0x62)
#define SFR_PECC3	(0x63)
#define SFR_PECC4	(0x64)
#define SFR_PECC5	(0x65)
#define SFR_PECC6	(0x66)
#define SFR_PECC7	(0x67)
#define SFR_PECSN0	(0x68)
#define SFR_PECSN1	(0x69)
#define SFR_PECSN2	(0x6a)
#define SFR_PECSN3	(0x6b)
#define SFR_PECSN4	(0x6c)
#define SFR_PECSN5	(0x6d)
#define SFR_PECSN6	(0x6e)
#define SFR_PECSN7	(0x6f)

#define SFR_PECXC0	(0x78)
#define SFR_PECXC2	(0x79)
#define SFR_ABS0CON	(0x7c)
#define SFR_ABSTAT	(0x7f)
#define SFR_P0L		(0x80)
#define SFR_P0H		(0x81)
#define SFR_P1L		(0x82)
#define SFR_P1H		(0x83)
#define SFR_BUSCON0	(0x86)
#define SFR_MDC		(0x87)
#define SFR_PSW		(0x88)
#define SFR_SYSCON	(0x89)
#define SFR_BUSCON1	(0x8a)
#define SFR_BUSCON2	(0x8b)
#define SFR_BUSCON3	(0x8c)
#define SFR_BUSCON4	(0x8d)
#define SFR_ZEROS	(0x8e)
#define SFR_ONES	(0x8f)
#define SFR_T2CON	(0xa0)
#define SFR_T3CON	(0xa1)
#define SFR_T4CON	(0xa2)
#define SFR_T5CON	(0xa3)
#define SFR_T6CON	(0xa4)
#define SFR_T2IC	(0xb0)
#define SFR_T3IC	(0xb1)
#define SFR_T4IC	(0xb2)
#define SFR_T5IC	(0xb3)
#define SFR_T6IC	(0xb4)
#define SFR_CRIC	(0xb5)
#define SFR_S0TIC	(0xb6)
#define SFR_S0RIC	(0xb7)
#define SFR_S0EIC	(0xb8)
#define SFR_SSCTIC	(0xb9)
#define SFR_SSCRIC	(0xba)
#define SFR_SSCEIC	(0xbb)
#define SFR_URD3IC	(0xbc)
#define SFR_URD4IC	(0xbd)
#define SFR_URD5IC	(0xbe)
#define SFR_URD6IC	(0xbf)
#define SFR_URD7IC	(0xc0)
#define SFR_UTD0IC	(0xc1)
#define SFR_UTD1IC	(0xc2)
#define SFR_UTD2IC	(0xc3)
#define SFR_FEI0IC	(0xc4)
#define SFR_FEI1IC	(0xc5)
#define SFR_RES4IC	(0xcb)
#define SFR_URD2IC	(0xce)
#define SFR_URD1IC	(0xcf)
#define SFR_CLISNC	(0xd4)
#define SFR_FOCON	(0xd5)
#define SFR_TFR		(0xd6)
#define SFR_WDTCON	(0xd7)
#define SFR_S0CON	(0xd8)
#define SFR_SSCCON	(0xd9)
#define SFR_S0CLC	(0xdd)
#define SFR_P2		(0xe0)
#define SFR_DP2		(0xe1)
#define SFR_P3		(0xe2)
#define SFR_DP3		(0xe3)
#define SFR_P4		(0xe4)
#define SFR_DP4		(0xe5)
#define SFR_P6		(0xe6)
#define SFR_DP6		(0xe7)


#define PSW_FLAG_N (1)
#define PSW_FLAG_C (1<<1)
#define PSW_FLAG_V (1<<2)
#define PSW_FLAG_Z (1<<3)
#define PSW_FLAG_E (1<<4)
#define PSW_MULIP  (1<<5)
#define PSW_USR0   (1<<6)
#define PSW_HLDEN  (1<<10)
#define PSW_IEN	   (1<<11)
#define PSW_ILVL_MASK (0xf000)
#define PSW_ILVL_SHIFT (12)

#define REG_SFR(x) 	(gc16x.sfr16[x])
#define REG_DPP(x) 	(gc16x.dpp[x])
#define REG_CP  	(gc16x.cp)
#define REG_PSW		(gc16x.psw)
#define REG_IP  	(gc16x.ip)
#define REG_SP  	(gc16x.sp)
#define REG_CSP  	(gc16x.csp)
#define REG_MDL 	(gc16x.mdl)
#define REG_MDH 	(gc16x.mdh)
#define REG_MDC 	(gc16x.mdc)
#define REG_SYSCON 	(gc16x.syscon)
#define  SYSCON_SGTDIS (REG_SYSCON & (1<<11))
#define 	MDC_MDRIU (1<<4)

/*
 * --------------------------------------------------
 * Bitfield definitions for CPU running in locked
 * EXT-Modes
 * --------------------------------------------------
 */
#define EXTMODE_ESFR		(1)
#define EXTMODE_PAGE		(2)
#define EXTMODE_SEG		(4)

#if 0
static inline void
sfr_set16(uint16_t value,uint16_t reg) 
{

}
#endif

typedef struct C16x {
	uint16_t dpp[4];
	uint16_t cp;
	uint16_t psw;
	uint16_t ip; 		/* Instruction Pointer */	
	uint16_t sp;
	uint16_t csp;
	uint16_t mdl;
	uint16_t mdh;
	uint16_t mdc;
	uint16_t syscon;
	uint16_t stkun;
	uint16_t stkov;
	uint16_t wdtcon;

	int ipl; /* Interrupt privilege level */
	/* lock for atomic extr extp, exts, extpr and extsr instructions */
	int lock_counter;
	uint32_t extmode;
	uint32_t extaddr;	/* Page/Segment addr during locked sequence */


	/* Interrupts and NMIs */
	uint16_t signals_raw;
	uint16_t signal_mask;
	uint16_t signals;

#if 0
        //cpu->s0rbuf = undefined;
        //cpu->sscrb = undefined;
        //cpu->syscon = reset config
        //cpu->buscon0 = reset config
        //cpu->rp0h = reset config
        cpu->ones = 0xffff;
#endif
} C16x;

extern C16x gc16x;

static inline uint32_t 
C16x_reg_address16(uint32_t reg) 
{
	if(reg>=0xf0) {
		return REG_CP + ((reg&0xf)<<1);
	}
	if(gc16x.extmode & EXTMODE_ESFR) {
		return 0xf000+(reg<<1);
	} else {
		return 0xfe00+(reg<<1);
	}
}

static inline uint32_t 
C16x_reg_address8(uint32_t reg) 
{
	if(reg>=0xf0) {
		return REG_CP + (reg&0xf);
	}
	if(gc16x.extmode & EXTMODE_ESFR) {
		return 0xf000+(reg<<1);
	} else {
		return 0xfe00+(reg<<1);
	}
}

static inline uint16_t
C16x_ReadReg16(uint8_t reg) 
{
	uint32_t addr= C16x_reg_address16(reg);
	return Bus_Read16(addr);
}

static inline void  
C16x_SetReg16(uint16_t value,uint8_t reg) 
{
	uint32_t addr= C16x_reg_address16(reg);
	Bus_Write16(value,addr);
}

static inline uint8_t
C16x_ReadReg8(uint8_t reg) 
{
	uint32_t addr= C16x_reg_address8(reg);
	return Bus_Read8(addr);
}

static inline void  
C16x_SetReg8(uint8_t value,uint8_t reg) 
{
	uint32_t addr= C16x_reg_address8(reg);
	Bus_Write8(value,addr);
}

static inline uint32_t
C16x_BitoffAddr(uint8_t bitoff) 
{
	int use_esfr = 0; /* complete this one day */ 
	if(bitoff>=0xf0) {
		return REG_CP + ((bitoff&0xf)<<1);
	}
	if(bitoff<0x80) {
		return 0xfd00 + (bitoff<<1);
	} 
	if(use_esfr) {
		return 0xf100 +((bitoff&0x7f)<<1);
	} else {
		return 0xff00 +((bitoff&0x7f)<<1);
	}
}

static inline uint16_t
C16x_ReadBitoff(uint8_t bitoff) 
{
	uint32_t addr = C16x_BitoffAddr(bitoff);
	return Bus_Read16(addr);
}

static inline void
C16x_WriteBitoff(uint16_t value,uint8_t bitoff) 
{
	uint32_t addr = C16x_BitoffAddr(bitoff);
	Bus_Write16(value,addr);
}

static inline uint16_t
C16x_ReadBitaddr(uint8_t bitaddr) 
{
	return  C16x_ReadBitoff(bitaddr);
}

static inline void
C16x_WriteBitaddr(uint16_t value,uint8_t bitaddr) 
{
	C16x_WriteBitoff(value,bitaddr);
}

/*
 * --------------------------------------------------
 * Page Translation using the DPP registers for 
 * Addressing mode "mem"
 * --------------------------------------------------
 */
static inline uint32_t
C16x_TranslateAddr(uint16_t addr) 
{
	C16x *c16x = &gc16x;
	uint16_t dpp;
	int dppnr = (addr>>14)&3;
	if(c16x->extmode & EXTMODE_PAGE) {
		return (addr & 0x3fff) | c16x->extaddr;
	} else if(c16x->extmode & EXTMODE_SEG) {
		return (addr & 0xffff) | c16x->extaddr;
	} else {
		dpp=REG_DPP(dppnr);
		if(SYSCON_SGTDIS) {
			return (addr & 0x3fff) | ((dppnr&0x3)<<14);
		} else {
			return (addr & 0x3fff) | ((dpp&0x3ff)<<14);
		}
	}
}

static inline uint16_t
C16x_MemRead16(uint16_t addr) 
{
	uint32_t taddr = C16x_TranslateAddr(addr);
	return Bus_Read16(taddr);
}

static inline uint8_t
C16x_MemRead8(uint16_t addr) 
{
	uint32_t taddr = C16x_TranslateAddr(addr);
	return Bus_Read8(taddr);
}
static inline void
C16x_MemWrite16(uint16_t value,uint32_t addr) 
{
	uint32_t taddr = C16x_TranslateAddr(addr);
	Bus_Write16(value,taddr);
}

static inline void
C16x_MemWrite8(uint8_t value,uint32_t addr) 
{
	uint32_t taddr = C16x_TranslateAddr(addr);
	Bus_Write8(value,taddr);
}

static inline uint16_t  
C16x_ReadGpr16(int reg) 
{
	return C16x_MemRead16(REG_CP+(reg<<1));		
}
static inline uint16_t  
C16x_ReadGpr8(int reg) 
{
	return C16x_MemRead8(REG_CP+reg);		
}

static inline void  
C16x_SetGpr16(uint16_t value,int reg) 
{
	return C16x_MemWrite16(value,REG_CP+(reg<<1));		
}

static inline void  
C16x_SetGpr8(uint8_t value,int reg) 
{
	return C16x_MemWrite8(value,REG_CP+reg);		
}

/*
 * -------------------------------------------
 * The interface to the outside world 
 * -------------------------------------------
 */

C16x *C16x_New();
void C16x_Run();
