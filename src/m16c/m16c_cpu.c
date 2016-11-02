#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "cycletimer.h"
#include "mainloop_events.h"
#include "idecode_m16c.h"
#include "fio.h"
#include "m16c_cpu.h"
#include "bus.h"
#include "configfile.h"
#include "sgstring.h"
#include "signode.h"
#include "throttle.h"

typedef struct M16C_Variant {
        char *name;
	uint32_t vector_reset;	
	uint32_t vector_base;	
} M16C_Variant;

M16C_Variant m16c_variants[] = {
	{ 
	  .name = "m16c65",	
	  .vector_reset = 0xFFFFC,
	  .vector_base = 0xFFFDC
	},
	{ 
	  .name = "r8c23",	
	  .vector_reset = 0xFFFC,
	  .vector_base = 0xFFDC,
	}
};

static void
M16C_SignalLevelConflict(const char *msg)
{
        fprintf(stderr,"PC 0x%06x: %s\n",M16C_REG_PC,msg);
}

/* global variable for faster access */
M16C_Cpu gm16c;

static inline void
M16C_UpdateIPL(void)
{
        int cpu_ilvl = (M16C_REG_FLG & M16C_FLG_IPL_MSK) >> M16C_FLG_IPL_SHIFT;
	//fprintf(stderr,"New ilvl %d\n",cpu_ilvl);
        if(gm16c.pending_ilvl > cpu_ilvl) {
		//fprintf(stderr,"UpdateIPL: Post irq pending %d, cpu %d, flags %04x\n",gm16c.pending_ilvl,cpu_ilvl,M16C_REG_FLG);
                M16C_PostSignal(M16C_SIG_IRQ);
        } else {
		//fprintf(stderr,"UpdateIPL: Unpost irq\n");
                M16C_UnpostSignal(M16C_SIG_IRQ);
        }
}

void
M16C_SyncRegSets(void)
{
        if(M16C_REG_FLG & M16C_FLG_BANK) {
                gm16c.bank[1] = gm16c.regs;
        } else {
                gm16c.bank[0] = gm16c.regs;
        }
        if(M16C_REG_FLG & M16C_FLG_U) {
                gm16c.regs.usp = M16C_REG_SP;
        } else {
                gm16c.regs.isp = M16C_REG_SP;
        }
}

void
M16C_SetRegFlg(uint16_t flg)
{
        uint16_t diff = flg ^ M16C_REG_FLG;
        M16C_REG_FLG = flg;
        if(!(diff & (M16C_FLG_U | M16C_FLG_BANK | M16C_FLG_I | M16C_FLG_IPL_MSK))) {
                return;
        }
        if(diff & M16C_FLG_I) {
                if(unlikely(flg & M16C_FLG_I)) {
                        gm16c.signals_mask |= M16C_SIG_IRQ;
                } else {
                        gm16c.signals_mask &= ~M16C_SIG_IRQ;
                }
                M16C_UpdateSignals();
        }
        if(diff & M16C_FLG_U) {
                if(flg & M16C_FLG_U) {
                        gm16c.regs.isp = M16C_REG_SP;
                        M16C_REG_SP = gm16c.regs.usp;
                } else {
                        gm16c.regs.usp = M16C_REG_SP;
                        M16C_REG_SP = gm16c.regs.isp;
                }
        }
        if(diff & M16C_FLG_BANK) {
                if(flg & M16C_FLG_BANK) {
                        gm16c.bank[0] = gm16c.regs;
                        gm16c.regs = gm16c.bank[1];
                } else {
                        gm16c.bank[1] = gm16c.regs;
                        gm16c.regs = gm16c.bank[0];
                }
        }
        if(diff & M16C_FLG_IPL_MSK) {
                M16C_UpdateIPL();
        }
}

M16C_Cpu *
M16C_CpuNew(const char *instancename,M16C_AckIrqProc *ackIrqProc, BusDevice *intco)
{
	char *variantname;
	int i;
	int nr_variants = array_size(m16c_variants);
	uint32_t cpu_clock = 125000;
	M16C_Variant *var;
	M16C_Cpu *m16c = &gm16c;
	memset(m16c,0,sizeof(M16C_Cpu));
	M16C_IDecoderNew();	
	gm16c.intco = intco;
	gm16c.ackIrq = ackIrqProc;
	Config_ReadUInt32(&cpu_clock,"global","cpu_clock");
	variantname = Config_ReadVar(instancename,"variant");
        if(!variantname) {
                fprintf(stderr,"No CPU variant selected\n");
                exit(1);
        }
        fprintf(stderr,"%d variants\n",nr_variants);
        for(i=0;i<nr_variants;i++) {
                var = &m16c_variants[i];
                if(strcmp(var->name,variantname) == 0) {
                        break;
                }
        }
	if(i == nr_variants) {
                fprintf(stderr,"Unknown M16C CPU variant \"%s\"\n",variantname);
                exit(1);
        }
	m16c->vector_reset = var->vector_reset;
	m16c->vector_base = var->vector_base;
	CycleTimers_Init(instancename,cpu_clock);
	m16c->signals_mask = M16C_SIG_DBG;
	Signodes_SetConflictProc(M16C_SignalLevelConflict);
	gm16c.throttle = Throttle_New(instancename);
	return m16c;
}

