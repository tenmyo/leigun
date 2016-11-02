#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include "arm9cpu.h"
#include "cycletimer.h"
//#include "mmu.h"
#include "bus.h"
#include "compiler_extensions.h"
#include "fio.h"
#include "mainloop_events.h"
#include "sgstring.h"

#define CR(crn,op1,crm,op2)	(((crn) << 12) | ((op1) << 8) | ((crm) << 4) | (op2))
#define REG_MAIN_ID		CR(0,0,0,0)
#define REG_CACHE_TYPE		CR(0,0,0,1)
#define REG_TCM_STATUS		CR(0,0,0,2)
#define REG_TLB_TYPE		CR(0,0,0,3)
#define REG_PROCFEAT0		CR(0,0,1,0)
#define REG_PROCFEAT1		CR(0,0,1,1)
#define REG_DEBUGFEAT0		CR(0,0,1,2)
#define REG_AUXFEAT0		CR(0,0,1,3)
#define REG_MEMMODFEAT0		CR(0,0,1,4)
#define REG_MEMMODFEAT1		CR(0,0,1,5)
#define REG_MEMMODFEAT2		CR(0,0,1,6)
#define REG_MEMMODFEAT3		CR(0,0,1,7)
#define REG_INSTRSETFEAT0	CR(0,0,2,0)
#define	REG_INSTRSETFEAT1	CR(0,0,2,1)
#define	REG_INSTRSETFEAT2	CR(0,0,2,2)
#define	REG_INSTRSETFEAT3	CR(0,0,2,3)
#define	REG_INSTRSETFEAT4	CR(0,0,2,4)
#define	REG_INSTRSETFEAT5	CR(0,0,2,5)
#define REG_CONTROL		CR(1,0,0,0)
#define REG_AUXCTRL		CR(1,0,0,1)
#define REG_COPROACCCTRL	CR(1,0,0,2)
#define REG_SECURECONF		CR(1,0,1,0)
#define REG_SECUREDEBUG		CR(1,0,1,1)
#define REG_NONSECACCCTRL	CR(1,0,1,2)
#define REG_TRANSTBLBASE0	CR(2,0,0,0)
#define REG_TRANSTBLBASE1	CR(2,0,0,1)
#define REG_TRANSTBLBASECTRL	CR(2,0,0,2)
#define REG_DOMACCCTRL		CR(3,0,0,0)
#define REG_DATAFAULTSTAT	CR(5,0,0,0)
#define REG_INSTRFAULTSTAT	CR(5,0,0,1)
#define REG_FAULTADDR		CR(6,0,0,0)
#define REG_WATCHPFAULTADDR	CR(6,0,0,1)
#define REG_INSTRFAULTADDR	CR(6,0,0,2)
#define REG_WAITFORINT		CR(7,0,0,4)
#define REG_PA			CR(7,0,4,0)
#define REG_INVINSTCACHE	CR(7,0,5,0)
#define REG_INVINSTCACHELNMVA	CR(7,0,5,1)
#define REG_INVINSTCACHELNIDX	CR(7,0,5,2)
#define REG_FLUSHPREFETCH	CR(7,0,5,4)
#define REG_FLUSHBRATARG	CR(7,0,5,6)
#define REG_FLUSHBRATARGMVA	CR(7,0,5,7)
#define REG_INVDATACACHE	CR(7,0,6,0)
#define REG_INVDATACACHLNEMVA	CR(7,0,6,1)
#define REG_INVDATACACHLNEIDX	CR(7,0,6,2)
#define REG_INVCACHES		CR(7,0,7,0)
#define REG_VATOPACURR0		CR(7,0,8,0)
#define REG_VATOPACURR1		CR(7,0,8,1)
#define REG_VATOPACURR2		CR(7,0,8,2)
#define REG_VATOPACURR3		CR(7,0,8,3)
#define REG_VATOPAOTHER0	CR(7,0,8,4)
#define REG_VATOPAOTHER1	CR(7,0,8,5)
#define REG_VATOPAOTHER2	CR(7,0,8,6)
#define REG_VATOPAOTHER3	CR(7,0,8,7)
#define REG_CLEANDATACACHE	CR(7,0,10,0)
#define REG_CLEANDATACACHELNMVA CR(7,0,10,1)
#define REG_CLEANDATACACHELNIDX CR(7,0,10,2)
#define REG_DATASYNCBARRIER	CR(7,0,10,4)
#define REG_DATAMEMBARRIER	CR(7,0,10,5)
#define REG_CACHEDIRTYSTAT	CR(7,0,10,6)
#define REG_PREFETCHICL		CR(7,0,13,1)
#define REG_CLINVDATACACHE	CR(7,0,14,0)
#define REG_CLINVDATACACHEMVA	CR(7,0,14,1)
#define REG_CLINVDATACACHEIDX	CR(7,0,14,2)
#define REG_INVITLBUNLCKD	CR(8,0,5,0)
#define REG_INVITBLMVA		CR(8,0,5,1)
#define REG_INVITBLASID		CR(8,0,5,2)
#define REG_INVDTLBUNLCKD	CR(8,0,6,0)
#define REG_INVDTBLMVA		CR(8,0,6,1)
#define REG_INVDTBLASID		CR(8,0,6,2)
#define REG_INVUTLBUNLCKD	CR(8,0,7,0)
#define REG_INVUTBLMVA		CR(8,0,7,1)
#define REG_INVUTBLASID		CR(8,0,7,2)
#define REG_DCACHELKDWN		CR(9,0,0,0)
#define REG_ICACHELKDWN		CR(9,0,0,1)
#define REG_DTCM		CR(9,0,1,0)
#define REG_ITCM		CR(9,0,1,1)
#define REG_DTCMNONSEC		CR(9,0,1,2)
#define REG_ITCMNONSEC		CR(9,0,1,3)
#define REG_TCMSEL		CR(9,0,2,0)
#define REG_CACHEBEHAV		CR(9,0,8,0)
#define REG_TLBLKDWN		CR(10,0,0,0)
#define REG_PRIMEMREMAP		CR(10,0,2,0)
#define REG_NORMMEMREMAP	CR(10,0,2,1)
#define REG_DMAIDENTSTAT0	CR(11,0,0,0)
#define REG_DMAIDENTSTAT1	CR(11,0,0,1)
#define REG_DMAIDENTSTAT2	CR(11,0,0,2)
#define REG_DMAIDENTSTAT3	CR(11,0,0,3)
#define REG_DMAUACC		CR(11,0,1,0)
#define REG_DMA_CHANNUM		CR(11,0,2,0)
#define REG_DMAEN0		CR(11,0,3,0)
#define REG_DMAEN1		CR(11,0,3,1)
#define REG_DMAEN2		CR(11,0,3,2)
#define REG_DMACTRL		CR(11,0,4,0)
#define REG_DMAINTSTARTADDR	CR(11,0,5,0)
#define REG_DMAEXTSTARTADDR	CR(11,0,6,0)
#define REG_DMAINTENDADDR	CR(11,0,7,0)
#define REG_DMACHANNSTAT	CR(11,0,8,0)
#define REG_DMACTXTID		CR(11,0,15,0)
#define REG_SECNONSECVECBASE	CR(12,0,0,0)
#define REG_MONVECBASE		CR(12,0,0,1)
#define REG_INTSTATUS		CR(12,0,1,0)
#define REG_FCSEPID		CR(13,0,0,0)
#define REG_CONTEXTID		CR(13,0,0,1)
#define REG_USRRWTPID		CR(13,0,0,2)
#define REG_USRRDTPID		CR(13,0,0,3)
#define REG_USRPRIVTPID		CR(13,0,0,4)
#define REG_PERPORTMEMREMAP	CR(15,0,2,4)
#define REG_ACCVALCTRL		CR(15,0,9,0)
#define REG_PERFMONCTRL		CR(15,0,12,0)
#define REG_CYCLECTR		CR(15,0,12,1)
#define REG_COUNT0		CR(15,0,12,2)
#define REG_COUNT1		CR(15,0,12,3)
#define REG_SYSVALOPS0		CR(15,0,13,1)
#define REG_SYSVALOPS1		CR(15,0,13,2)
#define REG_SYSVALOPS2		CR(15,0,13,3)
#define REG_SYSVALOPS3		CR(15,0,13,4)
#define REG_SYSVALOPS4		CR(15,0,13,5)
#define REG_SYSVALOPS5		CR(15,0,13,6)
#define REG_SYSVALOPS6		CR(15,0,13,7)
#define REG_SYSVALCSMSK		CR(15,0,14,0)
#define REG_INSTCACHEMSTRVALID	CR(15,3,8,0)
#define REG_DATACACHEMASTERVAL	CR(15,3,12,0)
#define REG_TLBLKDWNIDX		CR(15,5,4,2) 
#define REG_TLBLKDWNVA		CR(15,5,5,2)
#define REG_TLBLKDWNPA		CR(15,5,6,2)
#define REG_TLBLKDWNATTR	CR(15,5,7,2)

