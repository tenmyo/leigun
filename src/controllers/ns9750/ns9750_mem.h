#ifndef NS9750_MEM_H
#define NS9750_MEM_H
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <compiler_extensions.h>
#include <bus.h>
#include <dram.h>
#include <signode.h>

#define NS9750_CS0 (0)
#define NS9750_CS1 (1)
#define NS9750_CS2 (2)
#define NS9750_CS3 (3)
#define NS9750_CS4 (4)
#define NS9750_CS5 (5)
#define NS9750_CS6 (6)
#define NS9750_CS7 (7)

#define MEM_WRITABLE 1
#define MEM_READABLE 2

#define SYS_MCTRL 	(0xa0700000)
#define 	MCTRL_MCEN (1)
#define 	MCTRL_ADDM (2)
#define 	MCTRL_LPM  (4)

#define SYS_MSTATUS  	(0xa0700004)
#define		MSTATUS_SA	(1<<2)
#define 	MSTATUS_WBS	(1<<1)	
#define 	MSTATUS_BUSY	(1<<0)

#define SYS_MCONFIG	(0xa0700008)
#define		MCONFIG_CLK	(1<<8)
#define		MCONFIG_END	(1<<0)

#define SYS_MDYNCTRL 	(0xa0700020)
#define 	MDYNCTRL_NRP 		(1<<14)
#define		MDYNCTRL_DP  		(1<<13)
#define 	MDYNCTRL_SDRAMINIT	(3<<7)
#define		MDYNCTRL_SR		(1<<2)
#define		MDYNCTRL_CE		(1<<0)

#define SYS_MRFRSH_TMR	(0xa0700024)
#define SYS_MREADCONFIG	(0xa0700028)
#define SYS_MPRECHRG_PERIOD  (0xa0700030)
#define SYS_MACTTOPRECHRG    (0xa0700034)
#define SYS_MSRFRSH_EXITTIME (0xa0700038)
#define SYS_MLDO_TO_ACT	(0xa070003c)
#define SYS_MDATA_TO_ACT	(0xa0700040)
#define SYS_MDYN_RECOVER	(0xa0700044)
#define SYS_MACT_TO_ACT	(0xa0700048)
#define SYS_MARFRSH_PERIOD	(0xa070004c)
#define SYS_MSRFRSH_EXIT     (0xa0700050)
#define SYS_MACTA_TO_ACTB	(0xa0700054)
#define SYS_MLMOD_TO_ACT_CMD (0xa0700058)
#define SYS_MSTAT_EXT_WAIT	(0xa0700080)
#define SYS_MDYN0_CONFIG	(0xa0700100)
#define SYS_MDYN1_CONFIG	(0xa0700120)
#define SYS_MDYN2_CONFIG	(0xa0700140)
#define SYS_MDYN3_CONFIG	(0xa0700160)
#define SYS_MDYN0_RASCAS	(0xa0700104)
#define SYS_MDYN1_RASCAS	(0xa0700124)
#define SYS_MDYN2_RASCAS	(0xa0700144)
#define SYS_MDYN3_RASCAS	(0xa0700164)
#define SYS_MSTT0_CONFIG	(0xa0700200)
#define SYS_MSTT1_CONFIG	(0xa0700220)
#define SYS_MSTT2_CONFIG	(0xa0700240)
#define SYS_MSTT3_CONFIG	(0xa0700260)
#define SYS_MSTT0_WWEN	(0xa0700204)
#define SYS_MSTT1_WWEN	(0xa0700224)
#define SYS_MSTT2_WWEN	(0xa0700244)
#define SYS_MSTT3_WWEN	(0xa0700264)
#define SYS_MSTT0_WWOEN	(0xa0700208)
#define SYS_MSTT1_WWOEN	(0xa0700228)
#define SYS_MSTT2_WWOEN	(0xa0700248)
#define SYS_MSTT3_WWOEN	(0xa0700268)
#define SYS_MSTT0_WTRD	(0xa070020C)
#define SYS_MSTT1_WTRD	(0xa070022C)
#define SYS_MSTT2_WTRD	(0xa070024C)
#define SYS_MSTT3_WTRD	(0xa070026C)
#define SYS_MSTT0_WTPG	(0xa0700210)
#define SYS_MSTT1_WTPG	(0xa0700230)
#define SYS_MSTT2_WTPG	(0xa0700250)
#define SYS_MSTT3_WTPG	(0xa0700270)
#define SYS_MSTT0_WTWR	(0xa0700214)
#define SYS_MSTT1_WTWR	(0xa0700234)
#define SYS_MSTT2_WTWR	(0xa0700254)
#define SYS_MSTT3_WTWR	(0xa0700274)
#define SYS_MSTT0_WTTN	(0xa0700218)
#define SYS_MSTT1_WTTN	(0xa0700238)
#define SYS_MSTT2_WTTN	(0xa0700258)
#define SYS_MSTT3_WTTN	(0xa0700278)

