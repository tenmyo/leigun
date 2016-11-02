/**
 ********************************************************************************
 * 8051 CPU core
 ********************************************************************************
 */
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

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

/*
 *****************************************************
 * CPU core SFRs:
 *****************************************************
 */
#define SFR_REG_ACC	(0xe0)
#define SFR_REG_B	(0xf0)
#define SFR_REG_PSW	(0xd0)
#define SFR_REG_SP	(0x81)
#define SFR_REG_DPL	(0x82)
#define SFR_REG_DPH	(0x83)

MCS51Cpu g_mcs51;

void
MCS51_RegisterSFR(uint8_t byte_addr, C51_SfrReadProc * readProc, C51_SfrReadProc * latchedRead,
		  C51_SfrWriteProc * writeProc, void *cbData)
{
	if (byte_addr < 128) {
		fprintf(stderr, "Registering illegal SFR register 0x%02x\n", byte_addr);
		exit(1);
	} else {
		g_mcs51.sfrDev[byte_addr & 0x7f] = cbData;
		g_mcs51.sfrRead[byte_addr & 0x7f] = readProc;
		g_mcs51.sfrLatchedRead[byte_addr & 0x7f] = latchedRead;
		g_mcs51.sfrWrite[byte_addr & 0x7f] = writeProc;
	}
	return;
}

static void
MCS51_UpdateIPL()
{
	if (g_mcs51.maxPendingIpl > g_mcs51.currentIpl) {
		dbgprintf("Post signal IRQ, maxpending %d, currentIpl %d\n", g_mcs51.maxPendingIpl,
			  g_mcs51.currentIpl);
		//usleep(100000);
		MCS51_PostSignal(MCS51_SIG_IRQ);
	} else {
		dbgprintf("Unpost signal IRQ\n");
		MCS51_UnpostSignal(MCS51_SIG_IRQ);
	}
}

/**
 *******************************************************************
 * Push the current IPL onto the IPL stack for RETI
 *******************************************************************
 */
static inline void
MCS51_PushIpl(void)
{
	MCS51Cpu *mcs51 = &g_mcs51;
	if (mcs51->iplStackP < array_size(mcs51->iplStack)) {
		mcs51->iplStack[mcs51->iplStackP] = mcs51->currentIpl;
		dbgprintf("Pushed IPL %d onto stackP  %u\n", mcs51->currentIpl, mcs51->iplStackP);
		mcs51->iplStackP++;
	} else {
		fprintf(stderr, "Bug: IPL stack overflow\n");
	}
}

/**
 ************************************************************************
 * Pop one IPL from the IPL stack. Used  by RETI instruction to return to
 * the old ipl.
 ************************************************************************
 */
void
MCS51_PopIpl(void)
{
	MCS51Cpu *mcs51 = &g_mcs51;
	if (mcs51->iplStackP > 0) {
		mcs51->iplStackP--;
		mcs51->currentIpl = mcs51->iplStack[mcs51->iplStackP];
		dbgprintf("Poped IPL %d\n", mcs51->currentIpl);
		MCS51_UpdateIPL();
	} else {
		fprintf(stderr, "Warning: Unbalanced Pop IPL\n");
	}
}

/**
 ********************************************************
 * Same as lcall but with push of current IPL and 
 * switch to a new IPL.
 ********************************************************
 */
static void
MCS51_Interrupt(void)
{
	uint16_t addr = g_mcs51.pendingVectAddr;
	uint16_t sp;
	dbgprintf("Interrupt !,vect %x\n", g_mcs51.pendingVectAddr);
	MCS51_PushIpl();
	g_mcs51.currentIpl = g_mcs51.maxPendingIpl;
	MCS51_UnpostSignal(MCS51_SIG_IRQ);
	SigNode_Set(g_mcs51.sigAckIntOut, SIG_LOW);
	SigNode_Set(g_mcs51.sigAckIntOut, SIG_HIGH);
	sp = MCS51_GetRegSP();
	sp++;
	MCS51_WriteMemIndirect(GET_REG_PC & 0xff, sp);
	sp++;
	MCS51_WriteMemIndirect((GET_REG_PC >> 8) & 0xff, sp);
	MCS51_SetRegSP(sp);
	SET_REG_PC(addr);
}

static inline void
CheckSignals(void)
{
	if (unlikely(mainloop_event_pending)) {
		mainloop_event_pending = 0;
		if (mainloop_event_io) {
			FIO_HandleInput();
		}
		if (g_mcs51.signals & MCS51_SIG_IRQ) {
			MCS51_Interrupt();
		}
	}
}

