/*
 *************************************************************************************************
 *
 * Interface between emulator and gdb using the gdb 
 * remote protocol
 *
 * State:
 *	minimum required set of operations are working	
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
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
// include self header
#include "compiler_extensions.h"
#include "gdebug.h"

// include system header
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

// include library header

// include user header
#include "configfile.h"
#include "sgstring.h"
#include "core/asyncmanager.h"

#if 1
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define CMDBUF_SIZE (512)

typedef struct BreakPoint {
	uint64_t addr;
	int len;
	int type;
	uint8_t backup[8];
	struct BreakPoint *next;
} BreakPoint;

typedef struct GdbServer GdbServer;
typedef struct GdbSession {
	int rfh_is_active;
	DebugBackendOps *dbgops;
	GdbServer *gserv;
	StreamHandle_t *handle;

	char cmdbuf[CMDBUF_SIZE];
	int cmdstate;
	int cmdbuf_wp;
	uint8_t csum;
	void *backend;
	BreakPoint *bkpt_head;
	struct GdbSession *next;
	int last_sig;
} GdbSession;

struct GdbServer {
	Debugger debugger;
	DebugBackendOps *dbgops;
	void *backend;
	GdbSession *first_gsess;
};

#define CMDSTATE_WAIT_START (0)
#define CMDSTATE_WAIT_DATA	(1)
#define CMDSTATE_WAIT_CSUM1	(2)
#define CMDSTATE_WAIT_CSUM2	(3)

__attribute__ ((__unused__))
static inline void
print_timestamp(char *str)
{
#if 0
	struct timeval tv;
	gettimeofday(&tv, NULL);
	fprintf(stderr, "%s: %d\n", str, tv.tv_usec);
#endif
}

static int gsess_reply(GdbSession *, const char *format, ...);	//  __attribute__ ((format (printf, 2, 3)));;

static void
writed(int status, StreamHandle_t *handle, void *clientdata)
{
	free(clientdata);
}

/*
 * ----------------------------------------
 * Assemble a reply using varargs/vsnprintf
 * ----------------------------------------
 */

static int
gsess_reply(GdbSession * gsess, const char *format, ...)
{
	va_list ap;
	char *reply = malloc(1024);
	uint8_t chksum = 0;
	int count;
	int i;
	count = sprintf(reply, "$");
	va_start(ap, format);
	count += vsnprintf(reply + count, sizeof(reply) - count - 4, format, ap);
	va_end(ap);
	dbgprintf("Reply \"%s\"\n", reply);
	for (i = 1; i < count; i++) {
		chksum += reply[i];
	}
	count += sprintf(reply + count, "#%02x", chksum);
	AsyncManager_Write(gsess->handle, reply, count, &writed, reply);
	return 0;
}

/*
 * ---------------------------------------------------
 * Find a breakpoint by address/length pair
 * ---------------------------------------------------
 */
static BreakPoint *
find_breakpoint(GdbSession * gsess, uint64_t addr, int len)
{
	BreakPoint *cursor;
	for (cursor = gsess->bkpt_head; cursor; cursor = cursor->next) {
		if ((cursor->addr == addr) && (cursor->len == len)) {
			return cursor;
		}
	}
	return NULL;
}

/*
 * ---------------------------------------------------------------------------------
 * Unlink a breakpoint identified by a address/length pair from the linked list
 * ---------------------------------------------------------------------------------
 */
static BreakPoint *
unlink_breakpoint(GdbSession * gsess, uint64_t addr, int len)
{
	BreakPoint *cursor, *prev;
	for (prev = NULL, cursor = gsess->bkpt_head; cursor; prev = cursor, cursor = cursor->next) {
		if ((cursor->addr == addr) && (cursor->len == len)) {
			if (prev) {
				prev->next = cursor->next;
			} else {
				gsess->bkpt_head = cursor->next;
			}
			return cursor;
		}
	}
	return NULL;
}

/*
 * ------------------------------------------------------
 * delete_breakpoints
 * 	Delete all breakpoints belonging to a session
 * ------------------------------------------------------
 */
static void
delete_breakpoints(GdbSession * gsess)
{
	BreakPoint *cursor;
	DebugBackendOps *dbgops = gsess->dbgops;
	if (!dbgops->setmem) {
		return;
	}
	while (gsess->bkpt_head) {
		cursor = gsess->bkpt_head;
		dbgops->setmem(gsess->backend, cursor->backup, cursor->addr, cursor->len);
		gsess->bkpt_head = cursor->next;
		free(cursor);

	}
}

