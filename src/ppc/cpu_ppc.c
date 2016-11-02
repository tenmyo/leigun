#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <cpu_ppc.h>
#include <idecode_ppc.h>
#include <setjmp.h>
#include <cycletimer.h>
#include "fio.h"
#include "configfile.h"
#include "mainloop_events.h"
#include "mmu_ppc.h"

void
PpcCpu_Exception(uint32_t exception,uint32_t fault_addr,uint32_t machine_status) 
{
	gppc.srr0 = fault_addr; 
	gppc.srr1 = machine_status;
	if(gppc.msr & MSR_IP) {
		NIA = exception | 0xfff00000; 
	} else {
		NIA = exception; 
	}
}

void
PpcSetMsr(uint32_t value) {
	gppc.msr = value;
	gppc.msr_ee = value & MSR_EE; 	// External Interrupts
        gppc.msr_pr = value & MSR_PR; 	// Privilege level
        gppc.msr_me = value & MSR_ME; 	// Machine Check Exceptions enable
        gppc.msr_fe = ((value & MSR_FE0) >> (MSR_FE0_SHIFT-1))
			| ((value & MSR_FE1) >> MSR_FE1_SHIFT); // floating point exception mode
        gppc.msr_ip = value & MSR_IP; 	// exception prefix
        gppc.msr_ir = value & MSR_IR;  // translate instruction
        gppc.msr_dr = value & MSR_DR;  // translate data
	if(value & (1 | (1<<16))) {
		fprintf(stderr,"MSR: Little endian mode not supported\n");
	}	
	return;
}
/*
 * --------------------------------------
 * Create a pointer table for the SPRs
 * --------------------------------------
 */
static void
PpcCpu_CreateSprs(PpcCpu *cpu) 
{
	cpu->spr[1] = &cpu->xer;	
	cpu->spr[8] = &cpu->lr;	
	cpu->spr[9] = &cpu->ctr;	
	cpu->spr[18] = &cpu->dsisr;
	cpu->spr[19] = &cpu->dar;
	cpu->spr[22] = &cpu->dec;
	cpu->spr[25] = &cpu->sdr1;
	cpu->spr[26] = &cpu->srr0;
	cpu->spr[27] = &cpu->srr1;
	cpu->spr[268] = &cpu->tbl;
	cpu->spr[269] = &cpu->tbu;

	cpu->spr[272] = &cpu->sprg0;	
	cpu->spr[273] = &cpu->sprg1;	
	cpu->spr[274] = &cpu->sprg2;	
	cpu->spr[275] = &cpu->sprg3;	
	cpu->spr[282] = &cpu->ear;
	cpu->spr[287] = &cpu->pvr;
	cpu->spr[528] = &cpu->ibat0u;
	cpu->spr[529] = &cpu->ibat0l;
	cpu->spr[530] = &cpu->ibat1u;
	cpu->spr[531] = &cpu->ibat1l;
	cpu->spr[532] = &cpu->ibat2u;
	cpu->spr[533] = &cpu->ibat2l;
	cpu->spr[534] = &cpu->ibat3u;
	cpu->spr[535] = &cpu->ibat3l;

	cpu->spr[536] = &cpu->dbat0u;
	cpu->spr[537] = &cpu->dbat0l;
	cpu->spr[538] = &cpu->dbat1u;
	cpu->spr[539] = &cpu->dbat1l;
	cpu->spr[540] = &cpu->dbat2u;
	cpu->spr[541] = &cpu->dbat2l;
	cpu->spr[542] = &cpu->dbat3u;
	cpu->spr[543] = &cpu->dbat3l;
	cpu->spr[1013] = &cpu->dabr;
	cpu->spr[1022] = &cpu->fpecr;
	cpu->spr[1023] = &cpu->pir;
		
}

PpcCpu gppc;
/*
 * -----------------------------
 * Constructor for the CPU
 * -----------------------------
 */
PpcCpu *
PpcCpu_New(int cpu_type,uint32_t initial_msr) {
	PpcCpu *cpu = &gppc; /* global is faster */
	if(!cpu) {
		fprintf(stderr,"Out of memory allocating PowerPC CPU\n");
		exit(345);
	}
	memset(cpu,0,sizeof(PpcCpu));	
	/*  exception Vectors at 0xfffxxxxx p.77      */
	PpcSetMsr(initial_msr); 
	switch(cpu_type) {
		case CPU_MPC852T:
			PVR=0x00500000; /* Processor Version from MPC866UM p.134 3-18 */
			break;
		case CPU_MPC866P:
			PVR=0x00500000; 
			break;
		default:
			fprintf(stderr,"CPU-type %d not implemented\n",cpu_type);
			exit(175);
	}
	PpcCpu_CreateSprs(cpu); 
	PPCIDecoder_New(cpu_type);
	cpu->cpuclk = Clock_New("cpu.clk");
	cpu->tmbclk = Clock_New("cpu.tmbclk");
	cpu->last_tb_update = CycleCounter_Get();
	Clock_SetFreq(cpu->cpuclk,133000000);
	Clock_SetFreq(cpu->tmbclk,33000000);
	return cpu;
}

static inline void
CheckSignals() {
	if(unlikely(mainloop_event_pending)) {
                mainloop_event_pending = 0;
                if(mainloop_event_io) {
                        FIO_HandleInput();
                }
                if(gppc.signals) {

		}
	}

}

/*
 * --------------------------------------------------
 * Register a Read and a write function for a SPR
 * --------------------------------------------------
 */

void 
Ppc_RegisterSprHandler(PpcCpu *cpu,unsigned int spr,SPR_ReadProc *rproc,SPR_WriteProc *wproc, void *cd) 
{
	if(spr<1024) {
		cpu->spr_read[spr] = rproc;
		cpu->spr_write[spr] = wproc;
		cpu->spr_clientData[spr] = cd;
	}
}

void
PpcCpu_Run() 
{
        InstructionProc *iproc;
	uint32_t start_addr;
        uint32_t icode;
	if(Config_ReadUInt32(&start_addr,"global","start_address")<0) {
                start_addr=0;
        }
        fprintf(stderr,"Starting PPC-CPU at %08x\n",start_addr);
        gettimeofday(&gppc.starttime,NULL);
        NIA=start_addr;
        /* Exceptions use goto (longjmp) */
        setjmp(gppc.abort_jump);
        CycleCounter+=2;
        CycleTimers_Check();
        while(1) {
		icode=PPCMMU_IFetch(NIA);
		//fprintf(stderr,"addr %08x icode %08x ",NIA,icode);
		NIA+=4;
                iproc=InstructionProcFind(icode);
                iproc(icode);
                CycleCounter+=2;
                CycleTimers_Check();
                CheckSignals();
        }
}

#if 0
void
_init(void) {
	fprintf(stderr,"PowerPC Emulation Module loaded\n");
}
#endif
