#ifndef MAINLOOP_SIGNALS_H
#define MAINLOOP_SIGNALS_H

#include <stdint.h>

extern uint32_t mainloop_event_io;
extern uint32_t mainloop_event_pending;

static inline void
MainLoopSignal_PostIOEvent()
{
	mainloop_event_io = 1;
	mainloop_event_pending = 1;
}

#endif
