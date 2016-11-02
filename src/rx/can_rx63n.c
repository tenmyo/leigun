/**
 ***************************************************************************************
 *
 ***************************************************************************************
 */
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include "bus.h"
#include "sglib.h"
#include "sgstring.h"
#include "cycletimer.h"
#include "clock.h"
#include "can_rx63n.h"
#include "signode.h"
#include "socket_can.h"

#define TX_FIFO_MBX 24
#define RX_FIFO_MBX 28

#define REG_CAN_MB(base,idx)        ((base) + 0x000 + ((idx) << 4)) 
#define  REG_CAN_MB_ID(base,idx)        ((base) + 0x000 + ((idx) << 4))
#define  REG_CAN_MB_DLC(base,idx)       ((base) + 0x004 + ((idx) << 4))
#define  REG_CAN_MB_DATA(base,idx,dIdx)      ((base) + 0x006 + ((idx) << 4) + (dIdx))
#define  REG_CAN_MB_TIMEST(base,idx)    ((base) + 0x00e + ((idx) << 4))
#define REG_CAN_MKR(base,idx)       ((base) + 0x200 + ((idx) << 2))

#define REG_CAN_FIDCR0(base)        ((base) + 0x220)
#define REG_CAN_FIDCR1(base)        ((base) + 0x224)
#define REG_CAN_MKIVLR(base)        ((base) + 0x228)
#define REG_CAN_MIER(base)          ((base) + 0x22c)
#define REG_CAN_MCTL(base,idx)      ((base) + 0x620 + ((idx) << 0))
#define     MCTL_TRMREQ         (1 << 7)
#define     MCTL_RECREQ         (1 << 6)
#define     MCTL_ONESHOT        (1 << 4)
#define     MCTL_TRMABT         (1 << 2)
#define     MCTL_MSGLOSG        (1 << 2)
#define     MCTL_TRMACTIVE      (1 << 1)
#define     MCTL_INVALIDDATA    (1 << 1)
#define     MCTL_SENDDATA       (1 << 0)
#define     MCTL_NEWDATA        (1 << 0)

#define REG_CAN_CTLR(base)      ((base) + 0x640)
#define     CAN_CTLR_MBM        (1 << 0)
#define     CAN_CTLR_IDFM_MSK   (3 << 1)
#define     CAN_CTLR_MLM        (1 << 3)    
#define     CAN_CTLR_TPM        (1 << 4)
#define     CAN_CTLR_TSRC       (1 << 5)
#define     CAN_CTLR_TSPS_MSK   (3 << 6)
#define     CAN_CTLR_CANM_MSK   (3 << 8)
#define         CTLR_CANM_OPERATION  (0 << 8)
#define         CTLR_CANM_RESET      (1 << 8)
#define         CTLR_CANM_HALT       (2 << 8)
#define         CTLR_CANM_FORCERESET (3 << 8) 
#define     CAN_CTLR_SLPM       (1 << 10)
#define     CAN_CTLR_BOM_MSK    (3 << 11)
#define     CAN_CTLR_RBOC       (1 << 13)
#define REG_CAN_STR(base)       ((base) + 0x642)
#define     STR_NDST        (1 << 0)
#define     STR_SDST        (1 << 1)
#define     STR_RFST        (1 << 2)
#define     STR_TFST        (1 << 3)
#define     STR_NMLST       (1 << 4)
#define     STR_FMLST       (1 << 5)
#define     STR_TABST       (1 << 6)
#define     STR_EST         (1 << 7)
#define     STR_RSTST       (1 << 8)
#define     STR_HLTST       (1 << 9)
#define     STR_SLPST       (1 << 10)
#define     STR_EPST        (1 << 11)
#define     STR_BOST        (1 << 12)
#define     STR_TRMST       (1 << 13)
#define     STR_RECST       (1 << 14)
#define REG_CAN_BCR(base)       ((base) + 0x644)
#define     BCR_TSEG1_MSK       (0xf << 28)
#define     BCR_BRP_MSK     (0x3ff << 16)
#define     BCR_SJW_MSK     (3 << 12)
#define     BCR_TSEG2_MSK       (3 << 8)    
#define     BCR_CCLKS       (1 << 0)
#define REG_CAN_RFCR(base)      ((base) + 0x648)
#define     RFCR_RFE        (1 << 0)
#define     RFCR_RFUST_MSK      (7 << 1)
#define     RFCR_RFMLF      (1 << 4)
#define     RFCR_RFFST      (1 << 5)
#define     RFCR_RFWST      (1 << 6)
#define     RFCR_RFEST      (1 << 7)
#define REG_CAN_RFPCR(base)     ((base) + 0x649)

