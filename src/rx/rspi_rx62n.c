/**
 */

#include <stdint.h>
#include "rspi_rx62n.h"
#include "sgstring.h"
#include "bus.h"
#include "signode.h"

#define REG_SPCR(base)      ((base) + 0x00)
#define     SPCR_SPMS       (1 << 0)
#define     SPCR_TXMD       (1 << 1)
#define     SPCR_MODFEN     (1 << 2)
#define     SPCR_MSTR       (1 << 3)
#define     SPCR_SPEIE      (1 << 4)
#define     SPCR_SPTIE      (1 << 5)
#define     SPCR_SPE        (1 << 6)
#define     SPCR_SPRIE      (1 << 7)

#define REG_SSLP(base)      ((base) + 0x01)
#define     SSL0P           (1 << 0)
#define     SSL1P           (1 << 1)
#define     SSL2P           (1 << 2)
#define     SSL3P           (1 << 3)

#define REG_SPPCR(base)     ((base) + 0x02) 
#define     SPPCR_SPLP      (1 << 0)
#define     SPPCR_SPLP2     (1 << 1)
#define     SPPCR_SPOM      (1 << 2)
#define     SPPCR_MOIFV     (1 << 4) 
#define     SPPCR_MOIFE     (1 << 5)

#define REG_SPSR(base)      ((base) + 0x03)
#define     SPSR_OVRF       (1 << 0)
#define     SPSR_IDLNF      (1 << 1)
#define     SPSR_MODF       (1 << 2)
#define     SPSR_PERF       (1 << 3)
#define     SPSR_SPTEF      (1 << 5)
#define     SPSR_SPRF       (1 << 7)
#define REG_SPDR(base)      ((base) + 0x04)
#define REG_SPSCR(base)     ((base) + 0x08)
#define     SPSCR_SPSLN_MSK (7)
#define REG_SPSSR(base)     ((base) + 0x09)
#define     SPSSR_SPCP_MSK      (7)
#define     SPSSR_SPECM_MSK     (7 << 4) 
#define REG_SPBR(base)      ((base) + 0x0a)
#define REG_SPDCR(base)     ((base) + 0x0b)
#define     SPDCR_SPFC_MSK      (3 << 0)
#define     SPDCR_SPFC_SLSEL    (3 << 2)
#define     SPDCR_SPRDTD        (1 << 4)
#define     SPDCR_SPLW          (1 << 5)
#define REG_SPCKD(base)     ((base) + 0x0c)
#define     SPCKD_SCKDL_MSK     (7 << 0)
#define REG_SSLND(base)     ((base) + 0x0d)
#define     SSLND_SLNDL_MSK     (7 << 0)
#define REG_SPND(base)      ((base) + 0x0e)
#define      SPND_SPNDL_MSK     (7 << 0)
#define REG_SPCR2(base)     ((base) + 0x0f)
#define     SPCR2_SPPE      (1 << 0)
#define     SPCR2_SPOE      (1 << 1)
#define     SPCR2_SPIIE     (1 << 2)
#define     SPCR2_PTE       (1 << 3)
#define REG_SPCMD0(base)    ((base) + 0x10)
#define     SPCMD_CPHA       (1 << 0)
#define     SPCMD_CPOL       (1 << 1)
#define     SPCMD_BRDV_MSK   (3 << 2)
#define     SPCMD_SSLA_MSK   (3 << 4)
#define     SPCMD_SSLKP      (1 << 7) 
#define     SPCMD_SPB_MSK    (0xf << 8) 
#define     SPCMD_LSBF       (1 << 12)
#define     SPCMD_SPNDEN     (1 << 13)
#define     SPCMD_SLNDEN     (1 << 14)
#define     SPCMD_SCKDEN     (1 << 15)
#define REG_SPCMD1(base)    ((base) + 0x12)
#define REG_SPCMD2(base)    ((base) + 0x14)
#define REG_SPCMD3(base)    ((base) + 0x16)
#define REG_SPCMD4(base)    ((base) + 0x18)
#define REG_SPCMD5(base)    ((base) + 0x1A)
#define REG_SPCMD6(base)    ((base) + 0x1C)
#define REG_SPCMD7(base)    ((base) + 0x1E)

