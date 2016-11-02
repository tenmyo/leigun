/*
 *************************************************************************************************
 *
 * Simulation of SNES controller 
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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
#include "sgstring.h"
#include "uze_snes.h"
#include "configfile.h"

#if 0
#define dbgprintf(x...) fprintf(stderr,x)
#else
#define dbgprintf(x...)
#endif

#define BTN_SR     1
#define BTN_SL     2
#define BTN_X      4
#define BTN_A      8
#define BTN_RIGHT  16
#define BTN_LEFT   32
#define BTN_DOWN   64
#define BTN_UP     128
#define BTN_START  256
#define BTN_SELECT 512
#define BTN_Y      1024
#define BTN_B      2048

typedef struct KeyMapEntry {
	uint16_t keycode;
	uint16_t snes_value;
} KeyMapEntry;

typedef struct MouseMapEntry {
	uint16_t keycode;
	int delta_x;
	int delta_y;
} MouseMapEntry;

#define MAX_KEYMAP_SIZE		(16)
#define MAX_MOUSEMAP_SIZE	(16)

typedef struct Uze_Snes {
	SigNode *clk;
	SigTrace *clkTrace;
	SigNode *latch;
	SigTrace *latchTrace;
	SigNode *data;
	KeyMapEntry keymap[MAX_KEYMAP_SIZE];
	MouseMapEntry mousemap[MAX_MOUSEMAP_SIZE];

	uint32_t shiftreg;
	uint16_t keys_pressed;
	uint32_t with_mouse;
	int8_t mouse_delta_x;
	int8_t mouse_delta_y;
	int shiftcnt;
	int sensivity_count;
	uint32_t sensivity;
} Uze_Snes;

KeyMapEntry invalid_keymap_entry = { 0, 0x0000 };

KeyMapEntry keymap_template[] = {
	{'r', 0x0800},
	{'l', 0x0400},
	{'x', 0x0200},
	{'a', 0x0100},
	{0xff53, 0x0080},
	{0xff51, 0x0040},
	{0xff54, 0x0020},
	{0xff52, 0x0010},
	{'s', 0x0008},
	{0x20, 0x0004},
	{'y', 0x0002},
	{'b', 0x0001},
};

MouseMapEntry invalid_mousemap_entry = { 0, 0, 0 };

/* Use the keypad cursor keys for mouse */
MouseMapEntry mousemap_template[8] = {
	{0xffb2, 0, 1},
	{0xffb8, 0, -1},
	{0xffb6, 1, 0},
	{0xffb4, -1, 0},
	{'j', 0, 1},
	{'k', 0, -1},
	{'h', -1, 0},
	{'l', 1, 0},
};

/*
 ***********************************************************
 * Maybe I should think about key assignment a little bit 
 ***********************************************************
 */
static void
handle_key_event(void *clientData, KeyEvent * ev)
{
	int i;
	Uze_Snes *snes = (Uze_Snes *) clientData;
	//fprintf(stderr,"Key 0x%02x state %d, pressed %04x\n",ev->key,ev->down,snes->keys_pressed);
	for (i = 0; i < MAX_KEYMAP_SIZE; i++) {
		/* Finished after first invalid keymap entry. */
		if (snes->keymap[i].snes_value == 0) {
			break;
		}
		if (snes->keymap[i].keycode == ev->key) {
			if (ev->down) {
				snes->keys_pressed |= snes->keymap[i].snes_value;
			} else {
				snes->keys_pressed &= ~snes->keymap[i].snes_value;
			}
		}
	}
	for (i = 0; i < array_size(snes->mousemap); i++) {
		if ((snes->mousemap[i].delta_x == 0) && (snes->mousemap[i].delta_y == 0)) {
			break;
		}
		if (snes->mousemap[i].keycode == ev->key) {
			snes->mouse_delta_x += snes->mousemap[i].delta_x * (snes->sensivity + 1);
			snes->mouse_delta_y += snes->mousemap[i].delta_y * (snes->sensivity + 1);
		}
	}
}

