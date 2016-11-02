/*
 *************************************************************************************************
 *
 * Emulation of S3C2410 Nandflash Controller
 *
 * state: Not implemented
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


#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <bus.h>
#include <signode.h>
#include <configfile.h>
#include <s3c2410_nand.h>
#include <sgstring.h>

/* Base address is 0x4e000000 */
#define NFC_NFCONF(base)	((base)+0x00)
#define		NFCONF_NFEN	(1<<15)
#define		NFCONF_INIECC	(1<<12)
#define		NFCONF_NFMCE	(1<<11)	
#define		NFCONF_TACLS_MASK 	(7<<8)
#define		NFCONF_TACLS_SHIFT	(8)
#define 	NFCONF_TWRPH0_MASK	(7<<4)
#define		NFCONF_TWRPH0_SHIFT	(4)
#define		NFCONF_TWRPH1_MASK	(7)
#define		NFCONF_TWRPH1_SHIFT	(0)

#define NFC_NFCMD(base)		((base)+0x04)
#define NFC_NFADDR(base)	((base)+0x08)
#define NFC_NFDATA(base)	((base)+0x0c)
#define	NFC_NFSTAT(base)	((base)+0x10)
#define	NFC_NFECC(base)		((base)+0x14)


typedef struct S3Nfc {
	BusDevice bdev;
	SigNode *ncon;
	//NandFlashController nfc;
	uint16_t nfconf;
	uint16_t nfcmd;
	uint16_t nfaddr;
	uint16_t nfdata;
	uint16_t nfstat;
	uint32_t nfecc;
} S3Nfc;

static uint32_t
nfconf_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"S3NFC: read register %08x not implemented\n",address);
	return 0;
}

static void
nfconf_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"S3NFC: write register %08x not implemented\n",address);
}

static uint32_t
nfcmd_read(void *clientData,uint32_t address,int rqlen)
{
	S3Nfc *snfc = (S3Nfc *)clientData;
	return snfc->nfcmd;
}

static void
nfcmd_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	S3Nfc *snfc = (S3Nfc *)clientData;
	snfc->nfcmd = value & 0xff;
}

static uint32_t
nfaddr_read(void *clientData,uint32_t address,int rqlen)
{
	S3Nfc *snfc = (S3Nfc *)clientData;
	return snfc->nfaddr;
}

static void
nfaddr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	S3Nfc *snfc = (S3Nfc *)clientData;
	snfc->nfaddr = value & 0xff;
}

static uint32_t
nfdata_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"S3NFC: read register %08x not implemented\n",address);
	return 0;
}

static void
nfdata_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"S3NFC: write register %08x not implemented\n",address);
}

static uint32_t
nfstat_read(void *clientData,uint32_t address,int rqlen)
{
	/*S3Nfc *snfc = (S3Nfc *) clientData; */
	/* Should check the ready / busy pin here */
	return 1;
}

static void
nfstat_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"S3NFC: write register %08x not implemented\n",address);
}

static uint32_t
nfecc_read(void *clientData,uint32_t address,int rqlen)
{
	S3Nfc *snfc = (S3Nfc*)clientData;
	return snfc->nfecc;
}

static void
nfecc_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"S3NFC: Writing to readonly ECC register\n");
}


static void
S3Nfc_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	S3Nfc *snfc = (S3Nfc*) owner;
        IOH_New32(NFC_NFCONF(base),nfconf_read,nfconf_write,snfc);
        IOH_New32(NFC_NFCMD(base),nfcmd_read,nfcmd_write,snfc);
	IOH_New32(NFC_NFADDR(base),nfaddr_read,nfaddr_write,snfc);
	IOH_New32(NFC_NFDATA(base),nfdata_read,nfdata_write,snfc);
	IOH_New32(NFC_NFSTAT(base),nfstat_read,nfstat_write,snfc);
	IOH_New32(NFC_NFECC(base),nfecc_read,nfecc_write,snfc);
}

static void
S3Nfc_UnMap(void *owner,uint32_t base,uint32_t mask)
{
        IOH_Delete32(NFC_NFCONF(base));
        IOH_Delete32(NFC_NFCMD(base));
	IOH_Delete32(NFC_NFADDR(base));
	IOH_Delete32(NFC_NFDATA(base));
	IOH_Delete32(NFC_NFSTAT(base));
	IOH_Delete32(NFC_NFECC(base));

}

/*
 * ----------------------------------------------------------------------
 * The Samsung nand flash controller is a device on the bus and it is 
 * a nand flash controller, so it inherits from both
 * ----------------------------------------------------------------------
 */
void
S3C2410Nfc_New(const char *name,BusDevice **bdev)
{
	S3Nfc *snfc = sg_new(S3Nfc);
	snfc->ncon = SigNode_New("%s.ncon",name);
	if(!snfc->ncon) {
		fprintf(stderr,"Can not create Nand Flash configuration Pin \n");
		exit(1);
	}
	/* To lazy to connect to the outside, so pull it up */
	SigNode_Set(snfc->ncon,SIG_PULLUP);
	//snfc->nfc.owner = snfc;	
	snfc->bdev.first_mapping=NULL;
       	snfc->bdev.Map=S3Nfc_Map;
        snfc->bdev.UnMap=S3Nfc_UnMap;
        snfc->bdev.owner=snfc;
        snfc->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;

	//*nfc = &snfc->nfc;
	*bdev = &snfc->bdev;
	return; 
}
