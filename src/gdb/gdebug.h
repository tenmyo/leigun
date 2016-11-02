#ifndef _GDEBUG_H
#define _GDEBUG_H
#include "debugger.h"
Debugger * GdbServer_New(DebugBackendOps *,void *backend);
#endif