#define REG_CAN_TFCR(base)      ((base) + 0x64a)
#define     TFCR_TFE        (1 << 0)
#define     TFCR_TFUST_MSK      (7 << 1)
#define     TFCR_TFFST      (1 << 6)
#define     TFCR_TFEST      (1 << 7)
#define REG_CAN_TFPCR(base)     ((base) + 0x64b)
#define REG_CAN_EIER(base)      ((base) + 0x64c)
#define     EIER_BEIE       (1 << 0)
#define     EIER_EWIE       (1 << 1)
#define     EIER_EPIE       (1 << 2)
#define     EIER_BOEIE      (1 << 3)
#define     EIER_BORIE      (1 << 4)
#define     EIER_ORIE       (1 << 5)
#define     EIER_OLIE       (1 << 6)
#define     EIER_BLIE       (1 << 7)
#define REG_CAN_EIFR(base)      ((base) + 0x64d)
#define     EIFR_BEIF       (1 << 0)
#define     EIFR_EWIF       (1 << 1)
#define     EIFR_BOEIF      (1 << 3)
#define     EIFR_BORIF      (1 << 4)
#define     EIFR_ORIF       (1 << 5)
#define     EIFR_OLIF       (1 << 6)
#define     EIFR_BLIF       (1 << 7)
#define REG_CAN_RECR(base)      ((base) + 0x64e)
#define REG_CAN_TECR(base)      ((base) + 0x64f)
#define REG_CAN_ECSR(base)      ((base) + 0x650)
#define     ECSR_SEF        (1 << 0)
#define     ECSR_FEF        (1 << 1)
#define     ECSR_AEF        (1 << 2)
#define     ECSR_CEF        (1 << 3)
#define     ECSR_BE1F       (1 << 4)
#define     ECSR_BE0F       (1 << 5)
#define     ECSR_ADEF       (1 << 6)
#define     ECSR_EDPM       (1 << 7)
#define REG_CAN_CSSR(base)      ((base) + 0x651)
#define REG_CAN_MSSR(base)      ((base) + 0x652)
#define     MSSR_MBNST_MSK      (0x1f << 0)
#define     MSSR_SEST           (1 << 7)
#define REG_CAN_MSMR(base)      ((base) + 0x653)
#define     MSMR_MBSM_MSK       (3 << 0)
#define REG_CAN_TSR(base)       ((base) + 0x654)
#define REG_CAN_AFSR(base)      ((base) + 0x656)
#define REG_CAN_TCR(base)       ((base) + 0x658)
#define     TCR_TSTE            (1 << 0)
#define     TCR_TSTM_MSK        (3 << 1)
#define     TCR_TSTM_OTHER      (0 << 1)
#define     TCR_TSTM_LISTEN     (1 << 1)
#define     TCR_TSTM_EXT_LOOP   (2 << 1)
#define     TCR_TSTM_INT_LOOP   (3 << 1)

#define RXFIFO_WP(rc)   ((rc)->rxFifoWp & 3)
#define RXFIFO_RP(rc)   ((rc)->regRFPCR & 3)
#define RXFIFO_LVL(rc)  (((rc)->rxFifoWp - (rc)->regRFPCR) & 7)
#define TXFIFO_WP(rc)   ((rc)->regTFPCR & 3)
#define TXFIFO_RP(rc)   ((rc)->txFifoRp & 3)
#define TXFIFO_LVL(rc)  (((rc)->regTFPCR - (rc)->txFifoRp) & 7)

typedef struct RxCan RxCan; 
typedef struct MailBox {
    RxCan *rxCan;
    uint32_t regId;
    uint16_t regDlc; 
    uint8_t regData[8];
    uint16_t regTimeStamp; 
    uint8_t regMCTL;
    uint8_t mbNr;
} MailBox;

struct RxCan {
    BusDevice bdev;
    CanController *backend;
    SigNode *sigRxmIrq;
    SigNode *sigTxmIrq;
    SigNode *sigRxfIrq;
    SigNode *sigTxfIrq;
    SigNode *sigErsIrq;
    Clock_t *clkCan;
    Clock_t *clkCanClk;
    Clock_t *clkCanBaud;
    MailBox mailBox[32];     
    uint32_t regFIDCR0;
    uint32_t regFIDCR1;
    uint32_t regMKIVLR;
    uint32_t regMIER;
    uint16_t regCTLR;
    uint16_t regSTR;
    uint32_t regBCR;
    uint32_t regMKR[8];
    uint8_t regRFCR;
    uint8_t regRFPCR; 
    uint8_t regTFCR; 
    uint8_t rxFifoWp;
    uint8_t txFifoRp;
    uint8_t regTFPCR;
    uint8_t regEIER;
    uint8_t regEIFR;
    uint8_t regRECR; 
    uint8_t regTECR;
    uint8_t regECSR;
    uint8_t regCSSR;
    uint8_t regMSSR;
    uint8_t regMSMR;
    uint16_t regTSR;
    uint16_t regAFSR; 
    uint8_t regTCR;
};

static uint16_t 
partial_read16(uint16_t value, unsigned int bytes) 
{
    if(bytes == 1) {
        return swap16(value);
    } else {
        return value;
    }
}

static uint32_t 
partial_read32(uint32_t value, unsigned int bytes) 
{
    if(bytes == 1) {
        return swap32(value);
    } else if(bytes == 2) {
        return (value >> 16) | (value << 16);
    } else {
        //fprintf(stderr, "PR2 %08x\n, len %u",value, bytes);
        return value;
    }
}

static uint32_t 
partial_write32(uint32_t oldval, uint32_t value, unsigned int addr, unsigned int bytes) 
{
    uint32_t newval = 0;
    if(bytes == 4) {
        newval = value;
    } else if (bytes == 2) {
        if (addr & 2) {
            newval = (oldval & 0xffff0000) | (value & 0xffff);
        } else {
            newval = (oldval & 0xffff) | (value << 16);
        }
    } else if(bytes == 1) {
        uint32_t mask = 0x00ffffff;
        unsigned int shift = (addr & 3) << 3;
        mask = (0x00ffffff >> shift) | (UINT64_C(0xffffffff) << (32 - shift));
        //fprintf(stderr,"value %08x, bytes %u, addr %u, mask %08x shift %u\n", value,bytes, addr & 3, mask, shift);
        newval = (oldval & mask) | ((value << (24 - shift)) & ~mask);
    }
    return newval;
}

