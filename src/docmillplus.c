/*
 * ----------------------------------------------------
 * Emulation of Disc On Chip Millenium Plus
 *
 * State:
 *	nothing is working
 *
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 * ----------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <bus.h>
#include "sgstring.h"

#define MODE_RESET  (0)
#define MODE_NORMAL (1)
#define MODE_DEEP_POWER_DOWN (2)

#define REG_CHIP_ID 		(0x1000)
#define REG_NOP	    		(0x1002)
#define REG_TEST    		(0x1004)
#define REG_CTRL    		(0x1006)
#define		CTRL_MODE_RESET 		(0)
#define		CTRL_MODE_NORMAL 		(1)
#define		CTRL_MODE_DEEP_POWER_DOWN 	(2)
#define		CTRL_MDWREN			(1<<2)
#define		CTRL_BDET			(1<<3)
#define		CTRL_RST_LAT			(1<<4)

#define REG_ACCESS_STAT		(0x1008)
#define REG_DEVID_SEL		(0x1008)
#define REG_CONFIG		(0x100a)
#define REG_OUT_CTRL		(0x100c)
#define REG_INTR_CTRL		(0x100e)
#define REG_FLASH_CTRL		(0x1020)
#define REG_FLASH_SELECT	(0x1022)
#define REG_FLASH_CMD		(0x1024)
#define REG_FLASH_ADDRESS	(0x1026)
#define REG_FLASH_DATA0		(0x1028)
#define REG_FLASH_DATA1		(0x1029)
#define REG_READ_PIPE_INIT	(0x102a)
#define REG_LAST_DATA_READ	(0x102c)
#define REG_LAST_DATA_READ1	(0x102d)
#define REG_WRITE_PIPE_TERM	(0x102e)
#define REG_ECC_SYNDROME0	(0x1040)
#define REG_ECC_SYNDROME1	(0x1041)
#define REG_ECC_SYNDROME2	(0x1042)
#define REG_ECC_SYNDROME3	(0x1043)
#define REG_ECC_SYNDROME4	(0x1044)
#define REG_ECC_SYNDROME5	(0x1045)
#define REG_ECC_CONF		(0x1046)
#define REG_TOGGLE		(0x1046)
#define REG_DWNLD_STAT		(0x1074)
#define REG_CTRL_CONFIRM	(0x1076)

/*
 * Standard NAND flash commands
 */
#define NAND_CMD_READ0          0
#define NAND_CMD_READ1          1
#define NAND_CMD_PAGEPROG       0x10
#define NAND_CMD_READOOB        0x50
#define NAND_CMD_ERASE1         0x60
#define NAND_CMD_STATUS         0x70
#define NAND_CMD_STATUS_MULTI   0x71
#define NAND_CMD_SEQIN          0x80
#define NAND_CMD_READID         0x90
#define NAND_CMD_ERASE2         0xd0
#define NAND_CMD_RESET          0xff

/* Extended commands for large page devices */
#define NAND_CMD_READSTART      0x30
#define NAND_CMD_CACHEDPROG     0x15
	
typedef struct DocMP {
	BusDevice bdev;
	uint8_t *host_mem;
	int mode;
	uint32_t size;
	uint8_t chip_id;
	uint8_t nop;

	uint8_t ctrl_wbuf;
	int ctrl_write_pending;
	uint8_t ctrl_reg;

	uint8_t devid_sel;
	uint8_t config;
	uint8_t out_ctrl;
	uint8_t intr_ctrl;
	uint8_t toggle_reg;
	uint32_t flash_addr;
	int flash_addr_byte;
	uint8_t boot_block[1024];
} DocMP;


static uint16_t
ctrl_read(void *clientData,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	return doc->ctrl_reg;
}

static void
ctrl_write(void *clientData,uint16_t value,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	doc->ctrl_wbuf = value;
	doc->ctrl_write_pending=1;
}

static uint16_t
ctrl_confirm_read(void *clientData,uint32_t address,int rqlen) {
//	DocMP *doc = clientData;

	fprintf(stderr,"Not implemented\n");
	return 0;
}

static void
ctrl_confirm_write(void *clientData,uint16_t value,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	uint8_t val;
	if(!doc->ctrl_write_pending || (((value & 0xff)^0xff) != doc->ctrl_wbuf)) {
		fprintf(stderr,"DOC CTRL: Incorrect confirmation\n");
		doc->ctrl_write_pending = 0;
		return;
	} 
	doc->ctrl_write_pending = 0;
	val = doc->ctrl_wbuf;
	if(val & CTRL_BDET) {
		doc->ctrl_reg &= ~CTRL_BDET;
	} 
	if(val & CTRL_RST_LAT) {
		doc->ctrl_reg &= ~CTRL_RST_LAT;
	} 
	if((val & CTRL_MDWREN) || (doc->ctrl_reg & CTRL_MDWREN)) {
		doc->ctrl_reg = (doc->ctrl_reg & ~3) | (val & 3) | CTRL_MDWREN;
		doc->mode = val & 3;
		//update_memmap;
	}
	
}

