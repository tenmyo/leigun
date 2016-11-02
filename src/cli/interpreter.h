/*
 * ----------------------------------------------------
 *
 * Command interpreter for debugging language
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ----------------------------------------------------
 */

#ifndef _INTERPRETER_H
#define _INTERPRETER_H
#include <stdarg.h>
#include "channel.h"
#include "strhash.h"

#define CMD_RESULT_OK 	 	(0)
#define CMD_RESULT_QUIT		(2)
#define CMD_RESULT_DELAYED 	(3)
#define CMD_RESULT_ERROR	(-1)
#define CMD_RESULT_BADARGS	(-2)
#define CMD_RESULT_ABORT	(-3)
typedef struct Interp Interp;
typedef struct InterpCmd InterpCmd;

Interp *Interp_New(Channel * chan);
void Interp_Del(Interp * interp);

void Interp_AppendResult(Interp * interp, const char *format, ...);
/*
 ***************************************************************
 * Interp_FinishDelayed
 * 	Called by the cmd to tell the interpreter that a 
 *	delayed execution is finished
 ***************************************************************
 */
void Interp_FinishDelayed(Interp * interp, int cmd_retcode);
typedef void AbortDelayedProc(Interp * interp, void *clientData);
void Interp_SetAbortProc(Interp * interp, AbortDelayedProc * proc, void *clientData);

typedef struct CmdRegistry CmdRegistry;
void CmdRegistry_Init(void);

typedef int CmdProc(Interp * interp, void *clientData, int argc, char *argv[]);
int Cmd_Register(const char *cmdname, CmdProc *, void *clientData);

#endif