static uint16_t 
partial_write16(uint16_t oldval, uint16_t value, unsigned int addr, unsigned int bytes) 
{
    uint16_t newval = 0;
    if (bytes == 2) {
        newval = value;
    } else if(bytes == 1) {
        if(addr & 1) {
            newval = (oldval & 0xff00) | (value  & 0xff);
        } else {
            newval = (oldval & 0xff) | (value << 8);
        }
    }
    return newval;
}

static inline bool 
tx_fifo_mode(RxCan *rc) 
{
    return !!(rc->regTFCR & TFCR_TFE);
}

static inline bool 
rx_fifo_mode(RxCan *rc) 
{
    return !!(rc->regRFCR & RFCR_RFE);
}

static void
actualize_timestamp(RxCan *rc)
{

    /* Not yet implemented */
}

static void 
update_canclocks(RxCan *rc)
{
    unsigned int prescaler;
    unsigned int tseg1;
    unsigned int tseg2;
    unsigned int sjw;
    unsigned int clksPerBaud;
    uint32_t regBCR = rc->regBCR; 
    tseg2 = ((regBCR >> 8) & 7) + 1;
    sjw = ((regBCR >> 12) & 3) + 1;
    prescaler = ((regBCR >> 16) & 0x3ff) + 1;
    tseg1 = ((regBCR >> 28) & 0xf) + 1;
    if ((tseg1 < 4) || (tseg2 < 2)) {
        fprintf(stderr, "%s: Illegal TSEG value in regBCR\n", __FILE__);
        /* Should disable the receiver */
        return;
    }
    Clock_MakeDerived(rc->clkCanClk, rc->clkCan, 1, prescaler);  
    clksPerBaud = tseg1 + tseg2 + 1;
    Clock_MakeDerived(rc->clkCanBaud, rc->clkCanClk, 1, clksPerBaud);  
}


/**
 ***********************************************************************
 * \fn static void trigger_rfi_interrupt(RxCan *rc) 
 * Update the Rx Fifo interrupt signal line.
 * TXF interrupt is triggered only on message count change.
 * not on a level. MIER change triggers no interrupt even
 * if the fifo is not empty. 
 * so this may only be called on TX Fifo level.
 ***********************************************************************
 */
static void
trigger_rxf_interrupt(RxCan *rc) 
{
    bool rxIntEn = (rc->regMIER >> 28) & 1;
    bool rxIntWarning = (rc->regMIER >> 29) & 1;
    unsigned int rxFifoCnt;
    if (!rx_fifo_mode(rc) || !rxIntEn) {
        SigNode_Set(rc->sigRxfIrq, SIG_OPEN);
        return;
    }
    rxFifoCnt = RXFIFO_LVL(rc);
    if((rxFifoCnt >= 3) && rxIntWarning) {
        SigNode_Set(rc->sigRxfIrq, SIG_LOW);
        SigNode_Set(rc->sigRxfIrq, SIG_HIGH);
    } else if(rxFifoCnt > 0) {
        SigNode_Set(rc->sigRxfIrq, SIG_LOW);
        SigNode_Set(rc->sigRxfIrq, SIG_HIGH);
    } else {
        SigNode_Set(rc->sigRxfIrq, SIG_HIGH);
    } 
}

static void
trigger_txf_interrupt(RxCan *rc)
{
    bool txIntEn = (rc->regMIER >> 24) & 1;
    bool txIntEmpty = (rc->regMIER >> 25) & 1;
    unsigned int txFifoCnt; 
    if (!tx_fifo_mode(rc) || !txIntEn) {
        //fprintf(stderr,"Triggering nix in %u, %d %d\n", __LINE__, tx_fifo_mode(rc), txIntEn);
        SigNode_Set(rc->sigTxfIrq, SIG_HIGH);
        return;
    }
    txFifoCnt = TXFIFO_LVL(rc);
    if((txFifoCnt == 0) && txIntEmpty) {
        //fprintf(stderr,"Triggering txint in %u\n", __LINE__);
        SigNode_Set(rc->sigTxfIrq, SIG_LOW);
        SigNode_Set(rc->sigTxfIrq, SIG_HIGH);
    } else if(txFifoCnt < 4) {
        //fprintf(stderr,"Triggering txint in %u\n", __LINE__);
        SigNode_Set(rc->sigTxfIrq, SIG_LOW);
        SigNode_Set(rc->sigTxfIrq, SIG_HIGH);
    } else {
        //fprintf(stderr,"Triggering nix in %u\n", __LINE__);
        SigNode_Set(rc->sigTxfIrq, SIG_HIGH);
    }
}

static void
Rx_CanFifoReceive(RxCan *rc, const CAN_MSG *msg)
{
    unsigned int mbNr = RXFIFO_WP(rc) + 28;
    unsigned int i;
    MailBox *mb;
    mb = &rc->mailBox[mbNr];
    if (rc->regCTLR & CAN_CTLR_MLM) { /* Overrun mode */
        if(RXFIFO_LVL(rc) == 4) {
            /* Set some discard error flags is missing here */
            return;
        }
    } else { /* Overwrite mode */
        if(RXFIFO_LVL(rc) == 4) {
            fprintf(stderr,"set overwrite flag is missing here\n");
            rc->rxFifoWp = rc->regRFPCR;
        } 
    }
    mb->regId = CAN_ID(msg);
    if(CAN_MSG_29BIT(msg)) {
        mb->regId |= (UINT32_C(1) << 31);
    }
    if(CAN_MSG_RTR(msg)) {
        mb->regId |= (UINT32_C(1) << 30);
    }
    mb->regDlc = msg->can_dlc;
    for (i = 0; (i < msg->can_dlc) && (i < 8); i++) {
        mb->regData[i] = msg->data[i];
    }
    actualize_timestamp(rc);
    //mb->regTimeStamp = rc->regTimeStamp;
    rc->rxFifoWp = rc->rxFifoWp + 1;
    rc->regSTR |= STR_RFST;
    trigger_rxf_interrupt(rc);
}