#define HASH_SIZE	256
#define HASH_VALUE(w) ((w) + ((w) >> 8)) & 255; 

typedef void   McrProc(void *clientData,uint32_t icode,uint32_t value);
typedef uint32_t MrcProc(void *clientData,uint32_t icode);

typedef struct RegisterOps {
	struct RegisterOps *next;
	McrProc *mcrProc;
	MrcProc *mrcProc;
	void *clientData;
	uint16_t regIdx;
} RegisterOps;

typedef struct SysCopro {
	ArmCoprocessor copro; /* Inherits from ArmCoprocessor */
	/* The registers */
	RegisterOps *regHash[HASH_SIZE];
} SysCopro;

//static SysCopro g_mmu;

static void   
main_id_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
main_id_read(void *clientData,uint32_t icode)
{

	return 0;
}
static void   
cache_type_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
cache_type_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
tcm_status_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
tcm_status_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
tlb_type_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
tlb_type_read(void *clientData,uint32_t icode)
{
	return 0;
}
static void   
procfeat0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
procfeat0_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
procfeat1_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
procfeat1_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
debugfeat0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
debugfeat0_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
auxfeat0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
auxfeat0_read(void *clientData,uint32_t icode)
{
	return 0;
}
#if 0
static void   
memmodfeat0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
memmodfeat0_read(void *clientData,uint32_t icode)
{
	return 0;
}
static void   
memmodfeat1_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
memmodfeat1_read(void *clientData,uint32_t icode)
{
	return 0;
}
static void   
memmodfeat2_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
memmodfeat2_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
memmodfeat3_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
memmodfeat3_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
instrsetfeat0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
instrsetfeat0_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
instrsetfeat1_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
instrsetfeat1_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
instrsetfeat2_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
instrsetfeat2_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
instrsetfeat3_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
instrsetfeat3_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
instrsetfeat4_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
instrsetfeat4_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
instrsetfeat5_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
instrsetfeat5_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
control_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
control_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
auxctrl_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
auxctrl_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
coproaccctrl_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
coproaccctrl_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
secureconf_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
secureconf_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
securedebug_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
securedebug_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
nonsecaccctrl_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
nonsecaccctrl_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
transtblbase0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
transtblbase0_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
transtblbase1_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
transtblbase1_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
domaccctrl_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
domaccctrl_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
datafaultstat_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
datafaultstat_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
instrfaultstat_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
instrfaultstat_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
faultaddr_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
faultaddr_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
watchpfaultaddr_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
watchpfaultaddr_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
instrfaultaddr_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
instrfaultaddr_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
waitforint_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
waitforint_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
pa_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
pa_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
invinstcache_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invinstcache_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
invinstcachelnmva_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invinstcachelnmva_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
invinstcachelnidx_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invinstcachelnidx_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
flushprefetch_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
flushprefetch_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
flushbratarg_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
flushbratarg_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
flushbratargvma_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
flushbratargvma_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
invdatacache_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invdatacache_read(void *clientData,uint32_t icode)
{
	return 0;
}

