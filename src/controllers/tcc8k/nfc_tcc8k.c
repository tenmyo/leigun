#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "nfc_tcc8k.h"
#include "serial.h"
#include "cycletimer.h"

#define NFC_CMD(base) 		((base) + 0x00)
#define NFC_LADDR(base) 	((base) + 0x04)
#define NFC_BADDR(base) 	((base) + 0x08)
#define NFC_SADDR(base) 	((base) + 0x0C)
#define NFC_WDATA(base,idx) 	((base) + 0x10 + ((idx) << 2))
#define NFC_LDATA(base,idx) 	((base) + 0x20 + ((idx) << 2))
#define NFC_SDATA(base) 	((base) + 0x40)
#define NFC_CTRL(base)  	((base) + 0x50)
#define 	CTRL_RDY_IEN	(1 << 31)
#define 	CTRL_PROG_IEN 	(1 << 30)
#define 	CTRL_READ_IEN 	(1 << 29)
#define 	CTRL_DEN 	(1 << 28)
#define 	CTRL_FS 	(1 << 27)
#define 	CTRL_BW 	(1 << 26)
#define 	CTRL_CS3SEL 	(1 << 25)
#define 	CTRL_CS2SEL 	(1 << 24)
#define 	CTRL_CS1SEL 	(1 << 23)
#define 	CTRL_CS0SEL 	(1 << 22)
#define 	CTRL_RDY 	(1 << 21)
#define 	CTRL_BSIZE_MSK 	(3 << 19)
#define 	CTRL_PSIZE_MSK	(7 << 16)
#define 	CTRL_PSIZE_SHIFT	(16)
#define 	CTRL_MSK 	(1 << 15)
#define 	CTRL_CADDR_MSK	(7 << 12)
#define 	CTRL_CADDR_SHIFT	(12)
#define 	CTRL_BSTP_MSK	(15 << 8)
#define 	CTRL_BPW_MSK	(15 << 4)
#define 	CTRL_BHLD_MSK	(15)
#define NFC_PSTART(base) 	((base) + 0x54)
#define NFC_RSTART(base) 	((base) + 0x58)
#define NFC_DSIZE(base)	 	((base) + 0x5C)
#define NFC_IREQ(base)		((base) + 0x60)
#define		IREQ_FLGRDY	(1 << 6)
#define		IREQ_FLGPROG	(1 << 5)
#define		IREQ_FLGREAD	(1 << 4)
#define		IREQ_IRQRDY	(1 << 2)
#define		IREQ_IRQPROG	(1 << 1)
#define		IREQ_IRQREAD	(1 << 0)

#define NFC_RST(base)		((base) + 0x64)
#define NFC_CTRL1(base)		((base) + 0x68)
#define		CTRL1_DACK 	(1 << 31)
#define		CTRL1_IDL	(1 << 26)
#define		CTRL1_DNUM	(3 << 24)
#define		CTRL1_rSTP	(0xf << 20)
#define		CTRL1_rPW	(0xf << 16)
#define		CTRL1_rHLD	(0xf << 12)
#define		CTRL1_wSTP	(0xf << 8)
#define		CTRL1_wPW	(0xf << 4)
#define		CTRL1_wHLD	(0xf << 0)
#define NFC_MDATA(base,idx) 	((base) + 0x70 + ((idx) << 2))

#define DI_FIFO_SIZE	(4)
#define DI_FIFO_WP(nfc)	((nfc)->data_in_wp % DI_FIFO_SIZE)
#define DI_FIFO_RP(nfc)	((nfc)->data_in_rp % DI_FIFO_SIZE)

typedef struct TCC_Nfc {
        BusDevice bdev;
	SigNode *sigCS[4];
	SigNode *sigALE;
	SigNode *sigRDY;
	SigNode *sigCLE;
	SigNode *sigNWE;
	SigNode *sigNOE;
	SigNode *sigIO[16];
	SigNode *sigIrq;
	uint32_t regCmd;
	//uint32_t regLaddr;
	//uint32_t regBaddr;
	//uint32_t regSaddr;
	//uint32_t regWdata;
	//uint32_t regLdata;
	//uint32_t regSdata;
	uint32_t regCtrl;
	//uint32_t regPstart;
	//uint32_t regRstart;
	uint32_t regDsize;
	uint32_t regIreq;
	//uint32_t regRst;
	uint32_t regCtrl1;
	//uint32_t regMdata;
	uint32_t script[64];
	uint32_t scr_ip;
	uint32_t data_in[DI_FIFO_SIZE];
	uint32_t data_in_wp;
	CycleTimer ndelayTimer;
	SigTrace *traceRDY;
	int resultCode;
} TCC_Nfc;

