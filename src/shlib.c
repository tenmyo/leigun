/*
 **************************************************************************************************
 *
 * Load Emulation modules as shared libraries
 * Intended to allow proprietary device emulators.
 * It is always recommended to publish source but sometimes
 * this is not possible because of non disclosure agreements 
 *
 * Special hate to the following Vendors:
 * 	Broadcom
 *	MSystems
 *	Samsung
 *
 * Usage: create a shared library libhello.so with an initfunction 
 *        void _init(void)
 *	  and add a libpath and a list of shared libraries to your 
 *	  emulator configuration file.
 *
 * State:
 *	Working	
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

/* I do not know how other architectures handle shared libraries */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "configfile.h"
#include "sgstring.h"

/*
 * --------------------------------------------------
 * Split a string at whitespace and write
 * to an argv vector. Warning: string is modified 
 * --------------------------------------------------
 */
static int
split_args(char *str,char *argv[],int maxargs) {
	int i;
	int inspace=1;
	int argc=0;
	for(i=0;*str;i++,str++) {
		if((*str!=' ') && (*str != '\t')) {
			if(inspace) {
				argv[argc]=str;
				argc++;
				if(argc>=maxargs) {
					fprintf(stderr,"Shared library list to long\n");
					exit(32424);
				}
			}
			inspace=0;
		} else {
			inspace=1;
			*str=0;
		}
	}
	return argc;
}
/*
 * ---------------------------------------
 * Split a path-string at ';' or ':'
 * Warning: string is modified !
 * ---------------------------------------
 */
static int
split_path(char *str,char *argv[],int maxargs) {
	int i;
	int insemicolon=1;
	int argc=0;
	for(i=0;*str;i++,str++) {
		if((*str==';')) {
			insemicolon=1;
			*str=0;
		} else if((*str==':')) {
			insemicolon=1;
			*str=0;
		} else {
			if(insemicolon) {
				argv[argc]=str;
				argc++;
				if(argc>=maxargs) {
					fprintf(stderr,"Path has to many components\n");
					exit(32424);
				}
			}
			insemicolon=0;
		}
	}
	return argc;
}
/*
 * ----------------------------------------------------
 * Load a shared library and call its init function
 * ----------------------------------------------------
 */
static int
Shlib_Load(const char *dirname,const char *basenm) 
{
	char *path = alloca(strlen(dirname)+strlen(basenm)+2);
	void *dlhandle;
	struct stat stat_buf;
	sprintf(path,"%s/%s",dirname,basenm);
	if(stat(path,&stat_buf)<0) {
		//fprintf(stderr,"Can't stat\"%s\", dirname \"%s\"\n",path,dirname);
		return -1;	
	}
	dlhandle = dlopen(path,RTLD_NOW | RTLD_GLOBAL);
	if(!dlhandle)  {
		fprintf (stderr,"dlopen error: %s\n",dlerror());
		exit(3508);
	}
	return 0;	
}

#define MAXSHLIBS (30)
#define MAXDIRS (30)

/**
 ***********************************************************
 * \fn void Shlibs_Init(void); 
 * Load and initialize the shared libraries given in
 * configuration file
 ***********************************************************
 */
void
Shlibs_Init(void) {
	char *shlibs;
	char *libpath;
	char *pathdup;
	char *libs[MAXSHLIBS];
	char *dirs[MAXDIRS];
	int libc;
	int dirc;
	int i,j;
	shlibs = Config_ReadVar("global","libs");
	if(!shlibs)
		return;
	libc=split_args(shlibs,libs,MAXSHLIBS);
	libpath = Config_ReadVar("global","libpath");
	if(!libpath) {
		libpath=".";
	}
	pathdup = sg_strdup(libpath);
	dirc=split_path(pathdup,dirs,MAXDIRS);
	for(i=0;i<libc;i++) {
		int retval=-2;
		for(j=0;j<dirc;j++) {
			retval = Shlib_Load(dirs[j],libs[i]); 
			if(retval>=0) {
				break;
			}	
		}
		if(retval<0) {
			fprintf(stderr,"Can not open lib \"%s\" in path \"%s\"\n",libs[i],libpath);
			exit(34223);
		}
	}
	free(pathdup);
}
#ifdef SHLIB_TEST
int 
main() {
	Shlib_Load("libbla.so");	
	Shlib_Load("laber.so.2.3");	
}
#endif

