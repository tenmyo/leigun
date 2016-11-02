/*
 ****************************************************************************************
 * R8C23 IO Ports
 ****************************************************************************************
 */

#include "bus.h"
#include "pio_r8c23.h"
#include "signode.h"
#include "sgstring.h"

//#include "cpu_m32c.h"

#define REG_P0	(0xe0)
#define REG_P1	(0xe1)
#define REG_P2	(0xe4)
#define REG_P3	(0xe5)
#define REG_P4	(0xe8)
#define REG_P6	(0xec)

#define REG_PD0		(0xe2)
#define REG_PD1		(0xe3)
#define REG_PD2		(0xe6)
#define REG_PD3		(0xe7)
#define REG_PD4		(0xea)
#define REG_PD6		(0xee)

#define REG_PUR0	(0xfc)
#define REG_PUR1	(0xfd)
#define REG_PMR		(0xf8)

typedef struct R8C_Pio R8C_Pio;

typedef struct Port {
	R8C_Pio *pio;
	int port_nr;
	SigNode *ioPin;
	SigTrace *ioPinTrace;
	SigNode *puEn;
	SigTrace *puEnTrace;
	SigNode *puSel;
	SigTrace *puSelTrace;
	SigNode *portLatch;
	SigTrace *portLatchTrace;
	SigNode *portDir;
	SigTrace *portDirTrace;
	SigNode *pcr;
	SigTrace *pcrTrace;
} Port;

struct R8C_Pio {
	BusDevice bdev;
	char *name;
	int addrP[8];
	int addrPD[8];
	uint8_t regP[8];
	uint8_t regPD[8];
	Port *port[64];
};

static void
update_puen(Port * port)
{
	int pusel = SigNode_Val(port->puSel);
	int pdir = SigNode_Val(port->portDir);
	if ((pusel == SIG_HIGH) && (pdir == SIG_LOW)) {
		SigNode_Set(port->puEn, SIG_LOW);
	} else {
		SigNode_Set(port->puEn, SIG_HIGH);
	}
}

static void
update_iopin(Port * port)
{
	int pdir = SigNode_Val(port->portDir);
	int platch = SigNode_Val(port->portLatch);
	int puen = SigNode_Val(port->puEn);
	if (pdir == SIG_HIGH) {
		if (platch == SIG_HIGH) {
			if (port->port_nr == (3 * 8 + 5)) {
				//fprintf(stderr,"High iopin %d,%d,%d\n",pdir,platch,puen);
			}
			SigNode_Set(port->ioPin, SIG_HIGH);
		} else {
			SigNode_Set(port->ioPin, SIG_LOW);
			if (port->port_nr == (3 * 8 + 5)) {
				//fprintf(stderr,"Low iopin %d,%d,%d, val %d\n",pdir,platch,puen,SigNode_Val(port->ioPin));
			}
		}
	} else {
		if (puen == SIG_HIGH) {
			//fprintf(stderr,"Set pullup %s\n",SigName(port->ioPin));
			SigNode_Set(port->ioPin, SIG_PULLUP);
			if (port->port_nr == (3 * 8 + 5)) {
				//fprintf(stderr,"Pullup iopin %d,%d,%d, val %d\n",pdir,platch,puen,SigNode_Val(port->ioPin));
			}
		} else {
			if (port->port_nr == (3 * 8 + 5)) {
				//fprintf(stderr,"Open iopin %d,%d,%d\n",pdir,platch,puen);
			}
			SigNode_Set(port->ioPin, SIG_OPEN);
		}
	}
}

static void
iopin_trace(SigNode * node, int value, void *clientData)
{
//      Port *port = clientData;
//      update_puen(port);
}

static void
puen_trace(SigNode * node, int value, void *clientData)
{
	Port *port = clientData;
	update_iopin(port);
}

static void
pusel_trace(SigNode * node, int value, void *clientData)
{
	Port *port = clientData;
	update_puen(port);
}

static void
port_latch_trace(SigNode * node, int value, void *clientData)
{
	Port *port = clientData;
	update_iopin(port);
}

static void
port_dir_trace(SigNode * node, int value, void *clientData)
{
	Port *port = clientData;
	update_puen(port);
	update_iopin(port);
}

static void
pcr_trace(SigNode * node, int value, void *clientData)
{
#if 0
	Port *port = clientData;
	update_puen(port);
#endif
}

#if 0
static uint32_t
debug_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "0x%04x At %06x\n", address, M32C_REG_PC);
	return 0;
}

static void
debug_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "0x%04x At %06x\n", address, M32C_REG_PC);
}
#endif

static uint32_t
port_read(void *clientData, uint32_t address, int rqlen)
{
	Port *firstport = clientData;
	R8C_Pio *pio = firstport->pio;
	int port_base = firstport->port_nr;
	int i;
	uint8_t value;
	int idx = port_base >> 3;
	value = 0;
	for (i = 0; i < 8; i++) {
		Port *port = pio->port[port_base + i];
		if (pio->regPD[idx] & (1 << i)) {
			if (pio->regP[idx] & (1 << i)) {
				value |= (1 << i);
			}
		} else {
			if (SigNode_Val(port->ioPin) == SIG_HIGH) {
				value |= (1 << i);
			}
		}
	}
#if 0
	if ((idx == 6) || (idx == 7)) {
		fprintf(stderr, "Read Port%d: %02x\n", idx, value);
	}
#endif
	//value = value & port->regPD[i]
	return value;
}

