/*
 *************************************************************************************************
 *
 * Command interpreter for debugging language
 *
 * State: 
 *	nothing is working
 *
 * Copyright 2004 2008 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */

#include <interpreter.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <channel.h>
#include <filesystem.h>
#include "sgstring.h"

typedef struct Cmd {
	void *clientData;
	CmdProc *cmdProc;
	SHashEntry *hash_entry;
} Cmd;

struct CmdRegistry {
	int initialized;
	SHashTable cmdHash;
};
/* typedefed in header file */
#define MAXARGS (20)
struct Interp {
	Channel *chan;
	ChannelHandler rch;
	int delayed_execution;
	char *prompt;
	char *linebuf;
	int linebuf_size;
	Filesystem *filesys;
	AbortDelayedProc *abortProc;
	void *abortClientData;
};

static Cmd *Cmd_Find(const char *cmdname);
static CmdRegistry g_CmdRegistry;

void
Interp_AppendResult(Interp * interp, const char *format, ...)
{
	va_list ap;
	int size = strlen(format) * 3 + 512;
	int result;
	char *str = alloca(size);
	va_start(ap, format);
	result = vsnprintf(str, size, format, ap);
	va_end(ap);
	if (result > 0) {
		//fprintf(stderr,"The result has size %d\n",result);
		Channel_Write(interp->chan, str, result);
	}
}

void
Interp_SetAbortProc(Interp * interp, AbortDelayedProc * proc, void *clientData)
{
	interp->abortProc = proc;
	interp->abortClientData = clientData;
}

/*
 ******************************************************
 * Split a script into args 
 ******************************************************
 */
static int
split_args(char *script, int maxargc, char *argv[])
{
	int i, argc = 0;
	int state = 1;
	argv[0] = script;
	for (i = 0; script[i]; i++) {
		if ((script[i] == '\n') || (script[i] == '\r')) {
			script[i] = 0;
			break;
		}
		if ((state == 1) && (script[i] == ' ')) {
			continue;
		}
		if (state == 1) {
			state = 0;
			argv[argc++] = script + i;
			if (argc >= maxargc) {
				return argc;
			}
			continue;
		}
		if ((state == 0) && (script[i] == ' ')) {
			script[i] = 0;
			state = 1;
		}
	}
	return argc;
}

static void
Chan_Close(Interp * interp)
{
	/* Disconnect from interpreter */
	fprintf(stderr, "Closing CLI\n");
	if (interp->abortProc) {
		interp->abortProc(interp, interp->abortClientData);
	}
	Interp_Del(interp);
}

static void
Interp_Eval(Interp * interp, char *line)
{
	Cmd *cmd;
	int result;
	char *argv[MAXARGS];
	int argc;
	argc = split_args(line, MAXARGS, argv);
	if (argc < 1) {
		Channel_Write(interp->chan, interp->prompt, strlen(interp->prompt));
		return;
	}
	//fprintf(stderr,"Got %d bytes %d args for eval %s\n",count,argc,argv[0]);
	cmd = Cmd_Find(argv[0]);
	if (cmd && cmd->cmdProc) {
		result = cmd->cmdProc(interp, cmd->clientData, argc, argv);
		if (result == CMD_RESULT_QUIT) {
			return;
		} else if (result == CMD_RESULT_DELAYED) {
			interp->delayed_execution = 1;
		} else if (result == CMD_RESULT_BADARGS) {
			Interp_AppendResult(interp, "Bad arguments\r\n");
			Channel_Write(interp->chan, interp->prompt, strlen(interp->prompt));
		} else if (result != CMD_RESULT_OK) {
			Interp_AppendResult(interp, "Error %d\r\n", result);
			Channel_Write(interp->chan, interp->prompt, strlen(interp->prompt));
		} else {
			Channel_Write(interp->chan, interp->prompt, strlen(interp->prompt));
		}
	} else {
		Interp_AppendResult(interp, "Error: CMD %s not found\r\n", argv[0]);
		Channel_Write(interp->chan, interp->prompt, strlen(interp->prompt));
	}
}

/*
 ************************************************************+
 * Finish a delayed execution
 ************************************************************+
 */
void
Interp_FinishDelayed(Interp * interp, int cmd_retcode)
{
	interp->delayed_execution = 0;
	interp->abortProc = NULL;
	interp->abortClientData = NULL;
	if (cmd_retcode == CMD_RESULT_BADARGS) {
		Interp_AppendResult(interp, "Bad arguments\r\n");
	} else if (cmd_retcode != CMD_RESULT_OK) {
		Interp_AppendResult(interp, "Error %d\r\n", cmd_retcode);
	}
	Channel_Write(interp->chan, interp->prompt, strlen(interp->prompt));
}

/*
 ******************************************************
 * Receive the data from a channel
 * And send it to the interpreter for evaluation
 ******************************************************
 */
static void
Interp_Rx(void *clientData, int mask)
{
	Interp *interp = (Interp *) clientData;
	int readsize = interp->linebuf_size - 1;
	int count;
	count = Channel_Read(interp->chan, interp->linebuf, readsize);
	if (count < 0) {
		/* Repair this for the case of delayed execution !!!!!!!!!!!!!!!!!!!! */
		Chan_Close(interp);
		return;
	} else if (count == 0) {
		return;
	}
	/* Swallow it silently if it comes before interpreter has done the previous command */
	if (interp->delayed_execution) {
		return;
	}
	//fprintf(stderr,"Got %d bytes\n",count);
	interp->linebuf[count] = 0;
	/* Be careful: interpreter might be deleted by Quit after this command */
	Interp_Eval(interp, interp->linebuf);
}