static void   
invdatacachelnemva_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invdatacachelnemva_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invdatacachelneidx_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invdatacachelneidx_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invcaches_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invcaches_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
vatopacurr0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
vatopacurr0_read(void *clientData,uint32_t icode)
{

	return 0;
}


static uint32_t 
vatopacurr1_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
vatopacurr1_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
vatopacurr2_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
vatopacurr2_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
vatopacurr3_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
vatopacurr3_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
vatopaother0_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
vatopaother0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
vatopaother1_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
vatopaother1_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
vatopaother2_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
vatopaother2_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
vatopaother3_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
vatopaother3_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
cleandatacache_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
cleandatacache_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
cleandatacachelnmva_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
cleandatacachelnmva_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
cleandatacachelnidx_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
cleandatacachelnidx_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
datasyncbarrier_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
datasyncbarrier_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
datamembarrier_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
datamembarrier_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
cachedirtystat_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
cachedirtystat_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
prefetchicl_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
prefetchicl_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
clinvdatacache_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
clinvdatacache_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
clinvdatacachemva_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
clinvdatacachemva_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
clinvdatacacheidx_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
clinvdatacacheidx_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invitlbunlckd_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invitlbunlckd_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invitlbmva_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invitlbmva_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invitlbasid_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invitlbasid_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invdtlbunlckd_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invdtlbunlckd_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invdtlbmva_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invdtlbmva_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invdtlbasid_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invdtlbasid_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invutlbunlckd_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invutlbunlckd_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invutlbmva_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invutlbmva_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
invutlbasid_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
invutlbasid_write(void *clientData,uint32_t icode,uint32_t value)
{

}


