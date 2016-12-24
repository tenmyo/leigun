/*
 *************************************************************************************************
 * cliserver: Accept data from a terminal and feed it
 *            into an interpreter 
 *
 * Copyright 2004 2005 2008 Jochen Karrer. All rights reserved.
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

#include <fio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <configfile.h>
#include "telnetd.h"
#include "interpreter.h"
#include "cliserver.h"
#include "sgstring.h"

#if 1
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define MAX_LINELEN (1000)

struct CliServer {
	TelnetServer *tserv;
};

static void
CliSess_Accept(void *clientData, Channel * chan, char *hostName, int port)
{
	Interp_New(chan);
}

CliServer *
CliServer_New(const char *name)
{
	CliServer *cserv = sg_new(CliServer);
	char *host = Config_ReadVar(name, "host");
	int port;
	if (!cserv) {
		fprintf(stderr, "Out of memory for debugger\n");
		return NULL;
	}
	if (Config_ReadInt32(&port, name, "port") < 0) {
		fprintf(stderr, "CLI not configured\n");
		return NULL;
	}
	if (!host) {
		fprintf(stderr, "No host for TCP server of CLI\n");
		return NULL;
	}
	cserv->tserv = TelnetServer_New(host, port, CliSess_Accept, cserv);
	return cserv;
}
