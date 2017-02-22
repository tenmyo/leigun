/*
 *************************************************************************************************
 *
 * Directory 
 *	Place for registering an querying variables in softgun
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesystem.h"
#include "sgstring.h"

struct Dirent {
	char *d_name;
	int refcnt;
	int type;
	struct Dirent *next;
	SHashEntry *hashEntry;
};

struct Filesystem {
	SHashTable fsHash;
};

struct Directory {
	Dirent dent;		/* Inherits from dirent */
	Dirent *firstChild;
};

struct FS_File {
	Dirent dent;		/* Inherits from dirent  */
	int (*proc) (void *clientData, int argc, char *argv[]);
};

static Directory *
Path_FindParent(Filesystem * filesys, char *path)
{
	size_t len = strlen(path);
	int i;
	Dirent *dent;
	char *parname = alloca(len + 1);
	SHashEntry *hashEntry;
	strcpy(parname, path);
	for (i = len - 1; i >= 0; i--) {
		if ((parname[i] == '/') && (i != (len - 1))) {
			if (i != 0) {
				parname[i] = 0;
			}
			break;
		} else {
			parname[i] = 0;
		}
	}
	if (i < 0) {
		//fprintf(stderr,"Hier %d\n",__LINE__);
		return NULL;
	}
	hashEntry = SHash_FindEntry(&filesys->fsHash, parname);
	if (!hashEntry) {
		//fprintf(stderr,"Hier %d %s\n",__LINE__,parname);
		return NULL;
	}
	dent = SHash_GetValue(hashEntry);
	if (dent->type == DENT_DIR) {
		return (Directory *) dent;
	} else {
		//fprintf(stderr,"Hier %d\n",__LINE__);
		return NULL;
	}
}

Dirent *
Dent_Create(Filesystem * filesys, char *name, int type)
{
	char *dentname;
	Dirent *dent;
	Directory *parent;
	SHashEntry *hashEntry;
	parent = Path_FindParent(filesys, name);
	if (!parent && (strlen(name) != 1)) {
		fprintf(stderr, "Non root Directory \"%s\" has no parent\n", name);
		return NULL;
	}
	dentname = sg_strdup(name);
	if (!dentname) {
		fprintf(stderr, "Out of memory error\n");
		return NULL;
	}
	if (type == DENT_DIR) {
		dent = (Dirent *) sg_new(Directory);
	} else if (type == DENT_FILEOBJ) {
		dent = (Dirent *) sg_new(FS_File);
	} else {
		fprintf(stderr, "Fatal error: Direntry neither Directory nor File Object\n");
		exit(1);
	}
	if (!dent) {
		fprintf(stderr, "Out of memory error\n");
		free(dentname);
		return NULL;
	}
	dent->type = DENT_DIR;
	dent->d_name = dentname;
	dent->refcnt = 1;
	dent->hashEntry = hashEntry = SHash_CreateEntry(&filesys->fsHash, dentname);
	if (hashEntry == NULL) {
		free(dent);
		free(dentname);
		return NULL;
	}
	SHash_SetValue(hashEntry, dent);
	/* Lookup parent directory for Linking */
	if (parent) {
		//fprintf(stderr,"Linking %s with ch %s\n",parent->dent.name,dentname);
		dent->next = parent->firstChild;
		parent->dent.refcnt++;
		parent->firstChild = dent;
	} else {
		//fprintf(stderr,"%s has no parent\n",dentname);
		dent->next = NULL;
	}
	return dent;
}

Filesystem *
FS_New()
{
	Filesystem *filesys = sg_calloc(sizeof(Filesystem));
	SHash_InitTable(&filesys->fsHash);
	return filesys;
}

/*
 * Warning ! Filesystem_Del currently doesn't check refcnt, So don't use it
 * from a second thread.
 */
void
FS_Del(Filesystem * filesys)
{
	SHashEntry *hashEntry;
	SHashSearch search;
	Dirent *dent;

	do {
		hashEntry = SHash_FirstEntry(&filesys->fsHash, &search);
		//fprintf(stderr,"Hier %d\n",__LINE__);
		if (hashEntry) {
			dent = SHash_GetValue(hashEntry);
			SHash_DeleteEntry(&filesys->fsHash, hashEntry);
			//fprintf(stderr,"Hier %d\n",__LINE__);
			if (dent) {
				free(dent->d_name);
				free(dent);
			}
		}
	} while (hashEntry);
}

FS_DIR *
FS_Opendir(Filesystem * fs, const char *name)
{
	SHashEntry *entryPtr;
	Directory *dir;
	FS_DIR *dirHandle;
	Dirent *dent;
	entryPtr = SHash_FindEntry(&fs->fsHash, name);
	if (!entryPtr) {
		fprintf(stderr, "%s not found in fsHash\n", name);
		return NULL;
	}
	dent = SHash_GetValue(entryPtr);
	if (dent->type == DENT_FILEOBJ) {
		return NULL;
	} else if (dent->type != DENT_DIR) {
		dir = (Directory *) dent;
		dirHandle = sg_calloc(sizeof(FS_DIR));
		dent->refcnt++;
		dirHandle->dir = dir;
		dirHandle->cursor = NULL;
		return dirHandle;
	} else {
		fprintf(stderr, "Bug: unknown directory entry type\n");
		return NULL;
	}
}

void
FS_Closedir(FS_DIR * dirHandle)
{
	dirHandle->dir->dent.refcnt--;
	sg_free(dirHandle);
}

Dirent *
FS_Readdir(Filesystem * fs, FS_DIR * dirHandle)
{
	Directory *dir = dirHandle->dir;
	if (dirHandle->cursor == NULL) {
		dir->firstChild->refcnt++;
		dirHandle->cursor = dir->firstChild;
	} else {
		Dirent *prev = dirHandle->cursor;
		dir->dent.next->refcnt++;
		dirHandle->cursor = dir->dent.next;
		prev->refcnt--;
	}
	return dirHandle->cursor;
}
