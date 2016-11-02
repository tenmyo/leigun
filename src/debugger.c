#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "debugger.h"
#include "gdebug.h"

/**
 **************************************************************************
 * \fn Debugger * Debugger_New(DebugBackendOps *ops,void *backend);
 * Create a new debugger instance.
 * Currently I have no choice but gdb, so I always choose the gdbserver.
 **************************************************************************
 */
Debugger *
Debugger_New(DebugBackendOps *ops,void *backend)
{
	return GdbServer_New(ops,backend);
}
