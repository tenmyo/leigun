/*
 * ----------------------------------------------------
 *
 * PowerPC Memory Management Unit 
 * (C) 2008 Jochen Karrer 
 *   Author: Jochen Karrer
 *
 * ----------------------------------------------------
 */

#include <bus.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cpu_ppc.h>

#define SPR_MI_CTR	(784)
#define		MI_CTR_GPM	(1<<31)
#define		MI_CTR_PPM	(1<<30)
#define		MI_CTR_CIDEF	(1<<29)
#define		MI_CTR_RSV4I	(1<<27)
#define		MI_CTR_PPCS	(1<<25)
#define		MI_CTR_ITLB_INDX_SHIFT	(8)
#define		MI_CTR_ITLB_INDX_MASK	(31<<8)

#define SPR_MD_CTR	(792)
#define		MD_CTR_GPM	(1<<31)
#define		MD_CTR_PPM	(1<<30)
#define		MD_CTR_CIDEF	(1<<29)
#define		MD_CTR_WTDEF	(1<<28)
#define		MD_CTR_RSVD	(1<<27)
#define		MD_CTR_TWAM	(1<<26)
#define		MD_CTR_PPCS	(1<<25)
#define		MD_CTR_ITLB_INDX_SHIFT	(8)
#define		MD_CTR_ITLB_INDX_MASK	(31<<8)

#define SPR_MI_EPN	(787)
#define SPR_MD_EPN	(795)
#define		Mx_EPN_SHIFT 		(12)
#define		Mx_EPN_MASK  		(0xfffff<<12)
#define		Mx_EPN_EV    		(1<<22)
#define		Mx_EPN_ASID_MASK 	(0xf)

#define SPR_MI_TWC	(789)
#define SPR_MD_TWC	(797)
#define		MD_TWC_L2TB_SHIFT	(12)
#define		MD_TWC_L2TB_MASK	(0xfffff << 12)
#define		Mx_TWC_APG_SHIFT 	(5)
#define		Mx_TWC_APG_MASK		(0xf<<5)
#define 	Mx_TWC_G		(1<<4)
#define 	Mx_TWC_PS_SHIFT		(2)
#define 	Mx_TWC_PS_MASK		(3<<2)
#define		MD_TWC_WT		(1<<1)
#define		Mx_TWC_V		(1<<0)

#define SPR_MI_RPN	(790)
#define SPR_MD_RPN	(798)
#define		Mx_MD_RPN_SHIFT	(12)
#define		Mx_MD_RPN_MASK	(0xfffff << 12)
#define		Mx_MD_PP_SHIFT	(4)
#define		Mx_MD_PP_MASK	(0xff<<4)
#define		Mx_MD_SPS	(1<<3)
#define		Mx_MD_SH	(1<<2)
#define		Mx_MD_CI	(1<<1)
#define		Mx_MD_V		(1<<0)

#define SPR_M_TWB	(796)
#define		M_TWB_L1TB_SHIFT	(12)
#define		M_TWB_L1TB_MASK		(0xfffff<<12)
#define		M_TWB_L1INDX_SHIFT	(2)
#define		M_TWB_L1INDX_MASK	(0xffc)
#define SPR_M_CASID	(793)
#define		M_CASID_MASK	(0xf)

#define SPR_MI_AP	(786)
#define SPR_MD_AP	(794)
#define		Mx_AP_GP_SHIFT(x)	((30-((x)<<1))
#define		Mx_AP_GP_MASK(x)	((3 <<(Mx_AP_GP_SHIFT(x)))

/* Scratch register TW */
#define SPR_M_TW	(799)
#define SPR_MI_CAM	(816)
#define		MI_CAM_EPN_SHIFT	(12)
#define		MI_CAM_EPN_MASK		(0xfffff<<12)
#define		MI_CAM_PS_SHIFT		(9)
#define		MI_CAM_PS_MASK		(7<<9)
#define		MI_CAM_ASID_SHIFT	(5)
#define		MI_CAM_ASID_MASK	(0xf << 5)
#define		MI_CAM_SH		(1<<4)
#define		MI_CAM_SPV_MASK		(0xf)

#define SPR_MI_RAM0	(817)
#define		MI_RAM0_RPN_SHIFT	(12)
#define		MI_RAM0_RPN_MASK	(0xfffff << 12)
#define		MI_RAM0_PS_B_SHIFT	(9)
#define		MI_RAM0_PS_B_MASK	(7<<9)
#define		MI_RAM0_CI		(1<<23)
#define		MI_RAM0_APG_SHIFT	(5)
#define		MI_RAM0_APG_MASK	(0xf<<5)
#define		MI_RAM0_SFP		(0xf)