static void free_gsess(Handle_t *handle, void *gsess)
{
	GdbServer *gserv = ((GdbSession *)gsess)->gserv;
	if (gserv->first_gsess == gsess) {
		gserv->first_gsess = NULL;
	}
	delete_breakpoints(gsess);
	free(gsess);
}

/*
 * --------------------------------------------
 * gsess_terminate
 * 	Terminate a gdb session
 * --------------------------------------------
 */
static void
gsess_terminate(GdbSession * gsess)
{
	if (gsess->rfh_is_active) {
		gsess->rfh_is_active = 0;
		dbgprintf("remove the filehandler\n");
		AsyncManager_ReadStop(gsess->handle);
	}
	AsyncManager_Close((Handle_t *)gsess->handle, &free_gsess, gsess);
	dbgprintf("close %p\n", gsess->handle);
}

#define MAXREGS (40)
static void
gsess_getregs(GdbSession * gsess)
{
	char reply[MAXREGS * 64 + 1];
	int i, j;
	int len;
	int bytecnt;
	DebugBackendOps *dbgops = gsess->dbgops;
	if (!dbgops->getreg) {
		gsess_reply(gsess, "00000000");
	} else {
		bytecnt = 0;
		for (i = 0; i < MAXREGS; i++) {
			uint8_t value[32];
			len = dbgops->getreg(gsess->backend, value, i, 32);
			if (len <= 0) {
				break;
			}
			for (j = 0; j < len; j++) {
				bytecnt += sprintf(reply + bytecnt, "%02x", value[j]);
			}
		}
		*(reply + bytecnt) = 0;
		dbgprintf("Got %d registers, byted %d, %s\n", i, bytecnt, reply);
		gsess_reply(gsess, reply);
	}
}

/*
 **************************************************************************
 * \fn static void gsess_getreg(GdbSession * gsess, int index)
 * Read a register from the target.
 **************************************************************************
 */
static void
gsess_getreg(GdbSession * gsess, int index)
{
	char reply[65];
	int j;
	int len;
	int bytecnt;
	DebugBackendOps *dbgops = gsess->dbgops;
	if (!dbgops->getreg) {
		gsess_reply(gsess, "00000000");
	} else {
		uint8_t value[32];
		bytecnt = 0;
		len = dbgops->getreg(gsess->backend, value, index, 32);
		if (len <= 0) {
			fprintf(stderr, "GDEBUG: Can not get: R%d\n", index);

		}
		for (j = 0; j < len; j++) {
			bytecnt += sprintf(reply + bytecnt, "%02x", value[j]);
		}
		*(reply + bytecnt) = 0;
		dbgprintf("GDEBUG: R%d, %s\n", index, reply);
		gsess_reply(gsess, reply);
	}
}

static void
gsess_getmem(GdbSession * gsess, uint64_t addr, uint32_t len)
{
	char reply[1024];
	int writep = 0;
	uint8_t buf[32];
	int i;
	int result;
	DebugBackendOps *dbgops = gsess->dbgops;
	if (len > 256) {
		len = 256;
	}
	if (!dbgops->getmem) {
		gsess_reply(gsess, "00000000");
	} else {
		while (len >= 4) {
			result = dbgops->getmem(gsess->backend, buf, addr, 4);
			if (result > 0) {
				len -= result;
				for (i = 0; i < result; i++) {
					writep += sprintf(reply + writep, "%02x", buf[i]);
				}
			}
			if (result < 4) {
				break;
			}
			addr += 4;
		}
		while (len > 0) {
			result = dbgops->getmem(gsess->backend, buf, addr, 1);
			if (result > 0) {
				len -= result;
				for (i = 0; i < result; i++) {
					writep += sprintf(reply + writep, "%02x", buf[i]);
				}
			} else {
				break;
			}
			addr += 1;
		}
		reply[writep] = 0;
		dbgprintf("count %d gm reply \"%s\"\n", writep, reply);
		gsess_reply(gsess, reply);
	}
}

