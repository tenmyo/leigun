#ifndef NS9750_BBDMA_H
#define NS9750_BBDMA_H
#include <stdint.h>

typedef struct BBusDMACtrl BBusDMACtrl;
typedef struct BBusDMA_Channel BBusDMA_Channel;

typedef int BBDMA_FbrProc(BBusDMA_Channel * chan, uint8_t * buf, int len, void *clientData);

#define BBDMA_CHAN_1  (0)
#define BBDMA_CHAN_2  (1)
#define BBDMA_CHAN_3  (2)
#define BBDMA_CHAN_4  (3)
#define BBDMA_CHAN_5  (4)
#define BBDMA_CHAN_6  (5)
#define BBDMA_CHAN_7  (6)
#define BBDMA_CHAN_8  (7)
#define BBDMA_CHAN_9  (8)
#define BBDMA_CHAN_10 (9)
#define BBDMA_CHAN_11 (10)
#define BBDMA_CHAN_12 (11)
#define BBDMA_CHAN_13 (12)
#define BBDMA_CHAN_14 (13)
#define BBDMA_CHAN_15 (14)
#define BBDMA_CHAN_16 (15)

typedef struct BBDMA_SlaveOps {
	void (*io_event) (BBusDMA_Channel * chan);
} BBDMA_SlaveOps;

BBusDMACtrl *NS9750_BBusDMA_New(char *name);

int BBDMA_Fbw(BBusDMA_Channel * chan, uint8_t * buf, int count);
//int BBDMA_Read(BBusDMA_Channel *chan,uint8_t *buf,int maxlen);
void BBDMA_CloseBuf(BBusDMA_Channel * chan);

BBusDMA_Channel *BBDMA_Connect(BBusDMACtrl * bbdma, unsigned int channel);
void BBDMA_SetDataSink(BBusDMA_Channel * chan, BBDMA_FbrProc * proc, void *clientData);
#endif