#define INTERP_RES_OK		(0)
#define	INTERP_RES_TIMEOUT	(-1)
#define CMD_CLE_HIGH		(0x01000000)
#define CMD_CLE_LOW		(0x02000000)
#define CMD_ALE_HIGH		(0x03000000)
#define CMD_ALE_LOW		(0x04000000)
#define CMD_NWE_HIGH		(0x05000000)
#define CMD_NWE_LOW		(0x06000000)
#define CMD_CE_HIGH		(0x07000000)
#define CMD_CE_LOW		(0x08000000)
#define CMD_NOE_HIGH		(0x09000000)
#define CMD_NOE_LOW		(0x0a000000)
#define CMD_DATA_OUT		(0x0b000000)
#define CMD_DATA_RELEASE	(0x0c000000)
#define CMD_DATA_READ		(0x0d000000)
#define CMD_DELAY_NS		(0x0e000000)
#define	CMD_SIGNAL_RDY		(0x0f000000)
#define	CMD_WAIT_RDY		(0x10000000)
#define	CMD_RESET_INFIFO	(0x11000000)
#define CMD_ENDSCRIPT		(0xff000000)

#define RET_DO_NEXT	(0)
#define	RET_DONE	(1)

static void
update_interrupt(TCC_Nfc *nfc) {
	int interrupt = 0;
	
	if((nfc->regIreq & IREQ_FLGRDY) && (nfc->regCtrl & CTRL_RDY_IEN)) {
		nfc->regIreq |= IREQ_IRQRDY;
		interrupt = 1;
	}
	if((nfc->regIreq & IREQ_FLGPROG) && (nfc->regCtrl & CTRL_PROG_IEN)) {
		nfc->regIreq |= IREQ_IRQPROG;
		interrupt = 1;
	}
	if((nfc->regIreq & IREQ_FLGREAD) && (nfc->regCtrl & CTRL_READ_IEN)) {
		nfc->regIreq |= IREQ_IRQREAD;
		interrupt = 1;
	}
	if(interrupt) {
		SigNode_Set(nfc->sigIrq,SIG_HIGH);
	} else {
		SigNode_Set(nfc->sigIrq,SIG_LOW);
	}
}

static inline int 
nfc_ready(TCC_Nfc *nfc) {
	return !!(nfc->regCtrl & CTRL_RDY);
}

static void rdy_trace_proc(struct SigNode *,int value,void *clientData);

