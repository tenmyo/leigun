#include "bus.h"
#include "sgstring.h"
#include "signode.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define PIC_IEN(base)           ((base) + 0x00)
#define PIC_CREQ(base)          ((base) + 0x04)
#define PIC_IREQ(base)          ((base) + 0x08)
#define PIC_IRQSEL(base)        ((base) + 0x0c)
#define PIC_SRC(base)           ((base) + 0x10)
#define PIC_MREQ(base)          ((base) + 0x14)
#define PIC_TSTREQ(base)        ((base) + 0x18)
#define PIC_POL(base)           ((base) + 0x1c)
#define PIC_IRQ(base)           ((base) + 0x20)
#define PIC_FIQ(base)           ((base) + 0x24)
#define PIC_MIRQ(base)          ((base) + 0x28)
#define PIC_MFIQ(base)          ((base) + 0x2c)
#define PIC_TMODE(base)         ((base) + 0x30)
#define PIC_SYNC(base)          ((base) + 0x34)
#define PIC_WKUP(base)          ((base) + 0x38)
#define PIC_TMODEA(base)	((base) + 0x3c)
#define PIC_INTOEN(base)        ((base) + 0x40)
#define PIC_MEN0(base)          ((base) + 0x44)
#define PIC_MEN(base)           ((base) + 0x48)

typedef struct TccPic TccPic;

typedef struct PicIrqIn {
	TccPic *pic;
	int nr;
	SigNode *sigIrq;
	SigTrace *irqTrace;
} PicIrqIn;

struct TccPic {
	BusDevice bdev;
	SigNode *irqOut;
	SigNode *fiqOut;
	PicIrqIn irqIn[32];
	uint32_t lineStatus;
	uint32_t regIen;
	uint32_t regIreq;
	uint32_t regIrqsel;
	uint32_t regSync;
	uint32_t regPol;
	uint32_t regTmode;
	uint32_t regTmodea;
	uint32_t regIntoen;
	uint32_t regWkup;
	uint32_t regMen0;
	uint32_t regMen;
};

static void
update_irq(TccPic * pic)
{
	uint32_t ints;
	ints = pic->regIreq & pic->regIrqsel & pic->regIen;
	if ((pic->regMen0 & 1) == 0) {
		ints = 0;
	}
	if (ints) {
		//fprintf(stderr,"Posting CPU interrupt\n");
		SigNode_Set(pic->irqOut, SIG_LOW);
	} else {
		//fprintf(stderr,"Unposting CPU interrupt, irqsel %08x, ien %08x\n",pic->regIrqsel,pic->regIen);
		SigNode_Set(pic->irqOut, SIG_PULLUP);
	}
}

static void
update_fiq(TccPic * pic)
{
	uint32_t ints;
	ints = pic->regIreq & ~pic->regIrqsel & pic->regIen;
	if ((pic->regMen0 & 2) == 0) {
		ints = 0;
	}
	if (ints) {
		SigNode_Set(pic->fiqOut, SIG_LOW);
	} else {
		SigNode_Set(pic->fiqOut, SIG_PULLUP);
	}
}

/**
 * This is common for fiq/irq
 */
static void
check_for_level_int(TccPic * pic)
{
	uint32_t oldIreq = pic->regIreq;
	uint32_t diff;
	uint32_t level_mask = pic->regTmode;
	uint32_t pol_xor = pic->regPol;
	uint32_t ints = (pic->lineStatus ^ pol_xor) & level_mask;
	uint32_t clr_ints = (~pic->lineStatus ^ pol_xor) & level_mask;
	//fprintf(stderr,"Level ints %08x\n",ints);
	pic->regIreq |= ints;
	pic->regIreq &= ~clr_ints;	/* Not sure about this */
	diff = pic->regIreq ^ oldIreq;
	if (diff & (1 << 7)) {
		dbgprintf("Cfli Uart0: %d\n", (pic->regIreq >> 7) & 1);
	}

}

/**
 * Individually enable / disable interrupt
 */
static uint32_t
ien_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	return pic->regIen;
}

static void
ien_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	uint32_t diff = value ^ pic->regIen;
	pic->regIen = value;
	update_irq(pic);
	update_fiq(pic);
	if (diff & (1 << 7)) {
		dbgprintf("IEN Mod uart0 int %d\n", (value >> 7) & 1);
	}
}

/**
 * Clear interrupt request register 
 * by writing 1
 */