/* Only in ARM PL172 Docu */
#define MPMCPerihpID4	(0xa0700fd0)
#define MPMCPerihpID5	(0xa0700fd4)
#define MPMCPerihpID6	(0xa0700fd8)
#define MPMCPerihpID7	(0xa0700fdc)
#define MPMCPerihpID0	(0xa0700fe0)
#define MPMCPerihpID1	(0xa0700fe4)
#define MPMCPerihpID2	(0xa0700fe8)
#define MPMCPerihpID3	(0xa0700fec)
#define MPMCPCellID0	(0xa0700ff0)
#define MPMCPCellID1	(0xa0700ff4)
#define MPMCPCellID2	(0xa0700ff8)
#define MPMCPCellID3	(0xa0700ffc)

#define SYS_MCS4B		(0xa09001d0)
#define SYS_MCS4M		(0xa09001d4)
#define SYS_MCS5B		(0xa09001d8)
#define SYS_MCS5M		(0xa09001dc)
#define SYS_MCS6B		(0xa09001e0)
#define SYS_MCS6M		(0xa09001e4)
#define SYS_MCS7B		(0xa09001e8)
#define SYS_MCS7M		(0xa09001ec)

#define SYS_MCS0B		(0xa09001f0)
#define SYS_MCS0M		(0xa09001f4)
#define SYS_MCS1B		(0xa09001f8)
#define SYS_MCS1M		(0xa09001fc)
#define SYS_MCS2B		(0xa0900200)
#define SYS_MCS2M		(0xa0900204)
#define SYS_MCS3B		(0xa0900208)
#define SYS_MCS3M		(0xa090020c)


typedef struct NS9750_MemController {
	BusDevice *bdev[8];

	/* Registers  of memco */
	uint32_t mctrl;
	uint32_t mstatus;
	uint32_t mconfig;
	uint32_t mdynctrl;
	uint32_t mrfrsh_tmr;
	uint32_t mreadconfig;
	uint32_t mprechrg_period;
	uint32_t macttoprechrg;
	uint32_t msrfrsh_exittime;
	uint32_t mldo_to_act;
	uint32_t mdata_to_act;
	uint32_t mdyn_recover;
	uint32_t mact_to_act;
	uint32_t marfrsh_period;
	uint32_t msrfrsh_exit; 
	uint32_t macta_to_actb;
	uint32_t mlmod_to_act_cmd;
	uint32_t mstat_ext_wait;
	uint32_t mdyn0_config;
	uint32_t mdyn1_config;
	uint32_t mdyn2_config;
	uint32_t mdyn3_config;
	uint32_t mdyn0_rascas;
	uint32_t mdyn1_rascas;
	uint32_t mdyn2_rascas;
	uint32_t mdyn3_rascas;
	uint32_t mstt0_config;
	uint32_t mstt1_config;
	uint32_t mstt2_config;
	uint32_t mstt3_config;
	uint32_t mstt0_wwen;
	uint32_t mstt1_wwen;
	uint32_t mstt2_wwen;
	uint32_t mstt3_wwen;
	uint32_t mstt0_wwoen;
	uint32_t mstt1_wwoen;
	uint32_t mstt2_wwoen;
	uint32_t mstt3_wwoen;
	uint32_t mstt0_wtrd;
	uint32_t mstt1_wtrd;
	uint32_t mstt2_wtrd;
	uint32_t mstt3_wtrd;
	uint32_t mstt0_wtpg;
	uint32_t mstt1_wtpg;
	uint32_t mstt2_wtpg;
	uint32_t mstt3_wtpg;
	uint32_t mstt0_wtwr;
	uint32_t mstt1_wtwr;
	uint32_t mstt2_wtwr;
	uint32_t mstt3_wtwr;
	uint32_t mstt0_wttn;
	uint32_t mstt1_wttn;
	uint32_t mstt2_wttn;
	uint32_t mstt3_wttn;
	uint32_t cs_base[8];
	uint32_t cs_mask[8];

	SigNode *big_endianNode;
} NS9750_MemController;


NS9750_MemController *NS9750_MemCoInit(const char *name);
int NS9750_LoadFlash(NS9750_MemController *mcontr,char *filename);
void NS9750_RegisterDevice(NS9750_MemController *,BusDevice *marea,uint32_t cs);
#endif
