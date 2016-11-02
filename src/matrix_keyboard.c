/*
 *************************************************************************************************
 * X-Y Matrix Keyboard emulation on logical IO-pin level. 
 * Uses the keyboard.c as  event source.
 * Normaly this is connected to some CPU pins. For example GPIO pins or
 * keyboard scanner pins
 *
 * state: working 
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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
#include "keyboard.h"
#include "signode.h"
#include "cycletimer.h"
#include "matrix_keyboard.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define MIN_KEYPRESS_MS		(40)
#define MIN_KEYRELEASE_MS	(20)

#define MAX_KEYPAD_PINS (64)
#define MAX_PINS	MAX_KEYPAD_PINS
#define MAX_KEYS	(512)
#define FIFO_SIZE	(64)
#define FIFO_COUNT(kbd) ((kbd)->event_wp - (kbd)->event_rp)
#define FIFO_ROOM(kbd) (FIFO_SIZE - FIFO_COUNT(kbd))
#define FIFO_RINDEX(kbd) ((kbd)->event_rp % FIFO_SIZE)
#define FIFO_WINDEX(kbd) ((kbd)->event_wp % FIFO_SIZE)

typedef struct MatrixKeyboard  {
	char *name;
	SigNode *keypadNode[MAX_KEYPAD_PINS];
	int nr_keys;
	MatrixKey *keys[MAX_KEYS];
	CycleCounter_t last_event_time;
	CycleTimer eventTimer;
	KeyEvent eventFifo[FIFO_SIZE];
	uint64_t event_wp;
	uint64_t event_rp;
} MatrixKeyboard;

typedef struct
Keypad_Signals {
	char *keypad_line;
} KeypadSignal;

static KeypadSignal kpsigs[] = {
	{ "x0" },
	{ "x1" },
	{ "x2" },
	{ "x3" },
	{ "x4" },
	{ "x5" },
	{ "x6" },
	{ "x7" },
	{ "x8" },
	{ "x9" },
	{ "x10"},
	{ "x11"},
	{ "x12"},
	{ "x13"},
	{ "x14"},
	{ "x15"},
	{ "x16"},
	{ "y0"},
	{ "y1"},
	{ "y2"},
	{ "y3"},
	{ "y4"},
	{ "y5"},
	{ "y6"},
	{ "y7"},
	{ "y8"},
	{ "y9"},
	{ "y10"},
	{ "y11"},
	{ "y12"},
	{ "y13"},
	{ "y14"},
	{ "y15"},
	{ "y16"},
};

/*
 * --------------------------------------------------------
 * execute_key_event
 * 	execute a key event from the eventFifo
 * 	delay the next one 
 * --------------------------------------------------------
 */

static void 
execute_key_event(void *clientData) 
{
	int i;
	KeyEvent *ev;
	MatrixKeyboard *mkbd = (MatrixKeyboard*) clientData;
	char *name1 = alloca(strlen(mkbd->name) + 10);
	char *name2 = alloca(strlen(mkbd->name) + 10);
	MatrixKey *key = NULL;
	int xk_code;
	int down;
	int nr_keys = mkbd->nr_keys;
	int required_spacing;

	if(!FIFO_COUNT(mkbd)) {
		fprintf(stderr,"Emulator bug: Key fifo is empty\n");
		return;
	}
	ev = &mkbd->eventFifo[FIFO_RINDEX(mkbd)];
	mkbd->event_rp++;
	xk_code = ev->key;
	down = ev->down;

	for(i=0;i<nr_keys;i++) {
		if(xk_code == mkbd->keys[i]->xk_code) {
			key = mkbd->keys[i];
			sprintf(name1,"%s.%s",mkbd->name,key->row);
			sprintf(name2,"%s.%s",mkbd->name,key->col);
			if(down) {
				SigName_Link(name1,name2);
				dbgprintf("Matrix Keyboard: Key down %d, %s %s\n"
					,xk_code,name1,name2);
			} else {
				dbgprintf("Matrix Keyboard: Key up %d, %s %s\n"
					,xk_code,name1,name2);
				while(SigName_RemoveLink(name1,name2) > 0) {
					dbgprintf("Removed a link\n");
				}
			}
			mkbd->last_event_time = CycleCounter_Get();
			//break;
		}
	}
	if(down) {
		required_spacing = MIN_KEYPRESS_MS;
	} else {
		required_spacing = MIN_KEYRELEASE_MS;
	}
	if(FIFO_COUNT(mkbd) !=0) {
		if(!key || (key->flags & KEY_FLAG_IS_MODIFIER)) {
			CycleTimer_Mod(&mkbd->eventTimer,0);
		} else {
			CycleTimer_Mod(&mkbd->eventTimer,MillisecondsToCycles(required_spacing));
		}
	}
	if(!key) {
		dbgprintf("MatrixKeyboard: No key to emulate for Keycode 0x%04x\n",xk_code);
	}
	return;
}

