/*
 *************************************************************************************************
 *
 * Load Binary , SRecord or IntelHex files to target Memory
 *
 * Status:
 *	Working
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "bus.h"
#include "ihex.h"
#include "srec.h"
#include "configfile.h"
#include "loader.h"

/* Should be a linked list with many namepaces, but for now one is enough */

LoadProc *firstLoadProc = NULL;
void *firstLoadProcClientData = NULL;

int  
Loader_RegisterBus(const char *name,LoadProc *proc,void *clientData)
{
	firstLoadProc = proc;
	firstLoadProcClientData = clientData;
	return 0;
}

/*
 * -----------------------------------------------------
 * Write to Memory devices on CPU main bus
 * If a device is read-only (Flash,Rom) write anyway
 * by using the address in the Read-memory Map. 
 * -----------------------------------------------------
 */
static inline int 
write_to_bus(uint32_t addr,uint8_t *buf,unsigned int count,int flags) 
{
	return firstLoadProc(firstLoadProcClientData,addr,buf,count,flags);
}

typedef struct LoaderInfo {
	int flags;
	uint64_t region_start;
	uint64_t region_end;
} LoaderInfo;
/*
 * --------------------------------------------------------
 * write_srec_to_bus
 *	Callback function for external SRecord parser. 
 *	Is invoked by parser after parsing a data record
 * -------------------------------------------------------
 */
static int 
write_srec_to_bus(uint32_t addr,uint8_t *buf,int count,void *clientData) 
{
	LoaderInfo *li = (LoaderInfo *)clientData;
	if(count <= 0) {
		return 0;
	}
	if((addr < li->region_start) || ((addr + count - 1) > li->region_end)) {
		fprintf(stderr,"S-Record is outside of memory region\n");
		fprintf(stderr,"Region from 0x%08" PRIx64 " to 0x%08" PRIx64 "\n",li->region_start,li->region_end);
		fprintf(stderr,"S-Record at 0x%08x\n",addr);
		return -1;
	} else {
		write_to_bus(addr,buf,count,li->flags);
	}
	return 0;
}

static int
Load_SRecords(char *filename,uint32_t startaddr,int flags,uint64_t region_size) 
{
	LoaderInfo li;		
	li.flags = flags;
	li.region_start = startaddr;
	if(region_size > 0) {
		li.region_end = startaddr + region_size - 1;
	} else {
		li.region_end = ~UINT64_C(0);
	}
	return XY_LoadSRecordFile(filename,write_srec_to_bus,&li);
}
/*
 * --------------------------------------------------
 * write_ihex_to_bus
 *	Callback function for external IHex parser 
 *	is invoked after parsing a data record
 * --------------------------------------------------
 */
static int 
write_ihex_to_bus(uint32_t addr,uint8_t *buf,int count,void *cd) 
{
	LoaderInfo *li = (LoaderInfo *)cd;
	if(count <= 0) {
		return 0;
	}
	if((addr < li->region_start) || ((addr + count - 1) > li->region_end)) {
		fprintf(stderr,"Ihex: Record at 0x%08x is outside of memory_region\n",addr);
		return -1;
	} else {
		write_to_bus(addr,buf,count,li->flags);
	}
	return 0;
}

static int
Load_IHex(char *filename,uint32_t startaddr,int flags,uint64_t region_size) 
{
	LoaderInfo li;		
	li.flags = flags;
	li.region_start = startaddr;
	if(region_size > 0) {
		li.region_end = startaddr + region_size -1;
	} else {
		li.region_end = ~UINT64_C(0);
	}
	fprintf(stderr,"Loading Intel Hex Record file \"%s\"\n",filename);
        return XY_LoadIHexFile(filename,write_ihex_to_bus,&li);;
}

/*
 * -----------------------------------------
 * Load a Binary File to a given address
 * -----------------------------------------
 */
int
Load_Binary(char *filename,uint32_t addr,int flags,uint64_t maxlen) 
{
	int fd=open(filename,O_RDONLY);
        int count;
	int to_big = 0;
        int64_t total=0;
	uint8_t buf[4096];
        if(fd<=0) {
                fprintf(stderr,"Can not open file %s ",filename);
                perror("");
		return -1;
        }
	while(1) {
                count = read(fd,buf,4096);
		if(count==0) {
			close(fd);
			return total;
		} else if(count < 0) {
			perror("error reading binary file");
			return -1;
		}
		if(maxlen && (count + total > maxlen)) {
			count = maxlen - total;
			to_big = 1;
		}
		if(write_to_bus(addr,buf,count,flags) < 0) {
			fprintf(stderr,"Binary loader: Can not write to bus at addr 0x%08x\n",addr);
		}
		total+=count;
		addr+=count;
		if(to_big) {
			fprintf(stderr,"Binary file does not fit into memory region\n");
			return -1;
		}
	}
	close(fd);
	return total;
}

/*
 * --------------------------------------------------------------
 * recognize file type from suffix and then 
 * Loads binary, Intel Hex or Motorola SRecords. 
 * The load address is ignored for srecords
 * --------------------------------------------------------------
 */
int64_t  
Load_AutoType(char *filename,uint32_t load_addr,uint64_t region_size)  {
	uint32_t swap;
	int flags=0;
	int len = strlen(filename);
	if(Config_ReadUInt32(&swap,"loader","swap32") >=0) {
		if(swap) {
			flags=LOADER_FLAG_SWAP32;
		}
	}
	fprintf(stderr,"Loading %s to 0x%08x flags %d\n",filename,load_addr,flags);
	if((len>=5) && (!strcmp(filename+len-5,".srec"))){
		return Load_SRecords(filename,load_addr,flags,region_size);
	} else if((len>=4) && (!strcmp(filename+len-4,".s19"))){
		return Load_SRecords(filename,load_addr,flags,region_size);
	} else if((len>=4) && (!strcmp(filename+len-4,".mot"))){
		return Load_SRecords(filename,load_addr,flags,region_size);
	} else if((len>=4) && (!strcmp(filename+len-4,".hex"))){
		return Load_IHex(filename,load_addr,flags,region_size);
	} else {
		return Load_Binary(filename,load_addr,flags,region_size);
	}
}
