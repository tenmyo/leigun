/**
 *******************************************************************************************************
 * Telechips TCC8000 USB device controller  
 *******************************************************************************************************
 */
#include <unistd.h>
#include <stdint.h>
#include "bus.h"
#include "udc_tcc8k.h"
#include "sgstring.h"
#include "mmcard.h"
#include "mmcdev.h"
#include "signode.h"
#include "clock.h"
#include "cycletimer.h"

#define REG_UD_IR(base) 	((base) + 0x00)
#define REG_UD_EIR(base)	((base) + 0x04)
#define REG_UD_EIER(base)	((base) + 0x08)
#define REG_UD_FAR(base)	((base) + 0x0c)
#define REG_UD_FNR(base)	((base) + 0x10)
#define REG_UD_EDR(base)	((base) + 0x14)
#define REG_UD_RT(base)		((base) + 0x18)
#define REG_UD_SSR(base)	((base) + 0x1C)
#define		SSR_TMERR	(1 << 14)
#define		SSR_BSERR 	(1 << 13)
#define		SSR_TCERR 	(1 << 12)
#define		SSR_DCERR	(1 << 11)
#define		SSR_EOERR 	(1 << 10)
#define		SSR_VBOFF 	(1 << 9)
#define		SSR_VBON	(1 << 8)
#define		SSR_TBM		(1 << 7)
#define		SSR_DP		(1 << 6)
#define		SSR_DM		(1 << 5)
#define		SSR_HSP		(1 << 4)
#define		SSR_SDE 	(1 << 3)
#define		SSR_HFRM 	(1 << 2)
#define		SSR_HFSUSP	(1 << 1)
#define		SSR_HFRES 	(1 << 0)
#define REG_UD_SCR(base)	((base) + 0x20)
#define		SCR_DTZIEN 	(1 << 14)
#define		SCR_DIEN 	(1 << 12)
#define		SCR_VBOFE	(1 << 11)
#define		SCR_VBOE 	(1 << 10)
#define		SCR_RWDE	(1 << 9)
#define		SCR_EIE		(1 << 8)
#define		SCR_SPDCEN	(1 << 7)
#define		SCR_SPDEN	(1 << 6)
#define		SCR_RRDE	(1 << 5)
#define		SCR_IPS		(1 << 4)
#define		SCR_SPDC	(1 << 3)
#define		SCR_HSSPE 	(1 << 1)
#define		SCR_HRESE 	(1 << 0)
#define REG_UD_EP0SR(base)	((base) + 0x24)
#define		EP0SR_LWO 	(1 << 6)
#define		EP0SR_SHT 	(1 << 4)
#define		EP0SR_TST 	(1 << 1)
#define		EP0SR_RSR 	(1 << 0)
#define REG_UD_EP0CR(base)	((base) + 0x28)
#define		EP0CR_TTE	(1 << 3)
#define		EP0CR_TTS	(1 << 2)
#define		EP0CR_EES	(1 << 1)
#define		EP0CR_TZLS 	(1 << 0)
#define REG_UD_SCR2(base) 	((base) + 0x58)
#define REG_UD_EP0BUF(base)	((base) + 0x60)
#define REG_UD_EP1BUF(base) 	((base) + 0x64)
#define REG_UD_EP2BUF(base) 	((base) + 0x68)
#define REG_UD_EP3BUF(base) 	((base) + 0x6C)
#define REG_UD_PLICR(base) 	((base) + 0xA0)
#define REG_UD_PCR(base) 	((base) + 0xA4)
#define		PCR_URSTC 	(1 << 7)
#define		PCR_SIDC 	(1 << 6)
#define		PCR_OPMC_MSK	(3 << 4)
#define		PCR_TMSC 	(1 << 3)
#define		PCR_XCRC 	(1 << 2)
#define		PCR_SUSPC 	(1 << 1)
#define		PCR_PCE 	(1 << 0)
#define REG_UD_ESR(base) 	((base) + 0x2C)
#define 	ESR_FUDR 	(1 << 15)
#define 	ESR_FOVF 	(1 << 14)
#define 	ESR_FPID 	(1 << 11)
#define 	ESR_OSD 	(1 << 10)
#define 	ESR_DTCZ	(1 << 9)
#define 	ESR_SPT 	(1 << 8)
#define 	ESR_DOM		(1 << 7)
#define 	ESR_FFS		(1 << 6)
#define 	ESR_FSC		(1 << 5)
#define 	ESR_LWO		(1 << 4)
#define 	ESR_PSIF_MSK	(3 << 2)
#define 	ESR_TPS 	(1 << 1)
#define 	ESR_RPS 	(1 << 0)
#define REG_UD_ECR(base) 	((base) + 0x30)
#define		ECR_SPE 	(1 << 15)
#define		ECR_INHLD 	(1 << 12)
#define		ECR_OUTHD 	(1 << 11)
#define		ECR_TNPMF_MSK 	(3 << 9)
#define		ECR_IME 	(1 << 8)
#define		ECR_DUEN 	(1 << 7)
#define		ECR_FLUSH 	(1 << 6)
#define		ECR_TTE		(1 << 5)
#define		ECR_TTS_MSK 	(3 << 3)
#define		ECR_CDP 	(1 << 2)
#define		ECR_ESS 	(1 << 1)
#define		ECR_IEMS 	(1 << 0)
#define REG_UD_BRCR(base) 	((base) + 0x34)
#define REG_UD_BWCR(base) 	((base) + 0x38)
#define REG_UD_MPR(base) 	((base) + 0x3C)
#define REG_UD_DCR(base) 	((base) + 0x40)
#define REG_UD_DTCR(base) 	((base) + 0x44)
#define REG_UD_DFCR(base) 	((base) + 0x48)
#define REG_UD_DTTCR1(base) 	((base) + 0x4C)
#define REG_UD_DTTCR2(base) 	((base) + 0x50)
#define REG_UD_ESR2(base) 	((base) + 0x54)

