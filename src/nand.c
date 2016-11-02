/*
 *************************************************************************************************
 * Emulation of Samsung NAND Flash 
 *
 * State: Working 
 *
 * Copyright 2007 Jochen Karrer. All rights reserved.
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
#include "nand.h"
#include "sgstring.h" 
#include "cycletimer.h"
#include "diskimage.h"
#include "configfile.h"

typedef struct FlashDescription {
        char *type;
        int addrbits;
        int colbits;
	int blockBits;
        uint64_t rawSize;
        uint32_t dataPageSize;
        uint32_t sparePageSize;
        int planes;
        int blocks;
        int pagesPerBlock;
        uint32_t us_busy_read;
        uint32_t us_busy_erase;
	uint8_t id[4];
} FlashDescription;

static FlashDescription flash_types[] = {
        {
                .type = "K9F1208",
                .addrbits = 26,
                .colbits = 8,
                .rawSize = 528 * 32 * 4096,
                .planes = 4,
                .dataPageSize = 512,
                .sparePageSize = 16,
                .pagesPerBlock = 32,
		.blockBits = 14,
                .blocks = 4096,
                //.us_busy_read = 15,
                .us_busy_read = 1,
                .us_busy_erase = 10,
		.id = { 0xec, 0x76, 0xa5, 0xc0 },
        },
};

struct NandFlash {
        char *type;
        CycleCounter_t busy_until;
        DiskImage *diskimage;
	uint32_t us_busy_read;
	uint32_t us_busy_erase;
	uint64_t rawSize;
	uint32_t addrBits;
	uint32_t colBits;
	uint32_t blockBits;
	uint16_t cmd; 	/* 8 Bit, but > 8 bit means invalid */
	uint8_t pointerCmd;
	uint32_t cycleCtr;
	uint32_t colAddr;
        uint32_t rowAddr;
        uint8_t	 addrState;
	uint8_t areaPtr;
	uint32_t dataPageSize;
	uint32_t sparePageSize;
	uint32_t dataBlockSize;	/* erase Block size */
	uint32_t blockSize;	/* erase Block size with spare */
	uint32_t pageSize;
	uint8_t *pageCache;
	uint8_t *id;
};

#define CMD_READ1_L             (0x00)  /* First half (256 bytes) */
#define CMD_READ1_H             (0x01)  /* Second half (256 bytes) */
#define CMD_RNDOUT		(0x05)
#define CMD_READ2               (0x50)  /* 16 Byte ECC area */
#define CMD_READID              (0x90)
#define CMD_RESET               (0xff)  /** Accept when busy */
#define CMD_PP_SETUP            (0x80)
#define CMD_RNDIN            	(0x85)
#define CMD_PP                  (0x10)
#define CMD_PP_DUMMY		(0x11)
#define CMD_ERASE_SETUP         (0x60)
#define CMD_ERASE               (0xd0)
#define CMD_BLOCKPROT1          (0x41)
#define CMD_BLOCKPROT2          (0x42)
#define CMD_BLOCKPROT3          (0x43)
#define CMD_READSTATUS          (0x70)  /** Accept when busy */
#define CMD_READMULTPLANSTATUS	(0x71)
#define CMD_READPROTSTAT        (0x7a)
#define CMD_COPYBACK		(0x8a)

#define ASTATE_COL      (1)
#define ASTATE_ROW0     (2)
#define ASTATE_ROW1     (3)
#define ASTATE_ROW2     (4)
#define ASTATE_ROW3     (5)
#define ASTATE_IGNORE   (6)

#define APTR_AREA_A     (0)
#define APTR_AREA_B     (1)
#define APTR_AREA_C     (2)

/**
 *******************************************************************************
 * \fn static void make_busy(NandFlash *nf,uint32_t useconds)
 *******************************************************************************
 */
static void
make_busy(NandFlash *nf,uint32_t useconds)
{
	nf->busy_until = CycleCounter_Get() + MicrosecondsToCycles(useconds);
}

/**
 *********************************************************************************
 * Read one page to internal cache.
 *********************************************************************************
 */