#define SPR_MI_RAM1	(818)
#define		MI_RAM1_UFP_SHIFT	(2)
#define		MI_RAM1_UFP_MASK	(0xf << 2)
#define		MI_RAM1_PV		(1<<1)
#define		MI_RAM1_G		(1<<0)
#define SPR_MD_CAM	(824)
#define		MD_CAM_EPN_SHIFT	(12)
#define		MD_CAM_EPN_MASK		(0xfffff<<12)
#define		MD_CAM_SPVF_SHIFT		(8)
#define		MD_CAM_SPVF_MASK		(0xf<<8)
#define		MD_CAM_SH		(1<<5)
#define		MD_CAM_ASID_MASK	(0xf << 0)
#define		MD_CAM_ASID_SHIFT	(0)
#define SPR_MD_RAM0	(825)
#define		MD_RAM0_RPN_SHIFT	(12)
#define		MD_RAM0_RPN_MASK	(0xfffff<<12)
#define		MD_RAM0_PS_SHIFT	(9)
#define		MD_RAM0_PS_MASK		(7<<9)
#define		MD_RAM0_APGI_SHIFT	(5)
#define		MD_RAM0_APGI_MASK	(0xf<<5)
#define		MD_RAM0_G		(1<<4)
#define		MD_RAM0_WT		(1<<3)
#define		MD_RAM0_CI		(1<<2)

#define SPR_MD_RAM1	(826)
#define		MD_RAM1_C 		(1<<14)
#define		MD_RAM1_EVF		(1<<13)
#define		MD_RAM1_SA_SHIFT	(9)
#define		MD_RAM1_SA_MASK		(0xf<<9)
#define		MD_RAM1_SAT		(1<<8)
#define		MD_RAM1_URP0		(1<<7)
#define		MD_RAM1_UWP0		(1<<6)
#define		MD_RAM1_URP1		(1<<5)
#define		MD_RAM1_UWP1		(1<<4)
#define		MD_RAM1_URP2		(1<<3)
#define		MD_RAM1_UWP2		(1<<2)
#define		MD_RAM1_URP3		(1<<1)
#define		MD_RAM1_UWP3		(1<<0)

#define TLB_HASH_INDEX(addr) (((addr)>> 10) & 0x3ff)

typedef struct TlbEntry {
	uint32_t va;		// Target Virtual Address
	uint32_t pa;		// Target Physical Address
	uint32_t pa_mask;	// Target Physical Address
	void *hva;		// Host Virtual address
	struct TlbEntry *next;
} TlbEntry;

typedef struct MPC8xx_Mmu {
	TlbEntry itlbe[32];
	TlbEntry dtlbe[32];
	TlbEntry *itlbHash[1024];
	TlbEntry *dtlbHash[1024];
	uint32_t spr_mi_ctr;
	uint32_t spr_md_ctr;
	uint32_t spr_mi_epn;
	uint32_t spr_md_epn;
	uint32_t spr_mi_twc;
	uint32_t spr_md_twc;
	uint32_t spr_mi_rpn;
	uint32_t spr_md_rpn;
	uint32_t spr_m_twb;
	uint32_t spr_m_casid;
	uint32_t spr_mi_ap;
	uint32_t spr_md_ap;
	uint32_t spr_m_tw;
	uint32_t spr_mi_cam;
	uint32_t spr_mi_ram0;
	uint32_t spr_mi_ram1;
	uint32_t spr_md_cam;
	uint32_t spr_md_ram0;
	uint32_t spr_md_ram1;
} MPC8xx_Mmu;

#if 0
static uint32_t
itlb_translate(MPC8xx_Mmu * mmu, uint32_t pa)
{
	uint32_t index = TLB_HASH_INDEX(pa);
	TlbEntry *tlbe = mmu->itlbHash[index];
	while (tlbe) {
		if ((tlbe->pa & tlbe->pa_mask) == (pa & tlbe->pa_mask)) {
			return tlbe->pa | (pa & ~tlbe->pa_mask);
		}
		tlbe = tlbe->next;
	}
	// exception
}

static uint32_t
dtlb_translate(MPC8xx_Mmu * mmu, uint32_t pa)
{
	uint32_t index = TLB_HASH_INDEX(pa);
	TlbEntry *tlbe = mmu->dtlbHash[index];
	while (tlbe) {
		if ((tlbe->pa & tlbe->pa_mask) == (pa & tlbe->pa_mask)) {
			return tlbe->pa | (pa & ~tlbe->pa_mask);
		}
		tlbe = tlbe->next;
	}
	// exception
}
#endif

