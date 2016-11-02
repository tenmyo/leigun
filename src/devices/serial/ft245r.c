#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "sgstring.h"
#include "signode.h"
#include "ptmx.h"
#include "configfile.h"

#define RXBUF_SIZE	128
#define RXBUF_CNT(ft) 	((ft)->rxbuf_wp - (ft)->rxbuf_rp)
#define RXBUF_ROOM(ft)	(RXBUF_SIZE - RXBUF_CNT(ft))
#define RXBUF_WP(ft)	((ft)->rxbuf_wp % RXBUF_SIZE)
#define RXBUF_RP(ft)	((ft)->rxbuf_rp % RXBUF_SIZE)

#define TXBUF_SIZE	256
#define TXBUF_CNT(ft) 	((ft)->txbuf_wp - (ft)->txbuf_rp)
#define TXBUF_ROOM(ft)	(TXBUF_SIZE - TXBUF_CNT(ft))
#define TXBUF_WP(ft)	((ft)->txbuf_wp % TXBUF_SIZE)
#define TXBUF_RP(ft)	((ft)->txbuf_rp % TXBUF_SIZE)

typedef struct FT245R {
	PtmxIface *ptmxIf;
	char *ptmxname;
	uint8_t rx_buf[RXBUF_SIZE];
	uint16_t rxbuf_wp;
	uint16_t rxbuf_rp;
	uint8_t tx_buf[TXBUF_SIZE];
	uint16_t txbuf_wp;
	uint16_t txbuf_rp;
	SigNode *sigData[8];
	SigNode *sigWR;
	SigNode *signRD;
	SigNode *sigTXE;
	SigNode *sigRXF;
	SigTrace *tracenRD;
	SigTrace *traceWR;
} FT245R;

static void
FT245R_Input(void *evData, int mask)
{
	FT245R *ft = evData;
	uint32_t room;
	int result;
	int cnt = 0;
	//fprintf(stderr,"FT-Input\n");
	while ((room = RXBUF_ROOM(ft))) {
		result = Ptmx_Read(ft->ptmxIf, &ft->rx_buf[RXBUF_WP(ft)], 1);
		if (result > 0) {
			cnt += cnt;
			ft->rxbuf_wp += result;
			SigNode_Set(ft->sigRXF, SIG_LOW);
			fprintf(stderr, "RXF low\n");
		} else {
			return;
		}
	};
	Ptmx_SetInputEnable(ft->ptmxIf, 0);
	return;
}

static void
FT245R_Output(void *evData, int mask)
{
	FT245R *ft = evData;
	unsigned int cnt = TXBUF_CNT(ft);
	unsigned int max;
	unsigned int written;
	max = TXBUF_SIZE - TXBUF_RP(ft);
	if (max > cnt) {
		cnt = max;
	}
	//fprintf(stderr,"FT-Output\n");
	written = Ptmx_Write(ft->ptmxIf, ft->tx_buf + TXBUF_RP(ft), cnt);
	ft->txbuf_wp += written;
	if (written > 0) {
		SigNode_Set(ft->sigTXE, SIG_LOW);
	}
	if (TXBUF_CNT(ft) == 0) {
		Ptmx_SetOutputEnable(ft->ptmxIf, 0);
	}
	return;
}

static void
TracenRD(SigNode * signRD, int value, void *clientData)
{
	FT245R *ft = (FT245R *) clientData;
	uint8_t data;
	int i;
	if (value == SIG_LOW) {
		data = ft->rx_buf[RXBUF_RP(ft)];
		if (RXBUF_CNT(ft) != 0) {
			ft->rxbuf_rp++;
		}
		Ptmx_SetInputEnable(ft->ptmxIf, 1);
		for (i = 0; i < 8; i++) {
			if ((data >> i) & 1) {
				SigNode_Set(ft->sigData[i], SIG_HIGH);
			} else {
				SigNode_Set(ft->sigData[i], SIG_LOW);
			}
		}
	} else {
		for (i = 0; i < 8; i++) {
			SigNode_Set(ft->sigData[i], SIG_OPEN);
		}
		if (RXBUF_CNT(ft) == 0) {
			SigNode_Set(ft->sigRXF, SIG_HIGH);
		}
	}
}

static void
TraceWR(SigNode * sigWR, int value, void *clientData)
{
	FT245R *ft = (FT245R *) clientData;
	uint8_t data = 0;
	int i;
	if (value == SIG_LOW) {
		for (i = 0; i < 8; i++) {
			if (SigNode_Val(ft->sigData[i]) == SIG_HIGH) {
				data |= (1 << i);
			}
		}
		ft->tx_buf[TXBUF_WP(ft)] = data;
		ft->txbuf_wp++;
		if (TXBUF_ROOM(ft) == 0) {
			SigNode_Set(ft->sigTXE, SIG_HIGH);
		}
		Ptmx_SetOutputEnable(ft->ptmxIf, 1);
	}
}

void
FT245R_New(const char *name)
{
	int i;
	FT245R *ft = sg_new(FT245R);
	for (i = 0; i < 8; i++) {
		ft->sigData[i] = SigNode_New("%s.D%d", name, i);
		if (!ft->sigData[i]) {
			fprintf(stderr, "Can not create Data signals for %s\n", name);
			exit(1);
		}
	}
	ft->sigWR = SigNode_New("%s.WR", name);
	ft->signRD = SigNode_New("%s.nRD", name);
	ft->sigTXE = SigNode_New("%s.TXE", name);
	ft->sigRXF = SigNode_New("%s.RXF", name);
	if (!ft->sigWR || !ft->signRD || !ft->sigTXE || !ft->sigRXF) {
		fprintf(stderr, "Can not create Control signals for %s\n", name);
		exit(1);
	}
	SigNode_Set(ft->sigWR, SIG_HIGH);
	SigNode_Set(ft->signRD, SIG_OPEN);
	SigNode_Set(ft->sigRXF, SIG_HIGH);
	SigNode_Set(ft->sigTXE, SIG_LOW);
	ft->ptmxname = Config_ReadVar(name, "link");
	if (!ft->ptmxname) {
		ft->ptmxname = alloca(strlen(name) + 20);
		sprintf(ft->ptmxname, "/tmp/pty_%s", name);
	}
	ft->ptmxIf = PtmxIface_New(ft->ptmxname);
	Ptmx_SetDataSink(ft->ptmxIf, FT245R_Input, ft);
	Ptmx_SetDataSource(ft->ptmxIf, FT245R_Output, ft);
	ft->tracenRD = SigNode_Trace(ft->signRD, TracenRD, ft);
	ft->traceWR = SigNode_Trace(ft->sigWR, TraceWR, ft);
	Ptmx_SetInputEnable(ft->ptmxIf, 1);
}