static void
fill_cache(NandFlash *nf)
{
	uint64_t addr;
	addr = ((uint64_t)nf->rowAddr * nf->pageSize);
	//fprintf(stderr,"Fill cache with addr 0x%08llx, ps %d\n",addr,nf->pageSize);
	DiskImage_Read(nf->diskimage,addr,nf->pageCache,nf->pageSize);
}

/**
 *********************************************************************************
 * Write the internal cache to the page.
 *********************************************************************************
 */
static void
program_page(NandFlash *nf)
{
	uint64_t addr;
	uint32_t i;
	uint8_t tmpBuf[nf->pageSize]; 
	addr = ((uint64_t)nf->rowAddr * nf->pageSize);
	DiskImage_Read(nf->diskimage,addr,tmpBuf,nf->pageSize);
	for(i = 0; i < nf->pageSize;i++) {
		tmpBuf[i] &= nf->pageCache[i];
	}
	DiskImage_Write(nf->diskimage,addr,tmpBuf,nf->pageSize);
//	fprintf(stderr,"Program page 0x%02x, %02x\n",nf->rowAddr,nf->rowAddr/32);
}
/*
 *******************************************************************************
 * \fn static void erase_block(NandFlash *nf)
 * Only some address bits are valid. 
 *******************************************************************************
 */
static void
erase_block(NandFlash *nf)
{
	uint32_t block_nr;
	uint64_t imgAddr;
	uint64_t addr;
	uint32_t i;
	block_nr = nf->rowAddr >> (nf->blockBits - nf->colBits - 1);
	//fprintf(stderr,"Erase block 0x%x, row 0x%x, col 0x%x, bs 0x%x ps 0x%x\n",block_nr,nf->rowAddr,nf->colAddr,nf->blockSize,nf->pageSize);
	imgAddr = (uint64_t)block_nr * nf->blockSize;
	memset(nf->pageCache,0xff,nf->pageSize);
	for(i = 0; i < nf->blockSize; i += nf->pageSize) {
		addr = imgAddr + i;
		DiskImage_Write(nf->diskimage,addr,nf->pageCache,nf->pageSize);
	}
	make_busy(nf,nf->us_busy_erase);
}
/**
 *************************************************************************************
 * \fn static void read_current(void) 
 *************************************************************************************
 */
static uint8_t  
read_current(NandFlash *nf) 
{
	uint32_t col;
        switch(nf->areaPtr)
        {
		case APTR_AREA_A:
			col = nf->colAddr;
			break;

		case APTR_AREA_B:
			col = nf->colAddr | (nf->dataPageSize >> 1);
			break;

		case APTR_AREA_C:
			col = nf->colAddr + nf->dataPageSize;
			//fprintf(stderr,"C-Col %d %02x\n",col,nf->pageCache[col]);
			break;
		default:
		return 0;
	}
	if(col < nf->pageSize) {
		return nf->pageCache[col];
	} else {
		fprintf(stderr,"Nand Flash simulator bug: Outsize of Cache line\n");
		return 0;
	}
}

/**
 ********************************************************************************************
 * \fn static void cmd_read2_inc_addr(NandFlash *nf); 
 ********************************************************************************************
 */
static void
cmd_read2_inc_addr(NandFlash *nf) 
{
	nf->colAddr = (nf->colAddr + 1) & (nf->sparePageSize - 1);
//	fprintf(stderr,"C2 inc, cmd %02x\n",nf->cmd);
	
        if(nf->colAddr == 0) {
                nf->areaPtr = APTR_AREA_C;
                make_busy(nf,nf->us_busy_read);
                nf->rowAddr++;
		fill_cache(nf);
		//fprintf(stderr,"C2 done\n");
        }
}

/**
 ***************************************************************************************
 * \fn static void cmd_read1_inc_addr(NandFlash *nf); 
 ***************************************************************************************
 */