static void
itlbe_remHash(MPC8xx_Mmu * mmu, TlbEntry * itlbe)
{
	uint32_t index = TLB_HASH_INDEX(itlbe->pa & itlbe->pa_mask);
	TlbEntry *cursor = mmu->itlbHash[index];
	if (cursor == itlbe) {
		mmu->itlbHash[index] = cursor->next;
		return;
	}
	while (cursor) {
		if (cursor->next == itlbe) {
			cursor->next = itlbe->next;
			return;
		}
		cursor = cursor->next;
	}

}

static inline void
itlbe_addHash(MPC8xx_Mmu * mmu, TlbEntry * itlbe)
{
	uint32_t index = TLB_HASH_INDEX(itlbe->pa & itlbe->pa_mask);
	itlbe->next = mmu->itlbHash[index];
	mmu->itlbHash[index] = itlbe;
}

static void
dtlbe_remHash(MPC8xx_Mmu * mmu, TlbEntry * dtlbe)
{
	uint32_t index = TLB_HASH_INDEX(dtlbe->pa & dtlbe->pa_mask);
	TlbEntry *cursor = mmu->dtlbHash[index];
	if (cursor == dtlbe) {
		mmu->dtlbHash[index] = cursor->next;
		return;
	}
	while (cursor) {
		if (cursor->next == dtlbe) {
			cursor->next = dtlbe->next;
			return;
		}
		cursor = cursor->next;
	}

}

static inline void
dtlbe_addHash(MPC8xx_Mmu * mmu, TlbEntry * dtlbe)
{
	uint32_t index = TLB_HASH_INDEX(dtlbe->pa & dtlbe->pa_mask);
	dtlbe->next = mmu->dtlbHash[index];
	mmu->dtlbHash[index] = dtlbe;
}

static uint32_t
mi_ctr_read(int spr, void *cd)
{
	return 0;
}

static void
mi_ctr_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
md_ctr_read(int spr, void *cd)
{
	return 0;
}

static void
md_ctr_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
mi_epn_read(int spr, void *cd)
{
	return 0;
}

static void
mi_epn_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
md_epn_read(int spr, void *cd)
{
	return 0;
}

static void
md_epn_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
mi_twc_read(int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	return mmu->spr_mi_twc;
}

static void
mi_twc_write(uint32_t value, int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	mmu->spr_mi_twc = value;

}

static uint32_t
md_twc_read(int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	return mmu->spr_md_twc;
}

static void
md_twc_write(uint32_t value, int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	mmu->spr_mi_twc = value;
}

static uint32_t
mi_rpn_read(int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	TlbEntry *tlbe;
	int itlb_index = (mmu->spr_mi_ctr >> 8) & 0x1f;
	tlbe = &mmu->itlbe[itlb_index];
	return tlbe->pa;
}

static void
mi_rpn_write(uint32_t value, int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	TlbEntry *tlbe;
	int itlb_index = (mmu->spr_mi_ctr >> 8) & 0x1f;
	tlbe = &mmu->itlbe[itlb_index];
	itlbe_remHash(mmu, tlbe);
	tlbe->pa = value;
	tlbe->pa_mask = 0xfffff000;
	itlbe_addHash(mmu, tlbe);

}

static uint32_t
md_rpn_read(int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	TlbEntry *tlbe;
	int dtlb_index = (mmu->spr_md_ctr >> 8) & 0x1f;
	tlbe = &mmu->dtlbe[dtlb_index];
	return tlbe->pa;
}

static void
md_rpn_write(uint32_t value, int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	TlbEntry *tlbe;
	int dtlb_index = (mmu->spr_md_ctr >> 8) & 0x1f;
	tlbe = &mmu->dtlbe[dtlb_index];
	dtlbe_remHash(mmu, tlbe);
	tlbe->pa = value;
	tlbe->pa_mask = 0xfffff000;
	dtlbe_addHash(mmu, tlbe);
}

static uint32_t
m_twb_read(int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	if (mmu->spr_md_ctr & MD_CTR_TWAM) {
		return mmu->spr_m_twb | (mmu->spr_md_epn & (0x3ff << 22)) >> 20;
	} else {
		return mmu->spr_m_twb | (mmu->spr_md_epn & (0x3ff << 20)) >> 18;
	}
	return 0;
}

static void
m_twb_write(uint32_t value, int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	mmu->spr_m_twb = value & M_TWB_L1TB_MASK;
}

static uint32_t
m_casid_read(int spr, void *cd)
{
	//MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;
	return 0;
}