#define ICODE_SIZE  (512)

#define INSTR_MOSI_HIGH     UINT32_C(0x01000000)
#define INSTR_MOSI_LOW      UINT32_C(0x02000000)
#define INSTR_MISO_HIGH     UINT32_C(0x03000000)
#define INSTR_MISO_LOW      UINT32_C(0x04000000)
#define INSTR_CLK_HIGH      UINT32_C(0x05000000)
#define INSTR_CLK_LOW       UINT32_C(0x06000000)
#define INSTR_CS_HIGH(csNr) (UINT32_C(0x07000000) | (csNr))
#define INSTR_CS_LOW(csNr)  (UINT32_C(0x08000000) | (csNr))
#define INSTR_NDELAY(ns)    (UINT32_C(0x09000000) | (ns))
#define INSTR_CLKDELAY(cyc) (UINT32_C(0x0A000000) | (cyc))
#define INSTR_SAMPLE_MOSI   (UINT32_C(0x0B000000))
#define INSTR_SAMPLE_MISO   (UINT32_C(0x0C000000))
 
typedef struct RxRSpi {
    BusDevice bdev;
    uint32_t icode[ICODE_SIZE];
    uint32_t instrP;
    SigNode *sigMosi;
    SigNode *sigMiso;

    SigNode *sigShiftOut; /* Connected to MOSI in master mode or to Miso in Salve mode */
    SigNode *sigShiftIn;

    SigNode *sigSclk;
    SigNode *sigSSL[4];
    SigNode *sigIrqSpti; /* SPI TX interrupt */
    SigNode *sigIrqSpri; /* SPI RX interrupt */
    SigNode *sigIrqSpii; /* SPI Idle interrupt */
    SigNode *sigIrqSpei; /* SPI Error interrupt */
    uint8_t rxFifo[16];
    uint8_t txFifo[16];
    uint8_t regSPCR;
    uint8_t regSSLP;
    uint8_t regSPPCR; 
    uint8_t regSPSR;
    uint8_t regSPSCR;
    uint8_t regSPSSR;
    uint8_t regSPBR; 
    uint8_t regSPDCR;
    uint8_t regSPCKD;
    uint8_t regSSLND;
    uint8_t regSPND;
    uint8_t regSPCR2;
    uint16_t regSPCMD[8];
} RxRSpi;

static void
RxSpi_EvalInstr(RxRSpi *rspi) 
{
    uint32_t icode;
    uint32_t instr, arg;
    if (rspi->instrP >= array_size(rspi->icode)) {
        fprintf(stderr, "Illegal instruction pointer in %s\n", __func__); 
        return;
    } 
    icode = rspi->icode[rspi->instrP];
    instr = icode & UINT32_C(0xff000000); 
    arg = icode & 0xffffff;
    switch (instr) {
    }
}

static uint32_t
spcr_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPCR;
}

static void
spcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    rspi->regSPCR = value;
    // update_rx_interrupt();
}

static uint32_t
sslp_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSSLP;
}

/* 
 ***********************************************************************************************
 * \fn static void sslp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
 * Set polatity of chip select signals.
 * Change of polarity when during active controller (SPE == 1) may cause problems. 
 ***********************************************************************************************
 */
static void
sslp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    int i;
    uint8_t diff = rspi->regSSLP ^ value;
    if (rspi->regSPCR & SPCR_SPE) {
        fprintf(stderr, "Warning: Chip select polarity change while SPI is enabled\n");
    }
    rspi->regSSLP = value & 0xf;;
    for (i = 0; i < 4; i++) {
        if (!(diff &  (1 << i))) {
            continue;
        }
        if((value >> i) & 1) {
            SigNode_Set(rspi->sigSSL[i], SIG_LOW);
        } else {
            SigNode_Set(rspi->sigSSL[i], SIG_HIGH);
        }
    }
}

static uint32_t
sppcr_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPPCR;
}

