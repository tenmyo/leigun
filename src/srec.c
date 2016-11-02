/*
 *************************************************************************************************
 * Read Motorola S-Record files
 *
 * Copyright 2004 2005 Jochen Karrer. All rights reserved.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "srec.h"

/*
 * ----------------------------------------------------
 * Special functions for reading hex numbers 
 * because sscanf is incredible slow
 * ----------------------------------------------------
 */
static int
hexparse_n(const char *str,unsigned int *retval,int bytes) {
	int i;
        unsigned int val=0;
	for(i=0;i<(bytes<<1);i++) {
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

static inline int
hexparse(const char *str,uint8_t *retval) {
	int i;
	unsigned int val=0;
	for(i=0;i<2;i++) {
                if(*str>='0' && *str<='9') {
                        val=val*16+(*str - '0');
                } else if(*str>='A' && *str<='F') {
                        val=val*16+(*str - 'A'+10);
                } else if(*str>='a' && *str<='f') {
                        val=val*16+(*str - 'a'+10);
                } else {
			return -1;
		}
                str++;
        }
	*retval = val;
	return 1;
}

static int
parse_srec_data(uint8_t *buf,const char *line,int count,uint32_t sum) 
{
	int i;
	uint8_t chksum;
	uint8_t val;
	for(i=0;i<count-1;i++) {
		if(hexparse(line+2*i,&val)!=1) {
			fprintf(stderr,"can not parse data\n");
			return -9;
		}
		buf[i]=val;
		sum+=val;
	}	
	sum = ~sum & 0xff;
	if(hexparse(line+2*(count-1),&chksum)!=1) {
		fprintf(stderr,"Can not parse checksum\n");
		return -99;
	}
	if(sum!=chksum) {
		fprintf(stderr,"checksum error in srecord \"%s\" sum %02x chksum %08x\n",line,sum,chksum);
//warning no checksum
		return -1;
	}
	return count-1;
}
/*
 * ----------------------------------
 * Read one SRecord (one line) 
 * ----------------------------------
 */
static int 
read_record(FILE *file,uint32_t *addr,uint8_t *buf) {
	char line[257];
	int len;
	unsigned int sum;
	uint8_t count;
	if(fgets(line,256,file)!=line) {
		if(feof(file)) {
			return 0;
		}
		fprintf(stderr,"Error reading SRecords\n");
		return -1;
	}	
	len = strlen(line);
	if(len<4) {
		fprintf(stderr,"Not an SRecord: \"%s\"\n",line);
		return -1;
	}
	if(line[0]!='S') {
		fprintf(stderr,"Not an SRecord file\n");
		return -1;
	}
	if(hexparse(line+2,&count)!=1) {
		fprintf(stderr,"Can not read count from record\n");
		return -2;
	}
	if(len<(4+2*count)) {
		fprintf(stderr,"Truncated SRecord \"%s\"\n",line);
		return -45;
	}
	switch(line[1]) {
		case '0':
			{
				int i;
				count = parse_srec_data(buf,line+8,count-2,count);
				if(count>=10) {
					fprintf(stderr,"S0 record: \"");			
					for(i=0;i<count;i++) {
						fprintf(stderr,"%c",buf[i]);
					}
					fprintf(stderr,"\"\n");			
				} else {
					fprintf(stderr,"Ignore S0 record\n");			
				}
			}
			return 0;
			break;

		/* S1 Data line with 2 Byte address */
		case '1': 
			if(hexparse_n(line+4,addr,2)!=1) {
				fprintf(stderr,"Can not parse S1 address\n");
				return -34;
			}
			sum=(*addr&0xff)+((*addr>>8)&0xff) +((*addr>>16)&0xff) + ((*addr>>24)&0xff)+count;
			return parse_srec_data(buf,line+8,count-2,sum);

		/* S2 Data line with 3 Byte address */
		case '2': 
			if(hexparse_n(line+4,addr,3)!=1) {
				fprintf(stderr,"Can not parse S2 address\n");
				return -34;
			}
			sum=(*addr&0xff)+((*addr>>8)&0xff) +((*addr>>16)&0xff) + ((*addr>>24)&0xff)+count;
			return parse_srec_data(buf,line+10,count-3,sum);

		/* S3 Data line with 4 Byte address */
		case '3':
			if(hexparse_n(line+4,addr,4)!=1) {
				fprintf(stderr,"Can not parse S3 address\n");
				return -34;
			}
			sum=(*addr&0xff)+((*addr>>8)&0xff) +((*addr>>16)&0xff) + ((*addr>>24)&0xff)+count;
			return parse_srec_data(buf,line+12,count-4,sum);

		case '5':
			/* fprintf(stderr,"Ignore S5 record\n"); */
			return 0;

		case '7':
			if(hexparse_n(line+4,addr,4)!=1) {
				fprintf(stderr,"bad S7 record\n");
				return -1;
			}
			fprintf(stderr,"S7 record (Start address 0x%08x) Ignored !\n",*addr);
			return 0;
			
		case '8':
			if(hexparse_n(line+4,addr,3)!=1) {
				fprintf(stderr,"bad S8 record\n");
				return -1;
			}
			fprintf(stderr,"Ignore S8 record (Start address 0x%06x)\n",*addr);
			return 0;
		case '9':
			if(hexparse_n(line+4,addr,2)!=1) {
				fprintf(stderr,"bad S9 record\n");
				return -1;
			}
			fprintf(stderr,"Ignore S8 record (Start address 0x%04x)\n",*addr);
			return 0;
		default: 
			fprintf(stderr,"Unknown SRecord type %c\n",line[1]);
			break;
	}
	return 0;	
}

int64_t
XY_LoadSRecordFile(char *filename,XY_SRecCallback *callback,void *clientData) 
{
	uint8_t buf[100];
	uint32_t addr=0;
	uint64_t totallen = 0;
	int result;
	FILE *file = fopen(filename,"r");
	if(!file) {
		return -1;
	}
	while(!feof(file)) { 
		int count;
		if((count=read_record(file,&addr,buf))<0) {
			break;
		}
		if(callback && (count > 0)) { 
			result=callback(addr,buf,count,clientData);
			if(result<0) {
				return result;
			}
		}
		totallen += count;
	}
	fclose(file);
	return totallen;
}


#ifdef TEST
int
main(int argc,char *argv[]) {
	if(argc<2) {
		fprintf(stderr,"Argument missing\n");
		exit(4235);
	}
	Load_SRecords(argv[1]); 
	exit(0);	
}
#endif 
