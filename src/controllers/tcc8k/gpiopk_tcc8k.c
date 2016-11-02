/**
 *************************************************************
 * GPIO Port PK with interrupt capabilities
 *************************************************************
 */

#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "gpiopk_tcc8k.h"
#include "serial.h"
#include "cycletimer.h"

#define GPIOPK_RST(base)	((base) + 0x008)
#define GPIOPK_DAT(base)	((base) + 0x100)
#define GPIOPK_DOE(base) 	((base) + 0x104)
#define GPIOPK_FS0(base) 	((base) + 0x108)
#define GPIOPK_FS1(base) 	((base) + 0x10C)
#define GPIOPK_FS2(base) 	((base) + 0x110)
#define GPIOPK_IRQST(base) 	((base) + 0x210)
#define GPIOPK_IRQEN(base) 	((base) + 0x214)
#define GPIOPK_IRQPOL(base) 	((base) + 0x218)
#define GPIOPK_IRQTM0(base) 	((base) + 0x21C)
#define GPIOPK_IRQTM1(base) 	((base) + 0x220)
#define GPIOPK_CTL(base) 	((base) + 0x22C)

typedef struct GpioPK GpioPK; 

typedef struct GpioIrqLine {
	GpioPK *gpio;
        int pin_nr;
} GpioIrqLine;

struct GpioPK {
	BusDevice bdev;
	SigNode *sigIrq;
	SigNode *sigPmgpioDi[32];
	SigTrace *diTrace[32];
	GpioIrqLine irqLine[32];
	uint32_t regDat;
	uint32_t regDoe;
	uint32_t regFs0;
	uint32_t regFs1;
	uint32_t regFs2;
	uint32_t regIrqst;
	uint32_t regIrqen;
	uint32_t regIrqpol;
	uint32_t regIrqtm0;
	uint32_t regIrqtm1;
	uint32_t regCtl;
	uint32_t lineStatus;
};

static void
update_irq(GpioPK *gpio)
{
        uint32_t ints;
        ints = gpio->regIrqst & gpio->regIrqen;
        if(ints) {
                //fprintf(stderr,"Posting CPU interrupt\n");
                SigNode_Set(gpio->sigIrq,SIG_HIGH);
        } else {
                //fprintf(stderr,"Unposting CPU interrupt, irqsel %08x, ien %08x\n",gpio->regIrqst,gpio->regIrqen);
                SigNode_Set(gpio->sigIrq,SIG_LOW);
        }
}

/**
 */
static void
check_for_level_int(GpioPK *gpio)
{
        uint32_t level_mask = gpio->regIrqtm0;
        uint32_t pol_xor = gpio->regIrqpol;
        uint32_t ints = (gpio->lineStatus ^ pol_xor) & level_mask;
        uint32_t clr_ints = (~gpio->lineStatus ^ pol_xor) & level_mask;
        gpio->regIrqst |= ints;
        gpio->regIrqst &= ~clr_ints;      /* Not sure about this */

}

static void
irq_trace(struct SigNode * node,int value, void *clientData)
{
        GpioIrqLine *il = clientData;
	GpioPK *gpio = il->gpio;
        uint32_t posedge_mask;
        uint32_t negedge_mask;
        posedge_mask = ~gpio->regIrqtm0 & ~gpio->regIrqpol;
        negedge_mask = ~gpio->regIrqtm0 & gpio->regIrqpol;
	posedge_mask |= ~gpio->regIrqtm0 | gpio->regIrqtm1;
	negedge_mask |= ~gpio->regIrqtm0 | gpio->regIrqtm1;
        if(value == SIG_HIGH) {
                gpio->lineStatus = gpio->lineStatus | (1 << il->pin_nr);
                if(posedge_mask & (1 << il->pin_nr)) {
                        gpio->regIrqst |= (1 << il->pin_nr);
                }
        } else if(value == SIG_LOW) {
                gpio->lineStatus = gpio->lineStatus & ~(1 << il->pin_nr);
                if(negedge_mask & (1 << il->pin_nr)) {
                        gpio->regIrqst |= (1 << il->pin_nr);
                }
        }
        check_for_level_int(gpio);
        update_irq(gpio);
//      fprintf(stderr,"Irq trace: %d, Ireq %08x, Nr %d, name %s\n",value,gpio->regIreq,in->nr,SigName(in->sigIrq));
}


static uint32_t
rst_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
        return 0;
}

static void
rst_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
}

static uint32_t
dat_read(void *clientData,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
	uint32_t value = 0;
	int i;
	for(i = 0;i < 32; i++) {
		#if 0
		if((gpio->regDoe >> i) & 1) {
			/* Possibly takes DI anyway ? */
			value |= gpio->regDat & ( 1 << i );	
		} else {
		#endif
			if(SigNode_Val(gpio->sigPmgpioDi[i]) == SIG_HIGH) {
				value |= (1 << i);
			}
		#if 0
		}
		#endif
	}
        return value;
}

static void
dat_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
	gpio->regDat = value;	
	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
}

static uint32_t
doe_read(void *clientData,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
        return gpio->regDoe; 
}

static void
doe_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
}

static uint32_t
fs0_read(void *clientData,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
        return gpio->regFs0; 
}

static void
fs0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
	fprintf(stderr,"GPIOPK-FS0 write %08x\n",value);
//	sleep(3);
//	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
	gpio->regFs0 = value;
}