static void
Rx_CanReceive(RxCan *rc, const CAN_MSG * msg)
{
    if (rc->regCTLR & CAN_CTLR_MBM) {
        /* Fifo mode */
        if (rx_fifo_mode(rc)) {
            Rx_CanFifoReceive(rc, msg);
        }
    } else {
        /* Non Fifo mode */
    }
}
/**
 ********************************************************
 * Interface function for the HAL.
 ********************************************************
 */
static void
Rx_CanBkEndReceive(void *clientData, const CAN_MSG * msg)
{
    RxCan *rc = clientData;
    Rx_CanReceive(rc, msg);
}

static void
Rx_CanTransmit(RxCan * rc, unsigned int mbNr)
{
    CAN_MSG msg;
    MailBox *mb; 
    unsigned int i;

    if (mbNr >= array_size(rc->mailBox)) {
        fprintf(stderr, "Bug: Illegal Mailbox Nr in %s\n", __func__);
        exit(1);
    }
    mb = &rc->mailBox[mbNr];
    memset(&msg, 0, sizeof(msg));
    msg.can_id = mb->regId & 0x1fffffff; 
    if (mb->regId & (1 << 30)) {
        if (mb->regId & (1 << 31)) {
            CAN_MSG_T_29_RTR(&msg);
        } else {
            CAN_MSG_T_11_RTR(&msg); 
        }
        msg.can_dlc = 0;
    } else {
        msg.can_dlc = mb->regDlc;;
        for (i = 0; (i < 8) && i < (mb->regDlc); i++) {
            msg.data[i] = mb->regData[i];
            //fprintf(stderr, "Data %u: %02x\n", i, msg.data[i]);
        }
        if (mb->regId & (1 << 31)) {
            CAN_MSG_T_29(&msg); 
        } else {
            CAN_MSG_T_11(&msg); 
        }
    }
    if(rc->regTCR & TCR_TSTE) { 
        unsigned int tstm = rc->regTCR & TCR_TSTM_MSK;
        switch(tstm) {
            case TCR_TSTM_LISTEN:
                break;

            case TCR_TSTM_EXT_LOOP:
                fprintf(stderr,"RX-CAN: External loop mode not implemented\n");
                break;

            case TCR_TSTM_INT_LOOP:
                Rx_CanReceive(rc, &msg);
                break;

            case TCR_TSTM_OTHER:
                break;
        } 
    } else {
        CanSend(rc->backend, &msg);
    }
}

static void
StartTransmitter(RxCan *rc)
{
    unsigned int mbNr;
    while (rc->txFifoRp != rc->regTFPCR) {
        mbNr = TXFIFO_RP(rc) + TX_FIFO_MBX;  
        Rx_CanTransmit(rc, mbNr);
        rc->txFifoRp++;
        trigger_txf_interrupt(rc);
    }
}


static MailBox *
calc_fifo_mbx(RxCan *rc, unsigned int mbNr)
{
    MailBox *mb;
    if ((mbNr >= 24) && (mbNr < 28) && tx_fifo_mode(rc)) {
        mb = &rc->mailBox[24 + (rc->regTFPCR & 3)];
    } else if ((mbNr >= 28) && rx_fifo_mode(rc)) {
        mb = &rc->mailBox[28 + (rc->regRFPCR & 3)];
    } else {
        mb = &rc->mailBox[mbNr];
    }
    return mb;
}

/**
 ******************************************************************************
 * Message box ID field. 
 * Bits 0-17: EID
 * Bits 18-28: SID
 * Bit 30: RTR (Remote Frame)
 * Bit 31: IDE Extended ID (29 Bit message).
 ******************************************************************************
 */
static void
mbid_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    MailBox *mb = clientData;
    RxCan *rc = mb->rxCan;
    mb = calc_fifo_mbx(rc, mb->mbNr);
    value = partial_write32(mb->regId, value, address, rqlen);
    if(value & (1 << 29)) {
        fprintf(stderr,"Illegal write value in MBID register: %08x\n", value);
    }
    //fprintf(stderr,"MBID %08x\n", value);
    mb->regId = value;
}

static uint32_t
mbid_read(void *clientData, uint32_t address, int rqlen)
{
    MailBox *mb = clientData;
    RxCan *rc = mb->rxCan;
    mb = calc_fifo_mbx(rc, mb->mbNr);
    //mb->regId = 0x987654;
    return partial_read32(mb->regId, rqlen);
}

/**
 ****************************************************************************************************
 * \fn static void mbdlc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * Mailbox Data Length code.
 ****************************************************************************************************
 */
static void
mbdlc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    MailBox *mb = clientData;
    RxCan *rc = mb->rxCan;
    mb = calc_fifo_mbx(rc, mb->mbNr);
    value = partial_write16(mb->regDlc, value, address, rqlen);
    if(value > 8) {
        fprintf(stderr,"Illegal Data Length Code %u\n", value);
    } 
    mb->regDlc = value;
}

