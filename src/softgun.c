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
#ifndef NO_DEBUGGER
#include "debugvars.h"
#endif
#ifdef __unix__
#  include "senseless.h"
#endif

#include "core/byteorder.h"
#include "core/lib.h"
#include "core/logging.h"

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
	LOG_Info("MAIN", "LCA \"%s\" \"%s\"", addr_string, filename);
	if (stat(filename, &stat_buf) < 0) {
		LOG_Error("MAIN", "stat on file \"%s\" failed ", filename);
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
				LOG_Error("MAIN", "Load destination \"%s\" not understood",
					lce->addr_string);
				exit(1);
			}
			n = sscanf(dest, "0x%x 0x%x", &lce->addr, &lce->region_size);
			if (n == 0) {
				LOG_Error("MAIN", "Bad address \"%s\" for \"%s\" in configfile",
					dest, lce->addr_string);
				LOG_Error("MAIN", "Should be %s: <hexaddr> ?hex-maxsize?",
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
			LOG_Error("MAIN", "Can not read configfile %s", configfpath);
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
	LOG_Error("MAIN", "Error: Can Not read configuration file %s", str);
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
					    LOG_Error("MAIN", "Missing name of configuration file");
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
					    LOG_Error("MAIN", "Missing argument");
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
					    LOG_Error("MAIN", "Missing argument");
					    help();
					    exit(245);
				    }
				    break;

			    default:
				    LOG_Error("MAIN", "unknown argument \"%s\"", argv[0]);
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
	LOG_Info("MAIN", "%s", softgun_version);
	if (LIB_Init() < 0) {
		LOG_Error("MAIN", "LIB_Init %s.", "failed");
		exit(1);
	}
	SGLib_Init();
	CRC16_Init();
#ifdef SIGPIPE
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
		LOG_Info("MAIN", "Random Seed from Configuration file: %" PRIu64, seedval);
	} else {
		gettimeofday(&tv, NULL);
		seedval = tv.tv_usec + ((uint64_t) tv.tv_sec << 20);
		LOG_Info("MAIN", "Random Seed from time of day %" PRIu64, seedval);
	}
	srand48(seedval);
#endif
	SignodesInit();
	ClocksInit();
	boardname = Config_ReadVar("global", "board");
	if (!boardname) {
		LOG_Error("MAIN", "No Board selected in Configfile global section");
		exit(1);
	}
	board = Board_Find(boardname);
	if (!board) {
		LOG_Error("MAIN", "Board(%s) Not Found", boardname);
		exit(1);
	}
	if (Board_DefaultConfig(board)) {
		LOG_Warn("MAIN", "defaultconfig %s",Board_DefaultConfig(board));
		Config_AddString(Board_DefaultConfig(board));
	}
	Board_Create(board);
	LoadChain_Resolve();
	if (LoadChain_Load() < 0) {
		LOG_Error("MAIN", "Loading failed");
		exit(1);
	}
#ifdef __unix
	Senseless_Init();
#endif
	Board_Run(board);
	exit(0);
}
