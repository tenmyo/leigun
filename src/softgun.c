/*
 **************************************************************************************************
 * Softgun Main Programm
 *	Create the machine mentioned in configfile and run the
 *	emulator	
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include "compiler_extensions.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>


#include "signode.h"
#include "clock.h"
#include "loader.h"
#include "configfile.h"
#include "boards/boards.h"
#include "version.h"
#include "sgstring.h"
#include "sglib.h"
#include "crc16.h"
#include "fio.h"
#ifndef NO_DEBUGGER
#include "debugvars.h"
#include "debugger.h"
#  include "cli/cliserver.h"
//#  include "cli/interpreter.h"
#  include "web/webserv.h"
#endif
#ifndef NO_SHLIB
#  include "shlib.h"
#endif
#ifdef __unix__
#  include "senseless.h"
#endif

typedef struct LoadChainEntry {
	struct LoadChainEntry *next;
	char *filename;
	char *addr_string;
	uint32_t addr;
	uint32_t region_size;	/* 0 = unlimited */
} LoadChainEntry;

static char *configfpath = NULL;
static char *configname = "defaultboard";
static LoadChainEntry *loadChainHead = NULL;

static void
LoadChain_Append(const char *addr_string, const char *filename)
{
	struct stat stat_buf;
	LoadChainEntry *lce = sg_new(LoadChainEntry);
	if (!addr_string) {
		addr_string = "flash";
	}
	fprintf(stderr, "LCA \"%s\" \"%s\"\n", addr_string, filename);
	if (stat(filename, &stat_buf) < 0) {
		fprintf(stderr, "stat on file \"%s\" failed ", filename);
		perror("");
		exit(1);
	}
	lce->filename = sg_strdup(filename);
	lce->addr_string = sg_strdup(addr_string);
	if (!loadChainHead) {
		lce->next = NULL;
		loadChainHead = lce;
	} else {
		LoadChainEntry *last = loadChainHead;
		while (last->next) {
			last = last->next;
		}
		lce->next = NULL;
		last->next = lce;
	}

}

/*
 * ----------------------------------------------
 * Resolve the addresses 
 * ----------------------------------------------
 */
static void
LoadChain_Resolve(void)
{
	/* now decode the address */
	LoadChainEntry *lce;
	for (lce = loadChainHead; lce; lce = lce->next) {
		if (sscanf(lce->addr_string, "0x%x", &lce->addr) == 1) {
			lce->region_size = 0;
		} else if (sscanf(lce->addr_string, "%u", &lce->addr) == 1) {
			lce->region_size = 0;
		} else {
			char *dest = Config_ReadVar("regions", lce->addr_string);
			int n;
			if (!dest) {
				fprintf(stderr, "Load destination \"%s\" not understood\n",
					lce->addr_string);
				exit(1);
			}
			n = sscanf(dest, "0x%x 0x%x", &lce->addr, &lce->region_size);
			if (n == 0) {
				fprintf(stderr, "Bad address \"%s\" for \"%s\" in configfile\n",
					dest, lce->addr_string);
				fprintf(stderr, "Should be %s: <hexaddr> ?hex-maxsize?\n",
					lce->addr_string);
				exit(1);
			}
		}
	}
}

static int
LoadChain_Load(void)
{
	LoadChainEntry *lce;
	for (lce = loadChainHead; lce; lce = lce->next) {
		if (Load_AutoType(lce->filename, lce->addr, lce->region_size) < 0) {
			return -1;
		}
	}
	return 0;
}

static void
help()
{
	fprintf(stderr, "\nThis is %s first compiled on %s %s\n", softgun_version, __DATE__,
		__TIME__);
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\tsoftgun [options] ?configuration_name?\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-l <loadaddr | region> <file>:  Load a file to address or region\n");
	fprintf(stderr, "-c <configfile_path>:           Use alternate configfile\n");
	fprintf(stderr, "-g <startaddr>:                 Use non default startaddress\n");
	fprintf(stderr,
		"-d                              Debug: Do not start. Wait for gdb connection\n");
	fprintf(stderr, "\n");
}