static uint32_t 
dcachelkdwn_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dcachelkdwn_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
icachelkdwn_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
icachelkdwn_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dtcm_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dtcm_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
itcm_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
itcm_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dtcmnonsec_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dtcmnonsec_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
itcmnonsec_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
itcmnonsec_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
tcmsel_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
tcmsel_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
cachebehav_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
cachebehav_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
tlblkdwn_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
tlblkdwn_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
primemremap_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
primemremap_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
normmemremap_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
mormmemremap_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmaidentstat0_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmaidentstat0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmaidentstat1_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmaidentstat1_write(void *clientData,uint32_t icode,uint32_t value)
{

}
static uint32_t 
dmaidentstat2_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmaidentstat2_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmaidentstat3_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmaidentstat3_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmauacc_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmauacc_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dma_channum_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dma_channum_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmaen0_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmaen0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmaen1_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmaen1_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmaen2_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmaen2_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmactrl_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmactrl_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmaintstartaddr_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmaintstartaddr_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmaextstartaddr_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmaextstartaddr_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmaintendaddr_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmaintendaddr_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
dmachannstat_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmachannstat_write(void *clientData,uint32_t icode,uint32_t value)
{

}


static uint32_t 
dmactxtid_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
dmactxtid_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
secnonsecvecbase_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
secnonsecvecbase_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
monvecbase_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
monvecbase_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
intstatus_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
intstatus_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
fcsepid_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
fcsepid_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
contextid_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
contextid_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
usrrwtpid_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
usrrwtpid_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
usrrdtpid_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
usrrdtpid_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
usrprivtpid_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
usrprivtpid_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
perportmemremap_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
perportmemremap_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
accvalctrl_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
accvalctrl_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
perfmonctrl_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
perfmonctrl_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
cyclectr_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
cyclectr_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
count0_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
count0_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
count1_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
count1_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
sysvalops0_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
sysvalops0_write(void *clientData,uint32_t icode,uint32_t value)
{

}
static uint32_t 
sysvalops1_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
sysvalops1_write(void *clientData,uint32_t icode,uint32_t value)
{

}
static uint32_t 
sysvalops2_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
sysvalops2_write(void *clientData,uint32_t icode,uint32_t value)
{

}
static uint32_t 
sysvalops3_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
sysvalops3_write(void *clientData,uint32_t icode,uint32_t value)
{

}
static uint32_t 
sysvalops4_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
sysvalops4_write(void *clientData,uint32_t icode,uint32_t value)
{

}
static uint32_t 
sysvalops5_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
sysvalops5_write(void *clientData,uint32_t icode,uint32_t value)
{

}
static uint32_t 
sysvalops6_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
sysvalops6_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
sysvalcsmsk_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
sysvalcsmsk_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
instcachemstrvalid_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
instcachemstrvalid_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
datacachemasterval_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
datacachemasterval_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
tlblkdwnva_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
tlblkdwnva_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
tlblkdwnpa_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
tlblkdwnpa_write(void *clientData,uint32_t icode,uint32_t value)
{

}

static uint32_t 
tlblkdwnattr_read(void *clientData,uint32_t icode)
{

	return 0;
}

static void   
tlblkdwnattr_write(void *clientData,uint32_t icode,uint32_t value)
{

}

#endif
static RegisterOps * 
FindOps(SysCopro *cp,uint16_t regIdx)
{
	int hash = HASH_VALUE(regIdx);
	RegisterOps *cursor;
	for(cursor = cp->regHash[hash]; cursor ; cursor = cursor->next) {	
		if(cursor->regIdx == regIdx) {
			return cursor;
		}
	}
	return NULL;
}
static void
AddOps(SysCopro *cp,uint16_t regIdx,MrcProc *mrc,McrProc *mcr)
{
	RegisterOps *rops = sg_new(RegisterOps);
	int hash = HASH_VALUE(regIdx);
	rops->next = cp->regHash[hash];	
	rops->regIdx = regIdx;
	cp->regHash[hash] = rops;
}

