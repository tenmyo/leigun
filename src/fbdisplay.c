#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rfbserver.h"
#include "sdldisplay.h"
#include "configfile.h"
#include "mouse.h"

void
FbDisplay_New(const char *name, FbDisplay ** display, Keyboard ** keyboard, Mouse **mouse,
	      SoundDevice ** sdev)
{
	char *bename = Config_ReadVar(name, "backend");
	if (!bename) {
		fprintf(stderr, "No backend given for Display output in section \"%s\"\n", name);
		exit(1);
	}
	if (strcmp(bename, "rfbserver") == 0) {
		if (sdev) {
			*sdev = NULL;
		}
		RfbServer_New(name, display, keyboard, mouse);
		return;
	}
#if 0
	if (strcmp(bename, "sdl") == 0) {
		if (sdev) {
			*sdev = NULL;
		}
		SDLDisplay_New(name, display, keyboard, mouse);
		return;
	}
#endif
}
