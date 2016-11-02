
#include "cpu_mcs51.h"
#include "sgstring.h"
#include "instructions_mcs51.h"
#include "idecode_mcs51.h"
#include "signode.h"
#include "fio.h"
#include "diskimage.h"
#include "cycletimer.h"
#include "configfile.h"
#include "mainloop_events.h"
#include "loader.h"

static inline void
CheckSignals(void)
{
        if(unlikely(mainloop_event_pending)) {
                mainloop_event_pending = 0;
                if(mainloop_event_io) {
                        FIO_HandleInput();
                }
#if 0
                if(g_mcs5.cpu_signals & AVR8_SIG_IRQ) {
                        AVR8_Interrupt();
                } else if(gavr8.cpu_signals & AVR8_SIG_IRQENABLE) {
                        gavr8.cpu_signals_raw &= ~AVR8_SIG_IRQENABLE;
                        AVR8_UpdateCpuSignals();
                }
#endif
        }
}


void
MCS51_Run()  {
        uint8_t icode;
        uint16_t addr=0;
        MCS51_InstructionProc *iproc;
#if 0
        if(Config_ReadUInt32(&addr,"global","start_address")<0) {
                addr=0;
        }
#endif
        SET_REG_PC(addr);

        while(1) {
                icode = ICODE = MCS51_ReadPgmMem(GET_REG_PC);
                //logPC();
                SET_REG_PC(GET_REG_PC + 1);
                iproc = MCS51_InstructionProcFind(icode);
                iproc();
                CycleTimers_Check();
                CheckSignals();
        }
}

/**
 */
void
MCS51_Init(const char *instancename)
{
	MCS51Cpu *mcs51 = &g_mcs51;
	char *imagedir,*flashname;
       	MCS51_SetPSW(0);
	MCS51_IDecoderNew();
	imagedir = Config_ReadVar("global","imagedir");
        if(!imagedir) {
                fprintf(stderr,"No directory given for MCS51 ROM diskimage\n");
                exit(1);
        }
        flashname = alloca(strlen(instancename) + strlen(imagedir) + 20);
        sprintf(flashname,"%s/%s.rom",imagedir,instancename);
	mcs51->approm_size = 4096;
        mcs51->flash_di = DiskImage_Open(flashname,mcs51->approm_size,DI_RDWR | DI_CREAT_FF);
        if(!mcs51->flash_di) {
                fprintf(stderr,"Can not create or open the MCS internal ROM image \"%s\"\n",flashname);
                exit(1);
        }
        mcs51->approm = DiskImage_Mmap(mcs51->flash_di);
}
