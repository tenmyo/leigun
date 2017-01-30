/*
 *************************************************************************************************
 *
 * Configuration File Access
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include "configfile.h"
#include "sgstring.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define MAX_LINELEN 256
#define MAX_ARGC 32

typedef struct ConfigVar {
	char *section;
	char *name;
	char *value;
	struct ConfigVar *next;
} ConfigVar;

typedef struct Configuration {
	char curr_section[MAX_LINELEN];
	ConfigVar *firstVar;
	char *argv[MAX_ARGC];
} Configuration;

static Configuration config;

#define STATE_INSPACE (1)
#define STATE_ESCAPE  (2)
#define STATE_TEXT        (0)

static inline bool
issep(char ch)
{
	if ((ch == ' ') || (ch == ',') || (ch == '\t')) {
		return true;
	} else {
		return false;
	}
}

static int
split_args(char *script, char *argv[])
{
	int i, argc = 0;
	uint8_t state = STATE_INSPACE;
	argv[0] = script;
	for (i = 0; script[i]; i++) {
		if ((state == STATE_INSPACE) && issep(script[i])) {
			continue;
		}
		if (state == STATE_INSPACE) {
			if (script[i] == '"') {
				if (script[i]) {
					argv[argc++] = script + i + 1;
				}
				state = STATE_ESCAPE;
			} else {
				argv[argc++] = script + i;
				state = STATE_TEXT;
			}
			if (argc >= MAX_ARGC) {
				return argc;
			}
			continue;
		}
		if ((state == STATE_ESCAPE) && (script[i] == '"')) {
			script[i] = 0;
			state = STATE_INSPACE;
		}
		if ((state == STATE_TEXT) && issep(script[i])) {
			script[i] = 0;
			state = STATE_INSPACE;
		}
	}
	return argc;
}

char *
Config_ReadVar(const char *section, const char *name)
{
	Configuration *cfg = &config;
	ConfigVar *var;
	for (var = cfg->firstVar; var; var = var->next) {
		if (!strcmp(var->section, section) && !strcmp(var->name, name)) {
			return var->value;
		}
	}
	return NULL;
}

bool
Config_StrStrVar(const char *section, const char *name, const char *teststr)
{

	char *val = Config_ReadVar(section, name);
	if (!val) {
		return false;
	}
	return !!strstr(val, teststr);
}

/**
 ************************************************************************
 * Read a list into an argv
 ************************************************************************
 */
int
Config_ReadList(const char *section, const char *name, char **argvp[])
{
	Configuration *cfg = &config;
	int argc;
	char *valstr = Config_ReadVar(section, name);
	if (!valstr) {
		return 0;
	}
	argc = split_args(valstr, cfg->argv);
	*argvp = cfg->argv;
	return argc;
}

static void
add_var(Configuration * cfg, char *section, char *name, char *value)
{
	ConfigVar *var;
	//fprintf(stderr,"add sec \"%s\" var \"%s\" value \"%s\"\n",section,name,value);
	var = sg_new(ConfigVar);
	var->name = sg_strdup(name);
	var->section = sg_strdup(section);
	var->value = sg_strdup(value);
	var->next = cfg->firstVar;
	cfg->firstVar = var;
}

static void
add_line(Configuration * cfg, char *line)
{
	char *name_start;
	char *value_start;
	char *value_end;
	char *section;
	while (isspace((unsigned char)*line)) {
		line++;
	}
	if ((*line == '\n') || (*line == 0)) {
		return;
	}
	//fprintf(stderr,"Add \"%s\"\n",line); // jk
	if (*line == '[') {
		char *end;
		end = line;
		while (*end) {
			if (*end == ']') {
				*end = 0;
				dbgprintf("Curr section %s\n", line + 1);	// jk
				strcpy(cfg->curr_section, line + 1);
				return;
			}
			end++;
		}
	} else if (*line == '#') {
		return;
	}
	name_start = line;
	while (1) {
		if (!*line) {
			dbgprintf("Line end before colon\n");	// jk
			return;
		}
		if (*line == ':') {
			*line = 0;
			value_start = line + 1;
			break;
		}
		line++;
	}
	while (isspace((unsigned char)*value_start)) {
		value_start++;
	}
	if (!strlen(value_start)) {
		dbgprintf("Value is empty\n");	// jk
		return;
	}
	value_end = value_start + strlen(value_start) - 1;
	while (isspace((unsigned char)*value_end)) {
		*value_end = 0;
		value_end--;
	}
	section = cfg->curr_section;
	if (!*section) {
		dbgprintf("No current section\n");	// jk
		return;
	}
	//fprintf(stderr,"section \"%s\" var \"%s\", value \"%s\"\n",cfg->curr_section,name_start,value_start);
	if (!Config_ReadVar(cfg->curr_section, name_start)) {
		add_var(cfg, cfg->curr_section, name_start, value_start);
	}
}