static uint32_t
creq_read(void *clientData, uint32_t address, int rqlen)
{
	static int msgcnt = 0;
	if (msgcnt < 1) {
		dbgprintf("TCC8K PIC: %s Reading from writeonly register\n", __func__);
		msgcnt++;
	}
	return 0;
}

static void
creq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	//uint32_t level_mask = pic->regTmode;
	//fprintf(stderr,"TCC8K PIC: %s: clearing ints 0x%08x, before %08x\n",__func__,value,pic->regIreq);
	//pic->regIreq = (pic->regIreq & level_mask) |
	//      (pic->regIreq & (~value & ~level_mask));
	pic->regIreq = pic->regIreq & ~value;
	check_for_level_int(pic);
	update_irq(pic);
	update_fiq(pic);
	if (value & (1 << 7)) {
		dbgprintf("Clear uart0 int, after: %d\n", (pic->regIreq >> 7) & 1);
	}
	//fprintf(stderr,"TCC8K PIC: %s: After, Ireq: %08x, level_mask %08x\n",__func__,pic->regIreq,level_mask);
}

/**
 ********************************************
 * Interrupt request register. 
 * Clear by writing 1.
 ********************************************
 */
static uint32_t
ireq_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
//        fprintf(stderr,"TCC8K PIC: %s not implemented\n",__func__);
	return pic->regIreq;
}

static void
ireq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	fprintf(stderr, "TCC8K PIC: %s not implemented\n", __func__);
	pic->regIreq = pic->regIreq & ~value;
	update_irq(pic);
	update_fiq(pic);
}

/**
 *************************************************************
 * Interrupt select register. 1 means FIQ 0 means irq
 *************************************************************
 */
static uint32_t
irqsel_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	fprintf(stderr, "TCC8K PIC: %s not tested\n", __func__);
	return pic->regIrqsel;
}

static void
irqsel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	fprintf(stderr, "TCC8K PIC: %s not tested\n", __func__);
	pic->regIrqsel = value;
	update_irq(pic);
	update_fiq(pic);
}

/**
 **************************************************************************
 * Raw interrupt source line status register
 * What does line polarity control mean ? and why
 * is this register writable ?
 **************************************************************************
 */
static uint32_t
src_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	return pic->lineStatus ^ pic->regPol;
}

static void
src_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K PIC: %s documentation is shit\n", __func__);
}

/**
 ********************************************************************
 * Same as ireq but shows only enabled interrupts.
 * Why is this register listed as writable ?
 ********************************************************************
 */
static uint32_t
mreq_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	return pic->regIreq & pic->regIen;
}

static void
mreq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K PIC: %s documentation is shit\n", __func__);
}

/**
 *****************************************************************************************
 * Trigger an interrupt by writing a 1 to the
 *****************************************************************************************
 */
static uint32_t
tstreq_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K PIC: %s tstreq is a writeonly register\n", __func__);
	return 0;
}

static void
tstreq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	fprintf(stderr, "TCC8K PIC: %s not tested\n", __func__);
	pic->regIreq |= value;
	update_irq(pic);
	update_fiq(pic);
}

/**
 *********************************************************************************
 * Interrupt polarity:
 * 0 = active high, 1 = active low
 * default is 1
 *********************************************************************************
 */
static uint32_t
pol_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	return pic->regPol;
}

static void
pol_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	pic->regPol = value;
	check_for_level_int(pic);
	update_irq(pic);
	update_fiq(pic);
	fprintf(stderr, "TCC8K PIC: %s not complete\n", __func__);
}

/**
 * Irq raw status register, same as ireq but only true if irqsel bit is high
 */
static uint32_t
irq_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	fprintf(stderr, "TCC8K PIC: %s not tested\n", __func__);
	return pic->regIreq & pic->regIrqsel;
}

static void
irq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K PIC: %s don't know how to handle write\n", __func__);
}

/**
 * Same as ireq but only if irqsel is low
 */
static uint32_t
fiq_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	fprintf(stderr, "TCC8K PIC: %s not tested\n", __func__);
	return pic->regIreq & ~pic->regIrqsel;
}

static void
fiq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K PIC: %s don't know how to handle a write\n", __func__);
}

/**
 *******************************************************************************
 * Masked irq raw status register.
 *******************************************************************************
 */
