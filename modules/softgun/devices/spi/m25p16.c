/**
 **********************************************************************************
 * ST Microelectronics M25P16 SPI flash simulation
 **********************************************************************************
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "signode.h"
#include "sgstring.h"
#include "m25p16.h"
#include "diskimage.h"
#include "configfile.h"
#include "cycletimer.h"

#define M25CMD_WREN             (0x06)	/* Write Enable                  */
#define M25CMD_WRDI             (0x04)	/* Write Disable                 */
#define M25CMD_RDID_9E          (0x9E)	/* Read Identification   */
#define M25CMD_RDID_9F          (0x9F)	/* Read Identification   */
#define         M25_SR_WIP      (1)
#define         M25_SR_WEL      (2)
#define         M25_SR_BP0      (4)
#define         M25_SR_BP1      (8)
#define         M25_SR_BP2      (0x10)
#define         M25_SR_SRWD     (0x80)
#define M25CMD_RDSR             (0x05)	/* Read Status Register  */
#define M25CMD_WRSR             (0x01)	/* Write Status Register */
#define M25CMD_WRLR		(0xe5)	/* Write to lock register */
#define M25CMD_RDLR		(0xe8)	/* Read lock register */
#define M25CMD_READ             (0x03)	/* Read Data                             */
#define M25CMD_FASTREAD 	(0x0b)	/* Read Data at higher speed */
#define M25CMD_DOFR 		(0x3b)	/* Dual output fast read */
#define M25CMD_ROTP		(0x4b)	/* Read OTP area */
#define M25CMD_POTP		(0x42)	/* Program OTP area */
#define M25CMD_DIFP		(0xa2)	/* Dual input fast program */
#define M25CMD_SSE		(0x20)	/* Sub sector erase */
#define M25CMD_PP               (0x02)	/* Page Program                  */
#define M25CMD_SE               (0xd8)	/* Sector Erase                  */
#define M25CMD_BE               (0xc7)	/* Bulk Erase                    */
#define M25CMD_DP               (0xb9)	/* Deep Power Down               */
#define M25CMD_RES              (0xab)	/* Release from Power down / Read signature */

#if 0
#define dbgprintf(...) fprintf(stderr,__VA_ARGS__)
#else
#define dbgprintf(...)
#endif

#define ISTATE_CMD		(0)
#define ISTATE_ADDR0		(1)
#define ISTATE_ADDR1		(2)
#define ISTATE_ADDR2		(3)
#define ISTATE_IDLE		(4)
#define ISTATE_WRSR		(5)
#define ISTATE_PP		(6)	/* Page program */
#define ISTATE_FASTREAD_DUMMY	(7)

#define OSTATE_IDLE	(0)
#define OSTATE_RDSR	(1)
#define OSTATE_READ	(2)
#define OSTATE_RDID_9F	(3)
#define OSTATE_RDID_9E	(4)

typedef struct SpifDescr {
	char *type;
	uint32_t size;
	uint32_t pagesize;
	uint32_t secsize;
	uint32_t subsecsize;
	uint32_t sect_erasetime;
	uint32_t subsect_erasetime;
	uint32_t bulk_erasetime;
	uint32_t pptime;
	uint8_t rdid_0x9f[20];
	uint8_t rdid_0x9e[3];
} SpifDescr;