static uint32_t
mbdlc_read(void *clientData, uint32_t address, int rqlen)
{
    MailBox *mb = clientData;
    RxCan *rc = mb->rxCan;
    mb = calc_fifo_mbx(rc, mb->mbNr);
    return partial_read16(mb->regDlc, rqlen);
}

/**
 ************************************************************************************
 * Messagebox Data
 ************************************************************************************
 */
static void
mbdata_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    MailBox *mb = clientData;
    unsigned int idx;
    RxCan *rc = mb->rxCan;
    mb = calc_fifo_mbx(rc, mb->mbNr);
    idx = (address - 6) & 7;
    //fprintf(stderr,"Addr %08x Data %u write %02x\n", address, idx, value);
    mb->regData[idx] = value;
}

static uint32_t
mbdata_read(void *clientData, uint32_t address, int rqlen)
{
    unsigned int idx;
    MailBox *mb = clientData;
    RxCan *rc = mb->rxCan;
    mb = calc_fifo_mbx(rc, mb->mbNr);
    idx = (address - 6) & 7;
    return mb->regData[idx];
}

/**
 *************************************************************************************************
 * \fn static void mbts_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * Messagebox timestamp register. This register is writable for unknown reason.
 *************************************************************************************************
 */
static void
mbts_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    MailBox *mb = clientData;
    RxCan *rc = mb->rxCan;
    mb = calc_fifo_mbx(rc, mb->mbNr);
    value = partial_write16(mb->regTimeStamp, value, address, rqlen);
    mb->regTimeStamp = value;
}

static uint32_t
mbts_read(void *clientData, uint32_t address, int rqlen)
{
    MailBox *mb = clientData;
    RxCan *rc = mb->rxCan;
    mb = calc_fifo_mbx(rc, mb->mbNr);
    return partial_read16(mb->regTimeStamp, rqlen);
}

/**
 ***********************************************************************************************
 * \fn static void mctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * MCTL register.
 * Bit 0: SENTDATA/NEWDATA
 * Bit 1: INVALDATA/TRMACTIVE
 * Bit 2: MSGLOST
 * Bit 4: ONESHOT
 * Bit 6: RECREQ
 * Bit 7: TRMREQ
 ***********************************************************************************************
 */
static void
mctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    MailBox *mb = clientData;
    mb->regMCTL = value;
    //StartTransmitter(rc);
    if ((value & MCTL_TRMREQ) && !(value & MCTL_RECREQ)) {
        fprintf(stderr, "tranmission not yet implemented\n"); 
    } 
}

static uint32_t
mctl_read(void *clientData, uint32_t address, int rqlen)
{
    MailBox *mb = clientData;
    return mb->regMCTL;
}

/**
 *************************************************************************************
 * CAN Fifo received ID Compare Registers.
 * Only valid when MBM is 1. See section 31.6
 *************************************************************************************
 */
static void
fidcr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    value = partial_write32(rc->regFIDCR0, value, address, rqlen);
    rc->regFIDCR0 = value;
    fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
fidcr0_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    fprintf(stderr, "%s not implemented\n", __func__);
    return partial_read32(rc->regFIDCR0, rqlen); 
}

static void
fidcr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    value = partial_write32(rc->regFIDCR1, value, address, rqlen);
    rc->regFIDCR1 = value;
    fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
fidcr1_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    fprintf(stderr, "%s not implemented\n", __func__);
    return partial_read32(rc->regFIDCR1, rqlen); 
}

static void
mkivlr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    value = partial_write32(rc->regMKIVLR, value, address, rqlen);
    rc->regMKIVLR = value;
    fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
mkivlr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    fprintf(stderr, "%s not implemented\n", __func__);
    return partial_read32(rc->regMKIVLR, rqlen); 
}

/**
 *********************************************************************************************
 * \fn static void mier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * Mailbox interrupt enable register.
 * Normal mailbox mode: 1 Bit per Messagebox, 1 = interrupt enabled
 * Fifo mode: 
 * Bit 24 Transmit FIFO interrupt enable
 * Bit 25: Tx-Interrupt generation on message completion (0) or FIFO empty (1). 
 * Bit 28: Receive Fifo Interrupt Enable
 * Bit 29: Interrupt Generation every time / on fifo buffer warning level
 *********************************************************************************************
 */
static void
mier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    value = partial_write32(rc->regMIER, value, address, rqlen);
    rc->regMIER = value;
    fprintf(stderr,"MIER %08x\n",rc->regMIER);
    //update_interrupts(rc);
}

static uint32_t
mier_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    return partial_read32(rc->regMIER, rqlen); 
}

static void
mkr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    int idx = (address >> 2) & 7;
    value = partial_write32(rc->regMKR[idx], value, address, rqlen);
    rc->regMKR[idx] = value; 
}

static uint32_t
mkr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    int idx = (address >> 2) & 7;
    return partial_read32(rc->regMKR[idx], rqlen); 
}

/**
 ********************************************************************************************
 * \fn void ctlr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * CAN control Register
 ********************************************************************************************
 */
static void
ctlr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    value = partial_write16(rc->regCTLR, value, address, rqlen);
    rc->regCTLR = value;
    if (value & CAN_CTLR_SLPM) {
        rc->regSTR |= STR_SLPST;
    } else {
        rc->regSTR &= ~STR_SLPST;
    }
    rc->regSTR &= ~(STR_HLTST | STR_RSTST);
    switch (value & CAN_CTLR_CANM_MSK) {
        case CTLR_CANM_OPERATION:
            CanStartRx(rc->backend);
            break;
        case CTLR_CANM_RESET:
            CanStopRx(rc->backend);
            rc->regSTR |= STR_RSTST;
            break;
        case CTLR_CANM_HALT:
            CanStopRx(rc->backend);
            rc->regSTR |= STR_HLTST;
            break;
        case CTLR_CANM_FORCERESET:
            CanStopRx(rc->backend);
            rc->regSTR |= STR_RSTST;
            break;
    }
    //fprintf(stderr, "%s 0x%04x: str %08x\n", __func__, value,  rc->regSTR);
}

