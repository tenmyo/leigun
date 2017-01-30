#include "compiler_extensions.h"
#include "sound.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "configfile.h"
#include "sgstring.h"
#ifndef NO_ALSA
#include "alsasound.h"
#endif
#include "nullsound.h"

typedef struct SndBackEnd {
	char *name;
	struct SndBackEnd *next;
	SoundDevice_NewProc *newProc;
} SndBackEnd;

static SndBackEnd *firstBackEnd = NULL;

void
Sound_BackendRegister(const char *name, SoundDevice_NewProc * proc)
{
	SndBackEnd *be = sg_new(SndBackEnd);
	be->name = sg_strdup(name);
	be->newProc = proc;
	be->next = firstBackEnd;
	firstBackEnd = be;
}

SoundDevice *
SoundDevice_New(const char *name)
{
	SndBackEnd *cursor;
	SoundDevice *sdev;
	char *bename = Config_ReadVar(name, "backend");
	if (bename == NULL) {
		fprintf(stderr, "No sound backend configured for \"%s\"\n", name);
		return NullSound_New(name);
	}
	for (cursor = firstBackEnd; cursor; cursor = cursor->next) {
		if (strcmp(cursor->name, bename) == 0) {
			return cursor->newProc(name);
		}
	}
	/* To lazy to write the code to register sound backends */
#ifndef NO_ALSA
	if (strcmp(bename, "alsa") == 0) {
		sdev = AlsaSound_New(name);
		if (sdev) {
			return sdev;
		}
	}
#endif
	return NullSound_New(name);
}
