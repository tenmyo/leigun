/*
 **********************************************************************************
 * Interface to MMC/SD-Card emulator
 **********************************************************************************
 */

#ifndef _MMCARD_H
#include "mmcdev.h"

//typedef struct MMCard MMCard;

/* Return values for MMCard_DoCmd */
#define MMC_ERR_NONE    0
#define MMC_ERR_TIMEOUT 1

//int MMCard_DoCmd(MMCDev *dev,uint32_t cmd,uint32_t arg,MMCResponse *resp);
//void MMCard_Delete(MMCDev *dev);
MMCDev *MMCard_New(const char *name);

typedef int MMCard_DataSink(void *dev, const uint8_t * data, int count);
void MMCard_AddListener(MMCDev *, void *dev, int maxpkt, MMCard_DataSink *);
void MMCard_RemoveListener(MMCDev *, void *dev);

#define _MMCARD_H
#endif