static void
gsess_getstatus(GdbSession * gsess)
{
	int result;
	DebugBackendOps *dbgops = gsess->dbgops;
	if (!dbgops->get_status) {
		//gsess_reply(gsess, "S00");
		gsess_reply(gsess, "T00thread:0;");
	} else {
		result = dbgops->get_status(gsess->backend);
		if (result <= DbgStat_GDBEND) {
			if(gsess->last_sig >= 0) {
				//gsess_reply(gsess, "S%02x", gsess->last_sig);
				gsess_reply(gsess, "T%02xthread:0;", gsess->last_sig);
				gsess->last_sig = -1;
			} else {
				//gsess_reply(gsess, "S%02x", result);
				gsess_reply(gsess, "T%02xthread:0;", result);
			}
		} else if (result == DbgStat_RUNNING) {
			gsess_reply(gsess, "OK");
		}
	}
}

static void
gsess_stop(GdbSession * gsess,int sig)
{
	int result;
	DebugBackendOps *dbgops = gsess->dbgops;
	if (!dbgops->stop) {
		gsess_reply(gsess, "S00");
	} else {
		result = dbgops->stop(gsess->backend);
		if (result >= 0) {
			gsess_reply(gsess, "S%02x", sig);
		} else {
			/* delayed reply */
			gsess->last_sig = sig;
		}
	}
}

/*
 * ------------------------------------------
 * GdbServer_NotifyStatus
 *	Tell the gdb about some event
 * 	sig is a unix signal number 
 *	SIG_INT if stopped
 * ------------------------------------------
 */
int
GdbServer_Notify(void *_gserv, Dbg_TargetStat sig)
{
	GdbServer *gserv = (GdbServer *) _gserv;
	GdbSession *gsess = gserv->first_gsess;
	if (!gsess) {
		return 0;
	}
	fprintf(stderr,"Last sig is %d\n",gsess->last_sig);
	if(gsess->last_sig >= 0) {
		gsess_reply(gsess, "T%02xthread:0;", gsess->last_sig);
		gsess->last_sig = -1;
	} else {
		gsess_reply(gsess, "T%02xthread:0;", sig);
	}
	return 1;
}

static void
gsess_cont(GdbSession * gsess)
{
	int result;
	DebugBackendOps *dbgops = gsess->dbgops;
	if (!dbgops->cont) {

	} else {
		result = dbgops->cont(gsess->backend);
		if (result < 0) {
			gsess_reply(gsess, "S%02x", -result);
		}
	}
}

static void
gsess_step(GdbSession * gsess, uint32_t addr, int use_addr)
{
	DebugBackendOps *dbgops = gsess->dbgops;
	int result;
	if (!dbgops->step) {
		gsess_reply(gsess, "S00");
	} else {
		result = dbgops->step(gsess->backend, addr, use_addr);
		if (result >= 0) {
			gsess_reply(gsess, "S%02x", result);
		} else if (result == DbgStat_OK) {
			gsess_reply(gsess, "OK");
		}
	}
}

static int
hexparse(const char *str, uint8_t * retval, int maxbytes)
{
	int i, nibble;
	uint8_t val;
	for (i = 0; i < maxbytes; i++) {
		for (nibble = 0; nibble < 2; nibble++) {
			char c = *str;
			if ((c >= '0') && (c <= '9')) {
				val = (c - '0');
			} else if ((c >= 'A') && (c <= 'F')) {
				val = (c - 'A' + 10);
			} else if (c >= 'a' && c <= 'f') {
				val = (c - 'a' + 10);
			} else {
				return i;
			}
			if (nibble) {
				retval[i] = (retval[i] & ~0xf) | val;
			} else {
				retval[i] = (retval[i] & ~0xf0) | (val << 4);
			}
			str++;
		}
	}
	return maxbytes;
}