static void
cmd_read1_inc_addr(NandFlash *nf) {
	switch(nf->areaPtr) {
		case APTR_AREA_A:
			nf->colAddr = (nf->colAddr + 1) & ((nf->dataPageSize >> 1) - 1);
			if(nf->colAddr == 0) {
				nf->areaPtr = APTR_AREA_B;
			}
			break;

		case APTR_AREA_B:
			nf->colAddr = (nf->colAddr + 1) & ((nf->dataPageSize >> 1) - 1);
                        if(nf->colAddr == 0) {
                                nf->areaPtr = APTR_AREA_C;
                        }
                        break;

                case APTR_AREA_C:
                        nf->colAddr = (nf->colAddr + 1) & (nf->sparePageSize - 1);
                        if(nf->colAddr == 0) {
				/* 
				 * A check for being in the same block
				 * is missing here (page 29 1208)
				 */
                                nf->areaPtr = APTR_AREA_A;
                                nf->rowAddr++;
                                make_busy(nf,nf->us_busy_read);
				fprintf(stderr,"C done\n");
                                fill_cache(nf);
                        }
                        break;
        }
}

/**
 **********************************************************************************
 * \fn static uint8_t  ReadByte(NandFlash *nf);
 **********************************************************************************
 */
static uint8_t  
ReadByte(NandFlash *nf)
{
	uint8_t retval = 0xff;
	switch(nf->cmd) {
		case CMD_READ1_L:
		case CMD_READ1_H:
			retval = read_current(nf);
			cmd_read1_inc_addr(nf);
			break;

		case CMD_READ2:
			retval = read_current(nf);
			cmd_read2_inc_addr(nf);
			break;

		case CMD_READMULTPLANSTATUS:
		case CMD_READSTATUS:
			if(CycleCounter_Get() > nf->busy_until) {
				retval = 0xc0;
			} else {
				retval = 0x80;
			}
			break;

		case CMD_READID:
			switch(nf->cycleCtr) {
				case 0:
				case 1:
				case 2:
				case 3:
					retval = nf->id[nf->cycleCtr];
					break;
				default:
					retval = 0;
					break;
			}
			nf->cycleCtr++;
			break;

		default:
			break;
        }
	return retval;
}

/**
 ***********************************************************************************************
 * \fn static void cmd_write_inc_addr(NandFlash *nf); 
 ***********************************************************************************************
 */
static void
cmd_write_inc_addr(NandFlash *nf) {
	switch(nf->areaPtr) {
		case APTR_AREA_A:
			nf->colAddr = (nf->colAddr + 1) & ((nf->dataPageSize >> 1) - 1);
                        if(nf->colAddr == 0) {
                                nf->areaPtr = APTR_AREA_B;
                        }
                        break;

                case APTR_AREA_B:
			nf->colAddr = (nf->colAddr + 1) & ((nf->dataPageSize >> 1) - 1);
                        if(nf->colAddr == 0) {
                                nf->areaPtr = APTR_AREA_C;
                        }
                        break;

                case APTR_AREA_C:
                        nf->colAddr = (nf->colAddr + 1) & (nf->sparePageSize - 1);
                        if(nf->colAddr == 0) {
                                nf->areaPtr = APTR_AREA_A;
                        }
                        break;
        }
}

/**
 **********************************************************************************
 * \fn static void write_current(NandFlash *nf,uint8_t data); 
 **********************************************************************************
 */
static void
write_current(NandFlash *nf,uint8_t data) {
        uint32_t col;
        switch(nf->areaPtr)
        {
                case APTR_AREA_A:
                        col = nf->colAddr;
                        break;

                case APTR_AREA_B:
                        col = nf->colAddr | (nf->dataPageSize >> 1);
                        break;

                case APTR_AREA_C:
                        col = nf->colAddr + nf->dataPageSize;
                        break;

                default:
                        return;
        }
        if(col < nf->pageSize) {
                nf->pageCache[col] &= data;
                //nf->pageCache[col] = data;
        } else {
                fprintf(stderr,"Nand Flash simulator bug: Outsize of Cache line\n");
                return;
        }
}

/**
 **********************************************************************************
 * \fn static void WriteByte(NandFlash *nf,uint8_t value)
 **********************************************************************************
 */