static int 
execute_one_step(TCC_Nfc *nfc) 
{
	uint32_t cmd,data;
	int i;
	if(nfc->scr_ip >= array_size(nfc->script)) {
		fprintf(stderr,"Bug: NFC Instruction pointer behind end of script\n");
		exit(1);
	}
	data = nfc->script[nfc->scr_ip];
	nfc->scr_ip++;
	cmd = data & 0xff000000;
	data = data & 0xffffff;
	switch(cmd) {
		case CMD_CLE_HIGH:
			SigNode_Set(nfc->sigCLE,SIG_HIGH);
			break;
		case CMD_CLE_LOW:
			SigNode_Set(nfc->sigCLE,SIG_LOW);
			break;
		case CMD_ALE_HIGH:
			SigNode_Set(nfc->sigALE,SIG_HIGH);
			break;
		case CMD_ALE_LOW:
			SigNode_Set(nfc->sigALE,SIG_LOW);
			break;
		case CMD_NWE_HIGH:
			SigNode_Set(nfc->sigNWE,SIG_HIGH);
			break;
		case CMD_NWE_LOW:
			SigNode_Set(nfc->sigNWE,SIG_LOW);
			break;
		//case CMD_CE_HIGH:
		//case CMD_CE_LOW:
		case CMD_DATA_OUT:
			for(i = 0; i < 16; i++) {
				if(data & (1 << i)) {
					SigNode_Set(nfc->sigIO[i],SIG_HIGH);
				} else {
					SigNode_Set(nfc->sigIO[i],SIG_LOW);
				}
			}
			break;

		case CMD_DATA_RELEASE:
			for(i = 0; i < 16; i++) {
				SigNode_Set(nfc->sigIO[i],SIG_OPEN);
			}
			break;

		case CMD_RESET_INFIFO:
			nfc->data_in_wp = 0;
			break;

		case CMD_DATA_READ:
			data = 0;
			for(i = 0; i < 16; i++) {
				if(SigNode_Val(nfc->sigIO[i]) == SIG_HIGH) {
					data |= (1 << i);
				}
			}
			nfc->data_in[DI_FIFO_WP(nfc)]  = data;
			nfc->data_in_wp++;
			break;

		case CMD_SIGNAL_RDY:
			if(!nfc->regCtrl) {
				nfc->regCtrl |= CTRL_RDY; 
				nfc->regIreq |= IREQ_FLGRDY;
			}
			update_interrupt(nfc);
			break;
			
		case CMD_DELAY_NS:
			CycleTimer_Mod(&nfc->ndelayTimer,NanosecondsToCycles(data));
			return RET_DONE;

		case CMD_WAIT_RDY:
			if(SigNode_Val(nfc->sigRDY) == SIG_HIGH) {
				return RET_DO_NEXT;
			} else {
				CycleTimer_Mod(&nfc->ndelayTimer,NanosecondsToCycles(data));
				nfc->traceRDY = SigNode_Trace(nfc->sigRDY,rdy_trace_proc,nfc);
				return RET_DONE;
			}
			break;

		case CMD_ENDSCRIPT:
			nfc->scr_ip = 0;
			return RET_DONE;
	}
	return RET_DO_NEXT;
}

static void
nfc_run_script(void *clientData)
{
	TCC_Nfc *nfc = clientData;
	if(nfc->traceRDY) {
		nfc->resultCode = INTERP_RES_TIMEOUT;
		SigNode_Untrace(nfc->sigRDY,nfc->traceRDY);
		nfc->traceRDY = NULL;
	}
	while(execute_one_step(nfc) == RET_DO_NEXT) {
		;	
	} 
}
static void
rdy_trace_proc(struct SigNode *sig,int value,void *clientData)
{
	TCC_Nfc *nfc = clientData;
	SigNode_Untrace(nfc->sigRDY,nfc->traceRDY);
	nfc->traceRDY = NULL;
	nfc_run_script(nfc);
}

static void
nfc_start_script(TCC_Nfc *nfc) 
{
	nfc->resultCode = 0;
	if(CycleTimer_IsActive(&nfc->ndelayTimer)) {
		fprintf(stderr,"Warning, starting NFC while already running\n");
	} else {
		CycleTimer_Mod(&nfc->ndelayTimer,0);
	}
}

static uint32_t
cmd_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register is writeonly\n",__func__);
	return 0;
}

static void
cmd_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	TCC_Nfc *nfc = clientData;
	int cnt = 0;
	if(!nfc_ready(nfc)) {
		fprintf(stderr,"%s: NFC is busy\n",__func__);
		return;
	}
	nfc->script[cnt++] = CMD_CLE_HIGH;
	if(nfc->regCtrl & CTRL_MSK) {
		nfc->script[cnt++] = CMD_DATA_OUT | (value & 0x00ff);	
	} else {
		nfc->script[cnt++] = CMD_DATA_OUT | (value & 0xffff);	
	}
	nfc->script[cnt++] = CMD_NWE_LOW;
	nfc->script[cnt++] = CMD_NWE_HIGH;
	nfc->script[cnt++] = CMD_DATA_RELEASE;	
	nfc->script[cnt++] = CMD_CLE_LOW;
	nfc->script[cnt++] = CMD_WAIT_RDY;
	nfc->script[cnt++] = CMD_SIGNAL_RDY;
	nfc->script[cnt++] = CMD_ENDSCRIPT;
	nfc->regCtrl &= ~CTRL_RDY;
	nfc_start_script(nfc);
}

