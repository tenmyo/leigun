/*
 ************************************************************************************************* 
 *
 * Commandline editor with readline capabilities
 *
 * Copyright 2004 2008 2009 Jochen Karrer. All rights reserved.
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

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#include <fio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "sgstring.h"
#include "editor.h"

#define MAX_CMDLINE_LEN (1024)
#define HISTORY_LINES (100)

/*
 * Use only control sequences which are the same for ANSI and VT100
 * and most other terminals.
 */
#define ANSI_up     	"\033[A"
#define ANSI_down   	"\033[B" 
#define ANSI_left   	"\033[D" 
#define ANSI_right   	"\033[C" 
#define ANSI_delchar	"\033[P"
#define ANSI_dellineend	"\033[K"
#define ANSI_insertchar "\033[@"
#define ANSI_keydel	"\033[3~"
#define ANSI_pgup	"\033[5~"
#define ANSI_pgdown	"\033[6~"
#define ANSI_newline	"\033E"

typedef struct TelOpt {
	uint8_t state;
 	uint8_t nego_started; 	
} TelOpt;

#define ED_STATE_IDLE   	(0)
#define ED_STATE_ESC		(1)
#define ED_STATE_ESC_5B		(2)
#define ED_STATE_ESC_5B_33	(3)
#define ED_STATE_ESC_5B_35	(4)
#define ED_STATE_ESC_5B_36	(5)
#define ED_STATE_NL		(6)
#define ED_STATE_CR		(7)

typedef struct Line {
	char *data;	
	int len; 
	int size; /* alloced data size */
} Line;

struct Editor {
	int fd;
	int state;
	int cursor;
	unsigned int current_line; /* The currently edited line */
	unsigned int last_line;	  /* The line at the end of the circular buffer */
	unsigned int history_size;

	void *dataProvider; /* client data for echoproc and outproc */
	void (*echoproc)(void *clientData,void *buf,int len);
	/* Editor to line consumer */
	void (*linesink)(void *clientData,void *buf,int len); 

	Line *lines;
};


/*
 *************************************************************************
 * Convenience function for sending Ascii strings to a telnet client
 *************************************************************************
 */
static inline void 
editor_echo(Editor *ed,void *buf,int len) {
	ed->echoproc(ed->dataProvider,(char *)buf,len);
}
static inline void 
editor_echostr(Editor *ed,char *buf) {
	editor_echo(ed,buf,strlen(buf));
}

/*
 *****************************************************************************
 * editor_submit_line
 * 	Called when a CR or NL character is detected. The current
 *	line is sent to the consumer (For example a command interpreter)
 *	and stored in the history
 *****************************************************************************
 */
static void
editor_submit_line(Editor *ed) {
	Line *line;
	/* Copy current line into last line because we want to have it as the last
	   in history 
	*/
	if(ed->linesink) {
		Line *curr;
		curr = &ed->lines[ed->current_line];
		ed->linesink(ed->dataProvider,curr->data,curr->len); 
	}
	if(ed->current_line != ed->last_line) {
		Line *curr,*last;
		curr = &ed->lines[ed->current_line];
		last = &ed->lines[ed->last_line];
		//fprintf(stderr,"Was not the last line, copying (%s) len %d\n",curr->data,curr->len);
		if(last->data) {
			sg_free(last->data);
		}
		last->size = curr->size;
		last->data = sg_calloc(last->size+1);
		memcpy(last->data,curr->data,curr->len+1);
		last->len = curr->len;
	}
	ed->last_line = (ed->last_line + 1) % ed->history_size;
	ed->current_line = ed->last_line;
	ed->cursor = 0;
	line = &ed->lines[ed->current_line];
	if(line->size) {
		sg_free(line->data);
		line->data = NULL;
		line->size = 0;
	}
	line->len = 0;
}

/*
 ******************************************************************
 * editor_insert
 * 	Insert a character at any place in the line. Move the
 *	rest of the line if we are not at end of the line.
 *	Reallocate the line buffer if the line is longer than
 *	the buffer size.	
 ******************************************************************
 */
