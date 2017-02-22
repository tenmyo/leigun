#ifndef __MOUSE_H
#define __MOUSE_H

#include <stdint.h>

typedef struct MouseEvent {
	uint16_t x;
	uint16_t y;
	uint8_t eventMask;
} MouseEvent;

typedef struct Mouse Mouse;

typedef void MouseEventProc(void *clientData, MouseEvent *);

typedef struct MouseListener {
	struct MouseListener *next;
	void *clientData;
	MouseEventProc *eventProc;
} MouseListener;

struct Mouse {
	MouseListener *listener_head;
};

/* public functions */
void Mouse_AddListener(Mouse * mouse, MouseEventProc *, void *clientData);
void Mouse_RemoveListener(Mouse * mouse, MouseEventProc *);

/* KeyBoard_SendEvent is private ! */
void Mouse_SendEvent(Mouse *, MouseEvent *);

#endif
