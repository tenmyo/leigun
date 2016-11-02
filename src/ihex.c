/*
 *************************************************************************************************
 *
 * Parse Intel Hex Record files 
 * 	See Hexadecimal Object File Format Specification from
 * 	Intel Revision A, January 6, 1988	
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
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
#include <fcntl.h> 
#include "ihex.h"
#include "sgstring.h"

typedef struct HR_Context { 
	XY_IHexDataHandler *dataCB;
	void *dataCB_clientData;
	FILE *file;
	uint8_t chksum;
	uint32_t base_address;
	uint32_t start_address;
} HR_Context;

static inline int
bytescan(HR_Context *ctxt,const char *str,uint8_t *retval) {
	unsigned int val=0;
	int i;
        for(i=0;i<2;i++) {
                char c = *str;
                if(c>='0' && c<='9') {
                        val=val*16+(c - '0');
                } else if(c>='A' && c<='F') {
                        val=val*16+(c - 'A'+10);
                } else if(c>='a' && c<='f') {
                        val=val*16+(c - 'a'+10);
                } else {
                        return -1;
                }
                str++;
        }
        *retval = val;
	return 1;
}

/*
 * -------------------------------------------------------------------
 * hexscan
 * -------------------------------------------------------------------
 */
static int
hexscan(HR_Context *ctxt,const char *str,unsigned int *retval,int bytes) {
        int i;
	uint8_t b;
        unsigned int val=0;
        for(i=0;i<bytes;i++) {
		if(bytescan(ctxt,str,&b)<0) {
			return -1;
		}
		val=(val<<8) | b;
		str+=2;
		ctxt->chksum += b;
        }
        *retval = val;
        return 1;
}

static inline void 
add_checksum(HR_Context *ctxt,const char *str) 
{
	unsigned int dummy;	
	hexscan(ctxt,str,&dummy,1);
}

/*
 * ------------------------------------------------------
 * read record:
 *	Read one record from file to a binary buffer
 *	returns the length and
 * -------------------------------------------------------
 */
static int
read_record(HR_Context *ctxt,uint32_t *addr,uint8_t *buf) {
	int i;
        char line[30+256*2];
	unsigned int reclen;
	unsigned int load_offset;
	unsigned int rectyp;
        if(fgets(line,300,ctxt->file)!=line) {
                if(feof(ctxt->file)) {
                        return 0;
                }
                fprintf(stderr,"Error Reading Hex record\n");
                return -1;
        }
	if(!strlen(line)) {
		return 0;
	}
	if(line[0]!=0x3a) {
		fprintf(stderr,"Line is not an Hex record \"%s\"\n",line);
		return -2;
	}
	ctxt->chksum=0;
	if(hexscan(ctxt,line+1,&reclen,1)<0) {
		fprintf(stderr,"Parse error in Hex record \"%s\"\n",line);
		return -3;
	}
	if(hexscan(ctxt,line+3,&load_offset,2)<0) {
		fprintf(stderr,"Parse error in Hex record \"%s\"\n",line);
		return -4;
	}
	if(hexscan(ctxt,line+7,&rectyp,1)<0) {
		fprintf(stderr,"Parse error in Hex record \"%s\"\n",line);
		return -5;
	}
	switch(rectyp) {
		uint32_t ubsa;
		uint32_t ulba;
		uint32_t csip;
		uint32_t eip;

		case 0:	/* Data Record */
			for(i=0;i<reclen;i++) {
				unsigned int value;
				if(hexscan(ctxt,line+9+i*2,&value,1)<0) {
					fprintf(stderr,"Parse error in Hex record \"%s\"\n",line);
					return -9;
				}
				buf[i]=value;
			}	
			add_checksum(ctxt,line+9+i*2);
			//fprintf(stderr,"Data record len %d chksum %02x\n",reclen,ctxt->chksum);
			if(ctxt->chksum!=0) {
				fprintf(stderr,"IHex: Checksum error in \"%s\"\n",line);
				return -1;
			}
			*addr = ctxt->base_address + load_offset;
			return reclen;
			break;

		case 1:	/* End of File Record */
			// start application
			break;

		case 2:	/* Extended Segment Address Record */
			if(hexscan(ctxt,line+9,&ubsa,2)<0) {
				fprintf(stderr,"Parse error in Hex record \"%s\"\n",line);
				return -9;
                        }
			fprintf(stderr,"Warning: ESAR Record not implemented\n");
			add_checksum(ctxt,line+13);
			// store somewhere
			break;
			
			
		case 3:	/* Start Segment Address Record */
			if(hexscan(ctxt,line+9,&csip,4)<0) {
				fprintf(stderr,"Parse error in Hex record \"%s\"\n",line);
				return -9;
                        }
			fprintf(stderr,"SSAR Record\n");
			// store somewhere
			add_checksum(ctxt,line+17);
			// checksum(17)
			break;
		
		case 4:	/* Extended Linear Address Record */
			if(hexscan(ctxt,line+9,&ulba,2)<0) {
				fprintf(stderr,"Parse error in Hex record \"%s\"\n",line);
				return -9;
                        }
			add_checksum(ctxt,line+13);
			if(ctxt->chksum!=0) {
				fprintf(stderr,"IHex checksum error in \"%s\"\n",line);
				return -9;
			}
			ctxt->base_address = ulba<<16;
			break;

		case 5:	/* Start Linear Address Record */
			if(hexscan(ctxt,line+9,&eip,4)<0) {
				fprintf(stderr,"Parse error in Hex record \"%s\"\n",line);
				return -9;
                        }
			fprintf(stderr,"SLAR Record: start at 0x%08x (ignored !)\n",eip);
			add_checksum(ctxt,line+17);
			if(ctxt->chksum!=0) {
				fprintf(stderr,"IHex checksum error in \"%s\"\n",line);
				return -9;
			}
			ctxt->start_address = eip;
			// store somewhere
			break;
			
		default:
			fprintf(stderr,"Unknown Hex recordtype %d\n",rectyp);
			return -6;
	}
	return 0;
}

int64_t
XY_LoadIHexFile(const char *filename,XY_IHexDataHandler *callback,void *clientData)
{
	HR_Context *ctxt;
	int64_t total = 0;
	uint32_t addr = 0;
	uint8_t buf[256];
        FILE *file = fopen(filename,"r");
        if(!file) {
                return -1;
        }
	ctxt = sg_new(HR_Context);
	ctxt->file = file;
	ctxt->dataCB = callback;
	ctxt->dataCB_clientData = clientData;
        while(!feof(file)) {
                int count;
                if((count = read_record(ctxt,&addr,buf)) < 0) {
			fprintf(stderr,"can not read record %d\n",count);
			return -1;
                }
		if(count > 0) {
			if(callback(addr,buf,count,clientData) < 0) {
				return -1;
			}
		}
		total += count;
        }
        fclose(file);
	sg_free(ctxt);
        return total;
}

#ifdef IHEXTEST
int
main(int argc,char *argv[])  {
	if( XY_LoadIHexFile(argv[1],NULL)<0) {
		fprintf(stderr,"Mist\n");
	}
}
#endif