#define REG_UPCR0(base) 	((base) + 0xC8)
#define		UPCR0_PR	(1 << 14)
#define		UPCR0_CM	(1 << 13)
#define		UPCR0_RCS_SHIFT	(11)
#define		UPCR0_RCS_MSK	(3 << 11)
#define		UPCR0_RCD_SHIFT	(9)
#define		UPCR0_RCD_MSK	(3 << 9)
#define		UPCR0_SDI	(1 << 8)
#define		UPCR0_F0	(1 << 7)
#define		UPCR0_VBDS	(1 << 6)
#define		UPCR0_DMPD	(1 << 5)
#define		UPCR0_DPPD	(1 << 4)
#define		UPCR0_TBSH	(1 << 3)
#define		UPCR0_TBS	(1 << 2)
#define		UPCR0_VBD	(1 << 1)
#define		UPCR0_LBE	(1 << 0)
#define REG_UPCR1(base) 	((base) + 0xCC)
#define		UPCR1_TXFSLST_SHIFT	(12)
#define		UPCR1_TXFSLST_MSK	(0xf << 12)
#define		UPCR1_SQRXT_SHIFT	(8)
#define		UPCR1_SQRXT_MSK		(0x7 << 8)
#define		UPCR1_OTGT_SHIFT	(4)
#define		UPCR1_OTGT_MSK		(0x7 << 4)
#define		UPCR1_CDT_SHIFT		(0)
#define		UPCR1_CDT_MSK		(0x7)
#define REG_UPCR2(base) 	((base) + 0xD0)
#define REG_UPCR3(base) 	((base) + 0xD4)