static uint32_t
mirq_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	uint32_t src = pic->lineStatus ^ pic->regPol;
	uint32_t value;
	if ((pic->regMen0 & pic->regMen & 1) == 0) {
		return 0;
	}
	value = src & pic->regIen & pic->regIrqsel & pic->regIntoen;
	return value;
}

/**
 *********************************************************************************************
 * The mirq register is writable, writing a one clears the raw interrupt status.
 * The documentation is really bad. How can I clear the state of an interrupt
 * line.
 *********************************************************************************************
 */

static void
mirq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K PIC: %s documentation is very broken\n", __func__);
}

static uint32_t
mfiq_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	uint32_t src = pic->lineStatus ^ pic->regPol;
	uint32_t value;
	if ((pic->regMen0 & pic->regMen & 1) == 0) {
		return 0;
	}
	value = src & pic->regIen & ~pic->regIrqsel & pic->regIntoen;
	return value;
}

static void
mfiq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K PIC: %s documentation is very broken\n", __func__);
}

/**
 ************************************************************************************
 * Trigger mode: 0 is edge, 1 is level.
 ************************************************************************************
 */
static uint32_t
tmode_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	return pic->regTmode;
}

static void
tmode_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	pic->regTmode = value;
	check_for_level_int(pic);
	update_irq(pic);
	update_fiq(pic);
//      fprintf(stderr,"TMODE write %08x\n",value);
//      sleep(2);
}

/**
 **********************************************************************************
 * Make interrupt synchronous to HCLK. Does nothing but storing the
 * value in the simulator. 
 **********************************************************************************
 */
static uint32_t
sync_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	return pic->regSync;
}

static void
sync_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	pic->regSync = value;
}

/**
 ************************************************************************************
 * Wake up the system when an interrupt occurs during power saving mode.
 * 1 = wakeup disabled
 * 0 = wakeup enabled.
 ************************************************************************************
 */
static uint32_t
wkup_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	fprintf(stderr, "TCC8K PIC: %s Powersaving wakeup not implemented\n", __func__);
	return pic->regWkup;
}

static void
wkup_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = clientData;
	fprintf(stderr, "TCC8K PIC: %s Powersaving wakeup not implemented\n", __func__);
	pic->regWkup = value;
}

/**
 *********************************************************************************************
 * Selects if single edge or both edge trigger for each internal interrupt source.
 *********************************************************************************************
 */
static uint32_t
tmodea_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K PIC: %s not implemented\n", __func__);
	return 0;
}

static void
tmodea_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K PIC: %s not implemented\n", __func__);
}

/**
 * 
 */
static uint32_t
intoen_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = (TccPic *) clientData;
	return pic->regIntoen;
}

static void
intoen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = (TccPic *) clientData;
	pic->regIntoen = value;
	update_irq(pic);
	update_fiq(pic);
}

static uint32_t
men0_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = (TccPic *) clientData;
	return pic->regMen0 & 3;
}

static void
men0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = (TccPic *) clientData;
	pic->regMen0 = value;
	update_irq(pic);
	update_fiq(pic);
}

static uint32_t
men_read(void *clientData, uint32_t address, int rqlen)
{
	TccPic *pic = (TccPic *) clientData;
	return pic->regMen & 3;
}

static void
men_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccPic *pic = (TccPic *) clientData;
	pic->regMen = value;
	update_irq(pic);
	update_fiq(pic);
	fprintf(stderr, "TCC8K PIC: %s not implemented\n", __func__);
}

static void
irq_trace(struct SigNode *node, int value, void *clientData)
{
	PicIrqIn *in = clientData;
	TccPic *pic = in->pic;
	uint32_t posedge_mask;
	uint32_t negedge_mask;
	posedge_mask = pic->regTmodea & ~pic->regTmode;
	negedge_mask = pic->regTmodea & ~pic->regTmode;
	posedge_mask = posedge_mask & ~pic->regPol;
	negedge_mask = posedge_mask & pic->regPol;
	if (value == SIG_HIGH) {
		pic->lineStatus = pic->lineStatus | (1 << in->nr);
		if (posedge_mask & (1 << in->nr)) {
			pic->regIreq |= (1 << in->nr);
		}
	} else if (value == SIG_LOW) {
		pic->lineStatus = pic->lineStatus & ~(1 << in->nr);
		if (negedge_mask & (1 << in->nr)) {
			pic->regIreq |= (1 << in->nr);
		}
	}
	check_for_level_int(pic);
	update_irq(pic);
	update_fiq(pic);
//      fprintf(stderr,"Irq trace: %d, Ireq %08x, Nr %d, name %s\n",value,pic->regIreq,in->nr,SigName(in->sigIrq));
}