static void
gsess_setmem(GdbSession * gsess, char *data, int maxlen)
{
	uint32_t addr;
	uint32_t len;
	int readlen;
	uint32_t readp = 0;
	DebugBackendOps *dbgops = gsess->dbgops;
	if (!dbgops->setmem) {
		gsess_reply(gsess, "E00");
		return;
	}
	if (sscanf(data, "%x,%x:", &addr, &len) != 2) {
		gsess_reply(gsess, "E00");
		return;
	}
	while (readp < maxlen) {
		if (data[readp++] == ':') {
			break;
		}
	}
	if ((readp + 2 * len) > maxlen) {
		gsess_reply(gsess, "E00");
		return;
	}
	while (len) {
		uint8_t value[4];
		readlen = (len >= 4) ? 4 : len;
		if (hexparse(data + readp, value, readlen) <= 0) {
			fprintf(stderr, "setmem: Parse hex string %s failed\n", data + readp);
			gsess_reply(gsess, "E00");
			return;
		}
		fprintf(stderr, "setmem value %02x, addr %08x\n", value[0], addr);
		dbgops->setmem(gsess->backend, value, addr, readlen);
		len -= readlen;
		readp += readlen;
	}
	gsess_reply(gsess, "OK");

}

static void
gsess_setreg(GdbSession * gsess, char *data)
{
	DebugBackendOps *dbgops = gsess->dbgops;
	int reg;
	uint8_t value[8];
	int count;
	if (!dbgops->setreg) {
		gsess_reply(gsess, "E00");
		return;
	}
	if (sscanf(data, "%x=", &reg) != 1) {
		gsess_reply(gsess, "E00");	// fix the error number 
		return;
	}
	while (*data != '=') {
		data++;
	}
	data++;
	if ((count = hexparse(data, value, 8)) < 1) {
		gsess_reply(gsess, "E00");
		return;
	}
	dbgops->setreg(gsess->backend, value, reg, count);
	gsess_reply(gsess, "OK");
}

/*
 * -----------------------------------------------------------------------
 * add break- and watchpoints
 * Z0 set memory breakpoint
 * Z1 set hardware breakpoint
 * Z2 Insert a write watchpoint
 * Z3 Insert a read watchpoint
 * Z4 Insert an access watchpoint
 * -----------------------------------------------------------------------
 */
static void
gsess_add_breakpoint(GdbSession * gsess, char *cmd, int cmdlen)
{
	DebugBackendOps *dbgops = gsess->dbgops;
	BreakPoint *bkpt;
	int type;
	uint32_t addr;
	uint8_t bkpt_ins[8];
	unsigned int len;
	if (sscanf(cmd, "Z%d,%x,%x", &type, &addr, &len) != 3) {
		fprintf(stderr, "gdebug: parse error cmd \"%s\"\n", cmd);
		gsess_reply(gsess, "E00");
		return;
	}
	if (len > 8) {
		fprintf(stderr, "gdebug: bkpt instruction to long (%d)\n", len);
		gsess_reply(gsess, "E00");
		return;
	}
	if (!dbgops->get_bkpt_ins || !dbgops->setmem || !dbgops->getmem) {
		fprintf(stderr, "gdebug backend does not support breakpoints\n");
		gsess_reply(gsess, "");
		return;
	}
	dbgops->get_bkpt_ins(gsess->backend, bkpt_ins, addr, len);
	fprintf(stderr, "Insert bkpt %x at %08x, len %d\n", bkpt_ins[0], addr, len);
	switch (type) {
	    case 0:
		    /* The software breakpoint */
		    bkpt = find_breakpoint(gsess, addr, len);
		    if (bkpt) {
			    fprintf(stderr, "Breakpoint already exists\n");
			    gsess_reply(gsess, "E00");
			    return;
		    }
		    bkpt = sg_new(BreakPoint);
		    bkpt->addr = addr;
		    bkpt->len = len;
		    bkpt->next = gsess->bkpt_head;
		    bkpt->type = type;
		    gsess->bkpt_head = bkpt;
		    dbgops->getmem(gsess->backend, bkpt->backup, addr, len);
		    dbgops->setmem(gsess->backend, bkpt_ins, addr, len);
		    gsess_reply(gsess, "OK");
		    break;
	    case 1:
		    /* The hardware breakpoint */
	    default:
		    fprintf(stderr,
			    "softgun gdb interface does not support breakpoint type %d\n", type);
		    fprintf(stderr, "cmd %s\n", cmd);
		    gsess_reply(gsess, "");
		    break;
	}
}

/*
 * -------------------------------------------------------------------------
 * remove break- and watchpoints
 * z0 ... z4
 * -------------------------------------------------------------------------
 */
