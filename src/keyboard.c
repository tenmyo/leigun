/*
 *************************************************************************************************
 *
 * Virtual base class for keyboard emulation 
 *
 * state: working with rfbserver as implementation of a keyboard 
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
#include "keyboard.h"
#include "sgstring.h"

/* 
 * --------------------------------------
 * Keyboard_AddListener
 * --------------------------------------
 */
void
Keyboard_AddListener(Keyboard * keyboard, KeyEventProc * eventProc, void *clientData)
{
	KeyboardListener *listener;
	if (!keyboard)
		return;
	listener = sg_new(KeyboardListener);

	listener->clientData = clientData;
	listener->eventProc = eventProc;
	listener->next = keyboard->listener_head;
	keyboard->listener_head = listener;
}

/*
 * ----------------------------------------------------------------------------
 * Keyboard_RemoveListener
 * 	Remove a keyevent sink from the linked list of listeners
 * ----------------------------------------------------------------------------
 */
void
Keyboard_RemoveListener(Keyboard * keyboard, KeyEventProc * eventProc)
{
	KeyboardListener *cursor, *prev;
	for (prev = NULL, cursor = keyboard->listener_head; cursor;
	     prev = cursor, cursor = cursor->next) {
		if (cursor->eventProc == eventProc) {
			if (prev) {
				prev->next = cursor->next;
			} else {
				keyboard->listener_head = cursor->next;
			}
			free(cursor);
			return;
		}
	}
}

/* 
 * ------------------------------------------------------------------------------
 * KeyBoard_SendEvent 
 * 	The SendEvent is done by the keyboard implementation when it
 *	finds a change in some key status. Example
 *	The rfbserver calls send event when it receives a key_event message
 *	from the vncclient
 * ------------------------------------------------------------------------------
 */
void
Keyboard_SendEvent(Keyboard * keyboard, KeyEvent * event)
{
	KeyboardListener *cursor;
	if (!keyboard) {
		return;
	}
	for (cursor = keyboard->listener_head; cursor; cursor = cursor->next) {
		cursor->eventProc(cursor->clientData, event);
	}
}
