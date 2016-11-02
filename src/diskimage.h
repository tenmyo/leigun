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

#define _FILE_OFFSET_BITS 64

#ifndef O_LARGEFILE
/* O_LARGEFILE is not defined or needed on FreeBSD,
 * which is why it's defined here so that does nothing */
#define O_LARGEFILE 0
#endif



#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>

#define DI_RDONLY	(0)
#define DI_RDWR		(16)
#define DI_CREAT_FF	(1)
#define	DI_CREAT_00	(2)
#define DI_SPARSE	(4)
typedef struct DiskImage {
	int fd;
	int flags;
	uint64_t size;
	void *map;
} DiskImage;

typedef struct DiskOps {
	void (*close)(DiskImage *di);		
	void *(*mmap)(DiskImage *di);
	int  (*read)(DiskImage *di,off_t ofs,uint8_t *buf,int count);	
	int  (*write)(DiskImage *di,off_t ofs,const uint8_t *buf,int count);	
} DiskOps;

void DiskImage_Close(DiskImage *di);
DiskImage * DiskImage_Open(const char *name,uint64_t size,int flags);
void * DiskImage_Mmap(DiskImage *di); 

int DiskImage_Read(DiskImage *di,off_t ofs,uint8_t *buf,int count);
int DiskImage_Write(DiskImage *di,off_t ofs,const uint8_t *buf,int count);

#endif