static uint32_t
ctlr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    uint16_t value = rc->regCTLR;
    return partial_read16(value, rqlen);
}

/**
 **********************************************************************************************
 * \fn static void str_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * Status register is a readonly register. 
 **********************************************************************************************
 */
static void
str_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "Writing readonly register: \"%s\" \n", __func__);
}

/**
 ************************************************************************************************************
 * \fn static uint32_t str_read(void *clientData, uint32_t address, int rqlen)
 * Status Register:
 * Bit 0:  NDST  Newdata status flag 
 * Bit 1:  SDST  Senddata status flag  
 * BIt 2:  RFST  Receive FIFO Status flag.
 * Bit 3:  TFST  Transmit FIFO status flag.
 * Bit 4:  NMLST Normal Mailbox Message Lost status flag.
 * Bit 5:  FMLST FIFO Mailbox message lost status flag.
 * Bit 6:  TABST Tranmission Abort Status Flag
 * Bit 7:  EST   Error Status Flag
 * Bit 8:  RSTST CAN Reset Status flag
 * Bit 9:  HLTST CAN Halt Status flag
 * Bit 10: SLPST CAN Sleep status flag
 * Bit 11: EPST  Error Passive Status flag
 * Bit 12: BOST  Bus off status flag
 * Bit 13: TRMST Transmit status flag
 * Bit 14: RECST Receive Status flag.
 ************************************************************************************************************
 */
static uint32_t
str_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    return partial_read16(rc->regSTR, rqlen);
}

static void
bcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    value = partial_write32(rc->regBCR, value, address, rqlen);
    rc->regBCR = value;
    update_canclocks(rc);
}

static uint32_t
bcr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    return partial_read32(rc->regBCR, rqlen); 
}

/**
 ****************************************************************************************
 * Receive Fifo control Register
 * Bit 0: RFE   Receive FIFO Enable
 * Bit 1-3: RFUST   Receive FIFO Unread Message Number Status (0 - 4)
 * Bit 4: Receive FIFO Message Lost Flag
 * Bit 5: Receive FIFO Full Status flag
 * Bit 6: Receive FIFO Buffer Warning Status Flag
 * Bit 7: Receive FIFO Empty Status Flag
 ****************************************************************************************
 */
static void
rfcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    rc->regRFCR = value;
}

static uint32_t
rfcr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    return rc->regRFCR;
}

/**
 ******************************************************************************************************
 * \fn static void rfpcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * Receive FIFO Pointer Control Register. Write only register. The CPU side pointer
 * for the receive fifo is incremented by one by writing 0xff 
 ******************************************************************************************************
 */
static void
rfpcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    if (value == 0xff) {
        if (RXFIFO_LVL(rc) > 0) {
            rc->regRFPCR = rc->regRFPCR + 1; 
        } else {
            fprintf(stderr,"RX-Fifo already empty\n");
        }
        if (RXFIFO_LVL(rc) == 0) {
            rc->regSTR &= ~(STR_RFST);
        }
    } else {
        fprintf(stderr, "%s wrong value 0x%02x \n", __func__, value);
    }
}

static uint32_t
rfpcr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s reading writeonly register.\n", __func__);
    return 0;
}

/**
 **************************************************************************************************
 * \fn static void tfcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * Transmit FIFO control register.
 * Bit 0: TFE Transmit FIFO enable
 * Bit 1 to 3: 0 - 4 Unsent messages
 * Bit 6: TFFST Transmit fifo full status 1 = FIFO is full 
 * Bit 7: TFEST 0 = Transmit fifo has unsent messages.
 **************************************************************************************************
 */
static void
tfcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    rc->regTFCR = (rc->regTFCR & 0xfe) | (value & 1); /* Only TFE is writable */
}

static uint32_t
tfcr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    return rc->regTFCR;
}

/*
 ************************************************************************************************
 * Transmit FIFO pointer control register.
 * When the tranmit fifo is not full writing 0xff to FTPCR increments the CPU side pointer
 * for the next mailbox location. It also triggers the transmission.
 ************************************************************************************************
 */
static void
tfpcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    if (value != 0xff) {
        fprintf(stderr, "%s non 0xff value written: 0x%02x\n", __func__, value);
        return; 
    }
    rc->regTFPCR = rc->regTFPCR + 1; 
    StartTransmitter(rc);
}

static uint32_t
tfpcr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    fprintf(stderr, "%s reading writeonly register\n", __func__);
    return rc->regTFPCR & 3;
}

static void
eier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
eier_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
    return 0;
}
static void
eifr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
eifr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
    return 0;
}

static void
recr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
recr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
    return 0;
}

static void
tecr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
tecr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
    return 0;
}

static void
ecsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
ecsr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
    return 0;
}

/**
 *************************************************************************************************
 * \fn static void cssr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * Channel search support register. Returns the number of the lowest bit set in th MSSR MBNST.
 *************************************************************************************************
 */