static void
WriteByte(NandFlash *nf,uint8_t value)
{
	switch(nf->cmd) {
		case CMD_PP_SETUP:
			#if 0
			if(nf->addrState != ASTATE_IGNORE) {
				fprintf(stderr,"addr state %u\n",nf->addrState);
				exit(1);
			}
			#endif
			write_current(nf,value);
			cmd_write_inc_addr(nf);
			break;
	}
}
/**
 *********************************************************************************************
 * \fn uint8_t NandFlash_Read(NandFlash *nf,uint8_t signalLines)
 *********************************************************************************************
 */
uint8_t 
NandFlash_Read(NandFlash *nf,uint8_t signalLines)
{
	//fprintf(stderr,"NFR state %d, cmd %02x\n",nf->addrState,nf->cmd);
	//usleep(1000);
	if(signalLines == 0) {
		return ReadByte(nf);
	} else {
		fprintf(stderr,"%s: Unexpected signal lines 0x%02x\n",__func__,signalLines);
		return 0;
	}
}

static void
fix_ptr(NandFlash *nf) {
	if(nf->pointerCmd == CMD_READ1_H) {
		nf->areaPtr = APTR_AREA_A;
	}
}
static void
cmd_reset(NandFlash *nf) {
	nf->pointerCmd = CMD_READ1_L;
       	nf->areaPtr = APTR_AREA_A;
	nf->addrState = ASTATE_COL;
}
/**
 *******************************************************************************
 * \fn void NandFlash_Write(NandFlash *nf,uint8_t data,uint8_t signalLines)
 *******************************************************************************
 */
void 
NandFlash_Write(NandFlash *nf,uint8_t data,uint8_t signalLines)
{
	//fprintf(stderr,"NF state %d, sig %d, data 0x%02x\n",nf->addrState,signalLines,data);
	//usleep(1000);
	switch(signalLines) {
		case 0:
			WriteByte(nf,data);
			break;

		case NFCTRL_CLE:
			nf->cycleCtr = 0;
			switch(nf->cmd) {
				case CMD_COPYBACK:
				case CMD_PP_SETUP:
					if(data == CMD_PP) {
						program_page(nf);
						nf->rowAddr++; /* Is this correct ? */
						fix_ptr(nf);
					} else if(data == CMD_PP_DUMMY) {
						/* Do nothing */	
					}
					break;
				case CMD_ERASE_SETUP:
					if(data == CMD_ERASE) {
						erase_block(nf);
						fix_ptr(nf);
					}
					break;
				default:
					break;
			}
			nf->cmd = data;
			switch(data) {
				case CMD_RESET:
					cmd_reset(nf);
					break;

				case CMD_READ1_L:
					nf->pointerCmd = CMD_READ1_L;
                			nf->areaPtr = APTR_AREA_A;
					nf->addrState = ASTATE_COL;
					break;

				case CMD_READ1_H:
					nf->pointerCmd = CMD_READ1_H;
                			nf->areaPtr = APTR_AREA_B;
					nf->addrState = ASTATE_COL;
					break;

				case CMD_READ2:
					nf->pointerCmd = CMD_READ2;
                			nf->areaPtr = APTR_AREA_C;
					nf->addrState = ASTATE_COL;
					break;

				case CMD_ERASE_SETUP:
					nf->addrState = ASTATE_ROW0;
					break;
				case CMD_PP_SETUP:
					memset(nf->pageCache,0xff,nf->pageSize);
					switch(nf->pointerCmd) {

						case CMD_READ1_L:
                					nf->areaPtr = APTR_AREA_A;
							break;

						case CMD_READ1_H:
							break;

						case CMD_READ2:
                					nf->areaPtr = APTR_AREA_C;
							break;
					}
					break;

				case CMD_RNDIN:
					fprintf(stderr,"CMD 0x85 not implemented\n");
					exit(1);
					break;

				case CMD_RNDOUT:
					nf->addrState = ASTATE_COL;
					break;

				case CMD_READMULTPLANSTATUS:
				case CMD_READSTATUS:
				case CMD_READID:
				case CMD_PP:
				case CMD_ERASE:
					break;

				default:
					sleep(1);
					fprintf(stdout,"CMD 0x%02x not implemented\n",data);
					exit(1);
					nf->addrState = ASTATE_COL;
					break;
			}
			break;

		case NFCTRL_ALE:
			switch(nf->addrState) {
				case ASTATE_COL:
					//fprintf(stderr,"RNDOUT %02x, aptr %u before %u\n",data,nf->areaPtr,nf->colAddr);
                                	nf->colAddr = data & ((nf->dataPageSize >> 1) - 1);
                                	nf->addrState = ASTATE_ROW0;
					if(nf->cmd == CMD_RNDOUT) {
					}
					break;

                        	case ASTATE_ROW0:
					nf->rowAddr = data; 
					nf->addrState = ASTATE_ROW1;
					break;

				case ASTATE_ROW1:
					nf->rowAddr = nf->rowAddr | ((uint32_t)data << 8);
					nf->addrState = ASTATE_ROW2;
					break;

				case ASTATE_ROW2:
					nf->rowAddr = nf->rowAddr | ((uint32_t)data << 16);
					if(nf->addrBits < 33) {
						//fprintf(stderr,"New row %d, col %d\n",nf->rowAddr,nf->colAddr);
						switch(nf->cmd) {
							case CMD_READ1_L:
							case CMD_READ1_H:
							case CMD_READ2:
								fill_cache(nf);
								make_busy(nf,nf->us_busy_read);
								break;
							case CMD_PP_SETUP:
								break;
						}
						nf->addrState = ASTATE_IGNORE;
					} else {
						nf->addrState = ASTATE_ROW3;
					}
					break;

				case ASTATE_ROW3:
					nf->rowAddr = nf->rowAddr | ((uint32_t)data << 24);
					nf->addrState = ASTATE_IGNORE;
					// now fill the cache depending on command
					switch(nf->cmd) {
						case CMD_READ1_L:
						case CMD_READ1_H:
						case CMD_READ2:
							fill_cache(nf);
							make_busy(nf,nf->us_busy_read);
							break;
						case CMD_PP_SETUP:
							break;
					}
					break;
				case ASTATE_IGNORE:
					break;
			}
			break;
		default:
			break;
	}
}