void
MCS51_PostILvl(int ilvl, uint16_t vectAddr)
{
	g_mcs51.maxPendingIpl = ilvl;
	g_mcs51.pendingVectAddr = vectAddr;
	MCS51_UpdateIPL();
}

void
MCS51_Run()
{
	uint32_t addr = 0;
	MCS51_Instruction *instr;
	if (Config_ReadUInt32(&addr, "global", "start_address") < 0) {
		addr = 0;
	}
	SET_REG_PC(addr);

	while (1) {
		ICODE = MCS51_ReadPgmMem(GET_REG_PC);
		//logPC();
		//fprintf(stderr,"ICODE %02x at %04x\n",icode,GET_REG_PC);
		//usleep(10000);
		//fprintf(stderr,"Instr: %s at %08x\n",MCS51_InstructionFind(icode)->name,GET_REG_PC);
		SET_REG_PC(GET_REG_PC + 1);
		instr = MCS51_InstructionFind(ICODE);
		instr->iproc();
		/* meassurement gave 422566543/268435456*12 = 18.890 */
		CycleCounter += instr->cycles;
		CycleTimers_Check();
		CheckSignals();
	}
}

/*
 * The interface to the loader
 */
static int
load_to_bus(void *clientData, uint32_t addr, uint8_t * buf, unsigned int count, int flags)
{
	MCS51Cpu *mcs51 = clientData;
	uint32_t i;
	for (i = 0; i < count; i++) {
		uint32_t byte_addr;
		byte_addr = (addr + i);
		if (byte_addr >= mcs51->approm_size) {
			fprintf(stderr, "Loading past end of application memory\n");
			exit(1);
		}
		mcs51->approm[byte_addr] = buf[i];
	}
	return 0;
}

/**
 *****************************************************
 * Access to accumulator from SFR area
 *****************************************************
 */
static uint8_t
acc_read(void *eventData, uint8_t addr)
{
	return MCS51_GetAcc();
}

static void
acc_write(void *eventData, uint8_t addr, uint8_t value)
{
	return MCS51_SetAcc(value);
}

/**
 *****************************************************
 * Access to B Register from SFR area
 *****************************************************
 */
static uint8_t
b_read(void *eventData, uint8_t addr)
{
	return MCS51_GetRegB();
}

static void
b_write(void *eventData, uint8_t addr, uint8_t value)
{
	MCS51_SetRegB(value);
}

/**
 **************************************************
 * Access to Flags from SFR area
 **************************************************
 */
static uint8_t
psw_read(void *eventData, uint8_t addr)
{
	return PSW;
}

static void
psw_write(void *eventData, uint8_t addr, uint8_t value)
{
	MCS51_SetPSW(value);
}

/**
 **************************************************
 * Access to Stack Pointer from SFR area
 **************************************************
 */
static uint8_t
sp_read(void *eventData, uint8_t addr)
{
	return MCS51_GetRegSP();
}

static void
sp_write(void *eventData, uint8_t addr, uint8_t value)
{
	MCS51_SetRegSP(value);
}

/**
 **********************************************************
 * Access to data pointer from SFR area
 **********************************************************
 */
static uint8_t
dpl_read(void *eventData, uint8_t addr)
{
	return MCS51_GetRegDptr() & 0xff;
}

static void
dpl_write(void *eventData, uint8_t addr, uint8_t value)
{
	MCS51_SetRegDptr((MCS51_GetRegDptr() & 0xff00) | value);
}

static uint8_t
dph_read(void *eventData, uint8_t addr)
{
	return (MCS51_GetRegDptr() >> 8) & 0xff;
}

static void
dph_write(void *eventData, uint8_t addr, uint8_t value)
{
	MCS51_SetRegDptr((MCS51_GetRegDptr() & 0xff) | ((uint16_t) value << 8));
}

