/*
 * ------------------------------------------------------
 * Used documentation SMSC FDC37C665GT
 * ------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signode.h>
#include <bus.h>
#include <sgstring.h>

#define PP_DATA(base)    	((base) + 0)
#define PP_STATUS(base)  	((base) + 1)
#define		STATUS_TMOUT	(1<<0)
#define		STATUS_nERR	(1<<3)
#define		STATUS_SLCT	(1<<4)
#define		STATUS_PE	(1<<5)
#define		STATUS_nACK	(1<<6)
#define		STATUS_nBUSY	(1<<7)
#define PP_CONTROL(base) 	((base) + 2)
#define		CONTROL_STROBE	(1<<0)
#define		CONTROL_AUTOFD	(1<<1)
#define		CONTROL_nINIT	(1<<2)
#define		CONTROL_SLC	(1<<3)
#define		CONTROL_IRQE	(1<<4)
#define		CONTROL_PCD	(1<<5)
#define PP_EPP_ADDR(base) 	((base) + 3)
#define PP_EPP_DATA0(base)	((base) + 4)
#define PP_EPP_DATA1(base)	((base) + 5)
#define PP_EPP_DATA2(base)	((base) + 6)
#define PP_EPP_DATA3(base)	((base) + 7)

typedef struct Parport {
	BusDevice bdev;
	SigNode *dataport[8];
	SigNode *n_err;
	SigNode *slct;
	SigNode *pe;
	SigNode *n_ack;
	SigNode *n_busy;

	SigNode *n_strobe;
	SigNode *autofd;
	SigNode *n_init;
	SigNode *slctin;
} Parport;

static uint32_t
data_read(void *clientData, uint32_t address, int rqlen)
{
	//uint8_t data = 0;
	//int i;
	return 0;
}

static void
data_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
status_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
status_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
control_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
control_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
epp_addr_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
epp_addr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
epp_data_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
epp_data_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static void
Parport_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	Parport *pp = (Parport *) owner;
	IOH_New32(PP_DATA(base), data_read, data_write, pp);
	IOH_New32(PP_STATUS(base), status_read, status_write, pp);
	IOH_New32(PP_CONTROL(base), control_read, control_write, pp);
	IOH_New32(PP_EPP_ADDR(base), epp_addr_read, epp_addr_write, pp);
	IOH_New32(PP_EPP_DATA0(base), epp_data_read, epp_data_write, pp);
	IOH_New32(PP_EPP_DATA1(base), epp_data_read, epp_data_write, pp);
	IOH_New32(PP_EPP_DATA2(base), epp_data_read, epp_data_write, pp);
	IOH_New32(PP_EPP_DATA3(base), epp_data_read, epp_data_write, pp);
}

static void
Parport_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(PP_DATA(base));
	IOH_Delete32(PP_STATUS(base));
	IOH_Delete32(PP_CONTROL(base));
	IOH_Delete32(PP_EPP_ADDR(base));
	IOH_Delete32(PP_EPP_DATA0(base));
	IOH_Delete32(PP_EPP_DATA1(base));
	IOH_Delete32(PP_EPP_DATA2(base));
	IOH_Delete32(PP_EPP_DATA3(base));
}

BusDevice *
ParportPC_New(const char *name)
{
	Parport *pp = sg_new(Parport);
	int i;
	for (i = 0; i < 8; i++) {
		pp->dataport[i] = SigNode_New("%s.d%d", name, i);
	}
	pp->n_err = SigNode_New("%s.n_err", name);
	pp->slct = SigNode_New("%s.slct", name);
	pp->pe = SigNode_New("%s.pe", name);
	pp->n_ack = SigNode_New("%s.n_ack", name);
	pp->n_busy = SigNode_New("%s.n_busy", name);

	pp->n_strobe = SigNode_New("%s.n_strobe", name);
	pp->autofd = SigNode_New("%s.autofd", name);
	pp->n_init = SigNode_New("%s.n_init", name);
	pp->slctin = SigNode_New("%s.slctin", name);

	pp->bdev.first_mapping = NULL;
	pp->bdev.Map = Parport_Map;
	pp->bdev.UnMap = Parport_UnMap;
	pp->bdev.owner = pp;
	pp->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &pp->bdev;
}
