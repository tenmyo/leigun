/*
 **********************************************************************************
 * clock.h
 *      Emulation of clock trees
 *
 * (C) 2006 Jochen Karrer 
 **********************************************************************************
 */

#ifndef _CLOCK_H
#define _CLOCK_H
#include "strhash.h"
#include "sgtypes.h"
#include "sglib.h"

struct Clock;
typedef void ClockTraceProc(struct Clock *clock, void *clientData);

typedef struct ClockTrace {
	ClockTraceProc *proc;
	void *clientData;
	struct ClockTrace *next;
} ClockTrace_t;

typedef struct Clock {
	uint64_t systemMasterClock_Version;
	char *name;
	ClockTrace_t *traceHead;
	/* derivation */
	struct Clock *parent;

	/* ratio to parent clock is described by nom/denom */
	uint64_t derivation_nom;
	uint64_t derivation_denom;

	/* Accumulated fraction relative to toplevel clock */
	uint64_t acc_nom;
	uint64_t acc_denom;

	/* Cached ratio to master clock */
	uint64_t ratio_nom;
	uint64_t ratio_denom;

	struct Clock *first_child;
	struct Clock *next_sibling;
	struct Clock *prev_sibling;
	SHashEntry *hash_entry;
} Clock_t;

void Clock_SetFreq(Clock_t * clock, uint64_t hz);
ClockTrace_t *Clock_Trace(Clock_t * clock, ClockTraceProc * proc, void *traceData);
void Clock_Untrace(Clock_t *, ClockTrace_t *);
Clock_t *Clock_New(const char *format, ...) __attribute__ ((format(printf, 1, 2)));;

/*
 ********************************************************************
 * Outdated, should not be used anymore. 
 * Use integer variant Clock_Freq
 **********************************************************************
 */
static inline double
Clock_DFreq(Clock_t * clock)
{
	if (clock->acc_denom) {
		return (double)clock->acc_nom / clock->acc_denom;
	} else {
		return 0;
	}
}

/*
 *******************************************************
 * New version is a fraction
 *******************************************************
 */
/* Hopefully is written that denom is never 0 */
#define Clock_Freq(clock) ((clock)->acc_nom / (clock)->acc_denom)
#define Clock_FreqNom(clock) ((clock)->acc_nom)
#define Clock_FreqDenom(clock) ((clock)->acc_denom)

#define Clock_Ratio(clock1,clock2) ((clock2)->acc_nom ? \
	((clock1)->acc_nom * (clock2)->acc_denom / ((clock1)->acc_denom * (clock2)->acc_nom)) : ~UINT64_C(0))

/*
 * -------------------------------------------------------------------
 * Make a clock to be derived from another clock by a fraction
 * -------------------------------------------------------------------
 */

void Clock_MakeDerived(Clock_t * clock, Clock_t * parent, uint64_t nom, uint64_t denom);
void Clock_Decouple(Clock_t * child);
void Clock_DumpTree(Clock_t * top);
void ClocksInit(void);
/* Special versions of make derived and decouple with ratio 1:1 and names */
void Clock_Link(const char *child, const char *parent);
void Clock_Unlink(const char *child);
Clock_t *Clock_Find(const char *format, ...);

void Clock_MakeSystemMaster(Clock_t * clock);
/**
 *********************************************************************
 * \fn FractionU64_t Clock_MasterRatio(Clock_t *clock);
 * Get the ratio between clock and system master clock.
 * (Clock frequency) / (Master clock frequency)
 *********************************************************************
 */
FractionU64_t Clock_MasterRatio(Clock_t * clock);
#endif