#if 0
static void
print_dent(Interp * interp, Dirent * dent)
{
	char *basename = dent->name + strlen(dent->name) - 1;
	for (; basename >= dent->name; basename--) {
		if (*basename == '/') {
			basename++;
			break;
		}
	}
	Channel_Write(interp->chan, basename, strlen(basename));
	Channel_Write(interp->chan, "\r\n", 2);
}
#endif

#if 0
static int
Cmd_Ls(Interp * interp, void *clientData, int argc, char *argv[])
{
#if 0
	XY_HashEntry *entryPtr;
	Dirent *dent;
	Directory *dir;
	if (argc < 2) {
		/* Should list working directory */
		return CMD_RESULT_ERROR;
	}
	entryPtr = XY_FindHashEntry(&interp->fsHash, argv[1]);
	if (!entryPtr) {
		fprintf(stderr, "%s not found in fsHash\n", argv[1]);
		return CMD_RESULT_ERROR;
	}
	dent = XY_GetHashValue(entryPtr);
	if (dent->type == DENT_FILEOBJ) {
		print_dent(interp, dent);
		return 0;
	} else if (dent->type != DENT_DIR) {
		fprintf(stderr, "Bug unknown directory entry type %d\n", dent->type);
		exit(1);
	}
	dir = (Directory *) dent;
	for (dent = dir->firstChild; dent; dent = dent->next) {
		print_dent(interp, dent);
	}
#endif
	return CMD_RESULT_OK;
}

static void
Cmd_Delay(Interp * interp, void *clientData, int argc, char *argv[])
{

}
#endif
static int
Cmd_Quit(Interp * interp, void *clientData, int argc, char *argv[])
{
	Interp_Del(interp);
	return CMD_RESULT_QUIT;
}

/*
 *********************************************************************************
 * Constructor for debug Interpreter
 * Creates a Hashtable for commands 
 *********************************************************************************
 */
Interp *
Interp_New(Channel * chan)
{
	Interp *interp = sg_new(Interp);
	char *welcome = "\rWelcome to Softgun CLInterface\r\n";
	memset(interp, 0, sizeof(Interp));
	interp->linebuf_size = 1000;
	interp->linebuf = sg_calloc(interp->linebuf_size);
	interp->chan = chan;
	interp->prompt = "SG +> ";
	interp->filesys = FS_New();
	Channel_AddHandler(&interp->rch, interp->chan, CHAN_READABLE, Interp_Rx, interp);
	FS_CreateDir(interp->filesys, "/");
	FS_CreateDir(interp->filesys, "/bla");
	FS_CreateDir(interp->filesys, "/blub");
	FS_CreateDir(interp->filesys, "/blub/subblub");
	FS_CreateDir(interp->filesys, "/blub/subblub2");
	FS_CreateFile(interp->filesys, "/blub2");
	Channel_Write(interp->chan, welcome, strlen(welcome));
	Channel_Write(interp->chan, interp->prompt, strlen(interp->prompt));
	return interp;
}

void
Interp_Del(Interp * interp)
{
	Channel_RemoveHandler(&interp->rch);
	Channel_Close(interp->chan);
	FS_Del(interp->filesys);
	sg_free(interp);
}

static Cmd *
Cmd_Find(const char *cmdname)
{

	Cmd *cmd;
	CmdRegistry *reg = &g_CmdRegistry;
	if (!reg->initialized) {
		fprintf(stderr, "Cmd_Find: Searching in uninitialized Command hash\n");
		exit(1);
	}
	SHashEntry *entryPtr;
	entryPtr = SHash_FindEntry(&reg->cmdHash, cmdname);
	if (entryPtr) {
		cmd = SHash_GetValue(entryPtr);
		return cmd;
	} else {
		return NULL;
	}
}

#if 0
Cmd_Delall()
{
	do {
		hashEntry = SHash_FirstEntry(&interp->cmdHash, &search);
		if (hashEntry) {
			InterpCmd *cmd = SHash_GetValue(hashEntry);
			SHash_DeleteEntry(&interp->cmdHash, hashEntry);
			sg_free(cmd);
		}
	} while (hashEntry);
}
#endif

int
Cmd_Register(const char *cmdname, CmdProc * proc, void *clientData)
{
	Cmd *cmd;
	CmdRegistry *reg = &g_CmdRegistry;
	SHashEntry *entryPtr;
	if (!reg->initialized) {
		fprintf(stderr, "Cmd_Register: Registering cmd in uninitialized Command hash\n");
		exit(1);
	}
	entryPtr = SHash_CreateEntry(&reg->cmdHash, cmdname);
	if (!entryPtr) {
		fprintf(stderr, "Command already exists\n");
		return -1;
	}
	cmd = sg_new(Cmd);
	cmd->cmdProc = proc;
	cmd->hash_entry = entryPtr;
	SHash_SetValue(entryPtr, cmd);
	return 0;
}

/*
 *********************************************************************************
 * CmdRegistry_Init
 * 	Initialize the command registry hash table and add the quit command
 *********************************************************************************
 */
void
CmdRegistry_Init(void)
{
	CmdRegistry *reg = &g_CmdRegistry;
	if (reg->initialized) {
		fprintf(stderr, "Error: Re-Initializing CMD registry\n");
		exit(1);
	}
	reg->initialized = 1;
	SHash_InitTable(&reg->cmdHash);
	Cmd_Register("quit", Cmd_Quit, NULL);
	return;
}
