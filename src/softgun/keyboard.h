#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include <stdint.h>

typedef struct KeyEvent {
	uint16_t key;
	uint8_t down;
} KeyEvent;

typedef struct Keyboard Keyboard;

typedef void KeyEventProc(void *clientData, KeyEvent *);

typedef struct KeyboardListener {
	struct KeyboardListener *next;
	void *clientData;
	KeyEventProc *eventProc;
} KeyboardListener;

struct Keyboard {
	KeyboardListener *listener_head;
};

/* public functions */
void Keyboard_AddListener(Keyboard * keyboard, KeyEventProc *, void *clientData);
void Keyboard_RemoveListener(Keyboard * keyboard, KeyEventProc *);

/* KeyBoard_SendEvent is private ! */
void Keyboard_SendEvent(Keyboard *, KeyEvent *);

#endif
