/*
 *************************************************************************************************
 * Allow access to Variables using a symbolic name. 
 *
 * state: working 
 *
 * Copyright 2009 2010 Jochen Karrer. All rights reserved.
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


#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sgstring.h"
#include "strhash.h"
#include "debugvars.h"
#include "interpreter.h"

typedef struct DebugVarTable {
	SHashTable varHash;
} DebugVarTable;

static DebugVarTable debugVarTable;

typedef struct DebugVar {
	SHashEntry *hashEntry;	
	void *dataP;
	int type;
	void (*setProc)(void *clientData,uint32_t arg,uint64_t value);
	uint64_t (*getProc)(void *clientData,uint32_t arg);
	void *clientData;
	uint32_t arg;
} DebugVar;

int
DbgExport(int type,void *dataP,const char *format,...) 
{

	DebugVar *dvar;
	char name[512];
        va_list ap;
        SHashEntry *entryPtr;
        va_start (ap, format);
        vsnprintf(name,sizeof(name),format,ap);
        va_end(ap);

	entryPtr = SHash_CreateEntry(&debugVarTable.varHash,name);
	if(!entryPtr) {
		fprintf(stderr,"Var \"%s\" already exists\n",name);
		return -1;
	}
	dvar = sg_new(DebugVar); 
	SHash_SetValue(entryPtr,dvar);
	dvar->dataP = dataP;
	dvar->type = type;
	dvar->hashEntry = entryPtr;
	return 0;
}

int 
DbgSymHandler(DbgSetSymProc* setProc,DbgGetSymProc *getProc,void *cd,
	uint32_t arg,const char *format,...) 
{
	DebugVar *dvar;
	char name[512];
        va_list ap;
        SHashEntry *entryPtr;
        va_start (ap, format);
        vsnprintf(name,sizeof(name),format,ap);
        va_end(ap);

	entryPtr = SHash_CreateEntry(&debugVarTable.varHash,name);
	if(!entryPtr) {
		fprintf(stderr,"Var \"%s\" already exists\n",name);
		return -1;
	}
	dvar = sg_new(DebugVar); 
	SHash_SetValue(entryPtr,dvar);
	dvar->dataP = NULL;
	dvar->type = DBGT_PROC64_T;
	dvar->setProc = setProc;
	dvar->getProc = getProc;
	dvar->clientData = cd;
	dvar->arg = arg;
	dvar->hashEntry = entryPtr;
	return 0;	
}


void
DebugVar_Delete(DebugVar *dvar) 
{
	SHashEntry *entryPtr;	
	entryPtr = dvar->hashEntry;
	SHash_DeleteEntry(&debugVarTable.varHash,entryPtr);
	free(dvar);
}

static DebugVar * 
FindVarByName(const char *path) 
{
	SHashEntry *entryPtr;
	entryPtr = SHash_FindEntry(&debugVarTable.varHash,path);
	if(!entryPtr) {
		return NULL;
	}
	return  SHash_GetValue(entryPtr);
}

int
DebugVar_Set(const char *value, const char *path) 
{
	DebugVar *dvar;
	int result = 0;
	dvar = FindVarByName(path);
	if(!dvar)
		return -1;
	return result;
}

static void  
DebugVar_PrintStr(DebugVar *dvar,char *str,int size) {
	int32_t i;
	uint32_t u;
	uint64_t uu = 0;
	int64_t ii;
	double d;
	switch(dvar->type) {
		case DBGT_UINT8_T: 
			u = *(uint8_t*) dvar->dataP;
			snprintf(str,size,"%u",u);
			break;
		case DBGT_UINT16_T:
			u = *(uint16_t*) dvar->dataP;
			snprintf(str,size,"%u",u);
			break;
		case DBGT_UINT32_T:
			u = *(uint32_t*) dvar->dataP;
			snprintf(str,size,"%u",u);
			break;
		case DBGT_UINT64_T:
			uu = *(uint64_t*) dvar->dataP;
			snprintf(str,size,"%"PRIu64,uu);
			break;
		case DBGT_INT8_T: 
			i = *(int8_t*) dvar->dataP;
			snprintf(str,size,"%d",i);
			break;
		case DBGT_INT16_T:
			i = *(int16_t*) dvar->dataP;
			snprintf(str,size,"%d",i);
			break;
		case DBGT_INT32_T:
			i = *(int32_t*) dvar->dataP;
			snprintf(str,size,"%d",i);
			break;
		case DBGT_INT64_T: 
			ii = *(int64_t*) dvar->dataP;
			snprintf(str,size,"%"PRId64,ii);
			break;
		case DBGT_DOUBLE_T: 
			d = *(double*) dvar->dataP;
			snprintf(str,size,"%lf",d);
			break;

		case DBGT_PROC64_T: 
			if(dvar->getProc) {
				uu = dvar->getProc(dvar->clientData,dvar->arg);
			}
			snprintf(str,size,"%"PRIu64,uu);
			break;
		default:
			fprintf(stderr,"Variable has an illegal type\n");
			break;
	}
}

/*
 ****************************************************************************************
 * DebugVar_SetFromStr
 * 	set a variable from a string
 ****************************************************************************************
 */
