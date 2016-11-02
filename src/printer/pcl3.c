/*
 *************************************************************************************************
 *
 * PCL3 Interpreter
 *
 * state:	No commands implemented. 
 *		Understands only the UEL Escape sequence to exit to PJL 
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
#include "pcl3.h"
#include "sgstring.h"

#define STATE_IDLE   (0)
#define STATE_PCLCMD (1)
#define STATE_DATA   (2)

struct PCL3_Interp {
	int state;
	char cmdbuf[30];
	int cmd_wp;
	int data_expected;
};

/*
 * ------------------------------------------------------------------------------------------
 * Eval PCL3 Command returns <0 when no command was recoginzed (incomplete for example)
 * returns the number of following data bytes when a command was recognized
 * ------------------------------------------------------------------------------------------
 */

static int
eval_pcl3_cmd(PCL3_Interp * interp, int *done)
{
	if (strcmp("%-12345X", interp->cmdbuf) == 0) {
		fprintf(stderr, "Exiting from PCL3 Interpreter because of UEL\n");
		*done = 1;
		return 0;
	}
	return -1;
}

/*
 * --------------------------------------------------------
 * Feed the PCL3 interpreter with commands
 * --------------------------------------------------------
 */
int
PCL3Interp_Feed(PCL3_Interp * interp, uint8_t * buf, int len, int *done)
{

	int count;
	int result;
	*done = 0;
	for (count = 0; count < len; count++) {
		switch (interp->state) {
		    case STATE_IDLE:
			    if (buf[count] == 0x1b) {
				    interp->state = STATE_PCLCMD;
				    interp->cmd_wp = 0;
				    break;
			    } else {
				    // print text
			    }
			    break;
		    case STATE_PCLCMD:
			    if ((interp->cmd_wp + 1) >= sizeof(interp->cmdbuf)) {
				    interp->state = STATE_IDLE;
				    interp->cmd_wp = 0;
			    }
			    if (buf[count] == 0x1b) {
				    // Premature end or unimplemented command
				    interp->state = STATE_PCLCMD;
				    interp->cmd_wp = 0;
			    } else {
				    interp->cmdbuf[interp->cmd_wp++] = buf[count];
				    interp->cmdbuf[interp->cmd_wp] = 0;
				    result = eval_pcl3_cmd(interp, done);
				    if (result < 0) {
					    break;
				    } else if (result == 0) {
					    /* next command */
					    interp->state = STATE_IDLE;
					    interp->cmd_wp = 0;
					    if (*done == 1) {
						    return count;
					    }
				    } else if (result > 0) {
					    /* Data phase follows */
					    /* Should set a limit on expected data */
					    interp->cmd_wp = 0;
					    interp->state = STATE_DATA;
					    interp->data_expected = result;
				    }
			    }
			    break;
		    case STATE_DATA:
			    /* 
			     * Currently eat up data and do nothing because nothing is 
			     * implemented
			     */
			    interp->data_expected--;
			    if (interp->data_expected == 0) {
				    interp->state = STATE_IDLE;
				    interp->cmd_wp = 0;
			    }
			    break;
		}
	}
	return count;
}

void
PCL3Interp_Reset(PCL3_Interp * interp)
{
}

PCL3_Interp *
PCL3Interp_New(void)
{
	PCL3_Interp *interp = sg_new(PCL3_Interp);
	interp->state = STATE_IDLE;
	return interp;
}