/**
 *********************************************************************************************************
 * \fn static uint32_t laddr_read(void *clientData,uint32_t address,int rqlen)
 * Linear address read. This register is a writeonly register. 
 *********************************************************************************************************
 */
static uint32_t
laddr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register is writeonly\n",__func__);
	return 0;
}

/**
 **********************************************************************************************
 * \fn static void laddr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
 * Write a linear address.
 **********************************************************************************************
 */
static void
laddr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	TCC_Nfc *nfc = clientData;
	int caddr,i,psize;
	int colcycles,rowcycles;
	int cnt = 0;
	uint32_t rowval,colval;
	if(!nfc_ready(nfc)) {
		fprintf(stderr,"%s: NFC is busy\n",__func__);
		return;
	}
	caddr = (nfc->regCtrl & CTRL_CADDR_MSK) >> CTRL_CADDR_SHIFT;
	psize = (nfc->regCtrl & CTRL_PSIZE_MSK) >> CTRL_PSIZE_SHIFT; 
	nfc->script[cnt++] = CMD_ALE_HIGH;
	if(psize == 0) {
		colval = value & 0xff;
		rowval = value >> 9;
	} else if(psize == 1) {
		colval = value & 0xff;
		rowval = value >> 9;
	} else if(psize <= 3) {
		colval = value & 0xffff >> (7 - psize);
		rowval = value >> (psize + 9);
	} else {
		colval = value & 0xffff >> (7 - 4);
		rowval = value >> (4 + 9);
	}
	if(psize >= 2) {
		colcycles = 2;
		rowcycles = caddr - 1;
	} else {
		colcycles = 1;
		rowcycles = caddr;
	}
	for(i = 0; i < colcycles; i ++) {
		uint16_t outval = colval & 0xff;
		if(!(nfc->regCtrl & CTRL_MSK)) {
			outval |= (outval & 0xff) << 8;
		}
		nfc->script[cnt++] = CMD_DATA_OUT | (outval & 0xff);	
		nfc->script[cnt++] = CMD_NWE_LOW;
		nfc->script[cnt++] = CMD_NWE_HIGH;
		value >>= 8;
	}
	for(i = 0; i < rowcycles; i ++) {
		uint16_t outval = value & 0xff;
		if(!(nfc->regCtrl & CTRL_MSK)) {
			outval |= (outval & 0xff) << 8;
		}
		nfc->script[cnt++] = CMD_DATA_OUT | (outval & 0xff);	
		nfc->script[cnt++] = CMD_NWE_LOW;
		nfc->script[cnt++] = CMD_NWE_HIGH;
		value >>= 8;
	}
	nfc->script[cnt++] = CMD_ALE_LOW;
	nfc->script[cnt++] = CMD_SIGNAL_RDY;
	nfc->script[cnt++] = CMD_ENDSCRIPT;
	nfc->regCtrl &= ~CTRL_RDY;
	nfc_start_script(nfc);
        //fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

/**
 ****************************************************************************************
 * \fn static uint32_t baddr_read(void *clientData,uint32_t address,int rqlen)
 * Block addr read. This register is not readable.
 ****************************************************************************************
 */
static uint32_t
baddr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register is writeonly\n",__func__);
	return 0;
}

/**
 ***************************************************************************************************
 * \fn static void baddr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
 * Write the block address.
 ***************************************************************************************************
 */
static void
baddr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	TCC_Nfc *nfc = clientData;
	int cycles,i,psize;
	uint32_t colval,rowval;
	int cnt = 0;
	if(!nfc_ready(nfc)) {
		fprintf(stderr,"%s: NFC is busy\n",__func__);
		return;
	}
	cycles = (nfc->regCtrl & CTRL_CADDR_MSK) >> CTRL_CADDR_SHIFT;
	psize = (nfc->regCtrl & CTRL_PSIZE_MSK) >> CTRL_PSIZE_SHIFT; 
	nfc->script[cnt++] = CMD_ALE_HIGH;
	if(psize == 1) {
		value = value >> 9;
	} else if(psize <= 3) {
		value = value >> (psize + 9);
	} else {
		colval = (value & 0xffff) >> (7 - 4);
		rowval = value >> (4 + 9);
	}
	if(psize >= 2) {
		cycles -= 1;
	}
	for(i = 0; i < cycles; i ++) {
		uint16_t outval = value & 0xff;
		if(!(nfc->regCtrl & CTRL_MSK)) {
			outval |= (outval & 0xff) << 8;
		}
		nfc->script[cnt++] = CMD_DATA_OUT | (outval & 0xff);	
		nfc->script[cnt++] = CMD_NWE_LOW;
		nfc->script[cnt++] = CMD_NWE_HIGH;
		value >>= 8;
	}
	nfc->script[cnt++] = CMD_ALE_LOW;
	nfc->script[cnt++] = CMD_SIGNAL_RDY;
	nfc->script[cnt++] = CMD_ENDSCRIPT;
	nfc->regCtrl &= ~CTRL_RDY;
	nfc_start_script(nfc);
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