static void  
DebugVar_SetFromStr(DebugVar *dvar,char *str) {
	uint64_t uu;
	int64_t ii;
	double d;
	sscanf(str,"%"PRId64,&ii);
	sscanf(str,"%"PRIu64,&uu);
	sscanf(str,"%lf",&d);
	switch(dvar->type) {
		case DBGT_UINT8_T: 
			*(uint8_t *)dvar->dataP = uu;		
			break;
		case DBGT_UINT16_T:
			*(uint16_t *)dvar->dataP = uu;		
			break;
		case DBGT_UINT32_T:
			*(uint32_t *)dvar->dataP = uu;		
			break;
		case DBGT_UINT64_T:
			*(uint64_t *)dvar->dataP = uu;		
			break;
		case DBGT_INT8_T: 
			*(int8_t *)dvar->dataP = ii;		
			break;
		case DBGT_INT16_T:
			*(int16_t *)dvar->dataP = ii;		
			break;
		case DBGT_INT32_T:
			*(int32_t *)dvar->dataP = ii;		
			break;
		case DBGT_INT64_T: 
			*(int64_t *)dvar->dataP = ii;		
			break;
		case DBGT_DOUBLE_T: 
			*(double *)dvar->dataP = d;		
			break;
		case DBGT_PROC64_T: 
			if(dvar->setProc) {
				dvar->setProc(dvar->clientData,dvar->arg,uu);
			}
			break;
		default:
			fprintf(stderr,"Variable has an illegal type\n");
			break;
	}
}

/*
 ******************************************************************
 * Interpreter command to set a variable
 ******************************************************************
 */
int 
cmd_set_var (Interp *interp,void *clientData,int argc,char *argv[])
{
	DebugVar *dvar;
	if(argc < 3) {
		return CMD_RESULT_BADARGS; 
	}
	dvar = FindVarByName(argv[1]);
	if(!dvar) {
		return CMD_RESULT_ERROR;
	} 
	DebugVar_SetFromStr(dvar,argv[2]);
	return 0;
}

/*
 ******************************************************************
 *
 * Interpreter command to get the value of a variable
 *
 ******************************************************************
 */
int 
cmd_get_var (Interp *interp,void *clientData,int argc,char *argv[])
{
	DebugVar *dvar;
	char str[30] = {0,};
	if(argc < 2) {
		return CMD_RESULT_BADARGS; 
	}
	dvar = FindVarByName(argv[1]);
	if(!dvar) {
		return CMD_RESULT_ERROR;
	} 
	DebugVar_PrintStr(dvar,str,sizeof(str));
	Interp_AppendResult(interp,"%s\r\n",str);
	return CMD_RESULT_OK;
}

void
DbgVars_Init(void) 
{
	SHash_InitTable(&debugVarTable.varHash);
	if(Cmd_Register("setvar",cmd_set_var,&debugVarTable) < 0) {
		fprintf(stderr,"Can not register setvar command\n");
		exit(1);
	}
	if(Cmd_Register("getvar",cmd_get_var,&debugVarTable) < 0) {
		fprintf(stderr,"Can not register getvar command\n");
		exit(1);
	}
}