typedef struct TccUsbDev {
	BusDevice bdev;
	SigNode *sigIrq;
	uint32_t regIr;
	uint32_t regEir;
	uint32_t regEier;
	uint32_t regFar;
	uint32_t regFnr;
	uint32_t regEdr;
	uint32_t regRt;
	uint32_t regSsr;
	uint32_t regScr;
	uint32_t regEp0sr;
	uint32_t regEp0cr;
	uint32_t regScr2;
	uint32_t regEp0buf;
	uint32_t regEp1buf;
	uint32_t regEp2buf;
	uint32_t regEp3buf;
	uint32_t regPlicr;
	uint32_t regPcr;
	uint32_t regEsr;
	uint32_t regEcr;
	uint32_t regBrcr;
	uint32_t regBwcr;
	uint32_t regMpr;
	uint32_t regDcr;
	uint32_t regDtcr;
	uint32_t regDfcr;
	uint32_t regDttcr1;
	uint32_t regDttcr2;
	uint32_t regEsr2;

	uint32_t regCr0;
	uint32_t regCr1;
	uint32_t regCr2;
	uint32_t regCr3;
} TccUsbDev;

static void
update_interrupt(TccUsbDev * ud)
{
	if (ud->regEir & ud->regEier) {
		SigNode_Set(ud->sigIrq, SIG_HIGH);
	} else {
		SigNode_Set(ud->sigIrq, SIG_LOW);
	}
}

/**
 *****************************************************************************************
 * Index register:
 * Select the endpoint (Lower two bits only).
 *****************************************************************************************
 */
static uint32_t
ir_read(void *clientData, uint32_t address, int rqlen)
{
	TccUsbDev *ud = clientData;
	return ud->regIr;
}

static void
ir_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccUsbDev *ud = clientData;
	ud->regIr = value & 3;
}

/**
 ****************************************************************************************
 * Endpoint interrupt flag register.
 * Clear by writing a one.
 ****************************************************************************************
 */
static uint32_t
eir_read(void *clientData, uint32_t address, int rqlen)
{
	TccUsbDev *ud = clientData;
	return ud->regEir;
}

static void
eir_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccUsbDev *ud = clientData;
	ud->regEir &= ~value;
	update_interrupt(ud);
}

/**
 ****************************************************************************************
 * Endpoint interrupt enable register.
 ****************************************************************************************
 */
static uint32_t
eier_read(void *clientData, uint32_t address, int rqlen)
{
	TccUsbDev *ud = clientData;
	return ud->regEier;
}

static void
eier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccUsbDev *ud = clientData;
	ud->regEier = value & 0xf;
}

/**
 ******************************************************************************************
 * Function address. Address transfered by the host controller.
 ******************************************************************************************
 */
static uint32_t
far_read(void *clientData, uint32_t address, int rqlen)
{
	TccUsbDev *ud = clientData;
	return ud->regFar;
}

static void
far_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccUsbDev *ud = clientData;
	ud->regFar = value & 0x7f;
}

/**
 ******************************************************************************************
 * Frame number register.
 * Bit 14: FTL Frame Timer locked to hosts frame timer.
 * Bit 13: SM  SOF missing
 * Bit 0-10: Frame number, increased every SOF.
 ******************************************************************************************
 */
static uint32_t
fnr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
fnr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 *****************************************************************************************
 * Endpoint direction register.
 * Bits 1 - 3: Direction for Endpoint 1 - 3, 1 = Transmit, 0 = Receive
 * Endpoint 0 is always Bidirectional.
 *****************************************************************************************
 */
static uint32_t
edr_read(void *clientData, uint32_t address, int rqlen)
{
	TccUsbDev *ud = clientData;
	return ud->regEdr;
}

static void
edr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccUsbDev *ud = clientData;
	ud->regEdr = value;
}

/*
 ***************************************************************************************
 * Test register.
 * Bit 12: PERR is set to a when a PID error is detected and error interrupt is enabled.
 * Bit 11: FDWR Fifo direct write read enable ????
 * Bit 6+7: Speed select.
 * Bit 4: TMD Test mode
 * Bit 3: TPS Test packets.
 * Bit 2: TKS Test K Select, Enter high speed K-State
 * Bit 1: Enter High speed J state.
 * BIt 0: TSNS Test SE0 NAK select.
 ***************************************************************************************
 */