static uint16_t
devid_sel_read(void *clientData,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	return doc->devid_sel;
}

static void
devid_sel_write(void *clientData,uint16_t value,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	doc->devid_sel = value & 3;
	if(doc->devid_sel != 0) {
		fprintf(stderr,"Warning devidsel not implemented\n");
	}
}

static uint16_t
config_read(void *clientData,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	return doc->config;
}

static void
config_write(void *clientData,uint16_t value,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	doc->config = value & 0x30;
}
static uint16_t
out_ctrl_read(void *clientData,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	return doc->out_ctrl;
}

static void
out_ctrl_write(void *clientData,uint16_t value,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	if(value & 8) {
		doc->out_ctrl |= 8;
		fprintf(stderr,"Warning: SLOCK not implemented\n");
	}
}
static uint16_t
intr_ctrl_read(void *clientData,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	return doc->intr_ctrl;
}

static void
intr_ctrl_write(void *clientData,uint16_t value,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	if(value) {
		fprintf(stderr,"DOC: Interrupt control not implemented\n"); 
	}
	doc->intr_ctrl = value;
	// update_interrupts(doc);
}

static uint16_t
toggle_read(void *clientData,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	uint16_t value = doc->toggle_reg;
	doc->toggle_reg ^= 4;
	return value;
}

static void
toggle_write(void *clientData,uint16_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"DOC: Ignore write to toggle register\n"); 
}
static uint16_t
flash_cmd_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"Flash-CMD read not implemented\n");
	return 0;
}

static void
flash_cmd_write(void *clientData,uint16_t value,uint32_t address,int rqlen) {
	switch(value) {
		case NAND_CMD_READ0:
		case NAND_CMD_READ1:
		case NAND_CMD_PAGEPROG:
		case NAND_CMD_READOOB:
		case NAND_CMD_ERASE1:
		case NAND_CMD_STATUS:
		case NAND_CMD_STATUS_MULTI:
		case NAND_CMD_SEQIN:
		case NAND_CMD_READID:
		case NAND_CMD_ERASE2:
		case NAND_CMD_RESET:

		/* Extended commands for large page devices */
		case NAND_CMD_READSTART:
		case NAND_CMD_CACHEDPROG:
		default:
			fprintf(stderr,"DOC: FlashCmd 0x%04x not implemented\n",value); 
	}
}
static uint16_t
flash_addr_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"Flash-Addr read not implemented\n");
	return 0;
}

static void
flash_addr_write(void *clientData,uint16_t value,uint32_t address,int rqlen) {
	DocMP *doc = clientData;
	if(doc->flash_addr_byte==0) {
		doc->flash_addr = value;
	} else if (doc->flash_addr_byte == 1) {
		doc->flash_addr = (doc->flash_addr & 0xffff00ff) | ((value & 0x0ff)<<8);
	} else if (doc->flash_addr_byte == 2) {
		doc->flash_addr = (doc->flash_addr & 0xff00ffff) | ((value & 0x0ff)<<16);
	} else {
		fprintf(stderr,"Unknown Cycle %d in Flash address write\n",doc->flash_addr_byte);
	}
	doc->flash_addr_byte++;
}


static void
DocMP_write(void *clientData,uint32_t value, uint32_t mem_addr,int rqlen) 
{
	BusDevice *bdev = clientData;
	DocMP *doc = bdev->owner;
	uint32_t reg = mem_addr & 0x1fff;
	if(doc->mode == MODE_RESET) {
		if((reg != REG_CTRL) && (reg != REG_CTRL_CONFIRM)) {
			doc->ctrl_write_pending = 0;
			return;
		}
	}
	if(reg!=REG_CTRL_CONFIRM) {
		doc->ctrl_write_pending = 0;
		fprintf(stderr,"DOC CTRL: not confirmed\n");
	}
	switch(reg) {
		case REG_CHIP_ID:
			break;

		case REG_NOP:
			doc->nop=value;
			break;

		case REG_TEST:
			break;

		case REG_CTRL:
			if(rqlen<=2) {
				ctrl_write(doc,value,mem_addr,rqlen);
			}
			break;
		case REG_DEVID_SEL:
			devid_sel_write(doc,value,mem_addr,rqlen);
			break;
		case REG_CONFIG:	
			config_write(doc,value,mem_addr,rqlen);
			break;
		case REG_OUT_CTRL:
			out_ctrl_write(doc,value,mem_addr,rqlen);
			break;

		case REG_INTR_CTRL:
			break;

		case REG_FLASH_CTRL:
			break;
		case REG_FLASH_SELECT:
			break;
		case REG_FLASH_CMD:
			flash_cmd_write(doc,value,mem_addr,rqlen);
			break;
		case REG_FLASH_ADDRESS:
			break;
		case REG_FLASH_DATA0:
			break;
		case REG_FLASH_DATA1:
			break;
		case REG_READ_PIPE_INIT:
			break;
		case REG_LAST_DATA_READ:
			break;
		case REG_LAST_DATA_READ1:
			break;
		case REG_WRITE_PIPE_TERM:
			break;
		case REG_ECC_SYNDROME0:
			break;
		case REG_ECC_SYNDROME1:
			break;
		case REG_ECC_SYNDROME2:
			break;
		case REG_ECC_SYNDROME3:
			break;
		case REG_ECC_SYNDROME4:
			break;
		case REG_ECC_SYNDROME5:
			break;

		case REG_DWNLD_STAT:
			break;

		case REG_TOGGLE: // also REG_ECC_CONF
			return toggle_write(doc,value,mem_addr,rqlen);
			break;
		case REG_CTRL_CONFIRM:
			ctrl_confirm_write(doc,value,mem_addr,rqlen);
			break;
		default:
		fprintf(stderr,"Not implemented\n");
	}
	

}