/**
 ****************************************************************************************
 * \fn static uint32_t saddr_read(void *clientData,uint32_t address,int rqlen)
 * Single address cycle register read. This register is not readable.
 ****************************************************************************************
 */
static uint32_t
saddr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register is writeonly\n",__func__);
	return 0;
}

/**
 ****************************************************************************************************************
 * static void saddr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
 * Generate a single address cycle. It is legal for address generation if ALE is low between
 * the address cycles. Address is not finished by ALE low. 
 ****************************************************************************************************************
 */
static void
saddr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	TCC_Nfc *nfc = clientData;
	int cnt = 0;
	if(!nfc_ready(nfc)) {
		fprintf(stderr,"%s: NFC is busy\n",__func__);
		return;
	}
	nfc->script[cnt++] = CMD_ALE_HIGH;
	value = value & 0xff;
	if(!(nfc->regCtrl & CTRL_MSK)) {
		value = value | ((value << 8) & 0xff00);
	}
	nfc->script[cnt++] = CMD_DATA_OUT | (value & 0xffff);	
	nfc->script[cnt++] = CMD_NWE_LOW;
	nfc->script[cnt++] = CMD_NWE_HIGH;
	nfc->script[cnt++] = CMD_DATA_RELEASE;
	nfc->script[cnt++] = CMD_ALE_LOW;
	nfc->script[cnt++] = CMD_SIGNAL_RDY;
	nfc->script[cnt++] = CMD_ENDSCRIPT;
	nfc->regCtrl &= ~CTRL_RDY;
	nfc_start_script(nfc);
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

static uint32_t
wdata_read(void *clientData,uint32_t address,int rqlen)
{
	TCC_Nfc *nfc = clientData;
	uint32_t value = nfc->data_in[0] | (nfc->data_in[1] << 8);
	int cnt = 0;
	if(!nfc_ready(nfc)) {
		fprintf(stderr,"%s: NFC is busy\n",__func__);
		return value;
	}
	nfc->script[cnt++] = CMD_RESET_INFIFO;
	nfc->script[cnt++] = CMD_NOE_LOW;
	nfc->script[cnt++] = CMD_DELAY_NS | 0;
	nfc->script[cnt++] = CMD_DATA_READ;
	nfc->script[cnt++] = CMD_NOE_HIGH;
	nfc->script[cnt++] = CMD_DELAY_NS | 0;
	nfc->script[cnt++] = CMD_SIGNAL_RDY;
	nfc->script[cnt++] = CMD_ENDSCRIPT;
	nfc->regCtrl &= ~CTRL_RDY;
	nfc_start_script(nfc);
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	return value;
}

static void
wdata_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	int i;	
	int cnt = 0;
	TCC_Nfc *nfc = clientData;
	if(!nfc_ready(nfc)) {
		fprintf(stderr,"%s: NFC is busy\n",__func__);
		return;
	}
	for(i = 0; i < 4; i++) {
		nfc->script[cnt++] = CMD_DATA_OUT | (value & 0xff);	
		nfc->script[cnt++] = CMD_NWE_LOW;
		nfc->script[cnt++] = CMD_NWE_HIGH;
		value = value >> 8;
	}
	nfc->script[cnt++] = CMD_SIGNAL_RDY;
	nfc->script[cnt++] = CMD_ENDSCRIPT;
	nfc->regCtrl &= ~CTRL_RDY;
	nfc_start_script(nfc);
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