static int
read_configfile()
{
	char *str;
	const char *homepath;

	if (configfpath) {
		if (Config_ReadFile(configfpath) >= 0) {
			return 0;
		} else {
			fprintf(stderr, "Can not read configfile %s\n", configfpath);
			exit(3255);
		}
	}
#ifdef __unix__
	homepath = getenv("HOME");
#else
	homepath = getenv("HOMEPATH");
#endif
	str = (char *)alloca(100 + strlen(homepath) + strlen(configname));
	sprintf(str, "%s/.softgun/%s.sg", homepath, configname);
	if (Config_ReadFile(str) >= 0) {
		return 0;
	}
	fprintf(stderr, "Error: Can Not read configuration file %s\n", str);
	exit(1843);
}

static void
parse_commandline(int argc, char *argv[])
{
	char confstr[100];
	while (argc) {
		if (argv[0][0] == '-') {
			switch (argv[0][1]) {
			    case 'c':
				    if (argc > 1) {
					    configfpath = argv[1];
					    argc--;
					    argv++;
				    } else {
					    fprintf(stderr, "Missing name of configuration file\n");
					    exit(74530);
				    }
				    break;

			    case 'l':
				    if ((argc > 2) && (argv[1][0] != '-') && (argv[2][0] != '-')) {
					    LoadChain_Append(argv[1], argv[2]);
					    argc -= 2;
					    argv += 2;
#if 1
				    } else if ((argc > 1)) {
					    LoadChain_Append(NULL, argv[1]);
					    argc -= 1;
					    argv += 1;
#endif
				    } else {
					    fprintf(stderr, "Missing argument\n");
					    help();
					    exit(245);
				    }
				    break;

			    case 'd':
				    Config_AddString("\n[global]\ndbgwait: 1\n");
				    break;

			    case 'g':
				    if (argc > 1) {
					    snprintf(confstr, 100,
						     "\n[global]\nstart_address: %s\n", argv[1]);
					    Config_AddString(confstr);
					    argc--;
					    argv++;
				    } else {
					    fprintf(stderr, "Missing argument\n");
					    help();
					    exit(245);
				    }
				    break;

			    default:
				    fprintf(stderr, "unknown argument \"%s\"\n", argv[0]);
				    help();
				    exit(3245);
			}
		} else {
			configname = argv[0];
		}
		argc--;
		argv++;
	}
}

/*
 * ------------------------------------------------------------------
 * main
 *	Create board from configfile, Then run it
 * ------------------------------------------------------------------
 */

int
main(int argc, char *argv[])
{
	char *boardname;
#ifdef __unix
	struct timeval tv;
	uint64_t seedval;
#endif
	Board *board;
	SGLib_Init();
	CRC16_Init();
#ifdef __unix
	signal(SIGPIPE, SIG_IGN);
#endif
	parse_commandline(argc - 1, argv + 1);
#ifndef NO_DEBUGGER
//	CmdRegistry_Init();
	DbgVars_Init();
#endif
	read_configfile();
#ifdef __unix
	if (Config_ReadUInt64(&seedval, "global", "random_seed") >= 0) {
		fprintf(stderr, "Random Seed from Configuration file: %" PRIu64 "\n", seedval);
	} else {
		gettimeofday(&tv, NULL);
		seedval = tv.tv_usec + ((uint64_t) tv.tv_sec << 20);
		fprintf(stderr, "Random Seed from time of day %" PRIu64 "\n", seedval);
	}
	srand48(seedval);
#endif
	FIO_Init();
	SignodesInit();
	ClocksInit();
#ifndef NO_SHLIB
	Shlibs_Init();
#endif
	boardname = Config_ReadVar("global", "board");
	if (!boardname) {
		fprintf(stderr, "No Board selected in Configfile global section\n");
		exit(1);
	}
	board = Board_Find(boardname);
	if (!board) {
		exit(1);
	}
	if (Board_DefaultConfig(board)) {
		fprintf(stderr,"defaultconfig %s\n",Board_DefaultConfig(board));
		Config_AddString(Board_DefaultConfig(board));
	}
	Board_Create(board);
#ifndef NO_DEBUGGER
//	CliServer_New("cli");
	WebServ_New("webserv");
#endif
	LoadChain_Resolve();
	if (LoadChain_Load() < 0) {
		fprintf(stderr, "Loading failed\n");
		exit(1);
	}
#ifdef __unix
	Senseless_Init();
#endif
	Board_Run(board);
	exit(0);
}