static void
sppcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    if (value & SPPCR_SPLP) {
#if 0
            SigNode_RemoveLink(rspi->sigMosi, rspi->sigShiftOut);
            SigNode_RemoveLink(rspi->sigMiso, rspi->sigShiftOut);
            SigNode_RemoveLink(rspi->sigMosi, rspi->sigShiftIn);
            SigNode_RemoveLink(rspi->sigMiso, rspi->sigShiftIn);
#endif
    } else if (value & SPPCR_SPLP2) {
#if 0
            SigNode_RemoveLink(rspi->sigMosi, rspi->sigShiftOut);
            SigNode_RemoveLink(rspi->sigMiso, rspi->sigShiftOut);
            SigNode_RemoveLink(rspi->sigMosi, rspi->sigShiftIn);
            SigNode_RemoveLink(rspi->sigMiso, rspi->sigShiftIn);
#endif
    }
    rspi->regSPPCR = value & 0x37;
}

/**
 * RSPI Status Register SPSR
 */
static uint32_t
spsr_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPSR;
}

static void
spsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    uint8_t mask = value | 0xf2;
    rspi->regSPSR = rspi->regSPSR & mask;
}

static uint32_t
spdr_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return 0;
}

static void
spdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
spscr_read(void *clientData, uint32_t address, int rqlen)
{
    return 0;
}

static void
spscr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
spssr_read(void *clientData, uint32_t address, int rqlen)
{
    return 0;
}

static void
spssr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
spbr_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPBR;
}

static void
spbr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

/**
 * RSPI Data control register
 */
static uint32_t
spdcr_read(void *clientData, uint32_t address, int rqlen)
{
    return 0;
}

static void
spdcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
spckd_read(void *clientData, uint32_t address, int rqlen)
{
    return 0;
}

static void
spckd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
sslnd_read(void *clientData, uint32_t address, int rqlen)
{
    return 0;
}

static void
sslnd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
spnd_read(void *clientData, uint32_t address, int rqlen)
{
    return 0;
}

static void
spnd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
spcr2_read(void *clientData, uint32_t address, int rqlen)
{
    return 0;
}

static void
spcr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
spcmd0_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPCMD[0];
}

static void
spcmd0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    rspi->regSPCMD[0] = value;
}

static uint32_t
spcmd1_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPCMD[1];
}

static void
spcmd1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    rspi->regSPCMD[1] = value;
}

static uint32_t
spcmd2_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPCMD[2];
}

static void
spcmd2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    rspi->regSPCMD[2] = value;
}

static uint32_t
spcmd3_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPCMD[3];
}

static void
spcmd3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    rspi->regSPCMD[3] = value;
}

static uint32_t
spcmd4_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPCMD[4];
}

static void
spcmd4_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    rspi->regSPCMD[4] = value;
}

static uint32_t
spcmd5_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPCMD[5];
}

static void
spcmd5_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    rspi->regSPCMD[5] = value;
}

static uint32_t
spcmd6_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPCMD[6];
}

static void
spcmd6_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    rspi->regSPCMD[6] = value;
}

static uint32_t
spcmd7_read(void *clientData, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    return rspi->regSPCMD[7];
}

static void
spcmd7_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRSpi *rspi = clientData;
    rspi->regSPCMD[7] = value;
}

static void
Rspi_Unmap(void *owner, uint32_t base, uint32_t mask)
{
    IOH_Delete8(REG_SPCR(base));
    IOH_Delete8(REG_SSLP(base));
    IOH_Delete8(REG_SPPCR(base));
    IOH_Delete8(REG_SPSR(base));
    IOH_Delete32(REG_SPDR(base));
    IOH_Delete8(REG_SPSCR(base));
    IOH_Delete8(REG_SPSSR(base));
    IOH_Delete8(REG_SPBR(base));
    IOH_Delete8(REG_SPDCR(base));
    IOH_Delete8(REG_SPCKD(base));
    IOH_Delete8(REG_SSLND(base));
    IOH_Delete8(REG_SPND(base));
    IOH_Delete8(REG_SPCR2(base));
    IOH_Delete16(REG_SPCMD0(base));
    IOH_Delete16(REG_SPCMD1(base));
    IOH_Delete16(REG_SPCMD2(base));
    IOH_Delete16(REG_SPCMD3(base));
    IOH_Delete16(REG_SPCMD4(base));
    IOH_Delete16(REG_SPCMD5(base));
    IOH_Delete16(REG_SPCMD6(base));
    IOH_Delete16(REG_SPCMD7(base));
}

