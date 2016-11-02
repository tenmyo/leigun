#include <stdint.h>
#include <setjmp.h>
#include <sys/time.h>
#include <bus.h>
#include <cycletimer.h>
#include <clock.h>

#define ISNEG(x) ((x)&(1<<31))
#define ISNOTNEG(x) (!((x)&(1<<31)))
#define ISPOS(x) (-(x) & (1<<31)

#define CPU_MPC852T 	(1)
#define CPU_MPC866P 	(2)
#define CPU_MPC8260 	(3)

typedef uint32_t SPR_ReadProc(int spr, void *cd);
typedef void SPR_WriteProc(uint32_t value, int spr, void *cd);

typedef struct PpcCpu {
	Clock_t *cpuclk;
	uint32_t msr;
	uint32_t msr_ee;	// external interrupts enable
	uint32_t msr_pr;	// Privilege level
	uint32_t msr_me;	// Machine Check Exceptions enable
	uint32_t msr_fe;	// floating point exception mode
	uint32_t msr_ip;	// exception prefix     
	uint32_t msr_ir;	// translate instruction
	uint32_t msr_dr;	// translate data

	uint32_t pvr;
	uint32_t pir;
	uint32_t ibat0u;
	uint32_t ibat0l;
	uint32_t ibat1u;
	uint32_t ibat1l;
	uint32_t ibat2u;
	uint32_t ibat2l;
	uint32_t ibat3u;
	uint32_t ibat3l;

	uint32_t dbat0u;
	uint32_t dbat0l;
	uint32_t dbat1u;
	uint32_t dbat1l;
	uint32_t dbat2u;
	uint32_t dbat2l;
	uint32_t dbat3u;
	uint32_t dbat3l;

	uint32_t sdr1;
	uint32_t asr;
	uint32_t sr[16];
	uint32_t dar;
	uint32_t sprg0;
	uint32_t sprg1;
	uint32_t sprg2;
	uint32_t sprg3;
	uint32_t dsisr;
	uint32_t srr0;
	uint32_t srr1;
	Clock_t *tmbclk;
	CycleCounter_t last_tb_update;
	CycleCounter_t tb_saved_cycles;
	uint32_t tbl;
	uint32_t tbu;
	uint32_t dec;
	uint32_t dabr;
	uint32_t ear;
	uint32_t cr;
	uint32_t fpecr;
	uint32_t xer;
	uint32_t lr;
	uint32_t ctr;

	uint32_t *spr[1024];
	SPR_ReadProc *spr_read[1024];
	SPR_WriteProc *spr_write[1024];
	void *spr_clientData[1024];

	uint32_t gpr[0];
	uint32_t gpr0;
	uint32_t gpr1;
	uint32_t gpr2;
	uint32_t gpr3;
	uint32_t gpr4;
	uint32_t gpr5;
	uint32_t gpr6;
	uint32_t gpr7;
	uint32_t gpr8;
	uint32_t gpr9;
	uint32_t gpr10;
	uint32_t gpr11;
	uint32_t gpr12;
	uint32_t gpr13;
	uint32_t gpr14;
	uint32_t gpr15;
	uint32_t gpr16;
	uint32_t gpr17;
	uint32_t gpr18;
	uint32_t gpr19;
	uint32_t gpr20;
	uint32_t gpr21;
	uint32_t gpr22;
	uint32_t gpr23;
	uint32_t gpr24;
	uint32_t gpr25;
	uint32_t gpr26;
	uint32_t gpr27;
	uint32_t gpr28;
	uint32_t gpr29;
	uint32_t gpr30;
	uint32_t gpr31;
	uint64_t fpr[32];
	uint32_t fpscr;

	uint32_t nia;
	uint32_t reservation;
	int reservation_valid;
	struct timeval starttime;

	uint32_t signals;
	jmp_buf abort_jump;
} PpcCpu;

extern PpcCpu gppc;

#define NIA (gppc.nia)
#define CIA (gppc.nia-4)	/* This is a hack */
#define LR (gppc.lr)
#define GPR(index) (gppc.gpr[(index)])
#define FPR(index) (gppc.fpr[(index)])
#define PVR (gppc.pvr)
#define CTR (gppc.ctr)
#define FPSCR (gppc.fpscr)
#define SPR(x) (*gppc.spr[(x)])
#define HAS_SPR(x) (gppc.spr[(x)])
#define HAS_SPR_READ(x) (gppc.spr_read[(x)])
#define HAS_SPR_WRITE(x) (gppc.spr_write[(x)])
#define SPR_READ(x) (gppc.spr_read[(x)]((x),gppc.spr_clientData[(x)]))
#define SPR_WRITE(val,x) (gppc.spr_write[(x)]((val),(x),gppc.spr_clientData[(x)]))
#define SR(x) (gppc.sr[(x)])
#define TBL	(gppc.tbl)
#define TBU	(gppc.tbu)
#define SRR0	(gppc.srr0)
#define SRR1	(gppc.srr1)
#define EAR	(gppc.ear)