static inline void
editor_insert(Editor *ed,uint8_t c) 
{
	Line *line = &ed->lines[ed->current_line];
	int i;
	if(line->len + 1 >= line->size) {
		line->size = line->len+500;
		line->data = sg_realloc(line->data,line->size);
		//printf("line %d: %d, %p\n",ed->current_line,line->size,line->data);
	}
	for(i=line->len;i>ed->cursor;i--) {
		line->data[i] = line->data[i-1];
	}
	line->data[ed->cursor]=c;
	line->len++;
	ed->cursor++;
	line->data[line->len]=0;
	editor_echostr(ed,ANSI_insertchar);
	editor_echo(ed,&c,1);
}

/*
 ****************************************************************
 * editor_backspace
 * 	Delete one character before the cursor and move the
 *	rest of the line
 ****************************************************************
 */
static inline void
editor_backspace(Editor *ed)
{

	Line *line = &ed->lines[ed->current_line];
	int i;
	if(ed->cursor > 0) {
		line->len--;
		ed->cursor--;
		for(i=ed->cursor;i<line->len;i++) {
			line->data[i] = line->data[i+1];
		}
		line->data[line->len]=0;
		//fprintf(stderr,"cmdline %s cursor %d\n",ed->cmdline,ed->cursor);		
		editor_echostr(ed,ANSI_left);
		editor_echostr(ed,ANSI_delchar);
	}
}
 
/*
 ************************************************
 * editor_del
 *	Delete the character under the cursor
 *	and move the rest of the line
 ************************************************
 */

static inline void
editor_del(Editor *ed)
{

	Line *line = &ed->lines[ed->current_line];
	int i;
	if(line->len > ed->cursor) {
		line->len--;
		for(i=ed->cursor;i<line->len;i++) {
			line->data[i] = line->data[i+1];
		}
		line->data[line->len]=0;
		//fprintf(stderr,"cmdline %s cursor %d\n",ed->cmdline,ed->cursor);		
		editor_echostr(ed,ANSI_delchar);
	}
}

/*
 ************************************************
 * editor_goto_x
 *	Move the cursor to a position 
 *	by sending left or right movements until
 *	the position is reached
 ************************************************
 */

static inline void
editor_goto_x(Editor *ed,int pos) 
{
	while(ed->cursor > pos) {
		ed->cursor--;
		editor_echostr(ed,ANSI_left);
	}
	while(ed->cursor < pos) {
		ed->cursor++;
		editor_echostr(ed,ANSI_right);
	}
}

/*
 ************************************************
 * editor_up
 *	Goto previous line in history buffer
 ************************************************
 */
static void
editor_up(Editor *ed)
{
	Line *line;
	unsigned int new_current;
	new_current = (ed->history_size + ed->current_line - 1) % ed->history_size;
	line = &ed->lines[new_current];
	if(!line->len) {
		return;
	}	
	ed->current_line = new_current; 
	editor_goto_x(ed,0);
	editor_echostr(ed,ANSI_dellineend);
	if(line->len) {
		editor_echostr(ed,line->data);
	}
	ed->cursor = line->len;

}

/*
 ************************************************
 * editor_down
 *	Goto next line in history buffer
 ************************************************
 */
static void
editor_down(Editor *ed)
{
	Line *line;
	if(ed->current_line == ed->last_line) {
		return;
	}
	ed->current_line = (ed->current_line + 1) % ed->history_size;
	editor_goto_x(ed,0);
	editor_echostr(ed,ANSI_dellineend);
	line = &ed->lines[ed->current_line];
	if(line->len) {
		editor_echostr(ed,line->data);
	}
	ed->cursor = line->len;
}

/*
 *********************************************************
 * Feed a character into the state machine of the editor
 *********************************************************
 */