void
remove_comment(char *line)
{
	while (*line) {
		if (*line == '#') {
			*line = 0;
			return;
		}
		line++;
	}
}

/*
 * ------------------------------------------
 * Read a configfile
 * returns -1 if file is not readable
 * ------------------------------------------
 */
int
Config_ReadFile(char *filename)
{
	Configuration *cfg = &config;
	FILE *file;
	char line[MAX_LINELEN];
	file = fopen(filename, "r");
	if (!file) {
		return -1;
	}
	while (1) {
		if (!fgets(line, MAX_LINELEN, file)) {
			break;
		}
		if (feof(file)) {
			break;
		}
		remove_comment(line);
		add_line(cfg, line);
	}
	fprintf(stderr, "Configuration file \"%s\" loaded\n", filename);
	return 0;
}

void
Config_AddString(const char *cfgstr)
{
	Configuration *cfg = &config;
	int i, end = 0;
	char line[MAX_LINELEN];
	while (1) {
		for (i = 0; i < MAX_LINELEN - 1; i++) {
			line[i] = *cfgstr++;
			if (line[i] == 0) {
				end = 1;
			}
			if ((line[i] == '\n') || (line[i] == 0)) {
				line[i] = 0;
				//fprintf(stderr,"Line is \"%s\"\n",line);
				add_line(cfg, line);
				break;
			}
		}
		if (end)
			return;
	}

}

int
Config_ReadInt32(int32_t * retval, const char *section, const char *name)
{
	char *value = Config_ReadVar(section, name);
	if (!value) {
		//fprintf(stderr,"Warning: Variable %s::%s not found in configfile\n",section,name);    
		return -1;
	}
	if (sscanf(value, "%d", retval) != 1) {
		fprintf(stderr, "Warning: Variable %s::%s is should be an Integer\n", section,
			name);
		return -2;
	}
	return 0;
}

int
Config_ReadUInt64(uint64_t * retval, const char *section, const char *name)
{
	char *value = Config_ReadVar(section, name);
	if (!value) {
		return -1;
	}
	if (sscanf(value, "0x%" SCNx64, retval) == 1) {
		return 0;
	}
	if (sscanf(value, "%" SCNu64, retval) == 1) {
		return 0;
	}
	fprintf(stderr, "Warning: Variable %s::%s should be an unsigned integer\n", section, name);
	return -2;
}

int
Config_ReadUInt32(uint32_t * retval, const char *section, const char *name)
{
	char *value = Config_ReadVar(section, name);
	if (!value) {
		//fprintf(stderr,"Warning: Variable \"%s\"::\"%s\" not found in configfile\n",section,name);    
		return -1;
	}
	if (sscanf(value, "0x%" SCNx32, retval) == 1) {
		return 0;
	}
	if (sscanf(value, "%" SCNu32, retval) == 1) {
		return 0;
	}
	fprintf(stderr, "Warning: Variable %s::%s should be an unsigned integer\n", section, name);
	return -2;
}

int
Config_ReadFloat32(float *retval, const char *section, const char *name)
{
	char *value = Config_ReadVar(section, name);
	if (!value) {
		return -1;
	}
	if (sscanf(value, "%f", retval) == 1) {
		return 0;
	}
	fprintf(stderr, "Warning: Variable %s::%s should be a float \n", section, name);
	return -2;
}

#ifdef CONFTEST
int
main(int argc, char *argv[])
{
	char **a;
	int i, ac;
	if (argc > 1) {
		Config_ReadFile(argv[1]);
	} else {
		Config_ReadFile(".config");
	}
	printf("%s\n", Config_ReadVar("affe", "sau"));
	ac = Config_ReadList("affe", "sau", &a);
	for (i = 0; i < ac; i++) {
		printf("%d: \"%s\"\n", i, a[i]);
	}
	exit(0);
}
#endif