/**
 ********************************************************************************************
 * \fn NandFlash * NandFlash_New(const char *name)
 ********************************************************************************************
 */
NandFlash *
NandFlash_New(const char *name)
{
	NandFlash *nf;
	char *type,*imagedir,*filename;
	int nr_types;
	int i;
	FlashDescription *fld;
	type = Config_ReadVar(name,"type");
        if(!type) {
                fprintf(stderr,"No type for Nand Flash \"%s\". Creating nothing\n",name);
                return NULL;
        }
        nr_types = sizeof(flash_types) / sizeof(FlashDescription);
        for(i=0;i < nr_types;i++) {
                fld = &flash_types[i];
                if(!strcmp(type,fld->type)) {
                        break;
                }
        }
        if(i == nr_types) {
                fprintf(stderr,"Flash type \"%s\" not found\n",type);
                exit(1);
        }
	imagedir = Config_ReadVar("global","imagedir");
        if(!imagedir) {
                fprintf(stderr,"No directory for NAND Flash diskimage given\n");
                return NULL;
        }
        filename = alloca(strlen(imagedir) + strlen(name) + 50);
	sprintf(filename,"%s/%s.img",imagedir,name);
	nf = sg_new(NandFlash);
	nf->us_busy_read = fld->us_busy_read;
	nf->us_busy_erase = fld->us_busy_erase;
	nf->rawSize = fld->rawSize;
	nf->addrBits = fld->addrbits;
	nf->colBits = fld->colbits;
	nf->blockBits = fld->blockBits;
	nf->dataPageSize = fld->dataPageSize;
	nf->sparePageSize = fld->sparePageSize;
	nf->pageSize = fld->dataPageSize + fld->sparePageSize;
	nf->dataBlockSize = fld->dataPageSize * fld->pagesPerBlock;
	nf->blockSize = (fld->dataPageSize + fld->sparePageSize) * fld->pagesPerBlock;
	nf->id = fld->id;
	nf->diskimage = DiskImage_Open(filename,nf->rawSize,DI_RDWR | DI_CREAT_FF | DI_SPARSE);
	nf->pageCache = sg_calloc(nf->pageSize);
	fprintf(stderr,"NAND flash \"%s\" of type %s created\n",name,type);
	return nf;
}