static void
gsess_remove_breakpoint(GdbSession * gsess, char *cmd, int cmdlen)
{
	DebugBackendOps *dbgops = gsess->dbgops;
	BreakPoint *bkpt;
	int type;
	uint32_t addr;
	unsigned int len;
	if (sscanf(cmd, "z%d,%x,%x", &type, &addr, &len) != 3) {
		fprintf(stderr, "gdebug: parse error cmd \"%s\"\n", cmd);
		gsess_reply(gsess, "E00");
		return;
	}
	if (len > 8) {
		fprintf(stderr, "gdebug: bkpt instruction to long (%d)\n", len);
		gsess_reply(gsess, "E00");
		return;
	}
	if (!dbgops->setmem) {
		fprintf(stderr, "gdebug backend does not support breakpoints\n");
		gsess_reply(gsess, "");
		return;
	}
	switch (type) {
	    case 0:
		    bkpt = unlink_breakpoint(gsess, addr, len);
		    if (!bkpt) {
			    fprintf(stderr, "Removing nonexistent breakpoint\n");
			    gsess_reply(gsess, "E00");
			    return;
		    }
		    dbgops->setmem(gsess->backend, bkpt->backup, addr, len);
		    free(bkpt);
		    fprintf(stderr, "remove breakpoint %s\n", cmd);
		    gsess_reply(gsess, "OK");
		    break;

	    case 1:
	    default:
		    fprintf(stderr,
			    "softgun gdb interface does not support breakpoints of type %d\n",
			    type);
		    fprintf(stderr, "cmd %s\n", cmd);
		    gsess_reply(gsess, "");
		    break;
	}
}

static void
gsess_vcont(GdbSession *gsess,char *cmd)
{
	int i;
	for(i = 5; cmd[i]; i++) {
		if(cmd[i] == 'c') {
			fprintf(stderr,"Continue\n");	
			gsess_reply(gsess, "OK");
			gsess_cont(gsess);
		} else if(cmd[i] == 's') {
			fprintf(stderr,"Step\n");	
			gsess_reply(gsess, "OK");
			gsess_step(gsess,0,0);
		} else if(cmd[i] == 't') {
			fprintf(stderr,"Stop\n");	
			gsess_reply(gsess, "OK");
			gsess_stop(gsess,0); /* GDB docu says stopped by signal 0 */
		} else if(cmd[i] == ';') {
		} else {
			fprintf(stderr,"Unexpected char 0x%02x in vcont\n",cmd[i]);
		}	
	}	
}
/**
 ************************************************************************
 * Here are the commands longer than one character
 ************************************************************************
 */
static void
gsess_long_cmd(GdbSession * gsess)
{
	char *cmd = gsess->cmdbuf;
	if (strncmp(cmd, "qSupported", 10) == 0) {
		gsess_reply(gsess, "QNonStop+");
	} else if (strncmp(cmd, "QNonStop:", 9) == 0) {
		/* Just ack it */
		gsess_reply(gsess, "OK");
	} else if (strncmp(cmd, "vStopped", 8) == 0) {
		gsess_reply(gsess, "OK");
	} else if (strncmp(cmd, "qC", 2) == 0) {
		gsess_reply(gsess, "QC0");
	} else if (strncmp(cmd, "qfThreadInfo", 12) == 0) {
		gsess_reply(gsess, "m0");
	} else if (strncmp(cmd, "qsThreadInfo",12) == 0) {
		gsess_reply(gsess, "l");
	} else if (strncmp(cmd, "vCont?", 6) == 0) {
		/* Continue Step, Stop */
		gsess_reply(gsess, "vCont;c;s;t"); /* vCont;[action;action...] */
	} else if (strncmp(cmd, "vCont", 5) == 0) {
		gsess_vcont(gsess,cmd);
	} else if (strncmp(cmd, "qAttached", 9) == 0) {
		gsess_reply(gsess, "1"); 
	} else if (strncmp(cmd, "qTStatus", 8) == 0) {
		gsess_reply(gsess,"T0");
	} else if (strncmp(cmd, "qXfer:features:read:target.xml", 30) == 0) {
		AsyncManager_Write(gsess->handle, "$#00", 4, NULL, NULL);
	} else {
		dbgprintf("unknown cmd \'%s\'\n", cmd);
		AsyncManager_Write(gsess->handle, "$#00", 4, NULL, NULL);
	}
}

/*
 * -------------------------------------------------------
 * gsess_execute_cmd
 * 	The interpreter for commands from gdb 
 * -------------------------------------------------------
 */