static uint32_t
DocMP_read(void *clientData,uint32_t mem_addr,int rqlen) 
{
	BusDevice *bdev = clientData;
	DocMP *doc = bdev->owner;
	uint32_t reg = mem_addr & 0x1fff;
	if((reg<0x400) || (reg>0x1800)) {
		if(rqlen==4) {
			return *(uint32_t*)(doc->boot_block+(reg & 0x3fe));
		} else if(rqlen==2) {
			return *(uint16_t*)(doc->boot_block+(reg & 0x3fe));
		} else if(rqlen==1) {
			return *(uint8_t*)(doc->boot_block+(reg & 0x3ff));
		}
	}
	if(doc->mode == MODE_RESET) {
		doc->ctrl_write_pending=0;
		return 0;
	}
	if(doc->ctrl_write_pending) {
		fprintf(stderr,"DOC CTRL: not confirmed\n");
	}
	doc->ctrl_write_pending = 0;
	switch(reg) {
		case REG_CHIP_ID:
			return doc->chip_id;
			break;

		case REG_NOP:
			return doc->nop;
			break;
		case REG_TEST:
			break;
			
		case REG_CTRL:
			break;

		case REG_DEVID_SEL:
			return devid_sel_read(doc,mem_addr,rqlen);
			break;

		case REG_CONFIG:	
			return config_read(doc,mem_addr,rqlen);
			break;
		case REG_OUT_CTRL:
			return out_ctrl_read(doc,mem_addr,rqlen);
			break;
		case REG_INTR_CTRL:
			break;

		case REG_FLASH_CTRL:
			break;
		case REG_FLASH_SELECT:
			break;
		case REG_FLASH_CMD:
			return flash_cmd_read(doc,mem_addr,rqlen);
			break;
		case REG_FLASH_ADDRESS:
			break;
		case REG_FLASH_DATA0:
			break;
		case REG_FLASH_DATA1:
			break;
		case REG_READ_PIPE_INIT:
			break;
		case REG_LAST_DATA_READ:
			break;
		case REG_LAST_DATA_READ1:
			break;
		case REG_WRITE_PIPE_TERM:
			break;
		case REG_ECC_SYNDROME0:
			break;
		case REG_ECC_SYNDROME1:
			break;
		case REG_ECC_SYNDROME2:
			break;
		case REG_ECC_SYNDROME3:
			break;
		case REG_ECC_SYNDROME4:
			break;
		case REG_ECC_SYNDROME5:
			break;

		case REG_DWNLD_STAT:
			break;

		case REG_TOGGLE: // also REG_ECC_CONF
			return toggle_read(doc,mem_addr,rqlen);
			break;
		case REG_CTRL_CONFIRM:
			break;
		default:
		fprintf(stderr,"Not implemented\n");
	}
	return 0x47110815;		
}

static void
DocMP_Map(void *module_owner,uint32_t base,uint32_t mapsize,uint32_t flags) 
{
        DocMP *doc = module_owner;
        IOH_NewRegion(base,mapsize,DocMP_read,DocMP_write,doc);
}

static void
DocMP_UnMap(void *module_owner,uint32_t base,uint32_t mapsize) 
{
        IOH_DeleteRegion(base,mapsize);
}

/*
 * ---------------------------------------
 * Create a new Disk on Chip 
 * ---------------------------------------
 */
BusDevice *
DocMP_New(const char *doc_name) 
{
	DocMP *doc = sg_new(sizeof(DocMP));
	doc->size = 16 * 1024 * 1024;
	doc->host_mem = sg_calloc(doc->size);
	doc->chip_id = 0x41; // 16 MB
	doc->ctrl_reg = 0x10;
	doc->out_ctrl = 0x01;
	doc->toggle_reg = 0x82;
	doc->bdev.first_mapping=NULL;
        doc->bdev.Map=DocMP_Map;
       	doc->bdev.UnMap=DocMP_UnMap;
        doc->bdev.owner=doc;
        doc->bdev.hw_flags=MEM_FLAG_READABLE;
        return &doc->bdev;
}