static void
Pic_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	TccPic *pic = (TccPic *) owner;
	IOH_New32(PIC_IEN(base), ien_read, ien_write, pic);
	IOH_New32(PIC_CREQ(base), creq_read, creq_write, pic);
	IOH_New32(PIC_IREQ(base), ireq_read, ireq_write, pic);
	IOH_New32(PIC_IRQSEL(base), irqsel_read, irqsel_write, pic);
	IOH_New32(PIC_SRC(base), src_read, src_write, pic);
	IOH_New32(PIC_MREQ(base), mreq_read, mreq_write, pic);
	IOH_New32(PIC_TSTREQ(base), tstreq_read, tstreq_write, pic);
	IOH_New32(PIC_POL(base), pol_read, pol_write, pic),
	    IOH_New32(PIC_IRQ(base), irq_read, irq_write, pic);
	IOH_New32(PIC_FIQ(base), fiq_read, fiq_write, pic);
	IOH_New32(PIC_MIRQ(base), mirq_read, mirq_write, pic);
	IOH_New32(PIC_MFIQ(base), mfiq_read, mfiq_write, pic);
	IOH_New32(PIC_TMODE(base), tmode_read, tmode_write, pic);
	IOH_New32(PIC_SYNC(base), sync_read, sync_write, pic);
	IOH_New32(PIC_WKUP(base), wkup_read, wkup_write, pic);
	IOH_New32(PIC_TMODEA(base), tmodea_read, tmodea_write, pic);
	IOH_New32(PIC_INTOEN(base), intoen_read, intoen_write, pic);
	IOH_New32(PIC_MEN0(base), men0_read, men0_write, pic);
	IOH_New32(PIC_MEN(base), men_read, men_write, pic);
}

static void
Pic_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(PIC_IEN(base));
	IOH_Delete32(PIC_CREQ(base));
	IOH_Delete32(PIC_IREQ(base));
	IOH_Delete32(PIC_IRQSEL(base));
	IOH_Delete32(PIC_SRC(base));
	IOH_Delete32(PIC_MREQ(base));
	IOH_Delete32(PIC_TSTREQ(base));
	IOH_Delete32(PIC_POL(base));
	IOH_Delete32(PIC_IRQ(base));
	IOH_Delete32(PIC_FIQ(base));
	IOH_Delete32(PIC_MIRQ(base));
	IOH_Delete32(PIC_MFIQ(base));
	IOH_Delete32(PIC_TMODE(base));
	IOH_Delete32(PIC_SYNC(base));
	IOH_Delete32(PIC_WKUP(base));
	IOH_Delete32(PIC_TMODEA(base));
	IOH_Delete32(PIC_INTOEN(base));
	IOH_Delete32(PIC_MEN0(base));
	IOH_Delete32(PIC_MEN(base));
}

BusDevice *
TCC8K_PicNew(const char *name)
{
	TccPic *pic = sg_new(TccPic);
	int i;
	pic->bdev.first_mapping = NULL;
	pic->bdev.Map = Pic_Map;
	pic->bdev.UnMap = Pic_UnMap;
	pic->bdev.owner = pic;
	pic->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	pic->irqOut = SigNode_New("%s.irq", name);
	pic->fiqOut = SigNode_New("%s.fiq", name);
	pic->regIntoen = 0xffffffff;
	pic->regMen0 = 3;
	pic->regMen = 3;
	if (!pic->irqOut || !pic->fiqOut) {
		fprintf(stderr, "Can not create PIC interrupt line\n");
		exit(1);
	}
	for (i = 0; i < 32; i++) {
		PicIrqIn *irq = &pic->irqIn[i];
		irq->pic = pic;
		irq->nr = i;
		irq->sigIrq = SigNode_New("%s.irqIn%d", name, i);
		if (!irq->sigIrq) {
			fprintf(stderr, "Can not create PIC interrupt line\n");
			exit(1);
		}
		pic->irqIn[i].irqTrace = SigNode_Trace(irq->sigIrq, irq_trace, irq);
	}
	fprintf(stderr, "TCC8000 PIC \"%s\" created\n", name);
	return &pic->bdev;
}