static uint32_t
fs1_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
        return 0; 
}

static void
fs1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
}

static uint32_t
fs2_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
        return 0; 
}

static void
fs2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
}

static uint32_t
irqst_read(void *clientData,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
        return gpio->regIrqst; 
}

static void
irqst_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
	/* this is not well documented but i suspect clear on write 1 */
        gpio->regIrqst &= ~value; 
}

static uint32_t
irqen_read(void *clientData,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
        return gpio->regIrqen; 
}

static void
irqen_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
	int i;
	for(i = 0; i < 32; i ++) {
		if((value >> i) & 1) {
			if(!gpio->diTrace[i]) {
				gpio->diTrace[i] = SigNode_Trace(gpio->sigPmgpioDi[i],irq_trace,gpio->irqLine + i); 
			}
		} else {
			if(gpio->diTrace[i]) {
				SigNode_Untrace(gpio->sigPmgpioDi[i],gpio->diTrace[i]);
			}
		}
	}
	fprintf(stderr,"GpioPK: %s not implemented\n",__func__);
}

static uint32_t
irqpol_read(void *clientData,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
        return gpio->regIrqpol; 
}

static void
irqpol_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
        gpio->regIrqpol = value; 
	check_for_level_int(gpio);
	update_irq(gpio);
	/* Does this trigger an edge interrupt ? */
	fprintf(stderr,"GpioPK: %s not verified for edge interrupt triggering\n",__func__);
}

static uint32_t
irqtm0_read(void *clientData,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
        return gpio->regIrqtm0; 
}

static void
irqtm0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
        gpio->regIrqtm0 = value; 
	check_for_level_int(gpio);
	update_irq(gpio);
}

static uint32_t
irqtm1_read(void *clientData,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
        return gpio->regIrqtm1; 
}

static void
irqtm1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
        gpio->regIrqtm1 = value; 
	check_for_level_int(gpio);
	update_irq(gpio);
}

/**
 ****************************************************************
 * Tested with real device: Clear on read of
 * irqst does not work.
 ****************************************************************
 */
static uint32_t
ctl_read(void *clientData,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
        return gpio->regCtl; 
}

static void
ctl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	GpioPK *gpio = clientData;
	gpio->regCtl = value;
}



static void
GpioPK_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	GpioPK *gpio = owner;
	IOH_New32(GPIOPK_RST(base),rst_read,rst_write,gpio);
	IOH_New32(GPIOPK_DAT(base),dat_read,dat_write,gpio);
	IOH_New32(GPIOPK_DOE(base),doe_read,doe_write,gpio);
	IOH_New32(GPIOPK_FS0(base),fs0_read,fs0_write,gpio);
	IOH_New32(GPIOPK_FS1(base),fs1_read,fs1_write,gpio);
	IOH_New32(GPIOPK_FS2(base),fs2_read,fs2_write,gpio);
	IOH_New32(GPIOPK_IRQST(base),irqst_read,irqst_write,gpio);
	IOH_New32(GPIOPK_IRQEN(base),irqen_read,irqen_write,gpio);
	IOH_New32(GPIOPK_IRQPOL(base),irqpol_read,irqpol_write,gpio);
	IOH_New32(GPIOPK_IRQTM0(base),irqtm0_read,irqtm0_write,gpio);
	IOH_New32(GPIOPK_IRQTM1(base),irqtm1_read,irqtm1_write,gpio);
	IOH_New32(GPIOPK_CTL(base),ctl_read,ctl_write,gpio);
}

static void
GpioPK_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(GPIOPK_RST(base));
	IOH_Delete32(GPIOPK_DAT(base));
	IOH_Delete32(GPIOPK_DOE(base));
	IOH_Delete32(GPIOPK_FS0(base));
	IOH_Delete32(GPIOPK_FS1(base));
	IOH_Delete32(GPIOPK_FS2(base));
	IOH_Delete32(GPIOPK_IRQST(base));
	IOH_Delete32(GPIOPK_IRQEN(base));
	IOH_Delete32(GPIOPK_IRQPOL(base));
	IOH_Delete32(GPIOPK_IRQTM0(base));
	IOH_Delete32(GPIOPK_IRQTM1(base));
	IOH_Delete32(GPIOPK_CTL(base));
}

/**
 ********************************************************************
 * BusDevice * TCC8K_GpioNew(const char *name)
 ********************************************************************
 */
BusDevice *
TCC8K_GpioPKNew(const char *name)
{
        GpioPK *gpio = sg_new(GpioPK);
	int i;
	gpio->bdev.first_mapping = NULL;
        gpio->bdev.Map = GpioPK_Map;
        gpio->bdev.UnMap = GpioPK_UnMap;
        gpio->bdev.owner = gpio;
        gpio->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
        for(i = 0;i < 32;i++) {
		gpio->sigPmgpioDi[i] = SigNode_New("%s.pmgpio_di%d",name,i);
		//port->sigOE[j] = SigNode_New("%s.%s,OE%d",name,port->name,i);
		gpio->irqLine[i].pin_nr = i;
		gpio->irqLine[i].gpio = gpio;
	}
        gpio->sigIrq = SigNode_New("%s.irq",name);
        if(!gpio->sigIrq) {
                fprintf(stderr,"Can not create interrupt line for %s\n",name);
                exit(1);
        }
        return &gpio->bdev;

}
