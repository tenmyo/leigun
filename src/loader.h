/*
 **********************************************************************************
 *
 * Load Binary or srecord  files to Memory
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 **********************************************************************************
 */

#include <stdint.h>
int64_t Load_AutoType(char *filename,uint32_t addr,uint64_t region_size);
#define LOADER_FLAG_SWAP32 (2)
typedef int LoadProc(void *clientData,uint32_t addr,uint8_t *buf,unsigned int count,int flags);
int  Loader_RegisterBus(const char *name,LoadProc *,void *clientData);

