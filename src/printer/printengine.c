/*
 *************************************************************************************************
 * Printer Language independent Print Engine. Eats raster data, writes 
 * the result to images
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "printengine.h"
#include "sgstring.h"

/*
 * ----------------------------------------------------------------------------------------------------
 * The printengine takes the raster data sent by the Language Interpreters and writes it to an ppm 
 * ----------------------------------------------------------------------------------------------------
 */

struct PrintEngine {
	FILE *outfile;
	int page_in_progress;
	int page_xdpi;
	int page_width; 
	int page_ydpi;
	int page_height; 
	char *basefilename;
	int linecount;
	uint8_t *rgbbuf;
	int row_fraction;
};

static void
PEng_StartPage(PrintEngine *peng) 
{
	char *cmd;
	struct stat stat_buf;
	int img_nr;
	if(peng->page_in_progress) {
		fprintf(stderr,"Cannot start new page\n");
		return;
	}
	peng->linecount = 0;
	peng->page_in_progress = 1;
	cmd = alloca(strlen(peng->basefilename) + 100);
	for(img_nr=0;img_nr<1000;img_nr++) {
		sprintf(cmd,"%s/page_%03d.png",peng->basefilename,img_nr);
		//fprintf(stderr,"stat %s\n",cmd);
		if(stat(cmd,&stat_buf) < 0) {
			break;
		}
	}
	if(img_nr == 1000) {
		fprintf(stderr,"No outputfile with prefix %s available\n",peng->basefilename);
		peng->outfile = NULL;
		return;
	}
	sprintf(cmd,"pnmtopng > %s/page_%03d.png",peng->basefilename,img_nr);
	peng->outfile = popen(cmd,"w");
	if(!peng->outfile) {
		fprintf(stderr,"Opening Printer output pipe to pnmtopng failed\n");
		return;
	}
        fprintf (peng->outfile, "P6\n%i %i\n255\n", peng->page_width,peng->page_height);
	/* This is the default media size */
}


static void 
PEng_PrintLine(PrintEngine *peng) 
{
	if(peng->outfile) {
		fwrite(peng->rgbbuf,1,peng->page_width*3,peng->outfile);
	}
	peng->linecount++;
}

void
PEng_SendRasterData(PrintEngine *peng,int xdpi,int ydpi,void *data,int pixels) 
{
	int col_fraction = 0;
	int i;
	int sindex,dindex;
	sindex = dindex = 0;
	uint8_t *u8data = data;
	if(!peng->page_in_progress) {
		PEng_StartPage(peng);
	}
	for(i=0;i<pixels;i++) {
		col_fraction+=peng->page_xdpi;		
		while(col_fraction >= xdpi) {
			peng->rgbbuf[dindex++] = u8data[sindex+0];
			peng->rgbbuf[dindex++] = u8data[sindex+1];
			peng->rgbbuf[dindex++] = u8data[sindex+2];
			col_fraction-=xdpi;
		}
		sindex+=3;
	}	
	//fprintf(stderr,"xdpi %d, page_xdpi %d sindex %d, dindex %d\n",xdpi,peng->page_xdpi,sindex,dindex);
	while(dindex < peng->page_width*3) {
		peng->rgbbuf[dindex++] = 0xe0;
	}
	peng->row_fraction += peng->page_ydpi;
	while(peng->row_fraction >= ydpi) {
		PEng_PrintLine(peng);
		peng->row_fraction -= ydpi;
	}
}

/*
 * -------------------------------------------------------------------------------
 * PEng_NewPage finishes the current page and starts a new one. This is called
 * on Eject Command or on language by the printer emulator.
 * -------------------------------------------------------------------------------
 */
void
PEng_EjectPage(PrintEngine *peng) 
{
	if(!peng->page_in_progress) {
		return;
	}
	memset(peng->rgbbuf,0xe0,peng->page_width*3);
	while(peng->linecount < peng->page_height) {
		PEng_PrintLine(peng);
	}
	peng->page_in_progress = 0;
	if(peng->outfile) {
		fclose(peng->outfile);
		peng->outfile = NULL;
	}
		
}

void
PEng_Reset(PrintEngine *peng) 
{
	PEng_EjectPage(peng);
}

void 
PEng_SetMediaSize(PrintEngine *peng,int msize) 
{
	switch(msize) {
		case PE_MEDIA_SIZE_A4: 
			peng->page_width = 8.26389 * peng->page_xdpi; 
			peng->page_height = 11.6944 * peng->page_ydpi;
			break;

		case PE_MEDIA_SIZE_LETTER:
			peng->page_width = 8.5 * peng->page_xdpi; 
			peng->page_height = 11.0 * peng->page_ydpi;
			break;	

		default:
			fprintf(stderr,"Unknown media size %d\n",msize);			
			break;
	}
	//peng->rgbbuf_size = 32*1024;
	peng->rgbbuf = sg_realloc(peng->rgbbuf,32*1024);
}

void
PEng_Delete(PrintEngine *peng) 
{
	if(peng->basefilename) {
		free(peng->basefilename);
	}
	sg_free(peng->rgbbuf);
	sg_free(peng);
}

PrintEngine *
PEng_New(const char *basefilename) 
{
	PrintEngine *peng = sg_new(PrintEngine);
	peng->basefilename=sg_strdup(basefilename);
	peng->page_xdpi = 300;
	peng->page_ydpi = 300;
	return peng;
}