static void
Rspi_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
    RxRSpi *rspi = owner;
    IOH_New8(REG_SPCR(base), spcr_read, spcr_write, rspi);
    IOH_New8(REG_SSLP(base),sslp_read,sslp_write,rspi);
    IOH_New8(REG_SPPCR(base),sppcr_read,sppcr_write,rspi);
    IOH_New8(REG_SPSR(base),spsr_read,spsr_write,rspi);
    IOH_New32(REG_SPDR(base),spdr_read,spdr_write,rspi);
    IOH_New8(REG_SPSCR(base),spscr_read,spscr_write,rspi);
    IOH_New8(REG_SPSSR(base),spssr_read,spssr_write,rspi);
    IOH_New8(REG_SPBR(base),spbr_read,spbr_write,rspi);
    IOH_New8(REG_SPDCR(base),spdcr_read,spdcr_write,rspi);
    IOH_New8(REG_SPCKD(base),spckd_read,spckd_write,rspi);
    IOH_New8(REG_SSLND(base),sslnd_read,sslnd_write,rspi);
    IOH_New8(REG_SPND(base),spnd_read,spnd_write,rspi);
    IOH_New8(REG_SPCR2(base),spcr2_read,spcr2_write,rspi);
    IOH_New16(REG_SPCMD0(base),spcmd0_read,spcmd0_write,rspi);
    IOH_New16(REG_SPCMD1(base),spcmd1_read,spcmd1_write,rspi);
    IOH_New16(REG_SPCMD2(base),spcmd2_read,spcmd2_write,rspi);
    IOH_New16(REG_SPCMD3(base),spcmd3_read,spcmd3_write,rspi);
    IOH_New16(REG_SPCMD4(base),spcmd4_read,spcmd4_write,rspi);
    IOH_New16(REG_SPCMD5(base),spcmd5_read,spcmd5_write,rspi);
    IOH_New16(REG_SPCMD6(base),spcmd6_read,spcmd6_write,rspi);
    IOH_New16(REG_SPCMD7(base),spcmd7_read,spcmd7_write,rspi);
}


BusDevice *
RxRSpi_New(const char *name) 
{
    RxRSpi *rspi = sg_new(RxRSpi);
    int i;
    rspi->bdev.first_mapping = NULL;
    rspi->bdev.Map = Rspi_Map;
    rspi->bdev.UnMap = Rspi_Unmap;
    rspi->bdev.owner = rspi;
    rspi->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
    rspi->sigMosi = SigNode_New("%s.mosi", name);
    rspi->sigMiso = SigNode_New("%s.miso", name);
    rspi->sigSclk = SigNode_New("%s.sclk", name);
    rspi->sigShiftOut = SigNode_New("%s.shiftOut", name);
    rspi->sigShiftIn = SigNode_New("%s.shiftIn", name);
    if (!rspi->sigMosi || !rspi->sigMiso || !rspi->sigSclk) {
        fprintf(stderr,"Can not create signals for SPI controller\n");
        exit(1);
    }
    for (i = 0; i < array_size(rspi->sigSSL) ; i++) {
        rspi->sigSSL[i] = SigNode_New("%s.ssl%u", name, i);
        if (!rspi->sigSSL[i]) {
            fprintf(stderr,"Can not create signals for SPI controller\n");
            exit(1);
        }
    }
    rspi->sigIrqSpti = SigNode_New("%s.irqSpti", name);
    rspi->sigIrqSpri = SigNode_New("%s.irqSpri", name);
    rspi->sigIrqSpii = SigNode_New("%s.irqSpii", name);
    rspi->sigIrqSpei = SigNode_New("%s.irqSpei", name);
    if (!rspi->sigIrqSpti || !rspi->sigIrqSpri || !rspi->sigIrqSpii || !rspi->sigIrqSpei) {
        fprintf(stderr,"Can not create Interrupts for SPI controller\n");
        exit(1);
    }
    SigNode_Set(rspi->sigIrqSpti, SIG_HIGH); 
    SigNode_Set(rspi->sigIrqSpri, SIG_HIGH); 
    SigNode_Set(rspi->sigIrqSpii, SIG_HIGH); 
    SigNode_Set(rspi->sigIrqSpei, SIG_HIGH); 
    return &rspi->bdev;
}