static uint32_t 
MMUmrc(ArmCoprocessor *copro,uint32_t icode) 
{
	RegisterOps *rops;
	SysCopro *cp = copro->owner;
	uint32_t retval = 0;
	uint32_t crn = (icode >> 16) & 0xf;
	uint32_t crm = icode & 0xf;
	uint32_t op1 = (icode >> 21) & 7;
	uint32_t op2 = (icode >> 5) & 7;
	uint16_t regIdx = CR(crn,op1,crm,op2);
	rops = FindOps(cp,regIdx);
	if(rops && rops->mrcProc) {
		retval = rops->mrcProc(cp,icode);	
	}
	return retval;
}

static void
MMUmcr(ArmCoprocessor *copro,uint32_t icode,uint32_t data)
{
	SysCopro *cp = copro->owner;
	RegisterOps *rops;
	uint32_t crn = (icode >> 16) & 0xf;
	uint32_t crm = icode & 0xf;
	uint32_t op1 = (icode >> 21) & 7;
	uint32_t op2 = (icode >> 5) & 7;
	uint16_t regIdx = CR(crn,op1,crm,op2);
	rops = FindOps(cp,regIdx);
	if(rops && rops->mcrProc) {
		rops->mcrProc(cp,icode,data);	
	}
	return;
}