static int
gsess_execute_cmd(GdbSession * gsess)
{
	char *cmd = gsess->cmdbuf;
	int result = 0;
	//printf("The gdb command is executed %s\n",cmd);       
	switch (cmd[0]) {
		    /* Status query */
	    case '?':
		    gsess_getstatus(gsess);
		    break;

	    case 's':
		    {
			    uint32_t addr;
			    if (sscanf(cmd + 1, "%x", &addr) == 1) {
				    gsess_step(gsess, addr, 1);
			    } else {
				    gsess_step(gsess, addr, 0);
			    }
		    }
		    break;

		    /* continue: no reply until stopped again */
	    case 'c':
		    gsess_cont(gsess);
		    break;

	    case 'D':
		    gsess_cont(gsess);
		    result = -1;
		    break;

	    case 'p':
		    {
			    unsigned int reg;
			    if (sscanf(cmd + 1, "%02x#", &reg) == 1) {
				    gsess_getreg(gsess, reg);
			    }
		    }
		    break;

	    case 'P':
		    gsess_setreg(gsess, cmd + 1);
		    break;

		    /* Get registers */
	    case 'g':
		    gsess_getregs(gsess);
		    break;

		    /* Set registers */
	    case 'G':
		    gsess_reply(gsess, "OK");
		    break;

		    /* Read memory */
	    case 'm':
		    {
			    uint32_t addr;
			    uint32_t len;
			    sscanf(cmd + 1, "%x,%x", &addr, &len);
			    gsess_getmem(gsess, addr, len);
		    }
		    break;

		    /* Write memory */
	    case 'M':
		    gsess_setmem(gsess, cmd + 1, gsess->cmdbuf_wp - 1);
		    break;

		    /* Insert/Remove Breakpoints */
	    case 'z':
		    gsess_remove_breakpoint(gsess, cmd, gsess->cmdbuf_wp);
		    break;
	    case 'Z':
		    gsess_add_breakpoint(gsess, cmd, gsess->cmdbuf_wp);
		    break;

	    default:
		    gsess_long_cmd(gsess);
		    break;
	}
	return result;
}

/*
 * -------------------------------------------------------------------
 * feed_state_machine
 * 	The data sink for characters from gdb
 * -------------------------------------------------------------------
 */
static int
feed_state_machine(GdbSession * gsess, uint8_t c)
{
	int result = 0;
	dbgprintf("%c", c);
	switch (gsess->cmdstate) {
	    case CMDSTATE_WAIT_START:
		    if (c == '$') {
			    gsess->cmdbuf_wp = 0;
			    gsess->cmdstate = CMDSTATE_WAIT_DATA;
			    gsess->csum = 0;
		    } else if (c == '+') {
			    dbgprintf("Got acknowledge\n");
		    } else if (c == '-') {
			    dbgprintf("Got NACK\n");
		    } else if (c == 0x03) {
			    gsess_stop(gsess,2); // FIXME: SIGINT(2)
			    fprintf(stderr, "received abort\n");
			    dbgprintf("received abort\n");
		    } else {
			    dbgprintf("received unknown byte %02x in START state\n", c);
		    }
		    break;

	    case CMDSTATE_WAIT_DATA:
		    if (c == '#') {
			    gsess->cmdstate = CMDSTATE_WAIT_CSUM1;
			    /* Terminate the string with 0 */
			    gsess->cmdbuf[gsess->cmdbuf_wp] = 0;
		    } else {
			    if (gsess->cmdbuf_wp >= CMDBUF_SIZE) {
				    fprintf(stderr, "Message from gdb to long, ignoring\n");
			    } else {
				    gsess->cmdbuf[gsess->cmdbuf_wp++] = c;
				    gsess->csum += c;
			    }
		    }
		    break;

	    case CMDSTATE_WAIT_CSUM1:
		    if ((c >= '0') && (c <= '9')) {
			    gsess->csum ^= (c - '0') << 4;
			    gsess->cmdstate = CMDSTATE_WAIT_CSUM2;
		    } else if ((c >= 'a') && (c <= 'f')) {
			    gsess->csum ^= (c - 'a' + 10) << 4;
			    gsess->cmdstate = CMDSTATE_WAIT_CSUM2;
		    } else if ((c >= 'A') && (c <= 'F')) {
			    gsess->csum ^= (c - 'A' + 10) << 4;
			    gsess->cmdstate = CMDSTATE_WAIT_CSUM2;
		    } else {
			    gsess->cmdstate = CMDSTATE_WAIT_START;
			    fprintf(stderr, "illegal byte %02x in chksum\n", c);
		    }
		    break;

	    case CMDSTATE_WAIT_CSUM2:
		    gsess->cmdstate = CMDSTATE_WAIT_START;
		    if ((c >= '0') && (c <= '9')) {
			    gsess->csum ^= (c - '0');
		    } else if ((c >= 'a') && (c <= 'f')) {
			    gsess->csum ^= (c - 'a' + 10);
		    } else if ((c >= 'A') && (c <= 'F')) {
			    gsess->csum ^= (c - 'A' + 10);
		    } else {
			    fprintf(stderr, "illegal byte %02x in chksum\n", c);
			    break;
		    }
		    dbgprintf("\nreceived complete command, csum %02x\n", gsess->csum);
		    if (gsess->csum == 0) {
			    print_timestamp("Input");
			    AsyncManager_Write(gsess->handle, "+", 1, NULL, NULL);
			    result = gsess_execute_cmd(gsess);
		    } else {
			    fprintf(stderr, "Checksum error in gdb packet\n");
          AsyncManager_Write(gsess->handle, "-", 1, NULL, NULL);
		    }
		    break;
		    break;
	}
	return result;
}