/* Condition Register fields p.60 */
#define CR (gppc.cr)

#define CR_LT	(1<<31)
#define CR_GT	(1<<30)
#define CR_EQ	(1<<29)
#define CR_SO	(1<<28)

#define CR_OX	(1<<24)
#define CR_VX	(1<<25)
#define CR_FEX  (1<<26)
#define CR_FX	(1<<27)

#define FPSCR_RN_MSK 	(3<<0)
#define FPSCR_NI 	(1<<2)
#define FPSCR_XE 	(1<<3)
#define FPSCR_ZE 	(1<<4)
#define FPSCR_UE 	(1<<5)
#define FPSCR_OE 	(1<<6)
#define FPSCR_VE 	(1<<7)
#define FPSCR_VXCVI	(1<<8)
#define FPSCR_VXSQRT	(1<<9)
#define FPSCR_VXSOFT	(1<<10)
#define FPSCR_FPRF_MSK	(0xf<<12)
#define FPSCR_FI	(1<<17)
#define FPSCR_FR	(1<<18)
#define FPSCR_VXVC	(1<<19)
#define FPSCR_VXIMZ	(1<<20)
#define FPSCR_VXZDZ	(1<<21)
#define FPSCR_VXIDI	(1<<22)
#define FPSCR_VXISI	(1<<23)
#define FPSCR_VXSNAN	(1<<24)
#define FPSCR_XX	(1<<25)
#define FPSCR_ZX	(1<<26)
#define FPSCR_UX	(1<<27)
#define FPSCR_OX	(1<<28)
#define FPSCR_VX	(1<<29)
#define FPSCR_FEX	(1<<30)
#define FPSCR_FX	(1<<31)

/* Machine Status Register p.74 */
#define MSR (gppc.msr)

#define MSR_LE	(1<<0)
#define MSR_RI	(1<<1)
#define MSR_DR	(1<<4)
#define MSR_IR	(1<<5)
#define MSR_IP	(1<<6)
#define MSR_FE1	(1<<8)
#define MSR_FE1_SHIFT	(8)
#define MSR_BE	(1<<9)
#define MSR_SE	(1<<10)
#define MSR_FE0 (1<<11)
#define MSR_FE0_SHIFT (11)
#define MSR_ME	(1<<12)
#define MSR_FP	(1<<13)
#define MSR_PR	(1<<14)
#define MSR_EE	(1<<15)
#define MSR_ILE	(1<<16)
#define MSR_POW	(1<<18)

/* XER p.65 */
#define XER (gppc.xer)
#define XER_BC (XER&0x7f)
#define XER_CA (1<<29)
#define XER_OV (1<<30)
#define XER_SO (1<<31)

/*  Exceptions */
#define EX_SRI		(0x100)
#define EX_MCI		(0x200)
#define EX_DSI		(0x300)
#define EX_ISI		(0x400)
#define EX_ALIGN	(0x600)
#define EX_PROGRAM	(0x700)
#define EX_FPUA		(0x800)
#define EX_DECR		(0x900)
#define EX_SYSCALL	(0xc00)
#define EX_FPA		(0xe00)
#define EX_SWE		(0x1000)
#define EX_ITLBMISS	(0x1100)
#define EX_DTLBMISS	(0x1200)
#define EX_ITLBERR	(0x1300)
#define EX_DTLBERR	(0x1400)
#define EX_DBKPT	(0x1c00)
#define EX_IBKPT	(0x1d00)
#define EX_PBKPT	(0x1e00)
#define EX_NMSKDEVPRT	(0x1f00)

PpcCpu *PpcCpu_New(int cpu_type, uint32_t initial_msr);
void PpcCpu_Run(void);
void PpcSetMsr(uint32_t value);
void Ppc_RegisterSprHandler(PpcCpu * cpu, unsigned int spr, SPR_ReadProc * rproc,
			    SPR_WriteProc * wproc, void *cd);