/*
 * ---------------------------------------------------------------------------------------
 * handle_key_event
 * 	Fill the key event into a fifo and start execution if not already running
 * 	This handler is feed by the keyboard when a event is detected
 * ---------------------------------------------------------------------------------------
 */
static void 
handle_key_event(void *clientData,KeyEvent *ev) 
{
	MatrixKeyboard *mkbd = (MatrixKeyboard*) clientData;
	int64_t ms = CyclesToMilliseconds((CycleCounter_Get() - mkbd->last_event_time));
	int delay;
	if(FIFO_ROOM(mkbd)<1) {
		fprintf(stderr,"key event: Fifo full\n");
		return;
	}
	mkbd->eventFifo[FIFO_WINDEX(mkbd)] = *ev;
	mkbd->event_wp++;
	if(!CycleTimer_IsActive(&mkbd->eventTimer)) {
		if(ms<MIN_KEYPRESS_MS) {
			delay = MIN_KEYPRESS_MS-ms;
		} else {
			delay = 0;
		}
		CycleTimer_Mod(&mkbd->eventTimer,MillisecondsToCycles(delay));
	} 
}

static void
init_keys(MatrixKeyboard *mkbd,MatrixKey *keys,int nr_keys) 
{
	int i;
	mkbd->nr_keys = nr_keys;
	for(i=0;i<mkbd->nr_keys;i++) {
		mkbd->keys[i] = &keys[i];
	}
}

/*
 * -------------------------------------------------------------------------------------
 * Create the electrical signals which will be connected 
 * by the user of the GPIO emulator. For example a board definition
 * might connect theo CPU gpios to the keypad signals 
 * -------------------------------------------------------------------------------------
 */


static void
create_keypad_signals(MatrixKeyboard *mkbd) 
{
	int i;
	int nr_pins = (sizeof(kpsigs)/sizeof(KeypadSignal));
	if(nr_pins > MAX_KEYPAD_PINS) {
		fprintf(stderr,"Error: keypad has to many pins\n");
		exit(1);
	}
	for(i=0;i<nr_pins;i++) 
	{
		KeypadSignal *sig = &kpsigs[i];
		mkbd->keypadNode[i] = SigNode_New("%s.%s",mkbd->name,sig->keypad_line);
		//SigNode_Set(mkbd->keypadNode[i],SIG_PULLUP); 
	}
}

/*
 * -----------------------------------------------------------------------
 * MatrixKeyboard_New
 * 	Create a new xy matrix keyboard
 *	May be called with keyboard == NULL, but will not receive
 *	key events in this case
 * -----------------------------------------------------------------------
 */
void
MatrixKeyboard_New(const char *name,Keyboard *keyboard,MatrixKey *keys,int nr_keys) 
{
	MatrixKeyboard *mkbd = sg_new(MatrixKeyboard);
	CycleTimer_Init(&mkbd->eventTimer,execute_key_event,mkbd);
	mkbd->name = sg_strdup(name);
	create_keypad_signals(mkbd);
	init_keys(mkbd,keys,nr_keys);
	if(keyboard) {
		Keyboard_AddListener(keyboard,handle_key_event,mkbd);
	} else {
		fprintf(stderr,"Warning: No input source for keyboard \"%s\"\n"
			,name);
	}
	fprintf(stderr,"XY-Matrix keyboard \"%s\" created\n",name);
}