static void
port_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Port *firstport = clientData;
	R8C_Pio *pio = firstport->pio;
	int port_base = firstport->port_nr;
	int idx = port_base >> 3;
	int i;
	pio->regP[idx] = value;
	for (i = 0; i < 8; i++) {
		Port *port = pio->port[i + port_base];
		if (value & (1 << i)) {
			SigNode_Set(port->portLatch, SIG_HIGH);
		} else {
			SigNode_Set(port->portLatch, SIG_LOW);
		}
	}
}

static uint32_t
port_dir_read(void *clientData, uint32_t address, int rqlen)
{
	Port *firstport = clientData;
	R8C_Pio *pio = firstport->pio;
	int port_base = firstport->port_nr;
	int idx = port_base >> 3;
	return pio->regPD[idx];

}

static void
port_dir_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Port *firstport = clientData;
	R8C_Pio *pio = firstport->pio;
	int port_base = firstport->port_nr;
	int idx = port_base >> 3;
	int i;
	pio->regPD[idx] = value;
	for (i = 0; i < 8; i++) {
		Port *port = pio->port[i + port_base];
		if (value & (1 << i)) {
			SigNode_Set(port->portDir, SIG_HIGH);
		} else {
			SigNode_Set(port->portDir, SIG_LOW);
		}
	}
}

static void
R8C23Pio_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	R8C_Pio *pio = (R8C_Pio *) owner;
	for (i = 0; i < 8; i++) {
		if (pio->addrP[i] >= 0) {
			IOH_Delete8(pio->addrP[i]);
		}
		if (pio->addrPD[i] >= 0) {
			IOH_Delete8(pio->addrPD[i]);
		}
	}
}

static void
R8C23Pio_Map(void *owner, uint32_t _base, uint32_t mask, uint32_t mapflags)
{
	int i;
	R8C_Pio *pio = (R8C_Pio *) owner;
	for (i = 0; i < 8; i++) {
		Port *port = pio->port[i << 3];
		if (pio->addrP[i] >= 0) {
			IOH_New8(pio->addrP[i], port_read, port_write, port);
		}
		if (pio->addrPD[i] >= 0) {
			IOH_New8(pio->addrPD[i], port_dir_read, port_dir_write, port);
		}
	}
}

static void
R8C23_PortsInit(R8C_Pio * pio)
{
	int i, j;
	for (i = 0; i < array_size(pio->port); i++) {
		Port *port;
		port = pio->port[i] = sg_new(Port);
		j = i & 7;
		port->port_nr = i;
		port->pio = pio;
		port->ioPin = SigNode_New("%s.P%d.%d", pio->name, i >> 3, j);
		port->puEn = SigNode_New("%s.PuEn%d.%d", pio->name, i >> 3, j);
		port->puSel = SigNode_New("%s.PuSel%d.%d", pio->name, i >> 3, j);
		port->portLatch = SigNode_New("%s.PortLatch%d.%d", pio->name, i >> 3, j);
		port->portDir = SigNode_New("%s.PortDir%d.%d", pio->name, i >> 3, j);
		port->pcr = SigNode_New("%s.Pcr%d.%d", pio->name, i >> 3, j);
		if (!port->ioPin || !port->puEn || !port->puSel ||
		    !port->portLatch || !port->portDir || !port->pcr) {
			fprintf(stderr, "Can not create signal line\n");
			exit(1);
		}
		port->ioPinTrace = SigNode_Trace(port->ioPin, iopin_trace, port);
		port->puEnTrace = SigNode_Trace(port->puEn, puen_trace, port);
		port->puSelTrace = SigNode_Trace(port->puSel, pusel_trace, port);
		port->portLatchTrace = SigNode_Trace(port->portLatch, port_latch_trace, port);
		port->portDirTrace = SigNode_Trace(port->portDir, port_dir_trace, port);
		port->pcrTrace = SigNode_Trace(port->pcr, pcr_trace, port);

	}
	pio->addrP[0] = REG_P0;
	pio->addrP[1] = REG_P1;
	pio->addrP[2] = REG_P2;
	pio->addrP[3] = REG_P3;
	pio->addrP[4] = REG_P4;
	pio->addrP[5] = -1;
	pio->addrP[6] = REG_P6;
	pio->addrP[7] = -1;

	pio->addrPD[0] = REG_PD0;
	pio->addrPD[1] = REG_PD1;
	pio->addrPD[2] = REG_PD2;
	pio->addrPD[3] = REG_PD3;
	pio->addrPD[4] = REG_PD4;
	pio->addrPD[5] = -1;
	pio->addrPD[6] = REG_PD6;
	pio->addrPD[7] = -1;
}

BusDevice *
R8C23_PioNew(const char *name)
{
	R8C_Pio *pio = sg_new(R8C_Pio);
	pio->bdev.first_mapping = NULL;
	pio->bdev.Map = R8C23Pio_Map;
	pio->bdev.UnMap = R8C23Pio_Unmap;
	pio->bdev.owner = pio;
	pio->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	pio->name = sg_strdup(name);
	R8C23_PortsInit(pio);
	return &pio->bdev;
}