static uint32_t
rt_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
rt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 ***********************************************************************************
 * System Status register.
 *  
 * Bit 14:	SSR_TMERR Timeout Error
 * Bit 13:	SSR_BSERR Bit Stuff Error
 * Bit 12:	SSR_TCERR Token CRC Error
 * Bit 11:	SSR_DCERR Data CRC Error
 * Bit 10:	SSR_EOERR EB OVERRUN Error
 * Bit 9:	SSR_VBOFF VBUS OFF
 * Bit 8:	SSR_VBON VBUS ON
 * Bit 7:	SSR_TBM  Toggle Bit Mismatch
 * Bit 6:	SSR_DP DP Data Line State
 * Bit 5:	SSR_DM DM Data Line State
 * Bit 4:	SSR_HSP Host Speed
 * Bit 3:	SSR_SDE  Speed Detection End
 * Bit 2:	SSR_HFRM Host Forced Resume
 * Bit 1:	SSR_HFSUSP Host Forced Suspend
 * Bit 0:	SSR_HFRES Host Forced Reset
 ***********************************************************************************
 */
static uint32_t
ssr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ssr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/*
 **************************************************************
 * System control register
 * Bit 14: DTZIEN  DMA Total Counter Zero Int. Enable
 * Bit 12: DIEN  DUAL Interrupt Enable
 * Bit 11: VBOFE  VBUS OFF Enable
 * Bit 10: VBOE  VBUS ON Enable
 * Bit 9:  RWDE Reverse Write Data Enable
 * Bit 8:  EIE  Error Interrupt Enable
 * Bit 7:  SPDCEN  Speed Detection Control Enable
 * Bit 6:  SPDEN  Speed Detect End Interrupt Enable
 * Bit 5:  RRDE  Reverse Read Data Enable
 * Bit 4:  IPS  Interrupt Polarity Select
 * Bit 3:  SPDC  Speed detection Control
 * Bit 2:  HSSPE  Suspend Enable
 * Bit 0:  HRESE  Reset Enable
 **************************************************************
 */
static uint32_t
scr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
scr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 **************************************************************************************
 * Bit 6: LWO  Last Word Odd
 * Bit 4: SHT Stall Handshake Transmitted
 * Bit 1: TST Tx successfully transmitted
 * Bit 0: RSR Rx successfully received
 **************************************************************************************
 */
static uint32_t
ep0sr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ep0sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************
 * Bit 3: TTE Tx Test Enable
 * Bit 2: TTS Tx Toggle Set
 * Bit 1: EES Endpoint Stall Set
 * Bit 0: TZLS Tx Zero Length Set
 **********************************************************************************
 */
static uint32_t
ep0cr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ep0cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 ************************************************************************************
 * System Control register 2
 * Reset the endpoints (not during normal operation)
 ************************************************************************************
 */
static uint32_t
scr2_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
scr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 ***********************************************************************************************
 * Endpoint buffer register. 
 * Hold data for transfer.
 ***********************************************************************************************
 */
static uint32_t
ep0buf_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ep0buf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
ep1buf_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ep1buf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
ep2buf_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ep2buf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
ep3buf_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ep3buf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 ***************************************************************************
 * Phy link interface control register 
 * Control timing of Phy and interface
 ***************************************************************************
 */
static uint32_t
plicr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
plicr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 ***************************************************************************
 * Phy control register
 * Bit 7: URSTC UTMI_RESET Control
 * Bit 6: SIDC SIDDQ Control
 * Bit 4-5: OPMC  OPMODE Control
 * Bit 3: TMSC TERMSEL Control
 * Bit 2: XCRC XCVRSEL Control
 * Bit 1: SUSPC SUSPENDM Control
 * Bit 0: PCE SUSPENDM Control
 ***************************************************************************
 */