void
MCS51_MapExmem(MCS51Cpu * mcs51, uint16_t addr, uint32_t size,
	       Exmem_ReadProc * rProc, Exmem_WriteProc * wProc, void *dev)
{
	uint32_t i;
	uint32_t entry;
	if (addr & (EXMEM_MAP_ENTRY_SIZE - 1)) {
		fprintf(stderr, "Unaligned EXMem address: %u\n", EXMEM_MAP_ENTRY_SIZE);
		exit(1);
	}
	if (size & (EXMEM_MAP_ENTRY_SIZE - 1)) {
		fprintf(stderr, "Unaligned EXMem size: %u\n", EXMEM_MAP_ENTRY_SIZE);
		exit(1);
	}
	for (i = 0; i < size; i += EXMEM_MAP_ENTRY_SIZE) {
		entry = (addr + i) / EXMEM_MAP_ENTRY_SIZE;
		mcs51->exmemWriteProc[entry] = wProc;
		mcs51->exmemReadProc[entry] = rProc;
		mcs51->exmemDev[entry] = dev;
		//      fprintf(stderr,"MAPPED EXMEM entry %u\n",entry);
	}
}

void
MCS51_UnmapExmem(MCS51Cpu * mcs51, uint16_t addr, uint32_t size)
{
	uint32_t i;
	uint32_t entry;
	for (i = 0; i < size; i += EXMEM_MAP_ENTRY_SIZE) {
		entry = (addr + i) / EXMEM_MAP_ENTRY_SIZE;
		mcs51->exmemWriteProc[entry] = NULL;
		mcs51->exmemReadProc[entry] = NULL;
		mcs51->exmemDev[entry] = NULL;
	}
}

/**
 ****************************************************************************
 * \fn void MCS51_Init(const char *instancename)
 ****************************************************************************
 */
MCS51Cpu *
MCS51_Init(const char *instancename)
{
	MCS51Cpu *mcs51 = &g_mcs51;
	char *imagedir, *flashname;
	uint32_t cpu_clock = 1000000;
	uint32_t cycle_mult = 12;
	MCS51_SetPSW(0);
	SET_REG_PC(0);
	Config_ReadUInt32(&cycle_mult,instancename, "cycle_mult");
	MCS51_IDecoderNew(cycle_mult);
	imagedir = Config_ReadVar("global", "imagedir");
	if (!imagedir) {
		fprintf(stderr, "No directory given for MCS51 ROM diskimage\n");
		exit(1);
	}
	flashname = alloca(strlen(instancename) + strlen(imagedir) + 20);
	sprintf(flashname, "%s/%s.rom", imagedir, instancename);
	mcs51->currentIpl = -1;
	mcs51->sigAckIntOut = SigNode_New("%s.ackInt", instancename);
	if (!mcs51->sigAckIntOut) {
		fprintf(stderr, "Can not create Ack signal for Interrupts\n");
		exit(1);
	}
	SigNode_Set(g_mcs51.sigAckIntOut, SIG_HIGH);
	mcs51->approm_size = 65536;
	mcs51->flash_di = DiskImage_Open(flashname, mcs51->approm_size, DI_RDWR | DI_CREAT_FF);
	if (!mcs51->flash_di) {
		fprintf(stderr, "Can not create or open the MCS internal ROM image \"%s\"\n",
			flashname);
		exit(1);
	}
	mcs51->approm = DiskImage_Mmap(mcs51->flash_di);
	Loader_RegisterBus("bus", load_to_bus, mcs51);
	Config_ReadUInt32(&cpu_clock, "global", "cpu_clock");
	CycleTimers_Init(instancename, cpu_clock);
	mcs51->throttle = Throttle_New(instancename);
	MCS51_RegisterSFR(SFR_REG_ACC, acc_read, NULL, acc_write, mcs51);
	MCS51_RegisterSFR(SFR_REG_B, b_read, NULL, b_write, mcs51);
	MCS51_RegisterSFR(SFR_REG_PSW, psw_read, NULL, psw_write, mcs51);
	MCS51_RegisterSFR(SFR_REG_SP, sp_read, NULL, sp_write, mcs51);
	MCS51_RegisterSFR(SFR_REG_DPL, dpl_read, NULL, dpl_write, mcs51);
	MCS51_RegisterSFR(SFR_REG_DPH, dph_read, NULL, dph_write, mcs51);
	mcs51->clock12 = Clock_New("%s.clk12", instancename);
	mcs51->clock6 = Clock_New("%s.clk6", instancename);
	mcs51->clock1 = Clock_Find("%s.clk", instancename);
	if (!mcs51->clock1) {
		fprintf(stderr, "MCS51 clock 1 not found\n");
		exit(1);
	}
	Clock_MakeDerived(mcs51->clock6, mcs51->clock1, 1, 6);
	Clock_MakeDerived(mcs51->clock12, mcs51->clock1, 1, 12);

	return mcs51;
}