static void
m_casid_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
mi_ap_read(int spr, void *cd)
{
	return 0;
}

static void
mi_ap_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
md_ap_read(int spr, void *cd)
{
	return 0;
}

static void
md_ap_write(uint32_t value, int spr, void *cd)
{

}

/*
 ************************************************************
 *  TW
 *  Scratchpad register for the Software tablewalk routine
 ************************************************************
 */
static uint32_t
m_tw_read(int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;;
	return mmu->spr_m_tw;
}

static void
m_tw_write(uint32_t value, int spr, void *cd)
{
	MPC8xx_Mmu *mmu = (MPC8xx_Mmu *) cd;;
	mmu->spr_m_tw = value;

}

static uint32_t
mi_cam_read(int spr, void *cd)
{
	return 0;
}

static void
mi_cam_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
mi_ram0_read(int spr, void *cd)
{
	return 0;
}

static void
mi_ram0_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
mi_ram1_read(int spr, void *cd)
{
	return 0;
}

static void
mi_ram1_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
md_cam_read(int spr, void *cd)
{
	return 0;
}

static void
md_cam_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
md_ram0_read(int spr, void *cd)
{
	return 0;
}

static void
md_ram0_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
md_ram1_read(int spr, void *cd)
{
	return 0;
}

static void
md_ram1_write(uint32_t value, int spr, void *cd)
{

}

static uint32_t
translate_data(uint32_t va)
{
	if (!gppc.msr_dr) {
		return va;
	} else {
		fprintf(stderr, "Mist\n");
		exit(4234);
	}
}

static uint32_t
translate_ifetch(uint32_t va)
{
	if (!gppc.msr_ir) {
		return va;
	} else {
		fprintf(stderr, "Mist\n");
		exit(4237);
	}
}

/*
 * -------------------------------------------
 * Non-Static version of Page table walk
 * -------------------------------------------
 */
uint32_t
PPCMMU_translate_ifetch(uint32_t va)
{
	return translate_ifetch(va);
}

uint64_t
PPCMMU_Read64(uint32_t va)
{
	uint32_t pa = translate_data(va);
	return Bus_Read64(pa);

}

uint32_t
PPCMMU_Read32(uint32_t va)
{
	uint32_t pa = translate_data(va);
	return Bus_Read32(pa);

}

uint16_t
PPCMMU_Read16(uint32_t va)
{
	uint32_t pa = translate_data(va);
	return Bus_Read16(pa);
}

uint8_t
PPCMMU_Read8(uint32_t va)
{
	uint32_t pa = translate_data(va);
	return Bus_Read16(pa);
}

void
PPCMMU_Write64(uint32_t value, uint32_t va)
{
	uint32_t pa = translate_data(va);
	Bus_Write64(value, pa);
}

void
PPCMMU_Write32(uint32_t value, uint32_t va)
{
	uint32_t pa = translate_data(va);
	Bus_Write32(value, pa);
}

void
PPCMMU_Write16(uint16_t value, uint32_t va)
{
	uint32_t pa = translate_data(va);
	Bus_Write16(value, pa);
}

void
PPCMMU_Write8(uint8_t value, uint32_t va)
{
	uint32_t pa = translate_data(va);
	Bus_Write8(value, pa);
}

void
PPCMMU_InvalidateTlb()
{

}

void
MPC866_MMUNew(PpcCpu * cpu)
{
	Ppc_RegisterSprHandler(cpu, SPR_MI_CTR, mi_ctr_read, mi_ctr_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MD_CTR, md_ctr_read, md_ctr_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MI_EPN, mi_epn_read, mi_epn_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MD_EPN, md_epn_read, md_epn_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MI_TWC, mi_twc_read, mi_twc_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MD_TWC, md_twc_read, md_twc_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MI_RPN, mi_rpn_read, mi_rpn_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MD_RPN, md_rpn_read, md_rpn_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_M_TWB, m_twb_read, m_twb_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_M_CASID, m_casid_read, m_casid_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MI_AP, mi_ap_read, mi_ap_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MD_AP, md_ap_read, md_ap_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_M_TW, m_tw_read, m_tw_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MI_CAM, mi_cam_read, mi_cam_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MI_RAM0, mi_ram0_read, mi_ram0_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MI_RAM1, mi_ram1_read, mi_ram1_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MD_CAM, md_cam_read, md_cam_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MD_RAM0, md_ram0_read, md_ram0_write, NULL);
	Ppc_RegisterSprHandler(cpu, SPR_MD_RAM1, md_ram1_read, md_ram1_write, NULL);

}