static void
cssr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    rc->regCSSR = value;
    if ((rc->regMSMR & MSMR_MBSM_MSK) == 3) { /* Channel search mode */
        int i;
        for( i = 0; i < 8; i++) {
            if (value & (1 << i)) {
                rc->regMSSR = i;
                break;
            }
        }
        if (i == 8) {
            rc->regMSSR = MSSR_SEST; 
        }
    } else {
        fprintf(stderr, "Write CSSR not in channel search mode\n");
    }
}

static uint32_t
cssr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    return rc->regCSSR;
}
/**
 ****************************************************************************************************
 * \fn void mssr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * Mailbox Search Status Register
 * Bit 0-4 MBNST Search Result Mailbox Number status
 * Bit 7:  REST  Search Result status
 ****************************************************************************************************
 */
static void
mssr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s writing to readonly register\n", __func__);
}

static uint32_t
mssr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
#if 0
    if (rc->regCSSR == 0) {
            return MSSR_SEST;
    } else {
        //for (i = 0; i < 8; );
    }
#endif
    return rc->regMSSR;
}

/**
 *****************************************************************************
 * Mailbox Search Mode Register.
 * Bits 0-1: 00: Receive mailbox search mode 
 *           01: Transmit mailbox search mode 
 *           10: Message lost search mode
 *           11: Channel search mode
 *****************************************************************************
 */
static void
msmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    rc->regMSMR = value; 
}

static uint32_t
msmr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    return rc->regMSMR;
}

static void
tsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s writing readonly register\n", __func__);
}

static uint32_t
tsr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    actualize_timestamp(rc);
    return rc->regTSR;
}

static void
afsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
afsr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr, "%s not implemented\n", __func__);
    return 0;
}

/**
 **********************************************************************************************
 * \fn static void tcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * Test control register.
 * Bit 0:    TSTE CAN Test Mode Enable 
 * Bits 1-2: TSTM CAN Test Mode Select
 **********************************************************************************************
 */
static void
tcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    rc->regTCR = value & 7;
}

static uint32_t
tcr_read(void *clientData, uint32_t address, int rqlen)
{
    RxCan *rc = clientData;
    return rc->regTCR;
}


static void
RxCan_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
    //uint32_t flags = IOH_FLG_PWR_RMW | IOH_FLG_PRD_RARP | IOH_FLG_BIG_ENDIAN | IOH_FLG_PA_CBSE;
    uint32_t flags = /*IOH_FLG_PWR_RMW |*/ IOH_FLG_PRD_RARP |  IOH_FLG_PA_CBSE;
    RxCan *rc = owner;
    unsigned int mbIdx, dIdx, i;
    for (mbIdx = 0; mbIdx < array_size(rc->mailBox); mbIdx++) {
        MailBox *mb = &rc->mailBox[mbIdx];
        IOH_New32f(REG_CAN_MB_ID(base,mbIdx), mbid_read, mbid_write, mb, flags);
        IOH_New16f(REG_CAN_MB_DLC(base,mbIdx), mbdlc_read, mbdlc_write, mb, flags);
        for (dIdx = 0; dIdx < 8; dIdx++) {
            uint32_t dflags = IOH_FLG_OSZR_NEXT | IOH_FLG_OSZW_NEXT; /* | IOH_FLG_BIG_ENDIAN */;
            IOH_New8f(REG_CAN_MB_DATA(base, mbIdx, dIdx), mbdata_read, mbdata_write, mb, dflags);
        }
        IOH_New16f(REG_CAN_MB_TIMEST(base, mbIdx), mbts_read, mbts_write, mb, flags);
        IOH_New8(REG_CAN_MCTL(base,mbIdx), mctl_read, mctl_write, mb);
    }
    IOH_New32f(REG_CAN_FIDCR0(base), fidcr0_read, fidcr0_write, rc, flags);
    IOH_New32f(REG_CAN_FIDCR1(base), fidcr1_read, fidcr1_write, rc, flags);
    IOH_New32f(REG_CAN_MKIVLR(base), mkivlr_read, mkivlr_write, rc, flags);
    IOH_New32f(REG_CAN_MIER(base), mier_read, mier_write, rc, flags);
    
    IOH_New16f(REG_CAN_CTLR(base), ctlr_read, ctlr_write, rc, flags);
    IOH_New16f(REG_CAN_STR(base), str_read, str_write, rc, flags);
    IOH_New32f(REG_CAN_BCR(base), bcr_read, bcr_write, rc, flags);
    for (i = 0; i < 8; i++) {
        IOH_New32f(REG_CAN_MKR(base, i), mkr_read, mkr_write, rc, flags);
    }
    IOH_New8(REG_CAN_RFCR(base), rfcr_read, rfcr_write, rc);
    IOH_New8(REG_CAN_RFPCR(base), rfpcr_read, rfpcr_write, rc);
    IOH_New8(REG_CAN_TFCR(base), tfcr_read, tfcr_write, rc);
    IOH_New8(REG_CAN_TFPCR(base), tfpcr_read, tfpcr_write, rc);
    IOH_New8(REG_CAN_EIER(base), eier_read, eier_write, rc);
    IOH_New8(REG_CAN_EIFR(base), eifr_read, eifr_write, rc); 
    IOH_New8(REG_CAN_RECR(base), recr_read, recr_write, rc);
    IOH_New8(REG_CAN_TECR(base), tecr_read, tecr_write, rc);
    IOH_New8(REG_CAN_ECSR(base), ecsr_read, ecsr_write, rc);
    IOH_New8(REG_CAN_CSSR(base), cssr_read, cssr_write, rc);
    IOH_New8(REG_CAN_MSSR(base), mssr_read, mssr_write, rc); 
    IOH_New8(REG_CAN_MSMR(base), msmr_read, msmr_write, rc); 
    IOH_New16f(REG_CAN_TSR(base), tsr_read, tsr_write, rc, flags); 
    IOH_New16f(REG_CAN_AFSR(base), afsr_read, afsr_write, rc, flags);
    IOH_New8(REG_CAN_TCR(base), tcr_read, tcr_write, rc);
}

