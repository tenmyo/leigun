/*
 *************************************************************************************************
 * 
 * PJL interpreter
 *
 * state: Understands only the PJL ENTER LANGUAGE command 
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
#include "printer.h"
#include "pjl.h"
#include "sgstring.h"

/* The states */
#define STATE_IDLE	(0)
#define	STATE_STARTED	(1)
#define	STATE_FORMAT1	(2)
#define	STATE_PREFIX	(3)
#define STATE_CMD	(4)

struct PJL_Interp {
	int state;
	char cmdbuf[1024];
	int cmd_wp;
};

/*
 * -----------------------------------------------------------------
 * eval pjl cmd
 * -----------------------------------------------------------------
 */
static int
eval_pjl_cmd(PJL_Interp * interp, int *newlang)
{
	char *cmd = interp->cmdbuf;
	char lang[100];
	if (sscanf(cmd, " ENTER LANGUAGE=%99s", lang) == 1) {
		if (!strcmp(lang, "PCL3GUI")) {
			*newlang = Pl_Pcl3Gui;
		} else if (!strcmp(lang, "PCL3")) {
			*newlang = Pl_Pcl3;
		} else {
			fprintf(stderr, "Unknown Printer Language \"%s\"\n", lang);
		}
		return 0;
	} else {
		fprintf(stderr, "PJL command \"%s\" is not implemented\n", cmd);
		return -1;
	}
}

/*
 * --------------------------------------------------------
 * Feed the PJL Interpreter 
 * returns the number of consumed characters. It
 * leaves the loop early when language is switched
 * --------------------------------------------------------
 */
int
PJLInterp_Feed(PJL_Interp * interp, const uint8_t * buf, int len, int *newlang)
{
	int count;
	for (count = 0; count < len; count++) {
		switch (interp->state) {
		    case STATE_IDLE:
			    if (buf[count] == '@') {
				    interp->state = STATE_PREFIX;
				    interp->cmdbuf[0] = buf[count];
				    interp->cmd_wp = 1;
			    } else if (buf[count] == 0x1b) {
				    /* Command format 1 */
			    } else {
				    /* ignore non PJL */
			    }
			    break;
		    case STATE_FORMAT1:
			    if (buf[count] == 'X') {
				    /* Should check for complete UEL here */
				    interp->state = STATE_IDLE;
			    }
			    break;

		    case STATE_PREFIX:
			    interp->cmdbuf[interp->cmd_wp++] = buf[count];
			    interp->cmdbuf[interp->cmd_wp] = 0;
			    if (interp->cmd_wp == 4) {
				    interp->cmd_wp = 0;
				    if (strcmp("@PJL", interp->cmdbuf) == 0) {
					    interp->state = STATE_CMD;
				    } else {
					    fprintf(stdout, "Not a PJL command: \"%s\"\n",
						    interp->cmdbuf);
					    interp->state = STATE_PREFIX;
				    }
			    }
			    break;

		    case STATE_CMD:
			    if ((interp->cmd_wp + 1) >= sizeof(interp->cmdbuf)) {
				    fprintf(stderr, "PJL commandbuffer to small\n");
				    interp->state = STATE_IDLE;
				    break;
			    }
			    if (buf[count] == '\r') {
				    interp->cmdbuf[interp->cmd_wp] = 0;;
			    } else if (buf[count] == '\n') {
				    interp->cmdbuf[interp->cmd_wp] = 0;
				    eval_pjl_cmd(interp, newlang);
				    interp->cmd_wp = 0;
				    interp->state = STATE_IDLE;
				    if (*newlang != Pl_Pjl) {
					    return count;
				    }
			    } else {
				    interp->cmdbuf[interp->cmd_wp++] = buf[count];
				    interp->cmdbuf[interp->cmd_wp] = 0;;
			    }
			    break;
		}
	}
	return count;
}

void
PJLInterp_Reset(PJL_Interp * interp)
{
}

PJL_Interp *
PJLInterp_New()
{
	PJL_Interp *interp = sg_new(PJL_Interp);
	interp->state = STATE_IDLE;
	return interp;
}