static uint32_t
pcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
pcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 *****************************************************************************
 * Endpoint status register
 * Bit 15: FUDR  FIFO Underflow
 * Bit 14: FOVF  FIFO Overflow
 * Bit 11: FPID  First OUT Packet Interrupt Disable
 * Bit 10: OSD   OUT Start DMA
 * Bit 9:  DTCZ  DMA Total Count Zero
 * Bit 8:  SPT   Short Packet Received
 * Bit 7:  DOM	 Dual Operation Mode
 * Bit 6:  FFS   FIFO Flushed
 * Bit 5:  FSC Functional Stall Condition
 * Bit 4:  LWO Last Word Odd
 * Bit 2+3: PSIF Packet Status In FIFO
 * Bit 1:  TPS Tx Packet Success
 * Bit 0:  RPS Rx Packet Success
 *****************************************************************************
 */
static uint32_t
esr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
esr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/*
 **********************************************************************************
 * Endpoint control register 
 * Bit 16: SPE  Software Reset Enable
 * Bit 12: INHLD IN Packet HOLD
 * Bit 11: OUTHD Out Packet HOLD
 * Bit 9-10: TNPMF Transaction Number / Micro Frame
 * Bit 8:  IME ISO Mode Endpoint
 * Bit 7:  DUEN Dual FIFO mode Enable
 * Bit 6:  FLUSH FIFO Flush
 * Bit 5:  TTE TX Toggle Enable
 * Bit 3+4: TTS TX Toggle Select
 * Bit 2:  CDP Clear Data PID
 * Bit 1: ESS Endpoint Stall Set
 * Bit 0: IEMS Interrupt Endpoint Mode Set
 **********************************************************************************
 */
static uint32_t
ecr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
ecr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 ************************************************************************************
 * Byte count read register
 ************************************************************************************
 */
static uint32_t
brcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
brcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 **********************************************************************************
 * Byte count write register
 **********************************************************************************
 */
static uint32_t
bwcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
bwcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

/**
 **************************************************************************************
 * 
 **************************************************************************************
 */
static uint32_t
mpr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
mpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
dcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
dcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
dtcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
dtcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
dfcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
dfcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
dttcr1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
dttcr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
dttcr2_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
dttcr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
esr2_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
esr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
upcr0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
upcr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
upcr1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
upcr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
upcr2_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
upcr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static uint32_t
upcr3_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
	return 0;
}

static void
upcr3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K USBDEV: %s: Register not implemented\n", __func__);
}

static void
TccUsbdev_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(REG_UD_IR(base));
	IOH_Delete32(REG_UD_EIR(base));
	IOH_Delete32(REG_UD_EIER(base));
	IOH_Delete32(REG_UD_FAR(base));
	IOH_Delete32(REG_UD_FNR(base));
	IOH_Delete32(REG_UD_EDR(base));
	IOH_Delete32(REG_UD_RT(base));
	IOH_Delete32(REG_UD_SSR(base));
	IOH_Delete32(REG_UD_SCR(base));
	IOH_Delete32(REG_UD_EP0SR(base));
	IOH_Delete32(REG_UD_EP0CR(base));
	IOH_Delete32(REG_UD_SCR2(base));
	IOH_Delete32(REG_UD_EP0BUF(base));
	IOH_Delete32(REG_UD_EP1BUF(base));
	IOH_Delete32(REG_UD_EP2BUF(base));
	IOH_Delete32(REG_UD_EP3BUF(base));
	IOH_Delete32(REG_UD_PLICR(base));
	IOH_Delete32(REG_UD_PCR(base));
	IOH_Delete32(REG_UD_ESR(base));
	IOH_Delete32(REG_UD_ECR(base));
	IOH_Delete32(REG_UD_BRCR(base));
	IOH_Delete32(REG_UD_BWCR(base));
	IOH_Delete32(REG_UD_MPR(base));
	IOH_Delete32(REG_UD_DCR(base));
	IOH_Delete32(REG_UD_DTCR(base));
	IOH_Delete32(REG_UD_DFCR(base));
	IOH_Delete32(REG_UD_DTTCR1(base));
	IOH_Delete32(REG_UD_DTTCR2(base));
	IOH_Delete32(REG_UD_ESR2(base));

	IOH_Delete32(REG_UPCR0(base));
	IOH_Delete32(REG_UPCR1(base));
	IOH_Delete32(REG_UPCR2(base));
	IOH_Delete32(REG_UPCR3(base));
}