static void
RxCan_Unmap(void *owner, uint32_t base, uint32_t mask)
{
    RxCan *rc = owner;
    unsigned int mbIdx, dIdx, i;
    for (mbIdx = 0; mbIdx < array_size(rc->mailBox); mbIdx++) {
        IOH_Delete32(REG_CAN_MB_ID(base,mbIdx));
        IOH_Delete16(REG_CAN_MB_DLC(base,mbIdx));
        for (dIdx = 0; dIdx < 8; dIdx++) {
            IOH_Delete8(REG_CAN_MB_DATA(base, mbIdx, dIdx));
        }
        IOH_Delete16(REG_CAN_MB_TIMEST(base, mbIdx));
        IOH_Delete8(REG_CAN_MCTL(base,mbIdx));
    }
    IOH_Delete32(REG_CAN_FIDCR0(base));
    IOH_Delete32(REG_CAN_FIDCR1(base));
    IOH_Delete32(REG_CAN_MKIVLR(base));
    IOH_Delete32(REG_CAN_MIER(base));
    
    IOH_Delete16(REG_CAN_CTLR(base));
    IOH_Delete16(REG_CAN_STR(base));
    IOH_Delete32(REG_CAN_BCR(base));
    for (i = 0; i < 8; i++) {
        IOH_Delete32(REG_CAN_MKR(base, i));
    }
    IOH_Delete8(REG_CAN_RFCR(base));
    IOH_Delete8(REG_CAN_RFPCR(base));
    IOH_Delete8(REG_CAN_TFCR(base));
    IOH_Delete8(REG_CAN_TFPCR(base));
    IOH_Delete8(REG_CAN_EIER(base));
    IOH_Delete8(REG_CAN_EIFR(base)); 
    IOH_Delete8(REG_CAN_RECR(base));
    IOH_Delete8(REG_CAN_TECR(base));
    IOH_Delete8(REG_CAN_ECSR(base));
    IOH_Delete8(REG_CAN_CSSR(base));
    IOH_Delete8(REG_CAN_MSSR(base)); 
    IOH_Delete8(REG_CAN_MSMR(base)); 
    IOH_Delete16(REG_CAN_TSR(base)); 
    IOH_Delete16(REG_CAN_AFSR(base));
    IOH_Delete8(REG_CAN_TCR(base));
}

static CanChipOperations canOps = {
    .receive = Rx_CanBkEndReceive,
};

BusDevice *
RX63CAN_New(const char *name)
{
    unsigned int mbIdx;
    RxCan *rc = sg_new(RxCan);

    rc->backend = CanSocketInterface_New(&canOps, name, rc);
    if (!rc->backend) {
        fprintf(stderr, "Can not create CAN backend \"%s\"\n", name);
        exit(1);
    }
    rc->clkCan = Clock_New("%s.fcan", name);
    rc->clkCanClk = Clock_New("%s.fcanclk", name);
    rc->clkCanBaud = Clock_New("%s.baud", name);
    if (!rc->clkCan || !rc->clkCanClk || !rc->clkCanBaud) {
        fprintf(stderr,"Can not create clock for CAN module\n");
        exit(1);
    }
    rc->sigRxmIrq = SigNode_New("%s.irqRXM", name);
    rc->sigTxmIrq = SigNode_New("%s.irqTXM", name);
    rc->sigRxfIrq = SigNode_New("%s.irqRXF", name);
    rc->sigTxfIrq = SigNode_New("%s.irqTXF", name);
    rc->sigErsIrq = SigNode_New("%s.irqERS", name);
    if (!rc->sigRxmIrq || !rc->sigTxmIrq || !rc->sigRxfIrq || !rc->sigTxfIrq || !rc->sigErsIrq) {
        fprintf(stderr,"Can not create interrupt lines for CAN module \"%s\"\n", name);
        exit(1);
    }
    SigNode_Set(rc->sigTxfIrq, SIG_HIGH);
    SigNode_Set(rc->sigRxfIrq, SIG_HIGH);
    for (mbIdx = 0; mbIdx < array_size(rc->mailBox); mbIdx++) {
        MailBox *mb = &rc->mailBox[mbIdx];
        mb->rxCan = rc;
        mb->mbNr = mbIdx;
    }
    rc->regCTLR = 0x500;
    rc->regBCR = 0;
    rc->regRFCR = 0x80; 
    rc->regTFCR = 0x80;
    rc->regSTR = 0x500;
    rc->regMSMR = 0;
    rc->regMSSR = 0x80;
    rc->regEIER = 0;
    rc->regEIFR = 0;
    rc->regRECR = 0;
    rc->regTECR = 0;
    rc->regECSR = 0;
    rc->regTSR = 0;
    rc->regTCR = 0; 
    rc->regMIER = 0xf4150148;  /* Docu: Undefined, Value is from real board */

    rc->bdev.first_mapping = NULL;
    rc->bdev.Map = RxCan_Map;
    rc->bdev.UnMap = RxCan_Unmap;
    rc->bdev.owner = rc;
    rc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
    fprintf(stderr,"Renesas RX CAN controller %s Created\n", name);
    return &rc->bdev;
}
