/*
 **********************************************************************************
 * diskimage.h
 *	Interface to diskimage library	
 *
 * (C) 2005 Jochen Karrer
 *    Author: Jochen Karrer
 *
 * Status: Working
 **********************************************************************************
 */

#ifndef DISKIMAGE_H
#define DISKIMAGE_H
#include "compiler_extensions.h"
#include <stdint.h>

#define DI_RDONLY	(0)
#define DI_RDWR		(16)
#define DI_CREAT_FF	(1)
#define	DI_CREAT_00	(2)
#define DI_SPARSE	(4)
typedef struct DiskImage DiskImage;

void DiskImage_Close(DiskImage * di);
DiskImage *DiskImage_Open(const char *name, uint64_t size, int flags);
void *DiskImage_Mmap(DiskImage * di);

int DiskImage_Read(DiskImage * di, off_t ofs, uint8_t * buf, int count);
int DiskImage_Write(DiskImage * di, off_t ofs, const uint8_t * buf, int count);

#endif
