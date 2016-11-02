#include "bus.h"
#define R8C23_REGSET_TIMERA	(0)
#define R8C23_REGSET_TIMERB	(1)
BusDevice * R8C23_TimerABNew(const char *name,unsigned int regset);