static void
TccUsbdev_Map(void *owner, uint32_t base, uint32_t mask, uint32_t _flags)
{
	TccUsbDev *ud = owner;
	IOH_New32(REG_UD_IR(base), ir_read, ir_write, ud);
	IOH_New32(REG_UD_EIR(base), eir_read, eir_write, ud);
	IOH_New32(REG_UD_EIER(base), eier_read, eier_write, ud);
	IOH_New32(REG_UD_FAR(base), far_read, far_write, ud);
	IOH_New32(REG_UD_FNR(base), fnr_read, fnr_write, ud);
	IOH_New32(REG_UD_EDR(base), edr_read, edr_write, ud);
	IOH_New32(REG_UD_RT(base), rt_read, rt_write, ud);
	IOH_New32(REG_UD_SSR(base), ssr_read, ssr_write, ud);
	IOH_New32(REG_UD_SCR(base), scr_read, scr_write, ud);
	IOH_New32(REG_UD_EP0SR(base), ep0sr_read, ep0sr_write, ud);
	IOH_New32(REG_UD_EP0CR(base), ep0cr_read, ep0cr_write, ud);
	IOH_New32(REG_UD_SCR2(base), scr2_read, scr2_write, ud);
	IOH_New32(REG_UD_EP0BUF(base), ep0buf_read, ep0buf_write, ud);
	IOH_New32(REG_UD_EP1BUF(base), ep1buf_read, ep1buf_write, ud);
	IOH_New32(REG_UD_EP2BUF(base), ep2buf_read, ep2buf_write, ud);
	IOH_New32(REG_UD_EP3BUF(base), ep3buf_read, ep3buf_write, ud);
	IOH_New32(REG_UD_PLICR(base), plicr_read, plicr_write, ud);
	IOH_New32(REG_UD_PCR(base), pcr_read, pcr_write, ud);
	IOH_New32(REG_UD_ESR(base), esr_read, esr_write, ud);
	IOH_New32(REG_UD_ECR(base), ecr_read, ecr_write, ud);
	IOH_New32(REG_UD_BRCR(base), brcr_read, brcr_write, ud);
	IOH_New32(REG_UD_BWCR(base), bwcr_read, bwcr_write, ud);
	IOH_New32(REG_UD_MPR(base), mpr_read, mpr_write, ud);
	IOH_New32(REG_UD_DCR(base), dcr_read, dcr_write, ud);
	IOH_New32(REG_UD_DTCR(base), dtcr_read, dtcr_write, ud);
	IOH_New32(REG_UD_DFCR(base), dfcr_read, dfcr_write, ud);
	IOH_New32(REG_UD_DTTCR1(base), dttcr1_read, dttcr1_write, ud);
	IOH_New32(REG_UD_DTTCR2(base), dttcr2_read, dttcr2_write, ud);
	IOH_New32(REG_UD_ESR2(base), esr2_read, esr2_write, ud);

	IOH_New32(REG_UPCR0(base), upcr0_read, upcr0_write, ud);
	IOH_New32(REG_UPCR1(base), upcr1_read, upcr1_write, ud);
	IOH_New32(REG_UPCR2(base), upcr2_read, upcr2_write, ud);
	IOH_New32(REG_UPCR3(base), upcr3_read, upcr3_write, ud);
}

BusDevice *
TCC8K_UdcNew(const char *name)
{
	TccUsbDev *ud = sg_new(TccUsbDev);
	ud->bdev.first_mapping = NULL;
	ud->bdev.Map = TccUsbdev_Map;
	ud->bdev.UnMap = TccUsbdev_UnMap;
	ud->bdev.owner = ud;
	ud->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	ud->sigIrq = SigNode_New("%s.irq", name);
	if (!ud->sigIrq) {
		fprintf(stderr, "Can not create Interrupt line for USB device\n");
		exit(1);
	}
	return &ud->bdev;

}