static void
M16C_Interrupt(void)
{
        uint32_t irq_no;
	uint32_t intb;
        uint16_t flg = M16C_REG_FLG;
        irq_no = gm16c.pending_intno;
        //fprintf(stderr,"Handling pending int %d, flg %04x, sp %04x\n",irq_no,M16C_REG_FLG,M16C_REG_SP);
        M16C_SET_REG_FLG(flg & ~(M16C_FLG_I | M16C_FLG_D | M16C_FLG_U));
        M16C_SET_REG_FLG((M16C_REG_FLG & ~M16C_FLG_IPL_MSK)
                        | (gm16c.pending_ilvl << M16C_FLG_IPL_SHIFT));
        M16C_REG_SP -= 1;
        M16C_Write8(((M16C_REG_PC >> 16) & 0xf) | ((flg >> 8) & 0xf0) ,M16C_REG_SP);
        M16C_REG_SP -= 1;
        M16C_Write8(flg,M16C_REG_SP);
        M16C_REG_SP -= 2;
        M16C_Write16(M16C_REG_PC,M16C_REG_SP);
        M16C_REG_PC = M16C_Read24((M16C_REG_INTB + (irq_no << 2)) & 0xfffff) & 0xfffff;

	intb = M16C_REG_INTB;
        M16C_REG_PC = M16C_Read24(intb + (irq_no << 2)) & 0xfffff;
	//fprintf(stderr,"Interrupt will now be acked\n");
        gm16c.ackIrq(gm16c.intco,gm16c.pending_intno);
}

void
M16C_PostILevel(int ilvl,int int_no)
{
        gm16c.pending_ilvl = ilvl;
        gm16c.pending_intno = int_no;
        M16C_UpdateIPL();
}

static inline void
CheckSignals() {
        if(unlikely(mainloop_event_pending)) {
		#if 0
		fprintf(stderr,"Signals raw %08x, %08x, mask %08x\n",
			gm16c.signals_raw,gm16c.signals,gm16c.signals_mask);
		exit(1);
		#endif
                mainloop_event_pending = 0;
                if(mainloop_event_io) {
                        FIO_HandleInput();
                }
                if(gm16c.signals) {
                        if(gm16c.signals & M16C_SIG_IRQ) {
                                M16C_Interrupt();
                        }
			if(unlikely(gm16c.signals & M16C_SIG_DBG)) {
				//Do_Debug();
			}
                }
        }
}

void
M16C_Run(void) 
{
	M16C_Cpu *m16c = &gm16c;
        M16C_Instruction *instr;
        uint32_t startaddr;
        uint32_t dbgwait;

        if(Config_ReadUInt32(&startaddr,"global","start_address") < 0) {
                startaddr = M16C_Read24(m16c->vector_reset);
        }
        if(Config_ReadUInt32(&dbgwait,"global","dbgwait") < 0) {
                dbgwait=0;
        }
        M16C_REG_PC = startaddr;
        if(dbgwait) {
                fprintf(stderr,"CPU is waiting for debugger connection at %08x\n",startaddr);
                //g16c.dbg_state = M16CDBG_STOPPED;
                M16C_PostSignal(M16C_SIG_DBG);
        } else {
                fprintf(stderr,"Starting CPU at 0x%06x\n",M16C_REG_PC);
        }
	#if 0
        setjmp(m32->restart_idec_jump);
        while(m16c->dbg_state == M16CDBG_STOPPED) {
                struct timespec tout;
                tout.tv_nsec=0;
                tout.tv_sec=10000;
                FIO_WaitEventTimeout(&tout);
        }
	#endif
	while(1) 
	{
		//usleep(10000);
		gm16c.icode = M16C_IFetch(M16C_REG_PC);
		instr = M16C_InstructionFind(ICODE());
#if 0
		if((M16C_REG_PC >= 0xF89aa) && (M16C_REG_PC <= 0xF89db)) {
		fprintf(stderr,"ICODE 0x%04x at %06x found instr %s len %d, R0 %04x R1 %04x \n",ICODE(),M16C_REG_PC,instr->name,instr->len,M16C_REG_R0,M16C_REG_R1);
		}
#endif
		M16C_REG_PC += instr->len;
		instr->proc();
		CheckSignals();	
		CycleCounter+=3;
                CycleTimers_Check();
	}
}

#ifdef _SHARED_
void
_init(void) {
        fprintf(stderr,"Loading M16C emulation module.\n");
}
#endif