/*
 * ---------------------------------------------------------
 * gsess_input
 * 	The data sink for commands from gdb. It is called
 *	when filedescriptor is readable
 * ---------------------------------------------------------
 */

static void
gsess_input(StreamHandle_t *handle, const void *buf, signed long len, void *clientdata)
{
	signed long i;
	const uint8_t *p = buf;
	GdbSession *gsess = clientdata;
	if (len <= 0) {
		dbgprintf("Connection lost\n");
		gsess_terminate(gsess);
		return;
	}
	for (i = 0; i < len; i++) {
		if (feed_state_machine(gsess, p[i]) < 0) {
			gsess_terminate(gsess);
			return;
		}
	}
}

/*
 * -----------------------------------------------------------
 *  gserv_accept
 * 	Accept a new connection from on a network socket
 * -----------------------------------------------------------
 */

static void
gserv_accept(int status, StreamHandle_t *handle, const char *host, int port, void *clientdata)
{
	GdbSession *gsess;
	GdbServer *gserv = (GdbServer *)clientdata;
	if (gserv->first_gsess) {
		fprintf(stderr, "Only one gdb session allowed\n");
		AsyncManager_Close((Handle_t *)handle, NULL, NULL);
		return;
	}
	gsess = sg_new(GdbSession);
	gserv->first_gsess = gsess;
	gsess->gserv = gserv;
	gsess->handle = handle;
	gsess->cmdstate = CMDSTATE_WAIT_START;
	gsess->dbgops = gserv->dbgops;
	gsess->backend = gserv->backend;
	gsess->last_sig = -1;
	gsess->rfh_is_active = 1;
	AsyncManager_ReadStart(handle, &gsess_input, gsess);
	dbgprintf("Accepted connection for %s port %d\n", host, port);
}

/**
 ***********************************************************************
 * Create a new gdbserver
 ***********************************************************************
 */
Debugger *
GdbServer_New(DebugBackendOps * dbgops, void *backend)
{
	int result;
	int port;
	GdbServer *gserv;
	Debugger *debugger;
	char *host = Config_ReadVar("gdebug", "host");
	if (!host || (Config_ReadInt32(&port, "gdebug", "port") < 0)) {
		fprintf(stderr, "GDB server is not configured\n");
		return NULL;
	}
	gserv = sg_new(GdbServer);
	debugger = &gserv->debugger;
	debugger->implementor = gserv;
	debugger->notifyStatus = GdbServer_Notify;
	gserv->dbgops = dbgops;
	gserv->backend = backend;
	result = AsyncManager_InitTcpServer(host, port, 5, 1, &gserv_accept, gserv);
	if (result < 0) {
		sg_free(gserv);
		return NULL;
	}
	fprintf(stderr, "GDB server listening on host \"%s\" port %d\n", host, port);
	return debugger;
}
