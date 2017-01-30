/*
 *************************************************************************************************
 *
 * Deskjet 460 Emulator Interpreter main loop
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
#include "printer.h"
#include "printengine.h"
#include "dj460interp.h"
#include "pjl.h"
#include "pcl3.h"
#include "pcl3gui.h"
#include "sgstring.h"

struct Dj460Interp {
	int curlang;
	PrintEngine *print_engine;
	PJL_Interp *pjl_interp;
	PCL3_Interp *pcl3_interp;
	PCL3GUI_Interp *pcl3gui_interp;
};

Dj460Interp *
Dj460Interp_New(const char *outfiledir)
{
	Dj460Interp *interp = sg_new(Dj460Interp);
	PrintEngine *pe;
	pe = interp->print_engine = PEng_New(outfiledir);
	interp->pjl_interp = PJLInterp_New();
	interp->pcl3_interp = PCL3Interp_New();
	interp->pcl3gui_interp = PCL3GUIInterp_New(pe);
	interp->curlang = Pl_Pcl3;
	fprintf(stderr, "DJ460 PCL3/PCL3GUI interpreter created\n");
	return interp;
}

void
Dj460Interp_Reset(Dj460Interp * interp)
{
	PJLInterp_Reset(interp->pjl_interp);
	PCL3Interp_Reset(interp->pcl3_interp);
	PCL3GUIInterp_Reset(interp->pcl3gui_interp);
}

/*
 * -----------------------------------------------------------------------
 * Feed the interpreter with print data
 * -----------------------------------------------------------------------
 */
int
Dj460Interp_Feed(Dj460Interp * interp, void *buf, int len)
{
	int result;
	int count = 0;
	int done = 0;
	while (count < len) {
		switch (interp->curlang) {
		    case 0:
			    fprintf(stderr, "Done\n");
			    exit(0);

		    case Pl_Pjl:
			    result =
				PJLInterp_Feed(interp->pjl_interp, buf + count, len - count,
					       &interp->curlang);
			    break;

		    case Pl_Pcl3:
			    result =
				PCL3Interp_Feed(interp->pcl3_interp, buf + count, len - count,
						&done);
			    if (done) {
				    interp->curlang = Pl_Pjl;
			    }
			    break;

		    case Pl_Pcl3Gui:
			    result =
				PCL3GUIInterp_Feed(interp->pcl3gui_interp, buf + count, len - count,
						   &done);
			    if (done) {
				    fprintf(stderr, "PCL3GUI is done\n");
				    interp->curlang = Pl_Pjl;
			    }
			    break;
		    default:
			    result = -1;
			    fprintf(stderr, "Unknown Printer language %d\n", interp->curlang);
			    break;
		}
		if (result <= 0) {
			return result;
		}
		count += result;
		//fprintf(stderr,"New lang %d\n",interp->curlang);
	}
	return count;
}