static uint32_t
ldata_read(void *clientData,uint32_t address,int rqlen)
{
	int i;
	int cnt = 0;
	uint32_t ldata = 0;
	TCC_Nfc *nfc = clientData;
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	for(i = 0; i < 4; i++) {
		ldata = nfc->data_in[0] | (nfc->data_in[1] << 8) | 
			(nfc->data_in[2] << 16) | (nfc->data_in[3] << 24);
	}	
	nfc->script[cnt++] = CMD_RESET_INFIFO;
	for(i = 0; i < 4; i++) {
		nfc->script[cnt++] = CMD_NOE_LOW;
		nfc->script[cnt++] = CMD_DELAY_NS | 0;
		nfc->script[cnt++] = CMD_DATA_READ;
		nfc->script[cnt++] = CMD_NOE_HIGH;
		nfc->script[cnt++] = CMD_DELAY_NS | 0;
	}
	nfc->script[cnt++] = CMD_SIGNAL_RDY;
	nfc->script[cnt++] = CMD_ENDSCRIPT;
	nfc->regCtrl &= ~CTRL_RDY;
	nfc_start_script(nfc);
	return 0;
}

static void
ldata_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	TCC_Nfc *nfc = clientData;
	int i;	
	int cnt = 0;
	if(!nfc_ready(nfc)) {
		fprintf(stderr,"%s: NFC is busy\n",__func__);
		return;
	}
	for(i = 0; i < 4; i++) {
		nfc->script[cnt++] = CMD_DATA_OUT | (value & 0xff);	
		nfc->script[cnt++] = CMD_NWE_LOW;
		nfc->script[cnt++] = CMD_NWE_HIGH;
		value = value >> 8;
	}
	nfc->script[cnt++] = CMD_SIGNAL_RDY;
	nfc->script[cnt++] = CMD_ENDSCRIPT;
	nfc->regCtrl &= ~CTRL_RDY;
	nfc_start_script(nfc);
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

static uint32_t
sdata_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	int cnt = 0;
	TCC_Nfc *nfc = clientData;
	uint32_t value;
	value = nfc->data_in[0];	
	if(!nfc_ready(nfc)) {
		fprintf(stderr,"%s: NFC is busy\n",__func__);
		return value;
	}
	nfc->script[cnt++] = CMD_RESET_INFIFO;
	nfc->script[cnt++] = CMD_NOE_LOW;
	nfc->script[cnt++] = CMD_DELAY_NS | 0;
	nfc->script[cnt++] = CMD_DATA_READ;
	nfc->script[cnt++] = CMD_NOE_HIGH;
	nfc->script[cnt++] = CMD_DELAY_NS | 0;
	nfc->script[cnt++] = CMD_SIGNAL_RDY;
	nfc->script[cnt++] = CMD_ENDSCRIPT;
	nfc->regCtrl &= ~CTRL_RDY;
	nfc_start_script(nfc);
	return value;
}

static void
sdata_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	int cnt = 0;
	TCC_Nfc *nfc = clientData;
	if(!nfc_ready(nfc)) {
		fprintf(stderr,"%s: NFC is busy\n",__func__);
		return;
	}
	nfc->script[cnt++] = CMD_DATA_OUT | (value & 0xffff);	
	nfc->script[cnt++] = CMD_NWE_LOW;
	nfc->script[cnt++] = CMD_NWE_HIGH;
	nfc->script[cnt++] = CMD_SIGNAL_RDY;
	nfc->script[cnt++] = CMD_ENDSCRIPT;
	nfc->regCtrl &= ~CTRL_RDY;
	nfc_start_script(nfc);
}