SpifDescr flash_types[] = {
	{
	 .type = "M25P16",
	 .size = 1 << 21,
	 .pagesize = 256,
	 .secsize = 1 << 16,
	 .subsecsize = 0,
	 .sect_erasetime = 1000000,
	 .subsect_erasetime = 0,
	 .bulk_erasetime = 17000000,
	 .pptime = 1400,
	 .rdid_0x9f = {0x20, 0x20, 0x15, 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	 .rdid_0x9e = {0xff, 0xff, 0xff},
	 },
	{
	 .type = "M25PX32",
	 .size = 1 << 22,
	 .pagesize = 256,
	 .secsize = 1 << 16,
	 .subsecsize = 4096,
	 .sect_erasetime = 1000000,
	 .subsect_erasetime = 70000,
	 .bulk_erasetime = 34000000,
	 .pptime = 800,
	 .rdid_0x9f = {0x20, 0x71, 0x16, 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	 .rdid_0x9e = {0x20, 0x71, 0x16},
	 }
};

typedef struct M25P16 {
	SigNode *sigMosi;	/* connected to MOSI */
	SigNode *sigSck;	/* connected to SCK */
	SigTrace *sckTrace;
	SigNode *sigCsN;	/* connected to SPI CS */
	SigTrace *CsNTrace;
	SigNode *sigMiso;	/* connected to SPI MISO */
	int shiftin_cnt;
	int shiftout_cnt;
	uint8_t shift_in;
	uint8_t shift_out;

	uint8_t reg_sr;

	uint8_t cmd;		/* Only used when more than one byte is used */
	int write_enabled;
	CycleCounter_t busy_until;
	/* The input state machine */
	int istate;
	int ostate;
	int rdid_rp;

	uint32_t addr;
	DiskImage *diskimage;
	uint8_t *data;
	uint32_t size;
	uint32_t pagesize;
	uint32_t secsize;
	uint32_t subsecsize;
	uint32_t sect_erasetime;
	uint32_t subsect_erasetime;
	uint32_t bulk_erasetime;
	uint32_t pptime;
	uint8_t rdid_0x9f[20];
	uint8_t rdid_0x9e[3];
} M25Flash;

static void
make_busy(M25Flash * mf, uint32_t useconds)
{
	mf->busy_until = CycleCounter_Get() + MicrosecondsToCycles(useconds);
	mf->reg_sr |= M25_SR_WIP;
}

/**
 *************************************************
 * \fn static char * get_cmdstr(uint8_t cmd); 
 * Convert a command to a text string.
 *************************************************
 */
__UNUSED__ static char *
get_cmdstr(uint8_t cmd) {
	char *str;
	switch(cmd) {
		case M25CMD_WREN:
			str = "WREN";
			break;
		case M25CMD_WRDI:
			str = "WRDI";
			break;
		case M25CMD_RDID_9E:
			str = "RDID_9E";
			break;
		case M25CMD_RDID_9F:
			str = "RDID_9F";
			break;
		case M25CMD_RDSR:
			str = "RDSR";
			break;
		case M25CMD_WRSR:
			str = "WRSR";
			break;
		case M25CMD_WRLR:
			str = "WRLR";
			break;
		case M25CMD_RDLR:
			str = "RDLR";
			break;
		case M25CMD_READ:
			str = "READ";
			break;
		case M25CMD_FASTREAD:
			str = "FASTREAD";
			break;
		case M25CMD_DOFR:
			str = "DOFR";
			break;
		case M25CMD_ROTP:
			str = "ROTP";
			break;
		case M25CMD_POTP:
			str = "POTP";
			break;
		case M25CMD_DIFP:
			str = "DIFP";
			break;
		case M25CMD_SSE:
			str = "SSE";
			break;
		case M25CMD_PP:
			str = "PP";
			break;
		case M25CMD_SE:
			str = "SE";
			break;
		case M25CMD_BE:
			str = "BE";
			break;
		case M25CMD_DP:
			str = "DP";
			break;
		case M25CMD_RES:
			str = "RES";
			break;
		default: 
			str = "Unknown";
			break;
	};
	return str;
}
/**
 ************************************************************************
 * \fn static void spi_byte_in(M25Flash *mf,uint8_t data)
 * The main state machine eating the bytes received on the SPI
 ************************************************************************
 */
static void
spi_byte_in(M25Flash * mf, uint8_t data)
{
	switch (mf->istate) {
	    case ISTATE_CMD:
		    if (CycleCounter_Get() < mf->busy_until) {
			    if (data != M25CMD_RDSR) {
				    fprintf(stderr, "Got cmd %02x while busy\n", data);
				    return;
			    }
		    }
		    mf->cmd = data;
		    dbgprintf("CMD %s\n",get_cmdstr(mf->cmd));
		    switch (data) {
			case M25CMD_WREN:
				mf->write_enabled = 1;
				/* 
				 ************************************************
				 * The manual says that WREN must be followed 
				 * by a a CS high before it is effective.
				 ************************************************
				 */
				mf->istate = ISTATE_IDLE;
				break;

			case M25CMD_WRDI:
				mf->write_enabled = 0;
				mf->istate = ISTATE_IDLE;
				break;

			case M25CMD_RDID_9E:
				mf->rdid_rp = 0;
				mf->ostate = OSTATE_RDID_9E;
				mf->istate = ISTATE_IDLE;
				break;

			case M25CMD_RDID_9F:
				mf->rdid_rp = 0;
				mf->ostate = OSTATE_RDID_9F;
				mf->istate = ISTATE_IDLE;
				break;

			case M25CMD_RDSR:
				mf->ostate = OSTATE_RDSR;
				mf->istate = ISTATE_IDLE;
				break;
			case M25CMD_WRSR:
				mf->istate = ISTATE_WRSR;
				break;
			case M25CMD_READ:
				mf->istate = ISTATE_ADDR0;
				break;
			case M25CMD_FASTREAD:
				mf->istate = ISTATE_ADDR0;
				break;
			case M25CMD_PP:
				mf->istate = ISTATE_ADDR0;
				break;
			case M25CMD_SE:
				mf->istate = ISTATE_ADDR0;
				break;
			case M25CMD_SSE:
				if (mf->subsecsize) {
					mf->istate = ISTATE_ADDR0;
				} else {
					mf->istate = ISTATE_IDLE;
				}
				break;
			case M25CMD_BE:
				if (mf->write_enabled) {
					memset(mf->data, 0xff, mf->size);
					/* Manual says 17 to 40 seconds */
					make_busy(mf, mf->bulk_erasetime);
				}
				mf->istate = ISTATE_IDLE;
				break;
			case M25CMD_DP:
				break;
			case M25CMD_RES:
				break;
			default:
				fprintf(stderr, "Spiflash command 0x%02x not implemented s %d\n",
					data, mf->istate);
		    }
		    break;
	    case ISTATE_ADDR0:
		    mf->addr = data;
		    mf->istate = ISTATE_ADDR1;
		    break;
	    case ISTATE_ADDR1:
		    mf->addr = (mf->addr << 8) | data;
		    mf->istate = ISTATE_ADDR2;
		    break;
	    case ISTATE_ADDR2:
		    mf->addr = (mf->addr << 8) | data;
		    switch (mf->cmd) {
			case M25CMD_READ:
				mf->ostate = OSTATE_READ;
				mf->istate = ISTATE_IDLE;
				break;
			case M25CMD_FASTREAD:
				mf->istate = ISTATE_FASTREAD_DUMMY;
				break;
			case M25CMD_PP:
				mf->istate = ISTATE_PP;
				break;
			case M25CMD_SE:
				mf->addr = mf->addr & ~(mf->secsize - 1);
				if (mf->write_enabled && ((mf->addr + mf->secsize) <= mf->size)) {
					memset(mf->data + mf->addr, 0xff, mf->secsize);
					/* Manual says 1sec typ, max 3 sec */
					make_busy(mf, mf->sect_erasetime);
					dbgprintf("SE: 0x%08x\n",mf->addr);
#if 0
					if(mf->addr == 0) {
						fprintf(stderr,"Sector 0 Erase %llu\n",CycleCounter_Get());
						//exit(0);
						sleep(10);
						fprintf(stderr,"Sector 0 Erase %llu\n",CycleCounter_Get());
					}
#endif
				} else {
					fprintf(stderr, "Can not erase sector at %08x\n", mf->addr);
				}
				//mf->state = SECTOR_ERASE; 
				break;

			case M25CMD_SSE:
				mf->addr = mf->addr & ~(mf->subsecsize - 1);
				if (mf->subsect_erasetime == 0) {
					fprintf(stderr, "Got nonexisting CMD SubSectorErase\n");
				} else if (mf->write_enabled
					   && ((mf->addr + mf->subsecsize) <= mf->size)) {
					memset(mf->data + mf->addr, 0xff, mf->subsecsize);
					make_busy(mf, mf->subsect_erasetime);
				} else {
					fprintf(stderr, "Can not erase subsector at %08x\n",
						mf->addr);
				}
				break;
			default:
				fprintf(stderr, "Unknown cmd %02x in ADDR2 state\n", mf->cmd);
		    }
		    break;

	    case ISTATE_FASTREAD_DUMMY:
		    mf->ostate = OSTATE_READ;
		    break;

	    case ISTATE_WRSR:
		    if (mf->write_enabled) {
			    mf->reg_sr = (mf->reg_sr & M25_SR_WIP) | (data & ~M25_SR_WIP);
		    }
		    break;

	    case ISTATE_PP:
		    if (mf->write_enabled && (mf->addr < mf->size)) {
			    mf->data[mf->addr] = mf->data[mf->addr] & data;
			    mf->addr = ((mf->addr + 1) & (mf->pagesize - 1))
				| (mf->addr & ~(mf->pagesize - 1));
		    } else {
			    fprintf(stderr,"Write outside of SPI flash %08x\n",mf->addr);
		    }
		    break;

	    case ISTATE_IDLE:
		    break;
	    default:
		    fprintf(stderr, "M25 in unexpected input state %d\n", mf->istate);

	}
}

/**
 ************************************************************************
 * \fn static inline uint8_t spi_fetch_next_byte(M25Flash *mf)
 * The output generator.
 ************************************************************************
 */
static inline uint8_t
spi_fetch_next_byte(M25Flash * mf)
{
	uint8_t next;
	switch (mf->ostate) {
	    case OSTATE_IDLE:
		    next = 0xff;
		    break;

	    case OSTATE_RDSR:
		    if (CycleCounter_Get() >= mf->busy_until) {
			    mf->reg_sr &= ~M25_SR_WIP;
		    }
		    next = mf->reg_sr;
		    break;

	    case OSTATE_READ:
		    if (mf->addr >= mf->size) {
			    fprintf(stderr,"Read outside of SPI flash %08x\n",mf->addr);
			    mf->addr = mf->addr % mf->size;
		    }
		    next = mf->data[mf->addr];
		    mf->addr++;
		    if (mf->addr == mf->size) {
			    mf->addr = 0;
		    }
		    break;

	    case OSTATE_RDID_9E:
		    if (mf->rdid_rp < sizeof(mf->rdid_0x9e)) {
			    next = mf->rdid_0x9e[mf->rdid_rp++];
		    } else {
				/** ?? X32 is 0x00 */
			    next = 0xff;
		    }
		    break;

	    case OSTATE_RDID_9F:
		    if (mf->rdid_rp < sizeof(mf->rdid_0x9f)) {
			    next = mf->rdid_0x9f[mf->rdid_rp++];
		    } else {
			    next = 0;
		    }
		    break;

	    default:
		    next = 0;
		    fprintf(stderr, "M25 in unexpected output state %d\n", mf->ostate);
	}
	//fprintf(stderr,"Out %02x in state %d\n",next,mf->ostate);
	return next;
}

static void
spi_clk_change(SigNode * node, int value, void *clientData)
{
	M25Flash *mf = (M25Flash *) clientData;
	//fprintf(stderr,"Shift cnt %d\n",mf->shiftin_cnt);
	if (value == SIG_HIGH) {
		if (SigNode_Val(mf->sigMosi) == SIG_HIGH) {
			mf->shift_in = (mf->shift_in << 1) | 1;
		} else {
			mf->shift_in = (mf->shift_in << 1);
		}
		mf->shiftin_cnt++;
		if (mf->shiftin_cnt == 8) {
			spi_byte_in(mf, mf->shift_in);
			mf->shiftin_cnt = 0;
		}
	} else if (value == SIG_LOW) {
		if (mf->shiftout_cnt == 0) {
			mf->shift_out = spi_fetch_next_byte(mf);
		}
		if (mf->shift_out & 0x80) {
			SigNode_Set(mf->sigMiso, SIG_HIGH);
		} else {
			SigNode_Set(mf->sigMiso, SIG_LOW);
		}
		mf->shift_out <<= 1;
		mf->shiftout_cnt++;
		if (mf->shiftout_cnt == 8) {
			mf->shiftout_cnt = 0;
		}
	}
	return;
}

static void
spi_cs_change(SigNode * node, int value, void *clientData)
{
	M25Flash *mf = (M25Flash *) clientData;
	if (value == SIG_LOW) {
		mf->shift_in = 0;
		mf->shiftin_cnt = 0;
		mf->istate = ISTATE_CMD;
		mf->ostate = OSTATE_IDLE;

		mf->shift_out = 0xff;
		/** 
		 ***********************************************************
		 * If clock is already low shiftout first bit immediately 
		 ***********************************************************
		 */
		if (SigNode_Val(mf->sigSck) == SIG_LOW) {
			mf->shift_out = spi_fetch_next_byte(mf);
			if (mf->shift_out & 0x80) {
				SigNode_Set(mf->sigMiso, SIG_HIGH);
			} else {
				SigNode_Set(mf->sigMiso, SIG_LOW);
			}
			mf->shiftout_cnt = 1;
			mf->shift_out <<= 1;
		} else {
			mf->shiftout_cnt = 0;
		}
		if (mf->sckTrace) {
			fprintf(stderr, "Bug: clock trace already exists\n");
			return;
		}
		mf->sckTrace = SigNode_Trace(mf->sigSck, spi_clk_change, mf);
	} else {
		if (mf->istate == ISTATE_PP) {
			make_busy(mf, mf->pptime);	/* Manual typ 1400 max 5000 */
		}
		SigNode_Set(mf->sigMiso, SIG_OPEN);
		if (mf->sckTrace) {
			SigNode_Untrace(mf->sigSck, mf->sckTrace);
			mf->sckTrace = NULL;
		}
	}
	//dbgprintf("CS of the Dataflash %d\n", value);
	//fprintf(stdout,"CS %d\n",value);
	return;
}

void
M25P16_FlashNew(const char *name)
{
	M25Flash *mf = sg_new(M25Flash);
	char *dirname, *imagename;
	char *flashtypestr;
	int i;
	SpifDescr *flash_descr;
	mf->sigSck = SigNode_New("%s.sck", name);
	mf->sigMosi = SigNode_New("%s.mosi", name);
	mf->sigMiso = SigNode_New("%s.miso", name);
	mf->sigCsN = SigNode_New("%s.ncs", name);
	if (!mf->sigSck || !mf->sigMosi || !mf->sigMiso || !mf->sigCsN) {
		fprintf(stderr, "Can not create signal lines for SPI flash \"%s\"\n", name);
		exit(1);
	}
	mf->CsNTrace = SigNode_Trace(mf->sigCsN, spi_cs_change, mf);
	flashtypestr = Config_ReadVar(name, "type");
	if (!flashtypestr) {
		fprintf(stderr, "No type given for SPI flash \"%s\"\n", name);
		exit(1);
	}
	for (i = 0; i < array_size(flash_types); i++) {
		flash_descr = &flash_types[i];
		if (strcmp(flash_descr->type, flashtypestr) == 0) {
			break;
		}
	}
	if (i == array_size(flash_types)) {
		fprintf(stderr, "Flash type \"%s\" for \"%s\" not available\n", flashtypestr, name);
		exit(1);
	}
	mf->size = flash_descr->size;
	mf->pagesize = flash_descr->pagesize;
	mf->secsize = flash_descr->secsize;
	mf->subsecsize = flash_descr->subsecsize;
	mf->sect_erasetime = flash_descr->sect_erasetime;
	mf->bulk_erasetime = flash_descr->bulk_erasetime;
	mf->subsect_erasetime = flash_descr->subsect_erasetime;
	mf->pptime = flash_descr->pptime;
	memcpy(mf->rdid_0x9f, flash_descr->rdid_0x9f, sizeof(mf->rdid_0x9f));
	memcpy(mf->rdid_0x9e, flash_descr->rdid_0x9e, sizeof(mf->rdid_0x9e));
	Config_ReadUInt32(&mf->sect_erasetime, name, "sect_erasetime");
	Config_ReadUInt32(&mf->bulk_erasetime, name, "bulk_erasetime");
	Config_ReadUInt32(&mf->pptime, name, "pptime");
	dirname = Config_ReadVar("global", "imagedir");
	if (dirname) {
		imagename = alloca(strlen(dirname) + strlen(name) + 20);
		sprintf(imagename, "%s/%s.img", dirname, name);
		mf->diskimage = DiskImage_Open(imagename, mf->size, DI_RDWR | DI_CREAT_FF);
		if (!mf->diskimage) {
			fprintf(stderr, "Failed to open M25P16 flash image\n");
		} else {
			mf->data = DiskImage_Mmap(mf->diskimage);
		}
	}
	if (!mf->data) {
		mf->data = sg_calloc(mf->size);
		memset(mf->data, 0xff, mf->size);
	}
	fprintf(stderr, "M25C16 Spi Flash \"%s\" created\n", name);
}
