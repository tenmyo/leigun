#ifndef _DEBUGGER_H
#define _DEBUGGER_H
#include <unistd.h>

typedef enum Dbg_TargetStat {
	/* Start with definitions stolen from GDB */
	DbgStat_SIGNAL0 = 0,
	DbgStat_SIGHUP = 1,
	DbgStat_SIGINT = 2,
	DbgStat_SIGQUIT = 3,
	DbgStat_SIGILL = 4,
	DbgStat_SIGTRAP = 5,
	DbgStat_SIGABRT = 6,
	DbgStat_SIGEMT = 7,
	DbgStat_SIGFPE = 8,
	DbgStat_SIGKILL = 9,
	DbgStat_SIGBUS = 10,
	DbgStat_SIGSEGV = 11,
	DbgStat_SIGSYS = 12,
	DbgStat_SIGPIPE = 13,
	DbgStat_SIGALRM = 14,
	DbgStat_SIGTERM = 15,
	DbgStat_SIGURG = 16,
	DbgStat_SIGSTOP = 17,
	DbgStat_SIGTSTP = 18,
	DbgStat_SIGCONT = 19,
	DbgStat_SIGCHLD = 20,
	DbgStat_SIGTTIN = 21,
	DbgStat_SIGTTOU = 22,
	DbgStat_SIGIO = 23,
	DbgStat_SIGXCPU = 24,
	DbgStat_SIGXFSZ = 25,
	DbgStat_SIGVTALRM = 26,
	DbgStat_SIGPROF = 27,
	DbgStat_SIGWINCH = 28,
	DbgStat_SIGLOST = 29,
	DbgStat_SIGUSR1 = 30,
	DbgStat_SIGUSR2 = 31,
	DbgStat_GDBEND = 31,
	/* Now non-gdb definitions */
	DbgStat_RUNNING = 256,
	DbgStat_OK = 257,
} Dbg_TargetStat;

/*
 * -------------------------------------------------------
 * Every architecture which wants to use gdb for debugging 
 * has to implement the DebugBackendOps
 * -------------------------------------------------------
 */
typedef struct DebugBackendOps {
	Dbg_TargetStat(*get_status) (void *clientData);	/* For gdb "?" query */
	int (*step) (void *clientData, uint64_t addr, int use_addr);
	int (*stop) (void *clientData);
	int (*cont) (void *clientData);
	int (*getreg) (void *clientData, uint8_t * val, uint32_t index, int maxlen);
	void (*setreg) (void *clientData, const uint8_t * val, uint32_t index, int len);
	 ssize_t(*getmem) (void *clientData, uint8_t * data, uint64_t addr, uint32_t len);
	 ssize_t(*setmem) (void *clientData, const uint8_t * data, uint64_t addr, uint32_t len);
	void (*get_bkpt_ins) (void *clientData, uint8_t * ins, uint64_t addr, int len);
} DebugBackendOps;

typedef struct Debugger {
	void *implementor;
	int (*notifyStatus) (void *implementor, Dbg_TargetStat);
} Debugger;

Debugger *Debugger_New(DebugBackendOps * ops, void *backend);

static inline int
Debugger_Notify(Debugger * dbg, Dbg_TargetStat status)
{
	return dbg->notifyStatus(dbg->implementor, status);
}
#endif
