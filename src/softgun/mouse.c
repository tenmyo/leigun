/*
 *************************************************************************************************
 *
 * Virtual base class for mouse emulation 
 *
 * state: untested
 *
 * Copyright 2015 Jochen Karrer. All rights reserved.
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
#include "mouse.h"
#include "sgstring.h"

/* 
 * --------------------------------------
 * Mouse_AddListener
 * --------------------------------------
 */
void
Mouse_AddListener(Mouse * mouse, MouseEventProc * eventProc, void *clientData)
{
	MouseListener *listener;
	if (!mouse)
		return;
	listener = sg_new(MouseListener);

	listener->clientData = clientData;
	listener->eventProc = eventProc;
	listener->next = mouse->listener_head;
	mouse->listener_head = listener;
}

/*
 * ----------------------------------------------------------------------------
 * Mouse_RemoveListener
 * 	Remove a mouseevent sink from the linked list of listeners
 * ----------------------------------------------------------------------------
 */
void
Mouse_RemoveListener(Mouse * mouse, MouseEventProc * eventProc)
{
	MouseListener *cursor, *prev;
	for (prev = NULL, cursor = mouse->listener_head; cursor;
	     prev = cursor, cursor = cursor->next) {
		if (cursor->eventProc == eventProc) {
			if (prev) {
				prev->next = cursor->next;
			} else {
				mouse->listener_head = cursor->next;
			}
			free(cursor);
			return;
		}
	}
}

/* 
 * ------------------------------------------------------------------------------
 * MouseBoard_SendEvent 
 * 	The SendEvent is done by the mouse implementation when it
 *	finds a change in some mouse status. Example
 *	The rfbserver calls send event when it receives a mouse_event message
 *	from the vncclient
 * ------------------------------------------------------------------------------
 */
void
Mouse_SendEvent(Mouse * mouse, MouseEvent * event)
{
	MouseListener *cursor;
	if (!mouse) {
		return;
	}
	for (cursor = mouse->listener_head; cursor; cursor = cursor->next) {
		cursor->eventProc(cursor->clientData, event);
	}
}
