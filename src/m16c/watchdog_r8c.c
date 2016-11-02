#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "sgstring.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "watchdog_r8c.h"

#define REG_WDTS	(0x0e)
typedef struct Watchdog {
	BusDevice bdev;
} Watchdog;

static uint32_t
wdts_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
wdts_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Watchdog reset\n");
	exit(0);
}

static void
R8CWatchdog_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete16(REG_WDTS);
}

static void
R8CWatchdog_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	Watchdog *wdg = owner;
	uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_PRD_RARP | IOH_FLG_PA_CBSE;
	IOH_New16f(REG_WDTS, wdts_read, wdts_write, wdg, flags);
}

BusDevice *
R8C_WatchdogNew(const char *name)
{
	Watchdog *wdg = sg_new(Watchdog);
	wdg->bdev.first_mapping = NULL;
	wdg->bdev.Map = R8CWatchdog_Map;
	wdg->bdev.UnMap = R8CWatchdog_Unmap;
	wdg->bdev.owner = wdg;
	wdg->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &wdg->bdev;
}