/*
 *****************************************************************************
 * Bit 31: RDY_IEN  Nand Flash Ready Interrupt
 * Bit 30: PROG_IEN Nand Flash Program Interrupt
 * Bit 29: READ_IEN  Nand Flash Read Interrupt
 * Bit 28: DEN Nand Flash DMA Request
 * Bit 27: FS  Nand Flash FIFO Status
 * Bit 26: BW  Nand Flash Bus Width Select
 * Bit 25: CS3SEL NAND Flash CS3 Selection
 * Bit 24: CS2SEL NAND Flash CS2 Selection
 * Bit 23: CS1SEL NAND Flash CS1 Selection
 * Bit 22: CS0SEL NAND Flash CS0 Selection
 * Bit 21: RDY Nand Flash Ready Flag
 * Bit 19-20: BSIZE[1:0] Burst Size of Nand Controller
 * Bit 16-18: PSIZE[2:0] Page Size of Nand Flash
 * Bit 15: MSK  NAND Flash IO Mask Enable Bit
 * Bit 12-14: CADDR[2:0] Number of Address Cycles
 * Bit 8-11: bSTP[3:0]   umber of Base Cycle for Setup Time
 * Bit 4-7:  bPW[3:0]  Number of Base Cycle for Pulse Width
 * Bit 0-3:  bHLD[3:0] Pulse Width Cycle of NAND Flash ( PW ) = RPW.
 *****************************************************************************
*/
static uint32_t
ctrl_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	return 0;
}

static void
ctrl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	TCC_Nfc *nfc = clientData;
	int i;
	uint32_t keep = CTRL_RDY;
	/* The chip enable has an immediate effect */
	for(i = 0;i < 4;i++) {
		if(value & (CTRL_CS0SEL << i)) {
			SigNode_Set(nfc->sigCS[i],SIG_HIGH);
		} 
	}
	nfc->regCtrl = (nfc->regCtrl & keep) | (value & ~keep);
}

static uint32_t
pstart_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	return 0;
}

static void
pstart_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

static uint32_t
rstart_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	return 0;
}

static void
rstart_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

static uint32_t
dsize_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	return 0;
}

static void
dsize_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

static uint32_t
ireq_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	return 0;
}

static void
ireq_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

static uint32_t
rst_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	return 0;
}

static void
rst_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

/*
 **********************************************************************
 * ctrl1
 * Bit 31: DACK DMA Acknowledge Selection
 * Bit 26: IDL NFC IDLE State Flag
 * Bit 24-25: DNUM  DNUM
 * Bit 20-23: rSTP[3:0] Number of Read Cycle for Setup Time
 * Bit 16-19: rPW[3:0]  Number of Read Cycle for Pulse Width
 * Bit 12-15: rHLD[3:0] Number of Read Cycle for Hold Time
 * Bit 8-11:  wSTP[3:0] Number of Write Cycle for Setup Time
 * Bit 4-7:   wPW[3:0]  Number of Write Cycle for PulseWidth
 * Bit 0-3:   wHLD[3:0] Number of Write Cycle for Hold Time
 **********************************************************************
*/
static uint32_t
ctrl1_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	return 0;
}

static void
ctrl1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

static uint32_t
mdata_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
	return 0;
}

static void
mdata_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"TCC8K NFC: %s: Register not implemented\n",__func__);
}

static void
TNfc_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
        TCC_Nfc *nfc = owner;
	int i;
	IOH_New32(NFC_CMD(base),cmd_read,cmd_write,nfc);
	IOH_New32(NFC_LADDR(base),laddr_read,laddr_write,nfc);
	IOH_New32(NFC_BADDR(base),baddr_read,baddr_write,nfc);
	IOH_New32(NFC_SADDR(base),saddr_read,saddr_write,nfc);
	for(i = 0; i < 4; i++) {	
		IOH_New32(NFC_WDATA(base,i),wdata_read,wdata_write,nfc);
	}
	for(i = 0; i < 8; i++) {	
		IOH_New32(NFC_LDATA(base,i),ldata_read,ldata_write,nfc);
	}
	IOH_New32(NFC_SDATA(base),sdata_read,sdata_write,nfc);
	IOH_New32(NFC_CTRL(base),ctrl_read,ctrl_write,nfc); 
	IOH_New32(NFC_PSTART(base),pstart_read,pstart_write,nfc);
	IOH_New32(NFC_RSTART(base),rstart_read,rstart_write,nfc);
	IOH_New32(NFC_DSIZE(base),dsize_read,dsize_write,nfc);
	IOH_New32(NFC_IREQ(base),ireq_read,ireq_write,nfc);
	IOH_New32(NFC_RST(base),rst_read,rst_write,nfc);
	IOH_New32(NFC_CTRL1(base),ctrl1_read,ctrl1_write,nfc);
	for(i = 0; i < 4; i++) {	
		IOH_New32(NFC_MDATA(base,i),mdata_read,mdata_write,nfc);
	}
}

