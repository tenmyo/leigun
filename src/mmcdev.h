#ifndef _MMCDEV_H
#define _MMCDEV_H
#define MMCARD_MAX_RESPLEN 80	/* 16 + CRC == 17, 1 for word alignment because of fifos  */
#include "clock.h"
#include "cycletimer.h"

typedef int MMCDev_DataSink(void *dev, const uint8_t * data, int count);
/* 
 ***************************************************************
 * A MMCDev has a linked list of devices which listen
 * on for packets on the data line
 ***************************************************************
 */
typedef struct MMC_Listener {
	uint8_t buf[2048];
	void *device;
	int maxpkt;
	 MMCDev_DataSink(*dataSink);
	//int (*dataSink)(void *dev,uint8_t *buf,int count);
	struct MMC_Listener *next;
} MMC_Listener;

typedef struct MMCResponse {
	unsigned int len;
	uint8_t data[MMCARD_MAX_RESPLEN];
} MMCResponse;

typedef struct MMCDev MMCDev;
struct MMCDev {
	int (*doCmd) (MMCDev * card, uint32_t cmd, uint32_t arg, MMCResponse * resp);
	int (*write) (MMCDev * card, const uint8_t * buf, int count);
	int (*read) (MMCDev * card, uint8_t * buf, int count);
	void (*del) (MMCDev * card);
	void (*gotoSpi) (MMCDev * card);
	MMC_Listener *listener_head;
	Clock_t *clock;
	CycleTimer transmissionTimer;
};

/*
 *************************************************************************
 * MMCDev Init is called by the implementor of the MMCDev interface
 * before it fills the structure.
 *************************************************************************
 */
void MMCDev_Init(MMCDev *, const char *name);

void MMCDev_AddListener(MMCDev *, void *dev, int maxpkt, MMCDev_DataSink *);
void MMCDev_RemoveListener(MMCDev *, void *dev);

static inline int
MMCDev_DoCmd(MMCDev * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	return card->doCmd(card, cmd, arg, resp);
}

static inline int
MMCDev_Write(MMCDev * card, const uint8_t * data, int count)
{
	return card->write(card, data, count);
}

static inline int
MMCDev_Read(MMCDev * card, uint8_t * buf, int count)
{
	return card->read(card, buf, count);
}

static inline void
MMCDev_Delete(MMCDev * card)
{
	card->del(card);
}

static inline void
MMCDev_GotoSpi(MMCDev * card)
{
	card->gotoSpi(card);
}

#endif