int 
Editor_Feed(Editor *ed,char c)
{
	Line *line = &ed->lines[ed->current_line];
	switch(ed->state) {
		case ED_STATE_CR:
        	case ED_STATE_NL:
			if((ed->state == ED_STATE_CR) && (c == '\n')) {
				break;
			}
			if((ed->state == ED_STATE_NL) && (c == '\r')) {
				break;
			}
			ed->state = ED_STATE_IDLE;

		case ED_STATE_IDLE:	
			switch(c) {
				case '\033':
					ed->state = ED_STATE_ESC;
					break;

				case '\n':
                		case '\r':
					if(c == '\n') {
						ed->state = ED_STATE_NL;
					} else {
						ed->state = ED_STATE_CR;
					}
					if(line->len) {
						editor_echostr(ed,"\n\r");
						editor_submit_line(ed);
					}
					break;

				case 0x08:
				case 0x7f:
					editor_backspace(ed);
					break;

				default:
					editor_insert(ed,c);
			}
			break;
		case ED_STATE_ESC:
			if(c == '[') {
				ed->state = ED_STATE_ESC_5B;	
			} else if (c == 0x7f) {
                                editor_del(ed);
                                ed->state = ED_STATE_IDLE;
			} else {
				ed->state = ED_STATE_IDLE;	
			}
			break;
	
		case ED_STATE_ESC_5B:
			if(c == 'A') {
				editor_up(ed);
			} else if(c == 'B') {
				editor_down(ed);
			} else if(c == 'C') {
				/* RIGHT */
				if(ed->cursor < line->len) {
					editor_echostr(ed,ANSI_right);
					ed->cursor++;
				}
			} else if(c == 'D') {
				/* LEFT */
				if(ed->cursor > 0) {
					editor_echostr(ed,ANSI_left);
					ed->cursor--;
				}
			} else if(c == 'H') {
				editor_goto_x(ed,0);
			} else if(c == 'F') {
				editor_goto_x(ed,line->len);
			} else if(c == '3') {
				ed->state = ED_STATE_ESC_5B_33;
				break;
			} else if(c == '5') {
				ed->state = ED_STATE_ESC_5B_35;
				break;
			} else if(c == '6') {
				ed->state = ED_STATE_ESC_5B_36;
				break;
			}  else {
				fprintf(stderr,"%c\n",c);
			}
			ed->state = ED_STATE_IDLE;	
			break;

		case ED_STATE_ESC_5B_33:
			if(c=='~') {
				fprintf(stderr,"DEL\n");
				editor_del(ed);
			} else {
				fprintf(stderr,"Unknown %c",c);
			}
			ed->state = ED_STATE_IDLE;	
			break;

		case ED_STATE_ESC_5B_35:
			if(c=='~') {
				fprintf(stderr,"PGUP\n");
			} else {
				fprintf(stderr,"Unknown %c",c);
			}
			ed->state = ED_STATE_IDLE;	
			break;

		case ED_STATE_ESC_5B_36:
			if(c=='~') {
				fprintf(stderr,"PGDOWN\n");
			} else {
				fprintf(stderr,"Unknown %c",c);
			}
			ed->state = ED_STATE_IDLE;	
			break;
		default:
			fprintf(stderr,"Bug: Editor in unknown state %d\n",ed->state);
			exit(1);
			
	}
	return 0;
}

/*
 ********************************************************
 * Editor_Del
 *	Destructor of the editor
 ********************************************************
 */
void
Editor_Del(Editor *ed) {
        int i;
        Line *line;
        for(i=0;i<ed->history_size;i++) {
                line = &ed->lines[i];
                if(line->data) {
                        sg_free(line->data);
                        line->data = NULL;
                        line->size = 0;
                }
        }
        sg_free(ed->lines);
        ed->lines = 0;
	free(ed);
}

/*
 ********************************************************
 * Editor_New
 *	Constructor of the editor
 ********************************************************
 */

Editor *
Editor_New(Ed_EchoProc *echoproc,Ed_LineSink *linesink,void *clientData) {
	Editor *ed = sg_new(Editor);	 
	ed->echoproc = echoproc; 
	ed->linesink = linesink;
	ed->dataProvider = clientData;
	ed->history_size = 10;
	ed->last_line = ed->current_line = 0;
	ed->lines = (Line *)sg_calloc(ed->history_size * sizeof(Line));
	return ed;
}
