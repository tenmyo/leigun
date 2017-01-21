/*
 *************************************************************************************************
 *
 * Emulation of Uzebox Videoport
 *
 * state: working 
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

#include <stdint.h>
#include <stdio.h>
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "sgstring.h"
#include "signode.h"
#include "cycletimer.h"
#include "fbdisplay.h"

#define REG_PIN(base)   ((base) + 0x00)
#define REG_DDR(base)   ((base) + 0x01)
#define REG_PORT(base)  ((base) + 0x02)

#define DISPLAY_WIDTH 268
#define DISPLAY_HEIGHT 224

typedef struct Uze_VidPort {

	SigNode *hsync;
	SigTrace *hsyncTrace;
	uint8_t reg_portx;	/* The portx register     */
	uint8_t reg_ddrx;	/* The direction register */
	uint8_t reg_pin;
	FbDisplay *display;
	uint32_t display_width;
	uint32_t display_height;
	uint8_t *fbuffer;
	/* Video state machine */
	uint8_t *line;
	uint32_t current_linebuf;
	uint32_t pixel_nr;
	uint32_t current_line;
	int x_repeat;
	int y_repeat;
	uint32_t real_line_length;
	//uint32_t current_fbidx;
} Uze_VidPort;

static uint8_t
pin_read(void *clientData, uint32_t address)
{
	Uze_VidPort *port = (Uze_VidPort *) clientData;
	return port->reg_pin;
}

static void
pin_write(void *clientData, uint8_t value, uint32_t address)
{
	Uze_VidPort *port = (Uze_VidPort *) clientData;
	port->reg_portx ^= value;
	//update_port_status(port,value);
	return;
}

static uint8_t
ddr_read(void *clientData, uint32_t address)
{
	Uze_VidPort *port = (Uze_VidPort *) clientData;
	return port->reg_ddrx;
}

static void
ddr_write(void *clientData, uint8_t value, uint32_t address)
{
	Uze_VidPort *port = (Uze_VidPort *) clientData;
	//uint8_t diff = value ^ port->reg_ddrx;
	port->reg_ddrx = value;
	//update_port_status(port,diff);
	return;
}

static uint8_t
port_read(void *clientData, uint32_t address)
{
	Uze_VidPort *port = (Uze_VidPort *) clientData;
	return port->reg_portx;
}

static void
port_write(void *clientData, uint8_t value, uint32_t address)
{
	Uze_VidPort *port = (Uze_VidPort *) clientData;
	if (port->pixel_nr + 1 < port->display_width) {
		port->line[port->pixel_nr++] = value;
		if (port->x_repeat) {
			port->line[port->pixel_nr++] = value;
		}
	}
	return;
}

static void
hsync(SigNode * node, int value, void *clientData)
{
	Uze_VidPort *port = (Uze_VidPort *) clientData;
	static uint64_t last;
	uint32_t diff = CycleCounter_Get() - last;
	if (value == SIG_LOW) {
		if (port->current_line < port->display_height) {
			if (port->pixel_nr) {
				port->current_line++;
			}
		}
		if (port->current_line == 80) {
			port->real_line_length = port->pixel_nr;
			if (port->pixel_nr < (port->display_width >> 1)) {
				port->x_repeat = 1;
			} else if (port->pixel_nr >= port->display_width) {
				port->x_repeat = 0;
			}
			/*fprintf(stderr,"Port pixel %d\n",port->pixel_nr); */
		}
		if ((diff < 1000)) {
			if (port->current_line > 100) {
				uint32_t lines_empty;
				uint32_t pixels_empty;
				FbUpdateRequest fbudrq;
				lines_empty = (port->display_height - port->current_line);
				pixels_empty = (port->display_width - port->real_line_length);

				fbudrq.offset = ((lines_empty + 1) >> 1) * port->display_width
				    + ((pixels_empty + 1) >> 1);

				fbudrq.count = port->display_width * port->current_line;
				fbudrq.fbdata = port->fbuffer;
				FbDisplay_UpdateRequest(port->display, &fbudrq);
			}
			port->current_line = 0;
		}
		if (port->current_line < port->display_height) {
			port->line = port->fbuffer + port->current_line * port->display_width;
		}
		port->pixel_nr = 0;
	}
	last = CycleCounter_Get();
}

static void
set_fbformat(Uze_VidPort * port)
{
	FbFormat fbf;
	fbf.red_bits = 3;
	fbf.red_shift = 0;
	fbf.green_bits = 3;
	fbf.green_shift = 3;
	fbf.blue_bits = 2;
	fbf.blue_shift = 6;
	fbf.bits_per_pixel = 8;
	fbf.depth = 8;
	FbDisplay_SetFbFormat(port->display, &fbf);
}

void
Uze_VidPortNew(const char *name, uint16_t base, FbDisplay * display)
{
	Uze_VidPort *port = sg_new(Uze_VidPort);
	if (!display) {
		fprintf(stderr, "Uzebox Video requires a valid display\n");
		exit(1);
	}
	AVR8_RegisterIOHandler(REG_PIN(base), pin_read, pin_write, port);
	AVR8_RegisterIOHandler(REG_DDR(base), ddr_read, ddr_write, port);
	AVR8_RegisterIOHandler(REG_PORT(base), port_read, port_write, port);
	port->hsync = SigNode_New("%s.hsync", name);
	if (!port->hsync) {
		fprintf(stderr, "UzeVid: Can not create hsync signal line\n");
		exit(1);
	}
	port->display_width = FbDisplay_Width(display);
	port->display_height = FbDisplay_Height(display);
	port->fbuffer = sg_calloc(port->display_width * port->display_height);
	port->hsyncTrace = SigNode_Trace(port->hsync, hsync, port);
	port->display = display;
	port->line = port->fbuffer;
	port->x_repeat = port->y_repeat = 0;
	set_fbformat(port);
	fprintf(stderr, "Created Uzebox ATMega644 Video IO port \"%s\" width %d, height %d\n",
		name, port->display_width, port->display_height);
}
