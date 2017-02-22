/*
 *************************************************************************************************
 * Navigation Switch 5 Way 
 * Uses the keyboard.c as  event source.
 * state: working 
 *
 * Copyright 2011 Jochen Karrer. All rights reserved.
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
#include <string.h>
#include <time.h>
#include "keysymdef.h"
#include "keyboard.h"
#include "signode.h"
#include "cycletimer.h"
#include "matrix_keyboard.h"
#include "sgstring.h"
#include "configfile.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif
#define NR_KEYS 5

typedef struct NaviSwitch5 {
	const char *name;
	int active_level;
	int inactive_level;
	SigNode *sigKey[NR_KEYS];
} NaviSwitch5;

typedef struct OutPad {
	char *signal_name;
} OutPad;

static OutPad kpsigs[] = {
	{"up"},
	{"down"},
	{"left"},
	{"right"},
	{"push"},
};

/*
 *********************************************************************
 * handle_key_event
 * 	This handler is feed by the keyboard when a event is detected
 *********************************************************************
 */
static void
handle_key_event(void *clientData, KeyEvent * ev)
{
	NaviSwitch5 *ns = (NaviSwitch5 *) clientData;
	unsigned int index;
	SigNode *sigKey;
	switch (ev->key) {
	    case XK_Up:
		    index = 0;
		    break;
	    case XK_Down:
		    index = 1;
		    break;
	    case XK_Left:
		    index = 2;
		    break;
	    case XK_Right:
		    index = 3;
		    break;
	    case XK_Return:
		    index = 4;
		    break;
	    default:
		    return;
	}
	if (index >= NR_KEYS) {
		return;
	}
	sigKey = ns->sigKey[index];
	if (ev->down) {
		SigNode_Set(sigKey, ns->active_level);
	} else {
		SigNode_Set(sigKey, ns->inactive_level);
	}
}

/*
 ************************************************************
 * Create the signal lines
 ************************************************************
 */

static void
create_keypad_signals(NaviSwitch5 * ns)
{
	int i;
	for (i = 0; i < NR_KEYS; i++) {
		ns->sigKey[i] = SigNode_New("%s.%s", ns->name, kpsigs[i].signal_name);
		if (!ns->sigKey[i]) {
			fprintf(stderr, "Can not create keypad signal %d\n", i);
			exit(1);
		}
		SigNode_Set(ns->sigKey[i], ns->inactive_level);
	}
}

/*
 **********************************************************************
 * NaviSwitch5_New
 * 	Create a new 5 way navigation switch 
 *	May be called with keyboard == NULL, but will not receive
 *	key events in this case
 **********************************************************************
 */
void
NaviSwitch5_New(const char *name, Keyboard * keyboard)
{
	char *active;
	char *inactive;
	NaviSwitch5 *ns = sg_new(NaviSwitch5);
	ns->name = name;
	active = Config_ReadVar(name, "active");
	inactive = Config_ReadVar(name, "inactive");
	if (active) {
		if (strcmp(active, "high") == 0) {
			ns->active_level = SIG_HIGH;
		} else if (strcmp(active, "low") == 0) {
			ns->active_level = SIG_LOW;
		} else if (strcmp(active, "force_high") == 0) {
			ns->active_level = SIG_FORCE_HIGH;
		} else if (strcmp(active, "force_low") == 0) {
			ns->active_level = SIG_FORCE_LOW;
		} else if (strcmp(active, "pullup") == 0) {
			ns->active_level = SIG_PULLUP;
		} else if (strcmp(active, "pulldown") == 0) {
			ns->active_level = SIG_PULLUP;
		} else if (strcmp(active, "open") == 0) {
			ns->active_level = SIG_OPEN;
		} else {
			fprintf(stderr, "Syntax error %s: Bad level %s\n", name, active);
		}
	} else {
		ns->active_level = SIG_LOW;
	}
	if (active) {
		if (strcmp(inactive, "high") == 0) {
			ns->inactive_level = SIG_HIGH;
		} else if (strcmp(inactive, "low") == 0) {
			ns->inactive_level = SIG_LOW;
		} else if (strcmp(inactive, "force_high") == 0) {
			ns->inactive_level = SIG_FORCE_HIGH;
		} else if (strcmp(inactive, "force_low") == 0) {
			ns->inactive_level = SIG_FORCE_LOW;
		} else if (strcmp(inactive, "pullup") == 0) {
			ns->inactive_level = SIG_PULLUP;
		} else if (strcmp(inactive, "pulldown") == 0) {
			ns->inactive_level = SIG_PULLUP;
		} else if (strcmp(inactive, "open") == 0) {
			ns->inactive_level = SIG_OPEN;
		} else {
			fprintf(stderr, "Syntax error %s: Bad level %s\n", name, active);
		}
	} else {
		ns->inactive_level = SIG_PULLUP;
	}
	create_keypad_signals(ns);
	if (keyboard) {
		Keyboard_AddListener(keyboard, handle_key_event, ns);
	} else {
		fprintf(stderr, "Warning: No input source for keyboard \"%s\"\n", name);
	}
	fprintf(stderr, "5 Way Navi-Switch keyboard \"%s\" created\n", name);
}