static void
TNfc_UnMap(void *owner,uint32_t base,uint32_t mask)
{

	int i;
	IOH_Delete32(NFC_CMD(base));
	IOH_Delete32(NFC_LADDR(base));
	IOH_Delete32(NFC_BADDR(base));
	IOH_Delete32(NFC_SADDR(base));
	for(i = 0; i < 4; i++) {	
		IOH_Delete32(NFC_WDATA(base,i));
	}
	for(i = 0; i < 8; i++) {	
		IOH_Delete32(NFC_LDATA(base,i));
	}
	IOH_Delete32(NFC_SDATA(base));
	IOH_Delete32(NFC_CTRL(base)); 
	IOH_Delete32(NFC_PSTART(base));
	IOH_Delete32(NFC_RSTART(base));
	IOH_Delete32(NFC_DSIZE(base));
	IOH_Delete32(NFC_IREQ(base));
	IOH_Delete32(NFC_RST(base));
	IOH_Delete32(NFC_CTRL1(base));
	for(i = 0; i < 4; i++) {	
		IOH_Delete32(NFC_MDATA(base,i));
	}
}

BusDevice *
TCC8K_NfcNew(const char *name)
{
        TCC_Nfc *nfc = sg_new(TCC_Nfc);
	int i;
        nfc->bdev.first_mapping = NULL;
        nfc->bdev.Map = TNfc_Map;
        nfc->bdev.UnMap = TNfc_UnMap;
        nfc->bdev.owner = nfc;
        nfc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	for(i = 0; i < 4;i++) {
		nfc->sigCS[i] = SigNode_New("%s.cs%d",name,i);
		if(!nfc->sigCS[i]) {
			fprintf(stderr,"Can not create chip select %d for %s\n",i,name);
			exit(1);
		}
		SigNode_Set(nfc->sigCS[i],SIG_HIGH);
	}
	nfc->sigALE = SigNode_New("%s.ale",name);
	if(!nfc->sigALE) {
		fprintf(stderr,"Can not create ALE signal for %s\n",name);
	}	
	nfc->sigCLE = SigNode_New("%s.cle",name);
	if(!nfc->sigCLE) {
		fprintf(stderr,"Can not create CLE signal for %s\n",name);
	}	
	nfc->sigNWE = SigNode_New("%s.nwe",name);
	if(!nfc->sigNWE) {
		fprintf(stderr,"Can not create NWE signal for %s\n",name);
	}	
	nfc->sigNOE = SigNode_New("%s.noe",name);
	if(!nfc->sigNOE) {
		fprintf(stderr,"Can not create nOEN signal for %s\n",name);
	}	
	nfc->sigRDY = SigNode_New("%s.rdy",name);
	if(!nfc->sigRDY) {
		fprintf(stderr,"Can not create RDY signal for %s\n",name);
	}	
	nfc->sigIrq = SigNode_New("%s.irq",name);
	if(!nfc->sigIrq) {
		fprintf(stderr,"Can not create irq signal for %s\n",name);
	}	
	for(i = 0; i < 16;i++) {
		nfc->sigIO[i] = SigNode_New("%s.io%d",name,i);
		if(!nfc->sigIO[i]) {
			fprintf(stderr,"Can not create IO lines for %s\n",name);
			exit(1);
		}
		SigNode_Set(nfc->sigIO[i],SIG_OPEN);
	}
	SigNode_Set(nfc->sigALE,SIG_LOW);
	SigNode_Set(nfc->sigCLE,SIG_LOW);
	SigNode_Set(nfc->sigNWE,SIG_HIGH);
	SigNode_Set(nfc->sigNOE,SIG_HIGH);
	nfc->regCtrl = 0x03e08000;
	nfc->regDsize = 0xffff;
	nfc->regIreq = 0x07000000; 
	nfc->regCtrl1 = 0; 
	CycleTimer_Init(&nfc->ndelayTimer,nfc_run_script,nfc);
	update_interrupt(nfc);
        return &nfc->bdev;
}