static void
latch_in(SigNode * node, int value, void *clientData)
{
	Uze_Snes *snes = (Uze_Snes *) clientData;
	int i;
	if (value == SIG_HIGH) {
		if (snes->shiftcnt == 2) {
			snes->sensivity_count++;
			if (snes->sensivity_count == 31) {
				snes->sensivity = (snes->sensivity + 1) % 3;
				if (!snes->with_mouse) {
					fprintf(stderr,
						"Tried to change sensivity for Non-mouse Gamepad\n");
				}
				dbgprintf("changed to sensivity %d\n", snes->sensivity);
			}
		} else {
			snes->sensivity_count = 0;
		}
		snes->shiftreg = ~snes->keys_pressed;
		snes->shiftcnt = 0;
		if (snes->with_mouse) {
			snes->shiftreg =
			    (snes->shiftreg & 0xfffff3ff) | (((~snes->sensivity) & 3) << 10);
			snes->shiftreg &= ~0x8000;
			for (i = 0; i < 7; i++) {
				if (abs(snes->mouse_delta_y) & (1 << i)) {
					snes->shiftreg &= ~(1 << (23 - i));
				}
				if (abs(snes->mouse_delta_x) & (1 << i)) {
					snes->shiftreg &= ~(1 << (31 - i));
				}
			}
			if (snes->mouse_delta_y < 0) {
				snes->shiftreg &= ~(1 << 16);
			}
			if (snes->mouse_delta_x < 0) {
				snes->shiftreg &= ~(1 << 24);
			}
			snes->mouse_delta_x = snes->mouse_delta_y = 0;
		}
		if (snes->shiftreg & 1) {
			SigNode_Set(snes->data, SIG_HIGH);
		} else {
			SigNode_Set(snes->data, SIG_LOW);
		}
		snes->shiftreg >>= 1;
		snes->shiftcnt++;
	}
}

static void
clock_in(SigNode * node, int value, void *clientData)
{
	Uze_Snes *snes = (Uze_Snes *) clientData;
	if (value == SIG_HIGH) {
		if (snes->shiftreg & 1) {
			SigNode_Set(snes->data, SIG_HIGH);
		} else {
			SigNode_Set(snes->data, SIG_LOW);
		}
		snes->shiftreg >>= 1;
		snes->shiftcnt++;
	}
}

void
Uze_SnesNew(const char *name, Keyboard * keyboard)
{
	Uze_Snes *snes;
	int i;
	if (!keyboard) {
		fprintf(stderr, "No keyboard given for SNES\n");
		return;
	}
	snes = sg_new(Uze_Snes);
	snes->with_mouse = 1;
	Keyboard_AddListener(keyboard, handle_key_event, snes);
	snes->clk = SigNode_New("%s.clock", name);
	snes->latch = SigNode_New("%s.latch", name);
	snes->data = SigNode_New("%s.data", name);
	if (!snes->clk || !snes->latch || !snes->data) {
		fprintf(stderr, "Can not create communication lines for SNES\n");
		exit(1);
	}
	snes->clkTrace = SigNode_Trace(snes->clk, clock_in, snes);
	snes->latchTrace = SigNode_Trace(snes->latch, latch_in, snes);
	Config_ReadUInt32(&snes->with_mouse, name, "mouse");
	for (i = 0; i < array_size(snes->keymap); i++) {
		if (i >= array_size(keymap_template)) {
			snes->keymap[i] = invalid_keymap_entry;
		} else {
			snes->keymap[i] = keymap_template[i];
		}
	}
	for (i = 0; i < array_size(snes->mousemap); i++) {
		if (i >= array_size(mousemap_template)) {
			snes->mousemap[i] = invalid_mousemap_entry;
		} else {
			snes->mousemap[i] = mousemap_template[i];
		}
	}

	fprintf(stderr, "Created Uzebox SNES adapter \"%s\"", name);
	if (snes->with_mouse) {
		fprintf(stderr, " with mouse\n");
	} else {
		fprintf(stderr, " without mouse\n");
	}
}