ArmCoprocessor *
MMU1176_Create(const char *name,int endian,uint32_t mmu_variant)
{
	SysCopro *cp = sg_new(SysCopro);
	cp->copro.mrc = MMUmrc;
	cp->copro.mcr = MMUmcr;
	cp->copro.owner = cp;
	AddOps(cp,REG_MAIN_ID,main_id_read,main_id_write);
	AddOps(cp,REG_CACHE_TYPE,cache_type_read,cache_type_write);
	AddOps(cp,REG_TCM_STATUS,tcm_status_read,tcm_status_write);
	AddOps(cp,REG_TLB_TYPE,tlb_type_read,tlb_type_write);
	AddOps(cp,REG_PROCFEAT0,procfeat0_read,procfeat0_write);
	AddOps(cp,REG_PROCFEAT1,procfeat1_read,procfeat1_write);
	AddOps(cp,REG_DEBUGFEAT0,debugfeat0_read,debugfeat0_write);
	AddOps(cp,REG_AUXFEAT0,auxfeat0_read,auxfeat0_write);
#if 0
	AddOps(cp,REG_MEMMODFEAT0
	AddOps(cp,REG_MEMMODFEAT1
	AddOps(cp,REG_MEMMODFEAT2
	AddOps(cp,REG_MEMMODFEAT3
	AddOps(cp,REG_INSTRSETFEAT0
	AddOps(cp,REG_INSTRSETFEAT1
	AddOps(cp,REG_INSTRSETFEAT2
	AddOps(cp,REG_INSTRSETFEAT3
	AddOps(cp,REG_INSTRSETFEAT4
	AddOps(cp,REG_INSTRSETFEAT5
	AddOps(cp,REG_CONTROL
	AddOps(cp,REG_AUXCTRL
	AddOps(cp,REG_COPROACCCTRL
	AddOps(cp,REG_SECURECONF
	AddOps(cp,REG_SECUREDEBUG
	AddOps(cp,REG_NONSECACCCTRL
	AddOps(cp,REG_TRANSTBLBASE0
	AddOps(cp,REG_TRANSTBLBASE1
	AddOps(cp,REG_TRANSTBLBASECTRL
	AddOps(cp,REG_DOMACCCTRL
	AddOps(cp,REG_DATAFAULTSTAT
	AddOps(cp,REG_INSTRFAULTSTAT
	AddOps(cp,REG_FAULTADDR
	AddOps(cp,REG_WATCHPFAULTADDR
	AddOps(cp,REG_INSTRFAULTADDR
	AddOps(cp,REG_WAITFORINT
	AddOps(cp,REG_PA
	AddOps(cp,REG_INVINSTCACHE
	AddOps(cp,REG_INVINSTCACHELNMVA
	AddOps(cp,REG_INVINSTCACHELNIDX
	AddOps(cp,REG_FLUSHPREFETCH
	AddOps(cp,REG_FLUSHBRATARG
	AddOps(cp,REG_FLUSHBRATARGMVA
	AddOps(cp,REG_INVDATACACHE
	AddOps(cp,REG_INVDATACACHLNEMVA
	AddOps(cp,REG_INVDATACACHLNEIDX
	AddOps(cp,REG_INVCACHES	
	AddOps(cp,REG_VATOPACURR0
	AddOps(cp,REG_VATOPACURR1
	AddOps(cp,REG_VATOPACURR2
	AddOps(cp,REG_VATOPACURR3
	AddOps(cp,REG_VATOPAOTHER0
	AddOps(cp,REG_VATOPAOTHER1
	AddOps(cp,REG_VATOPAOTHER2
	AddOps(cp,REG_VATOPAOTHER3
	AddOps(cp,REG_CLEANDATACACHE
	AddOps(cp,REG_CLEANDATACACHELNMVA
	AddOps(cp,REG_CLEANDATACACHELNIDX
	AddOps(cp,REG_DATASYNCBARRIER
	AddOps(cp,REG_DATAMEMBARRIER
	AddOps(cp,REG_CACHEDIRTYSTAT
	AddOps(cp,REG_PREFETCHICL
	AddOps(cp,REG_CLINVDATACACHE
	AddOps(cp,REG_CLINVDATACACHEMVA
	AddOps(cp,REG_CLINVDATACACHEIDX
	AddOps(cp,REG_INVITLBUNLCKD
	AddOps(cp,REG_INVITBLMVA
	AddOps(cp,REG_INVITBLASID
	AddOps(cp,REG_INVDTLBUNLCKD
	AddOps(cp,REG_INVDTBLMVA
	AddOps(cp,REG_INVDTBLASID
	AddOps(cp,REG_INVUTLBUNLCKD
	AddOps(cp,REG_INVUTBLMVA
	AddOps(cp,REG_INVUTBLASID
	AddOps(cp,REG_DCACHELKDWN
	AddOps(cp,REG_ICACHELKDWN
	AddOps(cp,REG_DTCM
	AddOps(cp,REG_ITCM
	AddOps(cp,REG_DTCMNONSEC
	AddOps(cp,REG_ITCMNONSEC
	AddOps(cp,REG_TCMSEL
	AddOps(cp,REG_CACHEBEHAV
	AddOps(cp,REG_TLBLKDWN	
	AddOps(cp,REG_PRIMEMREMAP
	AddOps(cp,REG_NORMMEMREMAP
	AddOps(cp,REG_DMAIDENTSTAT0
	AddOps(cp,REG_DMAIDENTSTAT1
	AddOps(cp,REG_DMAIDENTSTAT2
	AddOps(cp,REG_DMAIDENTSTAT3
	AddOps(cp,REG_DMAUACC
	AddOps(cp,REG_DMA_CHANNUM
	AddOps(cp,REG_DMAEN0
	AddOps(cp,REG_DMAEN1
	AddOps(cp,REG_DMAEN2
	AddOps(cp,REG_DMACTRL
	AddOps(cp,REG_DMAINTSTARTADDR
	AddOps(cp,REG_DMAEXTSTARTADDR
	AddOps(cp,REG_DMAINTENDADDR
	AddOps(cp,REG_DMACHANNSTAT
	AddOps(cp,REG_DMACTXTID	
	AddOps(cp,REG_SECNONSECVECBASE
	AddOps(cp,REG_MONVECBASE
	AddOps(cp,REG_INTSTATUS	
	AddOps(cp,REG_FCSEPID
	AddOps(cp,REG_CONTEXTID
	AddOps(cp,REG_USRRWTPID
	AddOps(cp,REG_USRRDTPID	
	AddOps(cp,REG_USRPRIVTPID
	AddOps(cp,REG_PERPORTMEMREMAP
	AddOps(cp,REG_ACCVALCTRL
	AddOps(cp,REG_PERFMONCTRL
	AddOps(cp,REG_CYCLECTR
	AddOps(cp,REG_COUNT0
	AddOps(cp,REG_COUNT1
	AddOps(cp,REG_SYSVALOPS0
	AddOps(cp,REG_SYSVALOPS1
	AddOps(cp,REG_SYSVALOPS2
	AddOps(cp,REG_SYSVALOPS3
	AddOps(cp,REG_SYSVALOPS4
	AddOps(cp,REG_SYSVALOPS5
	AddOps(cp,REG_SYSVALOPS6
	AddOps(cp,REG_SYSVALCSMSK
	AddOps(cp,REG_INSTCACHEMSTRVALID
	AddOps(cp,REG_DATACACHEMASTERVAL
	AddOps(cp,REG_TLBLKDWNIDX
	AddOps(cp,REG_TLBLKDWNVA
	AddOps(cp,REG_TLBLKDWNPA
	AddOps(cp,REG_TLBLKDWNATTR
#endif
	return &cp->copro;
}
