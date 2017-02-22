/*
 *************************************************************************************************
 * Emulation of MMC/SD-Cards 
 *
 * state: working with u-boot and linux with 
 *        i.MX21 SD-Card Host Controller emulator.
 * 	Secure commands are missing because they are secret
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */
#if 0
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "signode.h"
#include "configfile.h"
#include "cycletimer.h"
#include "mmcard.h"
#include "mmcproto.h"
#include "diskimage.h"
#include "clock.h"
#include "mmc_crc.h"
#include "sgstring.h"
#include "mmcdev.h"
#include "sglib.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define CARD_TYPE_MMC           (2)
#define CARD_TYPE_MMCPLUS       (3)
#define CARD_TYPE_SD            (4)

#define STATE_IDLE 		(0)
#define STATE_READY		(1)
#define STATE_IDENT		(2)
#define STATE_STBY		(3)
#define	STATE_TRANSFER		(4)
#define STATE_DATA		(5)
#define STATE_RCV		(6)
#define STATE_PRG		(7)
#define STATE_DIS	 	(8)
#define STATE_INACTIVE  	(10)
#define GET_STATUS(card) ((card)->card_status & ~STATUS_STATE_MASK) | ((card)->state << STATUS_STATE_SHIFT)

#define RST_NOT_STARTED 	(7)
#define RST_STARTED 		(8)
#define RST_DONE 		(9)

#ifndef O_LARGEFILE
/* O_LARGEFILE does not exist and is not neededon FreeBSD, define
 * it so that it does not have any effect*/
#define O_LARGEFILE 0
#endif

#define _FILE_OFFSET_BITS 64

/*
 * ------------------------------------------------------------------------
 * Bit definitions for the upper field of the cmd (Additional information
 * for the data transfer
 * ------------------------------------------------------------------------
 */

#define CMD_FLAG_IS_APPCMD	(1<<8)

typedef struct MMCardSpec {
	char *comment;
	char *producttype;
	char *description;
	int type;
	int usec_reset;
	uint32_t ocr;
	uint8_t cid[16];
	uint8_t csd[16];
	uint8_t scr[8];
	uint8_t ssr[64];	/* ACMD13 */
	// secure: uint8_t idmedia[8];
	uint16_t rca;		/* Initial RCA */
} MMCardSpec;

static MMCardSpec cardspec[] = {
	{
	 /* Panasonic card comming with Minolta camera, borrowed from asklein */
	 .producttype = "Panasonic16M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .cid = {
		 0x01, 0x50, 0x41, 0x53, 0x30, 0x31, 0x36, 0x42,
		 0x41, 0x44, 0xd1, 0xb0, 0xd7, 0x00, 0x44, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00,
		 },
	 .csd = {
		 0x00, 0x5d, 0x01, 0x32, 0x13, 0x59, 0x80, 0xe3,
		 0x76, 0xd9, 0xcf, 0xff, 0x16, 0x40, 0x00, 0x00,
		 },
	 },
	{
	 .producttype = "Sandisk32M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0xc39e,
	 .csd = {
		 0x00, 0x26, 0x00, 0x32, 0x13, 0x59, 0x81, 0xd2,
		 0xf6, 0xd9, 0xc0, 0x1f, 0x12, 0x40, 0x40, 0x00,
		 },
	 .cid = {
		 0x03, 0x53, 0x44, 0x53, 0x44, 0x30, 0x33, 0x32,
		 0x23, 0x00, 0x02, 0xeb, 0x33, 0x00, 0x18, 0x00,
		 },
	 .scr = {
		 0x00, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x17,
		 0x00, 0x17, 0x00, 0x17, 0x00, 0x17, 0x00, 0x17,
		 0x00, 0x17, 0x00, 0x17, 0x00, 0x17, 0x00, 0x17,
		 0x00, 0x17, 0x00, 0x17, 0x00, 0x17, 0x00, 0x17,
		 0x00, 0x17, 0x00, 0x17, 0x00, 0x17, 0x00, 0x17,
		 0x00, 0x17, 0x00, 0x17, 0x00, 0x17, 0x00, 0x17,
		 }
	 },
	{
	 /* Canon 32MB Made in China. Comming with Canon A620 Camera */
	 .producttype = "Canon32M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0x80ca,
	 .cid = {
		 0x03, 0x53, 0x44, 0x53, 0x44, 0x30, 0x33, 0x32,
		 0x58, 0x10, 0x36, 0x02, 0x4b, 0x00, 0x56, 0x00,
		 },
	 .scr = {
		 0x00, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .csd = {
		 0x00, 0x26, 0x00, 0x32, 0x1f, 0x59, 0x81, 0xd2,
		 0xfe, 0xf9, 0xcf, 0xff, 0x92, 0x40, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x17,
		 0x00, 0x17, 0x00, 0x17, 0x00, 0x17, 0x00, 0x17,
		 0x00, 0x17, 0x00, 0x17, 0x00, 0x17, 0x00, 0x17,
		 0x00, 0x17, 0x00, 0x17, 0x00, 0x17, 0x00, 0x17,
		 0x00, 0x17, 0x00, 0x17, 0x00, 0x17, 0x00, 0x17,
		 0x00, 0x17, 0x00, 0x17, 0x00, 0x17, 0x00, 0x17,

		 },
	 },
	{
	 /* This is also the auto_sd template */
	 .rca = 0x0001,
	 .producttype = "Toshiba32M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .cid = {0x02,		/* Toshiba */
		 0x54,		/* 'T'    */
		 0x4d,		/* 'M'    */
		 0x53,		/* 'S'    */
		 0x44,		/* 'D'    */
		 0x30,		/* '0'    */
		 0x33,		/* '3'    */
		 0x32,		/* '2'    */
		 0,		/* Product revision */
		 /* Serial number has to be inserted from configuration file */
		 0, 0, 0, 0,
		 0,
		 0x63,
		 1,		// crc has to be inserted by software 
		 },
	 .scr = {0, 0xa5, 0, 0, 0, 0, 0, 0},
	 .csd = {0,		/*                        Bits 120-127 */
		 0x26,		/* TAAC                   Bits 112-119 */
		 0,		/* NSAC                   Bits 104-111 */
		 0x32,		/* TRAN_SPEED             Bits 96-103 */
		 0x13,		/* CCC High               Bits 88-95 */
		 0x59,		/* CCC low, block len 512         Bits 80-87 */
		 0x81,		/*                                Bits 72-79 */
		 0xd2,		/*                                Bits 64-71 */
		 0xf6,		/*                                Bits 56-63 */
		 0xd9,		/*                                Bits 48-55 */
		 0xc0,		/*                        Bits 40-47 */
		 0x1f,
		 0x12,
		 0x40,
		 0x40,
		 0x0,
		 }
	 },
	{
	 .producttype = "Toshiba64M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .cid = {
		 0x02, 0x54, 0x4d, 0x53, 0x44, 0x30, 0x36, 0x34,
		 0x07, 0x51, 0x17, 0xd4, 0x11, 0x00, 0x49, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x09, 0x02, 0x02, 0x02,
		 },
	 .csd = {
		 0x00, 0x2d, 0x00, 0x32, 0x13, 0x59, 0x83, 0xb1,
		 0xf6, 0xd9, 0xcf, 0x80, 0x16, 0x40, 0x00, 0x00,
		 },
	 },
	{
	 /* This is also the auto_mmc template */
	 .producttype = "ExtremeMemory128M",
	 .type = CARD_TYPE_MMC,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .cid = {
		 0x01, 0x00, 0x00, 0x49, 0x46, 0x58, 0x31, 0x32,
		 0x38, 0x20, 0x19, 0xf2, 0xee, 0x0d, 0x17, 0x00,
		 },
	 .scr = {},
	 .csd = {
		 0x48, 0x0e, 0x01, 0x2a, 0x0f, 0xf9, 0x81, 0xe9,
		 0xf6, 0xda, 0x81, 0xe1, 0x8a, 0x40, 0x00, 0x00,
		 },
	 },
	{
	 .producttype = "UnlabeledSD128M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0x0001,
	 .cid = {
		 0x06, 0x52, 0x4b, 0x53, 0x44, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x07, 0x81, 0x00, 0x58, 0x00,
		 },
	 .scr = {
		 0x01, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .csd = {
		 0x00, 0x6f, 0x00, 0x32, 0x5f, 0x59, 0x80, 0xf2,
		 0xf6, 0xdb, 0x7f, 0x87, 0x96, 0x40, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x0a,
		 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a,
		 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a,
		 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a,
		 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a,
		 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a,
		 }
	 },
	{
	 .producttype = "SiliconPower128M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0x0001,
	 .csd = {
		 0x00, 0x6f, 0x00, 0x32, 0x5b, 0x59, 0x80, 0xf2,
		 0x76, 0xdb, 0x7f, 0x87, 0x16, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x31, 0x53, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x01, 0x00, 0x00, 0x0a, 0xe9, 0x00, 0x5a, 0x00,
		 },
	 .scr = {
		 0x01, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x0a,
		 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a,
		 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a,
		 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a,
		 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a,
		 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x0a,
		 },
	 },
	{
	 /* testscript 25m 30.26 */
	 .producttype = "Kingston128M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x36, 0x00, 0x32, 0x17, 0x59, 0x81, 0xdf,
		 0x76, 0xda, 0xff, 0x81, 0x96, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x18, 0x49, 0x4e, 0x31, 0x32, 0x38, 0x4d, 0x42,
		 0x04, 0x40, 0x9f, 0x5d, 0xe9, 0x00, 0x61, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 },
	{
	 /* testscript 25m 11.37 */
	 .producttype = "Fuji128M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x2e, 0x01, 0x32, 0x5f, 0x59, 0x83, 0xc1,
		 0xf6, 0xda, 0x1f, 0xff, 0x9e, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x06, 0x52, 0x4b, 0x53, 0x44, 0x20, 0x20, 0x20,
		 0x00, 0x65, 0x00, 0x0b, 0x57, 0x00, 0x5a, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 },
	{
	 /* testscript w25M 19.97 s */
	 .producttype = "Ultron256M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x26, 0x00, 0x32, 0x13, 0x59, 0x81, 0xe3,
		 0x36, 0xdb, 0x7f, 0x81, 0x16, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x18, 0x49, 0x4e, 0x32, 0x35, 0x36, 0x4d, 0x42,
		 0x20, 0x40, 0x9b, 0xa1, 0x7d, 0x00, 0x5c, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 },
	{
	 /* w25M testscript 19.20s */
	 .producttype = "Verbatim256M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x2d, 0x00, 0x32, 0x1b, 0x59, 0x83, 0xcc,
		 0xf6, 0xda, 0xff, 0x80, 0x16, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x02, 0x54, 0x4d, 0x53, 0x44, 0x32, 0x35, 0x36,
		 0x15, 0x7e, 0x94, 0xac, 0x12, 0x00, 0x55, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x10, 0x01, 0x16, 0x02,
		 },
	 },
	{
	 /* w25M testscript 18.39 s */
	 .producttype = "Fuji256M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x6f, 0x00, 0x32, 0x5b, 0x59, 0x81, 0xe4,
		 0xf6, 0xdb, 0x7f, 0x87, 0x16, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x06, 0x52, 0x4b, 0x53, 0x44, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x01, 0xa0, 0x89, 0x00, 0x5b, 0x00,
		 },
	 .scr = {
		 0x01, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 },
	{
	 /* testscript 25m: 13.74 s */
	 .producttype = "Kingston256M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x5d, 0x01, 0x32, 0x13, 0x59, 0x83, 0xb9,
		 0xf6, 0xda, 0xff, 0xff, 0x16, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x01, 0x50, 0x41, 0x53, 0x32, 0x35, 0x36, 0x42,
		 0x84, 0x1e, 0x72, 0x26, 0x92, 0x00, 0x61, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x04, 0x84, 0x00, 0x00,
		 },

	 },
	{
	 /* testscript 25m: 7.36 s !! */
	 .producttype = "PNY256M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x5e, 0x00, 0x32, 0x5f, 0x59, 0x83, 0xcf,
		 0xed, 0xb6, 0xff, 0x87, 0x96, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x11, 0x44, 0x99, 0x20, 0x20, 0x20, 0x20, 0x20,
		 0x10, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x62, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },

	 },
	{
	 /* 
	  * -----------------------------------------------------------
	  * Unlabeled White card from Toshiba  
	  * This Card requires at least 3ms pause after ACMD13, or
	  * next command will be ignored
	  * -----------------------------------------------------------
	  */
	 .producttype = "White256M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0xe1fc,
	 .csd = {
		 0x00, 0x2d, 0x00, 0x32, 0x1b, 0x59, 0x83, 0xd8,
		 0xf6, 0xda, 0xff, 0x80, 0x12, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x02, 0x54, 0x4d, 0x53, 0x44, 0x32, 0x35, 0x36,
		 0x16, 0x76, 0x13, 0xcd, 0x13, 0x00, 0x59, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x10, 0x01, 0x16, 0x02,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,

		 },
	 },
	{
	 /* w25M testscript 46.24s */
	 .producttype = "ExtermeMemory512M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x4f, 0x00, 0x32, 0x5f, 0x59, 0x83, 0xca,
		 0xf6, 0xdb, 0x7f, 0x87, 0x8a, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x27, 0x50, 0x48, 0x53, 0x44, 0x35, 0x31, 0x32,
		 0x11, 0x70, 0x10, 0x0a, 0x0d, 0x00, 0x63, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
		 },
	 },
	{
	 /* Toshiba 512M White unlabeled */
	 .producttype = "Unlabeled512M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x2d, 0x00, 0x32, 0x1b, 0x59, 0x83, 0xdb,
		 0x76, 0xdb, 0x7f, 0x80, 0x12, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x02, 0x54, 0x4d, 0x53, 0x44, 0x35, 0x31, 0x32,
		 0x16, 0x8e, 0x61, 0xff, 0x13, 0x00, 0x5b, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x10, 0x01, 0x16, 0x02,
		 },
	 },

	{
	 /* testscript 25M 17.47 sec */
	 .producttype = "PNY512M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x2d, 0x00, 0x32, 0x1b, 0x59, 0x83, 0xd0,
		 0xf6, 0xdb, 0x7f, 0x80, 0x16, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x02, 0x54, 0x4d, 0x53, 0x44, 0x35, 0x31, 0x32,
		 0x76, 0x8a, 0xad, 0x91, 0x6b, 0x00, 0x61, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x10, 0x01, 0x19, 0x02,
		 },

	 },
	{
	 /* w25M testscript 18.4 s */
	 .producttype = "Kingston512M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .cid = {
		 0x02, 0x54, 0x4d, 0x53, 0x44, 0x35, 0x31, 0x32,
		 0x15, 0x88, 0xbf, 0x1b, 0x30, 0x00, 0x5b, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x10, 0x01, 0x16, 0x02,
		 },
	 .csd = {
		 0x00, 0x2d, 0x00, 0x32, 0x1b, 0x59, 0x83, 0xd0,
		 0xf6, 0xdb, 0x7f, 0x80, 0x16, 0x40, 0x00, 0x00,
		 },

	 },
	{
	 /* w25M testscript 15.98 s */
	 .producttype = "Fuji512M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x2f, 0x01, 0x32, 0x5f, 0x59, 0x83, 0xbd,
		 0x76, 0xdb, 0x5f, 0xff, 0x9e, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x06, 0x52, 0x4b, 0x20, 0x20, 0x20, 0x20, 0x20,
		 0x00, 0x65, 0x00, 0x02, 0xa1, 0x00, 0x5b, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 },
	{
	 /* w25M testscript 15.98 s */
	 .producttype = "SiliconPower512M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,

	 .rca = 0x0001,
	 .csd = {
		 0x00, 0x6f, 0x00, 0x32, 0x5b, 0x59, 0x83, 0xcc,
		 0xf6, 0xdb, 0x7f, 0x87, 0x16, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x31, 0x53, 0x50, 0x53, 0x50, 0x53, 0x44, 0x00,
		 0x00, 0x00, 0x00, 0x3c, 0xc8, 0x00, 0x68, 0x00,
		 },
	 .scr = {
		 0x01, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,

		 },
	 },
	{
	 /* w25M testscript 15.98 s */
	 .producttype = "Smart512M",
	 .description = "Industrial Card",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,

	 .rca = 0xb368,
	 .csd = {
		 0x00, 0x5e, 0x00, 0x32, 0x5f, 0x59, 0x83, 0xd2,
		 0xed, 0xb7, 0x7f, 0x8f, 0x96, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x04, 0x50, 0x44, 0x45, 0x31, 0x32, 0x31, 0x33,
		 0x10, 0x01, 0x62, 0xac, 0x1f, 0x00, 0x63, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,

		 },

	 },
	{
	 /* Shake card */
	 .producttype = "ATP-512M",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 30000,
	 .rca = 0x1,
	 .csd = {
		 0x00, 0x5e, 0x00, 0x32, 0x5f, 0x59, 0x83, 0xd2,
		 0xed, 0xb7, 0x7f, 0x8f, 0x96, 0x40, 0x00, 0xf7},
	 .cid = {
		 0x09, 0x41, 0x50, 0x41, 0x46, 0x20, 0x53, 0x44,
		 0x10, 0x26, 0x56, 0x00, 0x89, 0x00, 0x95, 0xad,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 },
	{
	 /* 
	  * ----------------------------------------------
	  * Very buggy card from asklein:
	  * Kingston Elite Pro 1GB 50x 3.3V
	  * Never buy this card !
	  * ----------------------------------------------
	  */
	 .producttype = "Kingston1G.bad",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .cid = {
		 0x27, 0x00, 0x00, 0x53, 0x44, 0x30, 0x31, 0x47,
		 0x11, 0x99, 0x20, 0x27, 0xc0, 0x00, 0x55, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .csd = {
		 0x00, 0x4f, 0x00, 0x32, 0x17, 0x59, 0xa3, 0xca,
		 0xf6, 0xdb, 0xff, 0x8f, 0x8a, 0x40, 0x00, 0x00,
		 },
	 },
	{
	 /* 
	  * ----------------------------------------------
	  * This card was sent by amazon as a substitute
	  * for the Kingston1G.bad with Manuf. ID 0x27
	  * Kingston Elite Pro 1GB 50x 3.3V
	  * This card is working reliable
	  * ----------------------------------------------
	  */
	 .producttype = "Kingston1G",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .cid = {
		 0x1f, 0x53, 0x4b, 0x53, 0x44, 0x31, 0x47, 0x42,
		 0x10, 0x21, 0x98, 0x07, 0xa0, 0x00, 0x68, 0x00,

		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .csd = {
		 0x00, 0x2f, 0x01, 0x32, 0x5f, 0x59, 0x83, 0xce,
		 0x36, 0xdb, 0xdf, 0xff, 0x9e, 0x40, 0x00, 0x00,
		 },
	 },
	{
	 /* 
	  * ----------------------------------------------
	  * another version of Kingston elite pro 50 x
	  * testskript 25M: 75s
	  * ----------------------------------------------
	  */
	 .producttype = "Kingston1G_2",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x0e, 0x01, 0x32, 0x13, 0x59, 0x83, 0xb0,
		 0xff, 0xff, 0xff, 0xff, 0x12, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x01, 0x50, 0x41, 0x53, 0x51, 0x30, 0x31, 0x47,
		 0x63, 0x9a, 0x80, 0x58, 0xaf, 0x00, 0x61, 0x00,
		 },
	 .scr = {
		 0x00, 0xa5, 0x00, 0x00, 0x06, 0x63, 0x00, 0x00,
		 },
	 },
	{
	 /* testscript 25M 20 sec */
	 .producttype = "Corsair1G",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x5e, 0x00, 0x32, 0x5f, 0x59, 0x83, 0xd0,
		 0x6d, 0xb7, 0xbf, 0x9f, 0x96, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x1d, 0x41, 0x44, 0x53, 0x44, 0x20, 0x20, 0x20,
		 0x10, 0x00, 0x01, 0xb6, 0x1e, 0x00, 0x62, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 }
	 },
	{
	 /* White Toshiba card for FS3 */
	 .producttype = "Toshiba1G",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x2d, 0x00, 0x32, 0x5b, 0x59, 0x83, 0xd6,
		 0x7e, 0xfb, 0xff, 0x80, 0x12, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x02, 0x54, 0x4d, 0x53, 0x44, 0x30, 0x31, 0x47,
		 0x25, 0x92, 0xe0, 0x7b, 0x2d, 0x00, 0x66, 0x00,
		 },
	 .scr = {
		 0x01, 0xa5, 0x00, 0x00, 0x16, 0x01, 0x01, 0x02,
		 },
	 },
	{
	 /* My card */
	 .producttype = "TranscendMini1G",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0x0002,
	 .csd = {
		 0x00, 0x2f, 0x01, 0x32, 0x5f, 0x59, 0x83, 0xbd,
		 0xf6, 0xdb, 0xdf, 0xff, 0x9e, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x1b, 0x53, 0x4d, 0x53, 0x44, 0x4d, 0x20, 0x20,
		 0x00, 0x00, 0x00, 0x85, 0xac, 0x00, 0x69, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x34,
		 0x00, 0x34, 0x00, 0x34, 0x00, 0x34, 0x00, 0x34,
		 0x00, 0x34, 0x00, 0x34, 0x00, 0x34, 0x00, 0x34,
		 0x00, 0x34, 0x00, 0x34, 0x00, 0x34, 0x00, 0x34,
		 0x00, 0x34, 0x00, 0x34, 0x00, 0x34, 0x00, 0x34,
		 0x00, 0x34, 0x00, 0x34, 0x00, 0x34, 0x00, 0x34,

		 },
	 },
	{
	 .producttype = "SandiskMicro1G",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0xa785,
	 .csd = {
		 0x00, 0x26, 0x00, 0x32, 0x5f, 0x59, 0x83, 0xc8,
		 0xbe, 0xfb, 0xcf, 0xff, 0x92, 0x40, 0x40, 0x00,
		 },
	 .cid = {
		 0x03, 0x53, 0x44, 0x53, 0x55, 0x30, 0x31, 0x47,
		 0x80, 0x00, 0x13, 0x8b, 0x03, 0x00, 0x6c, 0x00,
		 },
	 .scr = {
		 0x02, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28,
		 0x01, 0x01, 0x90, 0x00, 0x0a, 0x05, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,

		 },
	 },
	{
	 .producttype = "SiliconPower1G",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0x0002,
	 .csd = {
		 0x00, 0x2f, 0x01, 0x32, 0x5f, 0x59, 0x83, 0xb9,
		 0xf6, 0xdb, 0xdf, 0xff, 0x9e, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x31, 0x53, 0x50, 0x20, 0x20, 0x20, 0x20, 0x20,
		 0x00, 0x00, 0x68, 0x00, 0x0b, 0x00, 0x6a, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0x50,
		 0x00, 0x50, 0x00, 0x50, 0x00, 0x50, 0x00, 0x50,
		 0x00, 0x50, 0x00, 0x50, 0x00, 0x50, 0x00, 0x50,
		 0x00, 0x50, 0x00, 0x50, 0x00, 0x50, 0x00, 0x50,
		 0x00, 0x50, 0x00, 0x50, 0x00, 0x50, 0x00, 0x50,
		 0x00, 0x50, 0x00, 0x50, 0x00, 0x50, 0x00, 0x50,

		 },
	 },
	{
	 /* My card */
	 .producttype = "X2G",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0xb368,
	 .csd = {
		 0x00, 0x5e, 0x00, 0x32, 0x5f, 0x5a, 0x83, 0xbd,
		 0x2d, 0xb7, 0xff, 0xbf, 0x96, 0x80, 0x00, 0x00,
		 },
	 .cid = {
		 0x1b, 0x53, 0x4d, 0x53, 0x44, 0x20, 0x20, 0x20,
		 0x10, 0xca, 0x20, 0x74, 0x74, 0x00, 0x72, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,

		 },
	 },
	{
	 /* My card */
	 .producttype = "Kingston_uSD_2G",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 10000,
	 .rca = 0xb368,
	 .csd = {
		 0x00, 0x2e, 0x00, 0x32, 0x5b, 0x5a, 0x83, 0xa9,
		 0xff, 0xff, 0xff, 0x80, 0x16, 0x80, 0x00, 0x91},
	 .cid = {
		 0x02, 0x54, 0x4d, 0x53, 0x44, 0x30, 0x32, 0x47,
		 0x38, 0xa1, 0xce, 0x7b, 0x4c, 0x00, 0x9b, 0xf3,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 },
	{
	 /* MSW private card from Reichelt Vendor "PR" */
	 .producttype = "Platinum4G_1",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x2f, 0x01, 0x32, 0x5f, 0x5b, 0x83, 0xb0,
		 0xf6, 0xdb, 0x9f, 0xff, 0x9e, 0xc0, 0x00, 0x00,
		 },
	 .cid = {
		 0x30, 0x50, 0x52, 0x20, 0x20, 0x20, 0x20, 0x20,
		 0x00, 0x00, 0x20, 0x00, 0x66, 0x00, 0x71, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 }
	 },
	{
	 /* MSW private card from Reichelt Vendor "DY" */
	 .producttype = "Platinum4G_2",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x00, 0x2e, 0x01, 0x32, 0x5f, 0x5b, 0x83, 0xbb,
		 0x36, 0xdb, 0xdf, 0xff, 0x9e, 0xc0, 0x00, 0x00,
		 },
	 .cid = {
		 0x19, 0x44, 0x59, 0x53, 0x44, 0x20, 0x20, 0x20,
		 0x00, 0x00, 0x00, 0x18, 0x52, 0x00, 0x64, 0x00,
		 },
	 .scr = {
		 0xd2, 0xc2, 0x3f, 0x09, 0x1b, 0xf0, 0x17, 0x21,
		 }
	 },
	{
	 /* 
	  * ----------------------------------------------------------
	  * This card has a unknown algorithm for changing the rca
	  * when calling  SD_SEND_RELATIVE_ADDR
	  * 0xb368 -> 0x66d0 ..
	  * ----------------------------------------------------------
	  */
	 .producttype = "Transcend4GB",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0xb368,
	 .csd = {
		 0x00, 0x5e, 0x00, 0x32, 0x5f, 0x5b, 0x83, 0xd5,
		 0x6d, 0xb7, 0xff, 0xff, 0x96, 0xc0, 0x00, 0x00,
		 },
	 .cid = {
		 0x1c, 0x53, 0x56, 0x53, 0x44, 0x43, 0x20, 0x20,
		 0x10, 0x00, 0x02, 0xb6, 0xb3, 0x00, 0x72, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,
		 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28,

		 },
	 },
	{
	 /*
	  * My card from Reichelt
	  */
	 .producttype = "CnMemory4GB",
	 .type = CARD_TYPE_SD,
	 .ocr = 0x80ff8000,
	 .usec_reset = 5000,
	 .rca = 0x1234,
	 .csd = {
		 0x00, 0x4f, 0x00, 0x32, 0x5f, 0x5b, 0x83, 0xd4,
		 0xf6, 0xdb, 0xff, 0x8f, 0x8a, 0xc0, 0x00, 0x00,
		 },
	 .cid = {
		 0x27, 0x50, 0x48, 0x53, 0x44, 0x30, 0x34, 0x47,
		 0x11, 0xe5, 0xe0, 0x0e, 0x3b, 0x00, 0x72, 0x00,
		 },
	 .scr = {
		 0x01, 0x25, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08,
		 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08,
		 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08,
		 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08,
		 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08,
		 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08,

		 },
	 },
	{
	 .producttype = "Sandisk4GB_HC",
	 .type = CARD_TYPE_SD,
	 .ocr = 0xc0ff8000,
	 .usec_reset = 5000,
	 .rca = 0xe624,
	 .csd = {
		 0x40, 0x0e, 0x00, 0x32, 0x5b, 0x59, 0x00, 0x00,
		 0x1e, 0x5c, 0x7f, 0x80, 0x0a, 0x40, 0x40, 0x00,
		 },
	 .cid = {
		 0x03, 0x53, 0x44, 0x53, 0x44, 0x30, 0x34, 0x47,
		 0x80, 0x21, 0x09, 0x3b, 0x01, 0x00, 0x72, 0x00,
		 },
	 .scr = {
		 0x02, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
		 0x01, 0x01, 0x90, 0x00, 0x14, 0x05, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		 },

	 },
	{
	 .producttype = "Corsair4G_HC",
	 .type = CARD_TYPE_SD,
	 .ocr = 0xC0ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x40, 0x7f, 0x0f, 0x32, 0x5b, 0x59, 0x80, 0x00,
		 0x1e, 0x44, 0x7f, 0x80, 0x16, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x1d, 0x41, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x6c, 0x00,
		 },
	 .scr = {
		 0x02, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00,
		 0x01, 0x01, 0x50, 0x00, 0x01, 0x05, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		 },
	 },
	{
	 /* My 4GB SDHC card from Toshiba bought at Reichelt */
	 .producttype = "Toshiba4G_HC",
	 .type = CARD_TYPE_SD,
	 .ocr = 0xC0ff8000,
	 .usec_reset = 5000,
	 .rca = 0x35a7,
	 .csd = {
		 0x40, 0x0e, 0x00, 0x32, 0x5b, 0x59, 0x00, 0x00,
		 0x1d, 0xff, 0x7f, 0x80, 0x0a, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x02, 0x54, 0x4d, 0x53, 0x44, 0x30, 0x34, 0x47,
		 0x30, 0xb0, 0x75, 0xd5, 0x17, 0x00, 0x6b, 0x00,
		 },
	 .scr = {
		 0x02, 0xb5, 0x00, 0x00, 0x18, 0x01, 0x11, 0x02,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
		 0x02, 0x02, 0x90, 0x02, 0x00, 0xaa, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 }

	 },
	{
	 .producttype = "Transcend4G_HC",
	 .type = CARD_TYPE_SD,
	 .ocr = 0xC0ff8000,
	 .usec_reset = 5000,
	 .csd = {
		 0x40, 0x7f, 0x0f, 0x32, 0x5b, 0x59, 0x80, 0x00,
		 0x1e, 0x44, 0x7f, 0x80, 0x16, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x1c, 0x53, 0x56, 0x53, 0x44, 0x48, 0x43, 0x00,
		 0x0a, 0x00, 0x00, 0x23, 0xd5, 0x00, 0x6b, 0x00,
		 },
	 .scr = {
		 0x02, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
		 0x03, 0x01, 0x90, 0x00, 0x01, 0x05, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		 },
	 },
	{
	 .producttype = "Kingston4G_HC",
	 .type = CARD_TYPE_SD,
	 .ocr = 0xC0ff8000,
	 .usec_reset = 5000,
	 .rca = 0x1234,
	 .csd = {
		 0x40, 0x0e, 0x00, 0x32, 0x5b, 0x59, 0x00, 0x00,
		 0x1e, 0x77, 0x7f, 0x80, 0x0a, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x41, 0x34, 0x32, 0x53, 0x44, 0x34, 0x47, 0x42,
		 0x20, 0x5b, 0x00, 0x00, 0xa7, 0x00, 0x71, 0x00,
		 },
	 .scr = {
		 0x02, 0x35, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
		 0x02, 0x02, 0x90, 0x00, 0x01, 0x06, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		 },

	 },
	{
	 .producttype = "Transcend8G_HC",
	 .type = CARD_TYPE_SD,
	 .ocr = 0xC0ff8000,
	 .usec_reset = 5000,
	 .rca = 1,
	 .csd = {
		 0x40, 0x0e, 0x00, 0x32, 0x5b, 0x59, 0x00, 0x00,
		 0x3c, 0x4a, 0x7f, 0x80, 0x0a, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x1c, 0x53, 0x56, 0x53, 0x44, 0x48, 0x43, 0x00,
		 0x10, 0x00, 0x00, 0x04, 0xc2, 0x00, 0x72, 0x00,
		 },
	 .scr = {
		 0x02, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
		 0x01, 0x01, 0x90, 0x00, 0x08, 0x05, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		 },

	 },
	{
	 .producttype = "Hama8G_HC",
	 .type = CARD_TYPE_SD,
	 .ocr = 0xC0ff8000,
	 .usec_reset = 5000,
	 .rca = 0x1234,
	 .csd = {
		 0x40, 0x0e, 0x00, 0x32, 0x5b, 0x59, 0x00, 0x00,
		 0x3d, 0x0f, 0x7f, 0x80, 0x0a, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x27, 0x50, 0x48, 0x53, 0x44, 0x30, 0x38, 0x47,
		 0x20, 0xeb, 0x60, 0x25, 0xd2, 0x00, 0x71, 0x00,
		 },
	 .scr = {
		 0x02, 0x35, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
		 0x01, 0x01, 0x90, 0x00, 0x01, 0x06, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		 },

	 },
	{
	 .producttype = "Adata8G_HC",
	 .type = CARD_TYPE_SD,
	 .ocr = 0xC0ff8000,
	 .usec_reset = 5000,
	 .rca = 0x0001,
	 .csd = {
		 0x40, 0x0e, 0x00, 0x32, 0x5b, 0x59, 0x00, 0x00,
		 0x3c, 0x4a, 0x7f, 0x80, 0x0a, 0x40, 0x00, 0x00,
		 },
	 .cid = {
		 0x1d, 0x41, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0xc6, 0x00, 0x71, 0x00,
		 },
	 .scr = {
		 0x02, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 .ssr = {
		 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
		 0x01, 0x01, 0x90, 0x00, 0x08, 0x05, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 },
	 },
};

/* 
 * ----------------------------------------------------------
 * A MMCard has a linked list of devices which listen
 * on for packets on the data line
 * ----------------------------------------------------------
 */
typedef struct Listener {
	uint8_t buf[2048];
	void *device;
	int maxpkt;
	 MMCard_DataSink(*dataSink);
	//int (*dataSink)(void *dev,uint8_t *buf,int count);
	struct Listener *next;
} Listener;

typedef struct MMCard MMCard;
typedef int MMCCmdProc(MMCard *, uint32_t cmd, uint32_t arg, MMCResponse *);

struct MMCard {
	MMCDev mmcdev;		/* must be the first entry */
	Clock_t *clk;		/* The input from the slot */
	int type;		/* SD/MMC .. */
	int spimode;		/* In Spimode State is not observed ! */
	int state;
	int host_is_2_0;
	int usec_reset;
	int is_app_cmd;		/* > 0 app cmd state */
	int crc_on;
	uint32_t card_status;
	uint32_t ocr;
	uint8_t cid[16];
	uint8_t csd[16];
	int spec_vers;		/* copy from csd on startup */
	uint16_t rca;
	uint16_t dsr;
	uint8_t scr[8];
	uint8_t ssr[64];	/* sd_status register (queried with ACMD13) */
	uint16_t initial_rca;	/* The start value when CMD3 is called the first time */
	uint32_t blocklen;
	uint32_t well_written_blocks;	/* counter for multisector write */
	uint64_t cmdcount;	/* Counter incremented on every command to the card */
	uint64_t set_block_count_time;	/* cmd counter when set block count cmd was issued */
	uint32_t block_count;	/* for following read/write_multiple, 0=infinite */
	uint64_t erase_start;
	uint64_t erase_end;
	DiskImage *disk_image;
	uint64_t capacity;

	uint16_t cmd;		/* command attribute for data read/write operations, ACMD flag in high */
	uint64_t address;	/* address attribute for data read/write operations */
	uint32_t transfer_count;	/* transfer_count incremented during data read/write operations */
	/* Assembly buffer for incoming data (PGM_CSD/CID for example)  */
	uint8_t dataAssBuf[16];
	//uint16_t dataCrcAss; /* Assembly buffer for incoming data CRC */
	//uint16_t dataCrc;    /* The calculated crc of the incoming data */

	/* Reset delay timer: real card needs some undocumented time for reset */
	MMCCmdProc *cmdProc[64];
	MMCCmdProc *appCmdProc[64];
	int reset_state;
	CycleCounter_t reset_start_time;
	CycleTimer transmissionTimer;
	Listener *listener_head;
};

/*
 ********************************************************************+
 * Translation of status codes from Parallel to SPI mode
 ********************************************************************+
 */
static uint8_t
translate_status_to_spir1(uint32_t card_status)
{

	uint8_t spir1 = 0;
	if (card_status & STATUS_ERASE_RESET) {
		spir1 |= SPIR1_ERASE_RESET;
	}
	if (card_status & STATUS_COM_CRC_ERROR) {
		spir1 |= SPIR1_COM_CRC_ERR;
	}
	if (card_status & STATUS_ERASE_SEQ_ERROR) {
		spir1 |= SPIR1_ERASE_SEQ_ERR;
	}
	if (card_status & STATUS_ADDRESS_ERROR) {
		spir1 |= SPIR1_ADDR_ERR;
	}
	return spir1;
}

static void
translate_status_to_spir2(uint32_t card_status, uint8_t * spir2)
{
	spir2[0] = translate_status_to_spir1(card_status);
	spir2[1] = 0;
	if (card_status & STATUS_CARD_IS_LOCKED) {
		spir2[1] |= SPIR2_CARD_LCKD;
	}
	if (card_status & STATUS_WP_ERASE_SKIP) {
		spir2[1] |= SPIR2_WPERSKIPLCK_FAIL;
	}
	if (card_status & STATUS_ERROR) {
		spir2[1] |= SPIR2_ERROR;
	}
	if (card_status & STATUS_CC_ERROR) {
		spir2[1] |= SPIR2_CC_ERR;
	}
	if (card_status & STATUS_CARD_ECC_FAILED) {
		spir2[1] |= SPIR2_CARD_ECC_FAIL;
	}
	if (card_status & STATUS_WP_VIOLATION) {
		spir2[1] |= SPIR2_WP_VIOLATION;
	}
	if (card_status & STATUS_ERASE_PARAM) {
		spir2[1] |= SPIR2_ERASE_PARAM;
	}
	if (card_status & STATUS_CID_CSD_OVERWRITE) {
		spir2[1] |= SPIR2_OOR_CSD_OVER;
	}

}

/*
 * --------------------------------------------------------
 * Add a data packet listener to the card
 * --------------------------------------------------------
 */
void
MMCard_AddListener(MMCDev * mmcdev, void *dev, int maxpkt, MMCard_DataSink * proc)
{
	MMCard *card = container_of(mmcdev, MMCard, mmcdev);
	Listener *li = sg_new(Listener);
	li->device = dev;
	li->maxpkt = maxpkt;
	li->dataSink = proc;
	li->next = card->listener_head;
	if (li->next) {
		fprintf(stderr, "Bug: currently only one listener per MMC card allowed\n");
	}
	card->listener_head = li;
}

void
MMCard_RemoveListener(MMCDev * mmcdev, void *dev)
{
	MMCard *card = container_of(mmcdev, MMCard, mmcdev);
	Listener *li = card->listener_head;
	if (li->device != dev) {
		fprintf(stderr, "MMCard: Removing wrong device from listeners list\n");
	}
	card->listener_head = NULL;
	free(li);
}

/*
 ************************************************************
 * Bit access functions for card registers cid/csd/scr
 ************************************************************
 */
static uint32_t
getbits(uint8_t * arr, int arrlen, int from, int to)
{
	int i;
	uint32_t val = 0;
	for (i = to; i >= from; i--) {
		int idx = arrlen - 1 - (i >> 3);
		int bit = i & 7;
		if (arr[idx] & (1 << bit)) {
			val = (val << 1) | 1;
		} else {
			val = (val << 1);
		}
	}
	return val;
}

static void
copybits(uint32_t val, uint8_t * arr, int arrlen, int from, int to)
{
	int i;
	for (i = from; i <= to; i++) {
		int idx = arrlen - 1 - (i >> 3);
		int bit = i & 7;
		if (val & 1) {
			arr[idx] |= (1 << bit);
		} else {
			arr[idx] &= ~(1 << bit);
		}
		val = (val >> 1);
	}
}

#define GETBITS(a,from,to) getbits((a),sizeof((a)),(from),(to))
#define COPYBITS(value,a,from,to) copybits((value),(a),sizeof((a)),(from),(to))

/*
 * ----------------------------------------------------------
 * csd_get_blocklen
 *	Get the blocklen from CSD structure
 * ----------------------------------------------------------
 */
static uint32_t
csd_get_blocklen(uint8_t * csd, int cardtype)
{
	int csd_structure;
	uint32_t blocklen;
	int csdsize = 16;
	csd_structure = getbits(csd, csdsize, 126, 127);
	if (cardtype == CARD_TYPE_SD) {
		switch (csd_structure) {
		    case 0:
			    blocklen = 1 << getbits(csd, csdsize, 80, 83);
			    break;
		    case 1:
			    blocklen = 512;
			    break;
		    default:
			    blocklen = 512;
			    fprintf(stderr, "Unknown CSD structure version\n");
			    exit(1);
		}
	} else if (cardtype == CARD_TYPE_MMC) {
		switch (csd_structure) {
		    case 0:
		    case 1:
			    blocklen = 1 << getbits(csd, csdsize, 80, 83);
			    break;
		    default:
			    blocklen = 0;
			    fprintf(stderr, "Unknown CSD structure version\n");
			    exit(1);
		}
	} else {
		fprintf(stderr, "MMCard: Unknown card type %d\n", cardtype);
		exit(1);
	}
	return blocklen;
}

/*
 * ------------------------------------------------------
 * csd_get_capacity
 * 	Read the card capacity from the CSD structure.
 *	Depends on cardtype SD/MMC and on CSD structure	
 *	version.
 * ------------------------------------------------------
 */
static uint64_t
csd_get_capacity(uint8_t * csd, int cardtype)
{
	int csd_structure;
	uint32_t c_size_mult;
	uint64_t c_size;
	uint64_t capacity;
	uint32_t blocklen;
	int csdsize = 16;
	csd_structure = getbits(csd, csdsize, 126, 127);
	blocklen = csd_get_blocklen(csd, cardtype);
	if (cardtype == CARD_TYPE_SD) {
		switch (csd_structure) {
		    case 0:
			    c_size = getbits(csd, csdsize, 62, 73);
			    c_size_mult = getbits(csd, csdsize, 47, 49);
			    capacity = blocklen * ((c_size + 1) << (c_size_mult + 2));
			    break;
		    case 1:
			    c_size = getbits(csd, csdsize, 48, 69);
			    capacity = (c_size + 1) * 512 * 1024;
			    break;
		    default:
			    capacity = 0;
			    fprintf(stderr, "Unknown CSD structure version\n");
			    exit(1);
		}
	} else if (cardtype == CARD_TYPE_MMC) {
		switch (csd_structure) {
		    case 0:
		    case 1:
			    c_size = getbits(csd, csdsize, 62, 73);
			    c_size_mult = getbits(csd, csdsize, 47, 49);
			    capacity = blocklen * ((c_size + 1) << (c_size_mult + 2));
			    break;
		    default:
			    blocklen = 0;
			    capacity = 0;
			    fprintf(stderr, "Unknown CSD structure version\n");
			    exit(1);
		}
	} else {
		capacity = 0;
		fprintf(stderr, "Unknown cardtype %d\n", cardtype);
	}
	return capacity;
}

/*
 * ----------------------------------------------------------------------------
 * dump_cardtypes
 * 	Print a list of all implemented Cards with some technical information
 * ----------------------------------------------------------------------------
 */

static char *curr_max[8] = {
	"  1mA",
	"  5mA",
	" 10mA",
	" 25mA",
	" 35mA",
	" 45mA",
	" 80mA",
	"100mA",
};

static void
dump_cardtypes()
{
	int nr_types = sizeof(cardspec) / sizeof(MMCardSpec);
	int i;
	char *interface;
	MMCardSpec *spec;
	int manfact;
	char vendname[3] = { ' ', ' ', 0 };
	fprintf(stderr, "Possible MMC/SD Cards:\n");
	for (i = 0; i < nr_types; i++) {
		uint64_t size;
		int speed_class;
		spec = &cardspec[i];
		int csdvers = getbits(spec->csd, 16, 126, 127);
		int blocklen = csd_get_blocklen(spec->csd, spec->type);
		int erase_blk_en;
		int ccc;
		int max_rd_current = GETBITS(spec->csd, 56, 58);	/* only for csd-1.0 */
		int max_wr_current = GETBITS(spec->csd, 50, 52);	/* only for csd-1.0 */
		uint32_t psn;
		psn = GETBITS(spec->cid, 24, 55);
		erase_blk_en = GETBITS(spec->csd, 46, 46);
		ccc = GETBITS(spec->csd, 84, 95);
		manfact = spec->cid[0];
		if (isprint(spec->cid[1])) {
			vendname[0] = spec->cid[1];
		} else {
			vendname[0] = ' ';
		}
		if (isprint(spec->cid[2])) {
			vendname[1] = spec->cid[2];
		} else {
			vendname[1] = ' ';
		}
		if (spec->type == CARD_TYPE_SD) {
			if (csdvers == 0) {
				interface = "SD  ";
			} else if (csdvers == 1) {
				interface = "SDHC";
			} else {
				interface = "Unknown";
			}
		} else if (spec->type == CARD_TYPE_MMC) {
			interface = "MMC-Card ";
		} else {
			interface = "          ";
		}
		size = csd_get_capacity(spec->csd, spec->type);
		speed_class = GETBITS(spec->ssr, 440, 447) * 2;
		if (csdvers == 0) {
			fprintf(stderr,
				"%s: 0x%02x \"%s\" \"%18s\" bl %4d sz %11lld Class(%d) CCC %03x Iw %s Ir %s\n",
				interface, manfact, vendname, spec->producttype, blocklen,
				(long long)size, speed_class, ccc, curr_max[max_wr_current],
				curr_max[max_rd_current]);
		} else {
			fprintf(stderr,
				"%s: 0x%02x \"%s\" \"%18s\" bl %4d sz %11lld Class(%d) CCC %03x\n",
				interface, manfact, vendname, spec->producttype, blocklen,
				(long long)size, speed_class, ccc);
		}
		fflush(stderr);
		usleep(1000);
	}
	fprintf(stderr, "SD-Card  : \"auto_sd\"\n");
	fprintf(stderr, "MMC-Card : \"auto_mmc\"\n");
	fprintf(stderr, "SDHC-Card: \"auto_sdhc\"\n");
}

/*
 * ----------------------------------------------------------------
 * MMCard_Read:
 * retval: number of Bytes, -ERRCODE on error  0=eofdata
 * ----------------------------------------------------------------
 */
int
MMCard_Read(MMCDev * dev, uint8_t * buf, int count)
{
	int i;
	MMCard *card = container_of(dev, MMCard, mmcdev);
	if (!card) {
		return 0;
	}
	if (card->state != STATE_DATA) {
		return 0;
	}
	if (card->cmd == (SD_APP_SEND_SCR | CMD_FLAG_IS_APPCMD)) {
		for (i = 0; (i < count) && (card->transfer_count < 8); i++) {
			buf[i] = card->scr[card->transfer_count];
			card->transfer_count++;
		}
		if (card->transfer_count == 8) {
			card->state = STATE_TRANSFER;
		}
		return i;
	} else if (card->cmd == (SD_APP_SEND_NUM_WR_BLKS | CMD_FLAG_IS_APPCMD)) {
		for (i = 0; (i < count) && (card->transfer_count < 4); i++) {
			buf[i] =
			    (card->well_written_blocks >> ((3 - card->transfer_count) * 8)) & 0xff;
			card->transfer_count++;
		}
		if (card->transfer_count == 4) {
			card->state = STATE_TRANSFER;
		}
		//fprintf(stderr,"Send well written blocks %d state %d\n",card->well_written_blocks,card->state);
		return i;
	} else if (card->cmd == MMC_READ_SINGLE_BLOCK) {
		uint64_t address = card->address + card->transfer_count;
		if (card->transfer_count + count > card->blocklen) {
			count = card->blocklen - card->transfer_count;
			if (count < 0) {
				fprintf(stderr, "transfer count < 0 should never happen\n");
				return 0;
			}
		}
		if (address >= card->capacity) {
			return 0;
		}
		if ((address + count) > card->capacity) {
			count = card->capacity - address;
		}
		if (DiskImage_Read(card->disk_image, address, buf, count) < count) {
			fprintf(stderr, "MMCard: Error reading from diskimage\n");
		}
		card->transfer_count += count;
		if (card->transfer_count == card->blocklen) {
			card->state = STATE_TRANSFER;
		}
		return count;
	} else if (card->cmd == MMC_READ_MULTIPLE_BLOCK) {
		uint64_t address = card->address + card->transfer_count;
		if ((address + count) > card->capacity) {
			count = card->capacity - address;
		}
		if (card->block_count) {
			if ((address & ~(card->blocklen - 1)) !=
			    ((address + count) & ~(card->blocklen - 1))) {
				card->block_count--;
				if (!card->block_count) {
					card->state = STATE_TRANSFER;
				}
			}
		}
		if (DiskImage_Read(card->disk_image, address, buf, count) < count) {
			fprintf(stderr, "MMCard: Error reading from diskimage\n");
		}
		card->transfer_count += count;
		return count;
	} else if (card->cmd == MMC_READ_DAT_UNTIL_STOP) {
		uint64_t address = card->address + card->transfer_count;
		if ((address + count) > card->capacity) {
			count = card->capacity - address;
		}
		if (DiskImage_Read(card->disk_image, address, buf, count) < count) {
			fprintf(stderr, "MMCard: Error reading from diskimage\n");
		}
		card->transfer_count += count;
		return count;
	} else if (card->cmd == (SD_APP_SEND_STATUS | CMD_FLAG_IS_APPCMD)) {
		for (i = 0; (i < count) && (card->transfer_count < 64); i++) {
			buf[i] = card->ssr[card->transfer_count];
			card->transfer_count++;
		}
		fprintf(stderr, "MMC read: APP_SEND_STATUS count %d tc %d \n", count,
			card->transfer_count);
		if (card->transfer_count == 64) {
			card->state = STATE_TRANSFER;
		}
		return i;
	} else {
		fprintf(stderr, "MMC card read with unknown command %d\n", card->cmd);
		return 0;
	}
}

/* 
 * ----------------------------------------------------------------------
 * MMCard initiated transmission to host
 * The MMCard fires without checking if the host really has enough buffer
 * (Real cards have the same behaviour)
 * ----------------------------------------------------------------------
 */
static inline void
MMCard_StartTransmission(MMCard * card)
{
	if (CycleTimer_IsActive(&card->transmissionTimer)) {
		fprintf(stderr, "MMCard: Warning, transmission timer is already running !\n");
	} else {
		CycleTimer_Mod(&card->transmissionTimer, CycleTimerRate_Get() / 26600);
	}
}

static void
MMCard_DoTransmission(void *clientData)
{
	MMCDev *mmcdev = (MMCDev *) clientData;
	MMCard *card = container_of(mmcdev, MMCard, mmcdev);
	Listener *li = card->listener_head;
	int len;
	int result;
	uint64_t cycles;
	uint32_t freq;

	if (!li) {
		/* Old style Read Interface */
		return;
	}
	if (CycleTimer_IsActive(&card->transmissionTimer)) {
		fprintf(stderr, "Error: Card Transmission timer is already running\n");
		return;
	}
	if (card->state != STATE_DATA) {
		return;
	}
	len = li->maxpkt < sizeof(li->buf) ? li->maxpkt : sizeof(li->buf);
	result = MMCard_Read(mmcdev, li->buf, len);
	if (result <= 0) {
		return;
	}
//      fprintf(stderr,"MMCard: Do the transmission len %d, transfer cnt %d\n",result,card->transfer_count); // jk
	//MMC_CRC16Init(&dataBlock.crc,0);
	//MMC_CRC16(&dataBlock.crc,dataBlock.data,dataBlock.datalen);
	li->dataSink(li->device, li->buf, result);
	freq = Clock_Freq(card->clk);
	if (!freq) {
		freq = 1;
		fprintf(stderr, "Error: MMCard used with clock of 0 HZ\n");
	}
	if (card->type == CARD_TYPE_MMC) {
		cycles = NanosecondsToCycles(1000000000 / freq * 10 * result);
	} else {
		cycles = NanosecondsToCycles((1000000000 / 4) / freq * 10 * result);
	}
	if (CycleTimer_IsActive(&card->transmissionTimer)) {
		fprintf(stderr, "MMCard: Bug, transmission timer is already running !\n");
	} else {
		CycleTimer_Mod(&card->transmissionTimer, cycles);
	}
}

/*
 * ----------------------------------------------
 * CMD0: go_idle
 * No response
 * ----------------------------------------------
 */
static int
mmc_go_idle(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	if (card->state == STATE_INACTIVE) {
		return MMC_ERR_TIMEOUT;
	}
	card->state = STATE_IDLE;
	card->reset_state = RST_NOT_STARTED;
	card->rca = 0;
	card->card_status = STATUS_READY_FOR_DATA;
	card->host_is_2_0 = 0;
	resp->len = 0;
	return MMC_ERR_NONE;
}

/*
 * ----------------------------------------------
 * CMD0: SPI version 
 * ----------------------------------------------
 */
static int
spi_go_idle(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = mmc_go_idle(card, cmd, arg, resp);
	resp->len = 1;
	resp->data[0] = SPIR1_IDLE;
	return result;
}

/*
 * -------------------------------------------------------------------------
 * MMC send op cond: CMD1 Response format R3
 * MMC only
 * -------------------------------------------------------------------------
 */
static int
mmc_send_op_cond(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	if (card->state != STATE_IDLE) {
		fprintf(stderr, "MMCard: got SEND_OP_COND cmd in non idle state (%d)\n",
			card->state);
		resp->len = 0;
		return MMC_ERR_TIMEOUT;
	}
	if (card->reset_state == RST_NOT_STARTED) {
		card->reset_start_time = CycleCounter_Get();
		card->ocr &= ~OCR_NOTBUSY;
		card->reset_state = RST_STARTED;
	} else if (card->reset_state == RST_STARTED) {
		int64_t usec = CyclesToMicroseconds(CycleCounter_Get() - card->reset_start_time);
		if (usec > card->usec_reset) {
			card->state = STATE_READY;
			card->ocr |= OCR_NOTBUSY;
			card->reset_state = RST_DONE;
			card->block_count = 0;
		}
	} else if (card->reset_state != RST_DONE) {
		fprintf(stderr, "Emulator bug: MMC-Card reset_state %d not valid\n",
			card->reset_state);
		exit(1);
	}
	resp->len = 6;
	resp->data[0] = 0x3f;
	resp->data[1] = card->ocr >> 24;
	resp->data[2] = card->ocr >> 16;
	resp->data[3] = card->ocr >> 8;
	resp->data[4] = card->ocr;
	resp->data[5] = 0xff;
	dbgprintf("Send op cond arg 0x%08x\n", arg);
	return MMC_ERR_NONE;
}

/*
 * -------------------------------------------------------------------------
 * CMD1 SPI version 
 * -------------------------------------------------------------------------
 */
static int
spi_send_op_cond(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = mmc_send_op_cond(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;	/* in idle state */
	} else {
		resp->data[0] = 0;
	}
	dbgprintf("Send op cond arg 0x%08x\n", arg);
	return result;
}

/*
 * ---------------------------------------------------------------------
 * CMD2, no argument
 * Response format R2 (136 Bits)
 * doesn't respond when  rca is not 0.  (Toshiba docu says this)
 * ---------------------------------------------------------------------
 */
static int
mmc_all_send_cid(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int i;
	if (card->rca != 0) {
		return MMC_ERR_TIMEOUT;
	}
	if (card->state != STATE_READY) {
		/* Real card seems to behave this way ? Or is it the controller ? */
		fprintf(stderr, "ALL_SEND_CID: card not in ready state\n");
		resp->len = 0;
		return MMC_ERR_TIMEOUT;
	}
	resp->len = 17;
	resp->data[0] = 0x3f;
	for (i = 0; i < 16; i++) {
		resp->data[1 + i] = card->cid[i];
	}
	card->state = STATE_IDENT;
	return MMC_ERR_NONE;
}

/*
 * ---------------------------------------------------------------------------------
 * CMD3 for MMC cards: set relative address
 * for SD-Cards see sd_send relative address
 * Argument: Bits 16-31 RCA
 * Response format R1
 * --------------------------------------------------------------------------------
 */
static int
mmc_set_relative_addr(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->state != STATE_IDENT) {
		fprintf(stderr, "Got SET_RCA but not in IDENT state (%d)\n", card->state);
		resp->len = 0;
		return MMC_ERR_TIMEOUT;
	}
	card->state = STATE_STBY;
	card->rca = (arg >> 16) & 0xffff;
	fprintf(stderr, "New rca is %d\n", card->rca);
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

/*
 * ---------------------------------------------------------------------------------
 * CMD3 for SD-Cards, send relative address
 * All cards seem to increment the RCA by one when CMD3 is called a second time
 * Argument: stuff bits
 * Response format R6
 * --------------------------------------------------------------------------------
 */
static int
sd_send_relative_addr(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->state == STATE_IDENT) {
		card->rca = card->initial_rca;
	} else if (card->state == STATE_STBY) {
		card->rca++;
	} else {
		resp->len = 0;
		return MMC_ERR_TIMEOUT;
	}
	card->state = STATE_STBY;
	if (!card->rca)
		card->rca++;
	//card->card_status  &= ~(0xfd3fc020);
	card->card_status &= ~(0xc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card->rca >> 8) & 0xff;
	resp->data[2] = card->rca & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

/*
 * -------------------------------------------------------------
 * CMD4 SET_DSR optional broadcast command
 * only present when bit 96 in CSD is set
 * STATE_STBY -> STATE_STBY (Sandisk state table)
 * -------------------------------------------------------------
 */
static int
mmc_set_dsr(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->len = 0;
	if (card->state != STATE_STBY) {
		fprintf(stderr, "MMCard: Got set dsr cmd in wrong state %d\n", card->state);
		return MMC_ERR_TIMEOUT;
	}
	card->dsr = arg >> 16;
	return MMC_ERR_NONE;
}

/*
 * ------------------------------------------------------------------------------
 * CMD6: MMC_SWITCH
 * State: Transfer -> Data (involves data stage)
 * Valid in transfer state (Simplified Spec 2.0), Mandatory for SD >= 1.10
 * Response Format R1 and 512 Bit on the Data lines
 * ------------------------------------------------------------------------------
 */
static int
mmc_switch(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "MMC switch command not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ------------------------------------------------------------------------------
 * CMD7 select Card
 * State: Stdby->Trans Dis->Prg when addressed 
 * 	  STDBY|TRAN|DATA->STDBY , PRG->DIS when not addressed
 * Response format R1 arg argument RCA
 * ------------------------------------------------------------------------------
 */
static int
mmc_select_card(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint16_t rca = arg >> 16;
	uint32_t card_status = GET_STATUS(card);
	if ((card->rca != rca) || (rca == 0)) {
		if ((card->state == STATE_STBY) || (card->state == STATE_TRANSFER)
		    || (card->state == STATE_DATA)) {
			card->state = STATE_STBY;
		} else if (card->state == STATE_PRG) {
			card->state = STATE_DIS;
		} else {
			fprintf(stderr, "CMD7 unsel card not allowed in state %d\n", card->state);
			return MMC_ERR_TIMEOUT;
		}
	} else {
		if (card->state == STATE_DIS) {
			card->state = STATE_PRG;
		} else if (card->state == STATE_STBY) {
			card->state = STATE_TRANSFER;
		} else {
			fprintf(stderr, "CMD7 sel card not allowed in state %d\n", card->state);
			return MMC_ERR_TIMEOUT;
		}
	}
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

/*
 * ------------------------------------------------------------------------------
 * CMD8
 *	MMCPLUS_SEND_EXT_CSD
 * ------------------------------------------------------------------------------
 */
static int
mmcplus_send_ext_csd(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "MMC_SEND_EXT_CSD  command not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ----------------------------------------------------------------------------
 * CMD8
 *	SD_SEND_INTERFACE_COND
 *
 * Version: SD >= 2.0
 * Is mandatory before ACMD41 for high capacity cards !
 * State: Idle -> Idle
 * Response format R7 
 * -----------------------------------------------------------------------------
 */
static int
sd_send_interface_cond(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int vhs;
	int check_pattern;
	if (!card->spimode && (card->state != STATE_IDLE)) {
		fprintf(stderr, "SD-CMD8 received in wrong state %d\n", card->state);
		return MMC_ERR_TIMEOUT;
	}
	vhs = (arg >> 8) & 0xf;
	check_pattern = arg & 0xff;
	if (check_pattern != 0xaa) {
		fprintf(stderr, "SD-Card: Received non recommended checkpattern\n");
	}
	card->host_is_2_0 = 1;
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = 0;
	resp->data[2] = 0;
	resp->data[3] = vhs;	/* always accept the voltage */
	resp->data[4] = check_pattern;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

static int
spi_send_interface_cond(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = sd_send_interface_cond(card, cmd, arg, resp);
	resp->len = 5;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return result;
}

/*
 * -----------------------------------------------------------------------
 * CMD9 MMC_SEND_CSD
 *	read card specific data 
 * arg: rca in high
 * response: 136 Bit R2
 * STATE_STBY -> STATE_STBY
 * -----------------------------------------------------------------------
 */
static int
mmc_send_csd(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int i;
	uint16_t rca = arg >> 16;
	if (!card->spimode && (card->state != STATE_STBY)) {
		fprintf(stderr, "MMCard: SEND_CSD but not in standby state\n");
		return MMC_ERR_TIMEOUT;
	}
	if (rca != card->rca) {
		fprintf(stderr, "SEND CSD: card not selected, rca %d\n", rca);
		dbgprintf("SEND CSD: card not selected\n");
		return MMC_ERR_TIMEOUT;
	}
	resp->len = 17;
	resp->data[0] = 0x3f;
	for (i = 0; i < 16; i++) {
		resp->data[1 + i] = card->csd[i];
	}
	return MMC_ERR_NONE;
}

/*
 **************************************************************************
 * SPI version of CMD9 
 * Is a little bit a hack, It includes the data frame in the response
 **************************************************************************
 */
static int
spi_send_csd(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int i;
	uint16_t crc = 0;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	resp->data[1] = 0xfe;	/* Start block marker */
	for (i = 0; i < 16; i++) {
		resp->data[2 + i] = card->csd[i];
	}
	MMC_CRC16(&crc, card->csd, 16);
	resp->data[18] = crc >> 8;
	resp->data[19] = crc & 0xff;
	resp->len = 20;
	return MMC_ERR_NONE;
}

/*
 * -----------------------------------------------------------------
 * CMD10 SEND_CID
 * arg: rca
 * return R2 (128 + 8 Bits)
 * STATE_STBY -> STATE_STBY
 * -----------------------------------------------------------------
 */

static int
mmc_send_cid(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int i;
	uint16_t rca = arg >> 16;
	if (card->state != STATE_STBY) {
		resp->len = 0;
		return MMC_ERR_TIMEOUT;
	}
	if (rca != card->rca) {
		dbgprintf("SEND CID: card not selected\n");
		return MMC_ERR_TIMEOUT;
	}
	resp->len = 17;
	resp->data[0] = 0x3f;
	for (i = 0; i < 16; i++) {
		resp->data[1 + i] = card->cid[i];
	}
	return MMC_ERR_NONE;
}

/*
 *******************************************************
 * SPI version of send CID CMD10
 * Hack !, includes data frame in response
 *******************************************************
 */
static int
spi_send_cid(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int i;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	resp->data[1] = 0xfe;	/* Start block marker */
	for (i = 0; i < 16; i++) {
		resp->data[2 + i] = card->cid[i];
	}
	resp->data[18] = 0xff;
	resp->data[19] = 0xff;
	resp->len = 20;
	return MMC_ERR_NONE;
}

/*
 * ---------------------------------------------------------
 * CMD11 READ_DATA_UNTIL_STOP, Maybe only mmc ? not in SD-2.0 
 * arg: none
 * resp R1 
 * STATE ??? not found in SanDisk docu
 * ---------------------------------------------------------
 */
static int
mmc_read_dat_until_stop(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->state != STATE_TRANSFER) {
		fprintf(stderr, "MMCard: read data until stop in wrong state %d\n", card->state);
		return MMC_ERR_TIMEOUT;
	}
	/* Store the attributes for the recognized data operation */
	card->cmd = MMC_READ_DAT_UNTIL_STOP;
	card->address = arg;	/* Block alignement check required ? */
	card->transfer_count = 0;

	card->state = STATE_DATA;
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	MMCard_StartTransmission(card);
	return MMC_ERR_NONE;
}

/*
 * ---------------------------------------------------------------------------------
 * CMD12 STOP_TRANSMISSION
 * arg: none
 * resp: R1b  (busy signal was never observed after CMD12 ????)
 * STATE: Read:  STATE_DATA -> STATE_TRANSFER
 *	  Write: STATE_RCV -> STATE_PRG ... (delay) -> STATE_TRANSFER 
 *
 * The data transfer stops after the end bit of the stop cmd according to
 * Simplified Spec V2.0
 * ---------------------------------------------------------------------------------
 */
static int
mmc_stop_transmission(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->len = 0;
	uint32_t card_status = GET_STATUS(card);
	if ((card->state != STATE_DATA) && (card->state != STATE_RCV)) {
		fprintf(stderr, "MMCard: STOP_TRANSMISSION but in wrong state: %d\n", card->state);
		return MMC_ERR_TIMEOUT;
	}
	if (card->state == STATE_DATA) {
		card->state = STATE_TRANSFER;
	} else if (card->state == STATE_RCV) {
		/* This should go to STATE_PRG and after a delay go to STATE_TRANSFER ! */
		card->state = STATE_TRANSFER;
	} else {
		fprintf(stderr, "MMCard Bug: stop tranmission in state %d\n", card->state);
	}
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

static int
spi_stop_transmission(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = mmc_stop_transmission(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;	/* in idle state */
	} else {
		resp->data[0] = 0;
	}
	return result;
}

/*
 * -------------------------------------------------------------------
 * CMD13 MMC_SEND_STATUS
 * arg rca
 * response format R1
 * Keep STATE 
 * -------------------------------------------------------------------
 */
static int
mmc_send_status(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int state = card->state;
	uint32_t card_status = GET_STATUS(card);
	uint16_t rca = (arg >> 16) & 0xffff;
	if (rca != card->rca) {
		dbgprintf("MMC SEND STATUS: Card not selected\n");
		return MMC_ERR_TIMEOUT;
	}
	if (!card->spimode && (state != STATE_STBY) && (state != STATE_TRANSFER)
	    && (state != STATE_DATA)
	    && (state != STATE_RCV) && (state != STATE_PRG) && (state != STATE_DIS)) {
		fprintf(stderr, "MMC SEND STATUS: in wrong state %d\n", state);
		return MMC_ERR_TIMEOUT;
	}
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	dbgprintf("MMCard CMD13: Card status %08x\n", card->card_status);
	return MMC_ERR_NONE;
}

/**
 ******************************************************************************************
 * \fn static int spi_send_status(MMCard *card,uint32_t cmd,uint32_t arg,MMCResponse *resp) 
 ******************************************************************************************
 */
static int
spi_send_status(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int state = card->state;
	uint32_t card_status = GET_STATUS(card);
	card->card_status &= ~(0xfd3fc020);
	translate_status_to_spir2(card_status, resp->data);
	resp->len = 2;
	if (state == STATE_IDLE) {
		resp->data[0] |= SPIR1_IDLE;
	}
	dbgprintf("MMCard CMD13: Card status %08x\n", card->card_status);
	return MMC_ERR_NONE;
}

/*
 * -------------------------------------------------------------------------------
 * ACMD13 
 * SD_APP_SEND_STATUS
 * State: Tran -> Data
 * Argument: ignored
 *
 * Response: SD-Card Spec says R2. I think this is wrong
 * Sandisk says R1 real card also, but maybe its R1b.
 * White unlabeled 256M card from Toshiba needs a pause of > 3msec after
 * this command or it will stop working 
 * -------------------------------------------------------------------------------
 */
static int
sd_app_send_status(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->state != STATE_TRANSFER) {
		fprintf(stderr, "SD_SEND_STATUS in wrong state\n");
		return MMC_ERR_TIMEOUT;
	}
	card->transfer_count = 0;
	card->state = STATE_DATA;
	card->cmd = cmd | CMD_FLAG_IS_APPCMD;
	card->card_status &= ~(0xfd3fc020);
	/** Missing here: set appcmd in status field !!!!!!!!!!!!!!!!!!!! */
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	MMCard_StartTransmission(card);
	return MMC_ERR_NONE;
}

/**
 ******************************************************************************
 * Spi Version seems not to exist, seems to fall back to non APP version
 * PQI product documentation says this. Can not find this information
 * in the standard.
 ******************************************************************************
 */
__UNUSED__ static int
spi_app_send_status(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int i;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	resp->data[1] = 0xfe;	/* Start block marker */
	for (i = 0; i < 64; i++) {
		resp->data[2 + i] = card->ssr[i];
	}
	resp->data[66] = 0xff;
	resp->data[67] = 0xff;
	resp->len = 68;
	return MMC_ERR_NONE;
}

/*
 * ------------------------------------------------------------------------------
 * CMD14
 *	MMCPLUS_BUSTEST_R
 * ------------------------------------------------------------------------------
 */
static int
mmcplus_bustest_r(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "MMC_BUSTEST_R  command not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ------------------------------------------------------------------
 * CMD15 GO_INACTIVE
 * arg RCA
 * response none
 * STATE_xxx -> STATE_INACTIVE (Sandisk manual)
 * ------------------------------------------------------------------
 */
static int
mmc_go_inactive_state(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint16_t rca = (arg >> 16) & 0xffff;
	int state = card->state;
	resp->len = 0;
	if (rca != card->rca) {
		fprintf(stderr, "MMC SEND STATUS: Card not selected\n");
		return MMC_ERR_TIMEOUT;
	}
	if ((state != STATE_STBY) && (state != STATE_TRANSFER) && (state != STATE_DATA)
	    && (state != STATE_RCV) && (state != STATE_PRG) && (state != STATE_DIS)) {
		fprintf(stderr, "MMC SEND STATUS: in wrong state %d\n", state);
		return MMC_ERR_TIMEOUT;
	}
	card->state = STATE_INACTIVE;
	return MMC_ERR_NONE;
}

/*
 * -----------------------------------------------------------------------------------------------
 * CMD16 MMC_SET_BLOCKLEN 
 * arg: Blocklen in bytes for all following block read write commands
 *  Response R1
 * STATE_TRANSFER -> STATE_TRANSFER
 * -----------------------------------------------------------------------------------------------
 */
static int
mmc_set_blocklen(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int state = card->state;
	uint32_t card_status = GET_STATUS(card);
	if (!card->spimode && (state != STATE_TRANSFER)) {
		fprintf(stderr, "MMC SET BLOCKLEN: in wrong state %d\n", state);
		return MMC_ERR_TIMEOUT;
	}
	if (card->ocr & OCR_CCS) {
		if (arg != 512) {
			fprintf(stderr, "Set blocklen %d has no effect on High Capacity card\n",
				arg);
		}
	} else {
		card->blocklen = arg;
	}
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

static int
spi_set_blocklen(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = mmc_set_blocklen(card, cmd, arg, resp);
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	resp->len = 1;
	return result;
}

/*
 * ------------------------------------------------------------------------
 * CMD17 Read single block
 *	Argument: address
 *	Response Format R1
 * STATE_TRANSFER -> STATE_DATA
 * ------------------------------------------------------------------------
 */
static int
mmc_read_single_block(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int state = card->state;
	uint32_t card_status = GET_STATUS(card);
	if (state != STATE_TRANSFER) {
		fprintf(stderr, "MMC READ single block: not in TRANSFER state but %d\n", state);
		return MMC_ERR_TIMEOUT;
	}
	/* Store the attributes for the recognized data operation */
	card->cmd = MMC_READ_SINGLE_BLOCK;
	if (card->ocr & OCR_CCS) {
		card->address = ((uint64_t) arg) << 9;
	} else {
		/* any address is accepted, Table 4.2 Simpl-2.0 */
		card->address = arg;
	}
	card->transfer_count = 0;

	card->state = STATE_DATA;
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	dbgprintf("Read single block start transmission \n");
	MMCard_StartTransmission(card);
	return MMC_ERR_NONE;
}

/*
 * ------------------------------------------------------------------------
 * CMD17 SPI mode: Read single block
 *	Argument: address
 *	Response Format R1
 * State transition unclear: How can I go to TRANSFER state in SPI mode ?
 * Real card always returns resp[0] == 0 for successful read. 
 * Tested with kingston + atp card
 * ------------------------------------------------------------------------
 */
static int
spi_read_single_block(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	//uint32_t card_status = GET_STATUS(card);
	/* Store the attributes for the recognized data operation */
	dbgprintf("SPI version of read single block\n");
	card->cmd = MMC_READ_SINGLE_BLOCK;
	if (card->ocr & OCR_CCS) {
		card->address = ((uint64_t) arg) << 9;
	} else {
		/* any address is accepted, Table 4.2 Simpl-2.0 */
		card->address = arg;
	}
	card->transfer_count = 0;
	card->state = STATE_DATA;
	card->card_status &= ~(0xfd3fc020);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	dbgprintf("MMCard StartTransmission\n");
	MMCard_StartTransmission(card);
	return MMC_ERR_NONE;
}

/*
 * --------------------------------------------------------------------------
 * CMD18
 * 	MMC read multiple block
 *	Argument: address 
 *	Response format R1
 * --------------------------------------------------------------------------
 */
static int
mmc_read_multiple_block(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int state = card->state;
	uint32_t card_status = GET_STATUS(card);
	if (state != STATE_TRANSFER) {
		fprintf(stderr, "MMC READ multiple block: in wrong state %d\n", state);
		return MMC_ERR_TIMEOUT;
	}

	/* Store the attributes for the recognized data operation */
	card->cmd = MMC_READ_MULTIPLE_BLOCK;
	if (card->ocr & OCR_CCS) {
		card->address = ((uint64_t) arg) << 9;
	} else {
		card->address = arg & ~(card->blocklen - 1);
	}
	card->transfer_count = 0;
	if ((card->set_block_count_time + 1) != card->cmdcount) {
		card->block_count = 0;	/* means infinite */
	}
	card->state = STATE_DATA;
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	MMCard_StartTransmission(card);
	return MMC_ERR_NONE;
}

static int
spi_read_multiple_block(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	/* Store the attributes for the recognized data operation */
	card->cmd = MMC_READ_MULTIPLE_BLOCK;
	if (card->ocr & OCR_CCS) {
		card->address = ((uint64_t) arg) << 9;
	} else {
		card->address = arg & ~(card->blocklen - 1);
	}
	card->transfer_count = 0;
	if ((card->set_block_count_time + 1) != card->cmdcount) {
		card->block_count = 0;	/* means infinite */
	}
	card->state = STATE_DATA;
	card->card_status &= ~(0xfd3fc020);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	MMCard_StartTransmission(card);
	return MMC_ERR_NONE;
}

/*
 * ------------------------------------------------------------------------------
 * CMD19
 *	MMCPLUS_BUSTEST_W
 * ------------------------------------------------------------------------------
 */
static int
mmcplus_bustest_w(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "MMC_BUSTEST_W  command not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * --------------------------------------------------------------------
 * CMD20 MMC_WRITE_DAT_UNTIL_STOP Maybe only MMC ?
 * arg: address
 * response R1
 * STATE: ???? not mentioned in SanDisk docu
 * --------------------------------------------------------------------
 */
static int
mmc_write_dat_until_stop(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->state != STATE_TRANSFER) {
		fprintf(stderr, "MMCard: write dat until stop in wrong state %d\n", card->state);
		return MMC_ERR_TIMEOUT;
	}
	/* Store the attributes for the recognized data operation */
	card->cmd = MMC_WRITE_DAT_UNTIL_STOP;
	card->address = arg;
	card->transfer_count = 0;
	card->well_written_blocks = 0;

	card->state = STATE_DATA;
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

/* 
 * ---------------------------------------------------------------------------------
 * This command is only valid in MMC >= 3.1. See Section B.5 
 * in MMC-System-Spec-v3.31.pdf
 * Kingston1G and Kingston512M don't have it for example
 *
 * CMD23 MMC_SET_BLOCK_COUNT
 * arg: bits 0-15 block count
 *	16-31: 0
 * response: R1
 * STATE: suspect tranfer because before read/write mult 
 * Set block count for immediately following CMD 18 (read mult.) (write mult ????)
 * ---------------------------------------------------------------------------------
 */
static int
mmc_set_block_count(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->state != STATE_TRANSFER) {
		fprintf(stderr, "MMCard: set_block_count in wrong state %d\n", card->state);
		return MMC_ERR_TIMEOUT;
	}
	if (card->spec_vers < CSD_SPEC_VER_3) {
		return MMC_ERR_TIMEOUT;
	}
	fprintf(stderr, "MMCard: Warning: set block count %d is ignored\n", arg);
	card->card_status &= ~(0xfd3fc020);
	card->block_count = arg & 0xffff;	/* upper bits of arg should be 0 */
	card->set_block_count_time = card->cmdcount;
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

/*
 * ------------------------------------------------------------------------
 * CMD24 MMC_WRITE_SINGLE_BLOCK
 *	arg: address
 *	resp: R1
 *	STATE_TRANSFER -> STATE_RCV	
 * ------------------------------------------------------------------------
 */
static int
mmc_write_single_block(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int state = card->state;
	uint32_t card_status = GET_STATUS(card);
	if (!card->spimode && (state != STATE_TRANSFER)) {
		fprintf(stderr, "MMC WRITE single block: in wrong state %d\n", state);
		return MMC_ERR_TIMEOUT;
	}
	/* Store the attributes for the recognized data operation */
	card->cmd = MMC_WRITE_SINGLE_BLOCK;
	if (card->ocr & OCR_CCS) {
		card->address = ((uint64_t) arg) << 9;
	} else {
		card->address = arg & ~(card->blocklen - 1);
	}
	card->transfer_count = 0;
	card->well_written_blocks = 0;

	card->state = STATE_RCV;
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

static int
spi_write_single_block(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = mmc_write_single_block(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return result;
}

/*
 * ------------------------------------------------------------------------
 * CMD25 MMC_WRITE_MULTIPLE_BLOCK
 *	arg: address
 *	resp: R1
 *	STATE_TRANSFER -> STATE_RCV	
 * ------------------------------------------------------------------------
 */
static int
mmc_write_multiple_block(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int state = card->state;
	uint32_t card_status = GET_STATUS(card);
	if (!card->spimode && (state != STATE_TRANSFER)) {
		fprintf(stderr, "MMC write multiple block: in wrong state %d\n", state);
		return MMC_ERR_TIMEOUT;
	}
	/* Store the attributes for the recognized data operation */
	card->cmd = MMC_WRITE_MULTIPLE_BLOCK;
	if (card->ocr & OCR_CCS) {
		card->address = ((uint64_t) arg) << 9;
	} else {
		card->address = arg & ~(card->blocklen - 1);
	}
	card->transfer_count = 0;
	card->well_written_blocks = 0;
	if ((card->set_block_count_time + 1) != card->cmdcount) {
		card->block_count = 0;	/* means infinite */
	}
	card->state = STATE_RCV;
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

static int
spi_write_multiple_block(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = mmc_write_multiple_block(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return result;
}

/* 
 * ----------------------------------------------------------------------------
 * CMD26 PROGRAM_CID 
 * The argument is don't care.
 * Type ADTC (Addressed Data Transfer Command)
 * Response R1 
 * ----------------------------------------------------------------------------
 */
static int
mmc_program_cid(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int state = card->state;
	uint32_t card_status = GET_STATUS(card);
	if (!card->spimode && (state != STATE_TRANSFER)) {
		fprintf(stderr, "MMC WRITE single block: in wrong state %d\n", state);
		return MMC_ERR_TIMEOUT;
	}
	/* Store the attributes for the recognized data operation */
	card->cmd = MMC_PROGRAM_CID;
	card->transfer_count = 0;

	card->state = STATE_RCV;
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	fprintf(stderr, "MMCard: Reserved command PROGRAM_CID\n");
	return MMC_ERR_NONE;
}

/* 
 * ----------------------------------------------------------------------------
 * CMD26 PROGRAM_CSD 
 * reserved for manufacturer
 * ----------------------------------------------------------------------------
 */
static int
mmc_program_csd(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int state = card->state;
	uint32_t card_status = GET_STATUS(card);
	if (!card->spimode && (state != STATE_TRANSFER)) {
		fprintf(stderr, "MMC WRITE single block: in wrong state %d\n", state);
		return MMC_ERR_TIMEOUT;
	}
	/* Store the attributes for the recognized data operation */
	card->cmd = MMC_PROGRAM_CSD;
	card->transfer_count = 0;

	card->state = STATE_RCV;
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	fprintf(stderr, "MMCard: Reserved command PROGRAMM_CSD\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * -------------------------------------------------------------------------------
 * CMD28 SET_WRITE_PROT
 * arg: address of sector group
 * Response R1b
 * Set writeprotect for sector group. Group and sector size is stored in CSD
 * -------------------------------------------------------------------------------
 */
static int
mmc_set_write_prot(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->len = 0;
	fprintf(stderr, "SET WRITE PROT not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ---------------------------------------------------------------------------
 * CMD29 CLR_WRITE_PROT
 * arg: address of sector group
 * Response R1b
 * ---------------------------------------------------------------------------
 */
static int
mmc_clr_write_prot(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->len = 0;
	fprintf(stderr, "CLR_WRITE_PROT\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ---------------------------------------------------------------------------
 * CMD30 SEND_WRITE_PROT
 * Response Format R1
 * Send the write protection of 32 groups on the data lines 
 * ---------------------------------------------------------------------------
 */
static int
mmc_send_write_prot(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->len = 0;
	fprintf(stderr, "cmd 0x%02x not implemented\n", cmd);
	return MMC_ERR_TIMEOUT;
}

/*
 * -----------------------------------------------------------------------
 * CMD32: MMC_ERASE_WR_BLK_START  
 * Response Format R1
 * STATE: Tran -> Tran 
 * -----------------------------------------------------------------------
 */
static int
mmc_erase_wr_blk_start(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (!card->spimode && (card->state != STATE_TRANSFER)) {
		fprintf(stderr, "Erase WR blk start not in state transfer\n");
		return MMC_ERR_TIMEOUT;
	}
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	if (card->ocr & OCR_CCS) {
		card->erase_start = ((uint64_t) arg) << 9;
	} else {
		card->erase_start = arg;
	}
	fprintf(stderr, "erase start %lld\n", (long long)card->erase_start);
	return MMC_ERR_NONE;
}

static int
spi_erase_wr_blk_start(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = mmc_erase_wr_blk_start(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return result;
}

/* 
 * -------------------------------------------------------------------------------
 * CMD33: MMC_ERASE_WR_BLK_END
 * Response format R1
 * State Transfer -> Transfer
 * -------------------------------------------------------------------------------
 */
static int
mmc_erase_wr_blk_end(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{

	uint32_t card_status = GET_STATUS(card);
	if (!card->spimode && (card->state != STATE_TRANSFER)) {
		fprintf(stderr, "Erase WR blk end not in state transfer\n");
		return MMC_ERR_TIMEOUT;
	}
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	if (card->ocr & OCR_CCS) {
		card->erase_end = ((uint64_t) arg) << 9;
	} else {
		card->erase_end = arg;
	}
	fprintf(stderr, "erase end %lld\n", (long long)card->erase_end);
	return MMC_ERR_NONE;
}

/**
 **************************************************************************************************
 * \fn static int spi_erase_wr_blk_end(MMCard *card,uint32_t cmd,uint32_t arg,MMCResponse *resp) 
 **************************************************************************************************
 */
static int
spi_erase_wr_blk_end(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = mmc_erase_wr_blk_end(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return result;
}

/*
 * ------------------------------------------------------------------------
 * CMD35 erase group start MMC only !
 * arg: address of first write block to be erased (erase group ???)
 * Response R1
 * State Transfer -> Transfer 
 * ------------------------------------------------------------------------
 */
static int
mmc_erase_group_start(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->state != STATE_TRANSFER) {
		fprintf(stderr, "Erase not in state transfer\n");
		return MMC_ERR_TIMEOUT;
	}
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	card->erase_start = arg;
	return MMC_ERR_NONE;
}

/*
 * ----------------------------------------------------------------------
 * CMD36 ERASE_GROUP_END (MMC Only ?)
 * arg: address of last write block to be erased 
 * State: Transfer -> Transfer
 * Response R1
 * ----------------------------------------------------------------------
 */
static int
mmc_erase_group_end(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	card->card_status &= ~(0xfd3fc020);
	if (card->state != STATE_TRANSFER) {
		return MMC_ERR_TIMEOUT;
	}
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	card->erase_end = arg;
	return MMC_ERR_NONE;
}

/*
 * ---------------------------------------------------------------------------
 * CMD38 ERASE
 * This command should be reimplemented to work in the Background !
 * erase previously selected blocks
 * arg: none
 * State Transfer -> PRG -> (some time) Transfer
 * Response R1b (R1 with busy on data line)
 * ---------------------------------------------------------------------------
 */
static int
mmc_erase(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->len = 0;
	uint32_t card_status = GET_STATUS(card);
	uint64_t start = card->erase_start;
	uint64_t end = card->erase_end | (card->blocklen - 1);
	uint8_t buf[256];
	if (start > card->capacity) {
		start = card->capacity;
	}
	if (end > card->capacity) {
		fprintf(stderr, "Warning: erasing past end of card\n");
		end = card->capacity;
	}
	if ((end - start) > 256 * 1024 * 1024) {
		fprintf(stderr, "Warning: Erase (CMD%d) should be implemented in Background\n",
			cmd);
	}
	memset(buf, 0xff, sizeof(buf));
	while (start < end) {
		uint64_t count = end - start;
		if (count > sizeof(buf)) {
			count = sizeof(buf);
		}
		if (DiskImage_Write(card->disk_image, start, buf, count) < count) {
			fprintf(stderr, "Writing to diskimage failed\n");
			break;
		}
		start += count;
	}
	card->card_status &= ~(0xfd3fc020);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	fprintf(stderr, "erase done\n");
	return MMC_ERR_NONE;
}

static int
spi_erase(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = mmc_erase(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return result;
}

static int
mmc_fast_io(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->len = 0;
	fprintf(stderr, "cmd 0x%02x not implemented\n", cmd);
	return MMC_ERR_TIMEOUT;
}

static int
mmc_go_irq_state(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->len = 0;
	fprintf(stderr, "cmd 0x%02x not implemented\n", cmd);
	return MMC_ERR_TIMEOUT;
}

/*
 * ----------------------------------------------------------------------
 * CMD42: MMC_LOCK_UNLOCK
 * Response format R1
 * Arg is 0.
 * State: Trans -> RCV (Has a data stage)
 * ----------------------------------------------------------------------
 */
static int
mmc_lock_unlock(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "MMC lock/unlock not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ----------------------------------------------------------
 * CMD55 app_cmd
 * Response format R1
 * ----------------------------------------------------------
 */
static int
mmc_app_cmd(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card) | STATUS_APP_CMD;
	uint16_t rca = (arg >> 16) & 0xffff;
	/* behaviour for rca == 0 ???? */
	if ( /*(rca != 0) && */ (rca != card->rca)) {
		dbgprintf("MMC APP CMD: card not selected\n");
		fprintf(stderr, "MMC APP CMD: card not selected, rca %d instead of %d\n", rca,
			card->rca);
		return MMC_ERR_TIMEOUT;
	}
	card->card_status &= ~(0xfd3fc020);
	card->is_app_cmd = 1;
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

/**
 ****************************************************************************************
 * \fn static int spi_app_cmd(MMCard *card,uint32_t cmd,uint32_t arg,MMCResponse *resp) 
 ****************************************************************************************
 */
static int
spi_app_cmd(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = mmc_app_cmd(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return result;
}

/*
 * ------------------------------------------------------------------------
 * CMD56 GEN_CMD
 * ------------------------------------------------------------------------
 */
static int
mmc_gen_cmd(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->len = 0;
	fprintf(stderr, "MMC_GEN_CMD 0x%02x not implemented\n", cmd);
	return MMC_ERR_TIMEOUT;
}

/*
 * ----------------------------------------------------------
 * CMD58 READ_OCR
 * Response format R3
 * State: ?????
 * Argument 0-31 stuff bits
 * ----------------------------------------------------------
 */
static int
mmc_read_ocr(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->data[0] = 0x3f;
	resp->data[1] = card->ocr >> 24;
	resp->data[2] = card->ocr >> 16;
	resp->data[3] = card->ocr >> 8;
	resp->data[4] = card->ocr;
	resp->data[5] = 0xff;
	resp->len = 6;
	return MMC_ERR_NONE;
}

/*
 *******************************************
 * CMD58 SPI version
 * Response format SPI-R3
 *******************************************
 */
static int
spi_read_ocr(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	resp->data[1] = card->ocr >> 24;
	resp->data[2] = card->ocr >> 16;
	resp->data[3] = card->ocr >> 8;
	resp->data[4] = card->ocr;
	resp->len = 5;
	return MMC_ERR_NONE;
}

/*
 *******************************************
 * CMD59 CRC on/off
 * Response format R1 
 *******************************************
 */
static int
mmc_crc_on_off(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	card->crc_on = arg & 1;
	return MMC_ERR_NONE;
}

static int
spi_crc_on_off(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	resp->len = 1;
	card->crc_on = arg & 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return MMC_ERR_NONE;
}

/*
 * ----------------------------------------------------------------------------
 * ACMD6 SD_APP_SET_BUS_WIDTH
 * arg: 00 = 1 Bit, 10 = 4Bits
 * ----------------------------------------------------------------------------
 */
static int
sd_app_set_bus_width(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{

	uint32_t card_status = GET_STATUS(card) | STATUS_APP_CMD;
	if (card->type != CARD_TYPE_SD) {
		fprintf(stderr, "SD_APP_SET_BUS_WIDTH: Not an SD-Card !\n");
		return MMC_ERR_TIMEOUT;
	}
	if (card->state != STATE_TRANSFER) {
		fprintf(stderr, "MMCard: SD_APP_BUS_WIDTH in state %d\n", card->state);
		return MMC_ERR_TIMEOUT;
	}
	card->card_status &= ~(0xfd3fc020);
	COPYBITS(arg & 3, card->ssr, 510, 511);
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	return MMC_ERR_NONE;
}

/*
 * ------------------------------------------------------------------------
 * ACMD18: SECURE_READ_MULTI_BLOCK
 * Argument: ignored
 * Response: R1
 * State: Transfer -> Data ?
 * ------------------------------------------------------------------------
 */

static int
sd_app_secure_read_multi_block(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD18: SECURE_READ_MULTI_BLOCK is not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * -------------------------------------------------------------------------
 * ACMD22
 * SD_APP_SEND_NUM_WR_BLKS
 * Respond with the number of well written blocks
 * Response format R1 with 4 byte data phase
 * state: Not documented but suspect transfer -> data 
 * -------------------------------------------------------------------------
 */
static int
sd_app_send_num_wr_blks(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->type != CARD_TYPE_SD) {
		fprintf(stderr, "SD_APP_SEND_SCR: Not an SD-Card !\n");
		return MMC_ERR_TIMEOUT;
	}
#if 1
	if (card->state != STATE_TRANSFER) {
		fprintf(stderr, "send_num_wr_blks in wrong state\n");
		return MMC_ERR_TIMEOUT;
	}
#endif
	card->transfer_count = 0;
	card->state = STATE_DATA;
	card->card_status &= ~(0xfd3fc020);

	/* Store the attributes for the recognized data operation */
	card->cmd = cmd | CMD_FLAG_IS_APPCMD;

	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	/* resp->len = 0; */
	MMCard_StartTransmission(card);
	return MMC_ERR_NONE;
}

static int
spi_app_send_num_wr_blks(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = sd_app_send_num_wr_blks(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return result;
}

/*
 * -------------------------------------------------------------------------
 * ACMD23
 * SD_APP_SET_WR_BLK_ERASE_COUNT
 * Currently this command is ignored
 * state ????
 * -------------------------------------------------------------------------
 */
static int
sd_app_set_wr_blk_erase_count(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->type != CARD_TYPE_SD) {
		fprintf(stderr, "SD_APP_SEND_SCR: Not an SD-Card !\n");
		return MMC_ERR_TIMEOUT;
	}
#if 0
	if (card->state != STATE_TRANSFER) {
		fprintf(stderr, "send_num_wr_blks in wrong state\n");
		return MMC_ERR_TIMEOUT;
	}
#endif
	card->card_status &= ~(0xfd3fc020);

	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	resp->len = 0;
	return MMC_ERR_NONE;

}

/*
 * ----------------------------------------------------------------------
 * ACMD25: SECURE_WRITE_MULT_BLOCK
 * Argument: ignored
 * Response Format: R1
 * State: Transfer -> RCV ?
 * ----------------------------------------------------------------------
 */
static int
sd_app_secure_write_mult_block(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD25: SECURE_WRITE_MULT_BLOCK not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ----------------------------------------------------------------------
 * ACMD26: SECURE_WRITE_MKB
 * Argument: ignored 
 * Response Format: R1
 * State: Transfer -> RCV ?
 * ----------------------------------------------------------------------
 */
static int
sd_app_secure_write_mkb(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD26: SECURE_WRITE_MKB not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ---------------------------------------------------------------------
 * ACMD38: SECURE_ERASE
 * ---------------------------------------------------------------------
 */
static int
sd_app_secure_erase(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD38: SECURE_ERASE not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ----------------------------------------------------------------------
 * SD app op condition ACMD41 (0x29)
 * Response format R3
 * STATE_IDLE -> STATE_READY when voltage good and card not busy else
 * go to inactive
 * ----------------------------------------------------------------------
 */
static int
sd_app_op_cond(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int ccs = !!(card->ocr & OCR_CCS);	/* Card Capacity status */
	int hcs = !!(arg & (1 << 30));	/* Host capacity status */
	resp->len = 0;
	if (!(arg & card->ocr & OCR_VOLT_MSK)) {
		fprintf(stderr, "Warning: OCR Voltage not matching, arg %08x ocr %08x\n", arg,
			card->ocr);
	}
	if (card->type != CARD_TYPE_SD) {
		fprintf(stderr, "SD_AP_OP_COND for non SD-Card\n");
		return MMC_ERR_TIMEOUT;
	}
	if (card->state != STATE_IDLE) {
		resp->len = 0;
		fprintf(stderr, "SD-Card: SD_AP_OP_COND when not in IDLE state (%d)\n",
			card->state);
		return MMC_ERR_TIMEOUT;
	}
	/* Do not start the reset if card is version 2.0 but host not 2.0 */
	if (ccs && !card->host_is_2_0) {
		fprintf(stderr, "SD-AppOpCond: Host is not 2.0\n");
		card->ocr &= ~OCR_NOTBUSY;
	} else if (ccs && !hcs) {
		fprintf(stderr, "SD-AppOpCond: Host did not enable High Capacity mode : 0x%08x\n",
			arg);
		card->ocr &= ~OCR_NOTBUSY;
	} else if (card->reset_state == RST_NOT_STARTED) {
		card->reset_start_time = CycleCounter_Get();
		card->ocr &= ~OCR_NOTBUSY;
		card->reset_state = RST_STARTED;
	} else if (card->reset_state == RST_STARTED) {
		int64_t usec = CyclesToMicroseconds(CycleCounter_Get() - card->reset_start_time);
		if (usec > card->usec_reset) {
			card->state = STATE_READY;
			card->ocr |= OCR_NOTBUSY;
			card->reset_state = RST_DONE;
		}
	} else if (card->reset_state != RST_DONE) {
		fprintf(stderr, "Emulator bug: MMC-Card reset_state %d not valid\n",
			card->reset_state);
		exit(1);
	}
	// init card is missing here, need some time, return busy the first few times
	resp->data[0] = 0x3f;
	resp->data[1] = card->ocr >> 24;
	resp->data[2] = card->ocr >> 16;
	resp->data[3] = card->ocr >> 8;
	resp->data[4] = card->ocr;
	resp->data[5] = 0xff;
	resp->len = 6;
	dbgprintf("SD app op cond arg 0x%08x, ocr %08x\n", arg, card->ocr);
	return MMC_ERR_NONE;
}

static int
spi_app_op_cond(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = sd_app_op_cond(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return result;
}

/*
 * ---------------------------------------------------------------------------
 * ACMD42: SD_APP_SET_CLR_CARD_DETECT
 * State: Trans -> Trans
 * Response format R1
 * ---------------------------------------------------------------------------
 */
static int
sd_app_set_clr_card_detect(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->state != STATE_TRANSFER) {
		fprintf(stderr, "SET_CLR_CARD_DETECT in wrong state\n");
		return MMC_ERR_TIMEOUT;
	}
	if (arg & 1) {
		dbgprintf("Connect Pullup resistor\n");
	} else {
		dbgprintf("Disconnect Pullup resistor\n");
	}
	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = 1;	/* CRC missing here */
	return MMC_ERR_NONE;
}

/*
 * ------------------------------------------------------------------
 * ACMD43: GET_MKB
 * Response Format R1
 * Argument [0:15] UNIT OFFSET
 *	    [16:23] MKB ID
 *	    [24:31] UNIT COUNT
 * State: transfer -> data ?
 * ------------------------------------------------------------------
 */
static int
sd_app_get_mkb(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD43 GET_MKB: not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * -------------------------------------------------------------------
 * ACMD44: GET_MID
 * Response Format R1
 * Argument: ignored
 * State: transfer -> data ?
 * -------------------------------------------------------------------
 */
static int
sd_app_get_mid(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD44 GET_MID: not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ------------------------------------------------------------------
 * ACMD45: SET_CER_RN1
 * Response format R1
 * Argument: ignored
 * State: Transfer -> RCV ?
 * ------------------------------------------------------------------
 */
static int
sd_app_set_cer_rn1(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD45 SET_CER_RN1: not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * -------------------------------------------------------------------
 * ACMD46: GET_CER_RN2
 * Response format R1
 * Argument: ignored
 * State: Transfer -> Data ?
 * -------------------------------------------------------------------
 */

static int
sd_app_get_cer_rn2(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD46 GET_CER_RN2: not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ------------------------------------------------------------------
 * ACMD47: SET_CER_RES2
 * Response Format R1
 * Argument: ignored
 * State: Transfer -> RCV ?
 * ------------------------------------------------------------------
 */
static int
sd_app_set_cer_res2(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD47: SET_CER_RES2 not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ------------------------------------------------------------------
 * ACMD48: GET_CER_RES1
 * Response Format R1
 * Argument: ignored
 * State: Transfer -> Data
 * ------------------------------------------------------------------
 */
static int
sd_app_get_cer_res1(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD47: GET_CER_RES1 not implemented\n");
	return MMC_ERR_TIMEOUT;

}

/*
 * ------------------------------------------------------------------
 * ACMD49: Change Secure Area
 * Response Format R1b
 * Argument: ignored
 * State: ??
 * ------------------------------------------------------------------
 */
static int
sd_app_change_secure_area(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	fprintf(stderr, "ACMD49: Change Secure Area not implemented\n");
	return MMC_ERR_TIMEOUT;
}

/*
 * ------------------------------------------------------------------
 * ACMD51: SEND SCR
 * Response format R1, dummy argument, goes to data state
 * State: Transfer -> Data
 * ------------------------------------------------------------------
 */
static int
sd_app_send_scr(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	uint32_t card_status = GET_STATUS(card);
	if (card->type != CARD_TYPE_SD) {
		fprintf(stderr, "SD_APP_SEND_SCR: Not an SD-Card !\n");
		return MMC_ERR_TIMEOUT;
	}
	if (!card->spimode && (card->state != STATE_TRANSFER)) {
		fprintf(stderr, "Send scr in wrong state\n");
		return MMC_ERR_TIMEOUT;
	}
	card->transfer_count = 0;
	card->state = STATE_DATA;
	card->card_status &= ~(0xfd3fc020);

	/* Store the attributes for the recognized data operation */
	card->cmd = cmd | CMD_FLAG_IS_APPCMD;

	resp->len = 6;
	resp->data[0] = cmd & 0x3f;
	resp->data[1] = (card_status >> 24) & 0xff;
	resp->data[2] = (card_status >> 16) & 0xff;
	resp->data[3] = (card_status >> 8) & 0xff;
	resp->data[4] = card_status & 0xff;
	resp->data[5] = MMC_RespCRCByte(resp);
	/* put data to somewhere */
	MMCard_StartTransmission(card);
	return MMC_ERR_NONE;
}

/**
 *************************************************************************
 * \fn static int spi_app_send_scr(MMCard *card,uint32_t cmd,uint32_t arg,MMCResponse *resp) 
 *************************************************************************
 */
static int
spi_app_send_scr(MMCard * card, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = sd_app_send_scr(card, cmd, arg, resp);
	resp->len = 1;
	if (card->state == STATE_IDLE) {
		resp->data[0] = SPIR1_IDLE;
	} else {
		resp->data[0] = 0;
	}
	return result;
}

#if 0
static void
MMC_VerifyDataCrc(MMCDev * dev, const uint8_t buf, int count)
{
	MMCard *card = container_of(dev, MMCard, mmcdev);
	if (!card) {
		return 0;
	}
return}
#endif

/*
 * ------------------------------------------------------------------
 * MMCard_Write
 * The Data Phase of a command
 * retval: number of Bytes, -errcode on error
 * Busy polling implenentation is missing
 * ------------------------------------------------------------------
 */
int
MMCard_Write(MMCDev * dev, const uint8_t * buf, int count)
{
	MMCard *card;
	if (!dev) {
		return 0;
	}
	card = container_of(dev, MMCard, mmcdev);
	if (card->state != STATE_RCV) {
		fprintf(stderr, "MMCard_Write: Card not in RCV: %d\n", card->state);
		return 0;
	}
	if (card->cmd == MMC_WRITE_SINGLE_BLOCK) {
		uint32_t address = card->address + card->transfer_count;
		if (card->transfer_count + count > card->blocklen) {
			count = card->blocklen - card->transfer_count;
			if (count < 0) {
				fprintf(stderr, "transfer count < 0 should never happen\n");
				return 0;
			}
		}
		if ((address + count) > card->capacity) {
			count = card->capacity - address;
		}
		if (DiskImage_Write(card->disk_image, address, buf, count) < count) {
			fprintf(stderr, "MMCard: Error writing to diskimage\n");
		}
		card->transfer_count += count;
		if (card->transfer_count == card->blocklen) {
			card->state = STATE_TRANSFER;
			card->well_written_blocks = 1;
		}
		return count;
	} else if (card->cmd == MMC_WRITE_MULTIPLE_BLOCK) {
		uint32_t address = card->address + card->transfer_count;
		if ((address + count) > card->capacity) {
			count = card->capacity - address;
		}
		if (DiskImage_Write(card->disk_image, address, buf, count) < count) {
			fprintf(stderr, "MMCard: Error writing to diskimage\n");
		}
		card->transfer_count += count;
		if ((address & ~(card->blocklen - 1)) !=
		    ((address + count) & ~(card->blocklen - 1))) {
			if (card->block_count) {
				card->block_count--;
				if (!card->block_count) {
					card->state = STATE_TRANSFER;
				}
			}
			if (card->blocklen) {
				card->well_written_blocks = card->transfer_count / card->blocklen;
			}
		}
		return count;
	} else if (card->cmd == MMC_WRITE_DAT_UNTIL_STOP) {
		uint32_t address = card->address + card->transfer_count;
		if ((address + count) > card->capacity) {
			count = card->capacity - address;
		}
		if (DiskImage_Write(card->disk_image, address, buf, count) < count) {
			fprintf(stderr, "MMCard: Error writing to diskimage\n");
		}
		card->transfer_count += count;
		if (card->blocklen) {
			card->well_written_blocks = card->transfer_count / card->blocklen;
		}
		return count;
	} else if (card->cmd == MMC_PROGRAM_CSD) {
		int i;
		for (i = 0; (i < count) && (card->transfer_count < 16); i++) {
			card->dataAssBuf[card->transfer_count] = buf[i];
			++card->transfer_count;
		}
		if (card->transfer_count == 16) {
			//card->data_crc = bla;
			++card->transfer_count;
			++i;
		} else if (card->transfer_count == 17) {
			//card->data_crc |= blub;
			++card->transfer_count;
			++i;
			card->state = STATE_TRANSFER;
		}
		return i;
	} else if (card->cmd == MMC_PROGRAM_CID) {
		int i;
		card->transfer_count += count;
		for (i = 0; (i < count) && (card->transfer_count < 16); i++) {
			card->dataAssBuf[card->transfer_count] = buf[i];
			++card->transfer_count;
		}
		/* Warning, CRC transmission by master not yet active ! 
		 * So this will not work currently 
		 */
		if ((card->transfer_count == 16) && (i < count)) {
			//card->data_crc = bla;
			++card->transfer_count;
			++i;
		}
		if ((card->transfer_count == 17) && (i < count)) {
			//card->data_crc |= blub;
			++card->transfer_count;
			++i;
			card->state = STATE_TRANSFER;
		}
		return i;
	} else {
		fprintf(stderr, "Write cmd %d not known\n", card->cmd);
		return 0;
	}
}

/*
 * -------------------------------------------------------------------------
 * MMCardInitCmds
 * 	Fill the pointer tables for the Command Procs
 * -------------------------------------------------------------------------
 */
static void
MMCardInitCmds(MMCard * card)
{
	int ccc = GETBITS(card->csd, 84, 95);
	if (ccc & CCC_BASIC) {
		card->cmdProc[MMC_GO_IDLE_STATE] = mmc_go_idle;
		card->cmdProc[MMC_SEND_OP_COND] = mmc_send_op_cond;
		card->cmdProc[MMC_ALL_SEND_CID] = mmc_all_send_cid;
		card->cmdProc[MMC_SET_RELATIVE_ADDR] = mmc_set_relative_addr;
		card->cmdProc[MMC_SET_DSR] = mmc_set_dsr;
		card->cmdProc[MMC_SELECT_CARD] = mmc_select_card;
		card->cmdProc[MMC_SEND_CSD] = mmc_send_csd;
		card->cmdProc[MMC_SEND_CID] = mmc_send_cid;
	}
	if (ccc & CCC_STREAM_READ) {
		card->cmdProc[MMC_READ_DAT_UNTIL_STOP] = mmc_read_dat_until_stop;
	}
	if (ccc & CCC_BASIC) {
		card->cmdProc[MMC_STOP_TRANSMISSION] = mmc_stop_transmission;
		card->cmdProc[MMC_SEND_STATUS] = mmc_send_status;
		card->cmdProc[MMC_GO_INACTIVE_STATE] = mmc_go_inactive_state;
	}
	if (ccc & (CCC_BLOCK_READ | CCC_BLOCK_WRITE)) {
		card->cmdProc[MMC_SET_BLOCKLEN] = mmc_set_blocklen;
	}
	if (ccc & CCC_BLOCK_READ) {
		card->cmdProc[MMC_READ_SINGLE_BLOCK] = mmc_read_single_block;
		card->cmdProc[MMC_READ_MULTIPLE_BLOCK] = mmc_read_multiple_block;
	}
	if (ccc & CCC_STREAM_WRITE) {
		card->cmdProc[MMC_WRITE_DAT_UNTIL_STOP] = mmc_write_dat_until_stop;
	}
	if (ccc & (CCC_BLOCK_READ | CCC_BLOCK_WRITE)) {
		card->cmdProc[MMC_SET_BLOCK_COUNT] = mmc_set_block_count;
	}
	if (ccc & CCC_BLOCK_WRITE) {
		card->cmdProc[MMC_WRITE_SINGLE_BLOCK] = mmc_write_single_block;
		card->cmdProc[MMC_WRITE_MULTIPLE_BLOCK] = mmc_write_multiple_block;
		card->cmdProc[MMC_PROGRAM_CID] = mmc_program_cid;
		card->cmdProc[MMC_PROGRAM_CSD] = mmc_program_csd;
	}
	if (ccc & CCC_WRITE_PROT) {
		card->cmdProc[MMC_SET_WRITE_PROT] = mmc_set_write_prot;
		card->cmdProc[MMC_CLR_WRITE_PROT] = mmc_clr_write_prot;
		card->cmdProc[MMC_SEND_WRITE_PROT] = mmc_send_write_prot;
	}
	if (ccc & CCC_ERASE) {
		card->cmdProc[MMC_ERASE_GROUP_START] = mmc_erase_group_start;
		card->cmdProc[MMC_ERASE_GROUP_END] = mmc_erase_group_end;
		card->cmdProc[MMC_ERASE] = mmc_erase;
	}
	if (ccc & CCC_IO_MODE) {
		card->cmdProc[MMC_FAST_IO] = mmc_fast_io;
		card->cmdProc[MMC_GO_IRQ_STATE] = mmc_go_irq_state;
	}
	if (ccc & CCC_LOCK_CARD) {
		card->cmdProc[MMC_LOCK_UNLOCK] = mmc_lock_unlock;
	}
	if (ccc & CCC_APP_SPEC) {
		card->cmdProc[MMC_APP_CMD] = mmc_app_cmd;
		card->cmdProc[MMC_GEN_CMD] = mmc_gen_cmd;
	}
	card->cmdProc[MMC_READ_OCR] = mmc_read_ocr;
}

static void
MMPlusCardInitCmds(MMCard * card)
{
	int ccc = GETBITS(card->csd, 84, 95);
	if (ccc & CCC_BASIC) {
		card->cmdProc[MMC_GO_IDLE_STATE] = mmc_go_idle;
		card->cmdProc[MMC_SEND_OP_COND] = mmc_send_op_cond;
		card->cmdProc[MMC_ALL_SEND_CID] = mmc_all_send_cid;
		card->cmdProc[MMC_SET_RELATIVE_ADDR] = mmc_set_relative_addr;
		card->cmdProc[MMC_SET_DSR] = mmc_set_dsr;
		card->cmdProc[MMC_SELECT_CARD] = mmc_select_card;
		card->cmdProc[MMC_SEND_EXT_CSD] = mmcplus_send_ext_csd;
		card->cmdProc[MMC_SEND_CSD] = mmc_send_csd;
		card->cmdProc[MMC_SEND_CID] = mmc_send_cid;
		card->cmdProc[MMC_CRC_ON_OFF] = mmc_crc_on_off;
	}
	if (ccc & CCC_SWITCH) {
		card->cmdProc[MMC_SWITCH] = mmc_switch;	/* only mmcplus */
	}
	if (ccc & CCC_STREAM_READ) {
		card->cmdProc[MMC_READ_DAT_UNTIL_STOP] = mmc_read_dat_until_stop;
	}
	if (ccc & CCC_BASIC) {
		card->cmdProc[MMC_STOP_TRANSMISSION] = mmc_stop_transmission;
		card->cmdProc[MMC_SEND_STATUS] = mmc_send_status;
		card->cmdProc[MMC_GO_INACTIVE_STATE] = mmc_go_inactive_state;
	}
	card->cmdProc[MMC_BUSTEST_R] = mmcplus_bustest_r;	// mmcplus
	card->cmdProc[MMC_BUSTEST_W] = mmcplus_bustest_w;
	if (ccc & (CCC_BLOCK_READ | CCC_BLOCK_WRITE)) {
		card->cmdProc[MMC_SET_BLOCKLEN] = mmc_set_blocklen;
	}
	if (ccc & CCC_BLOCK_READ) {
		card->cmdProc[MMC_READ_SINGLE_BLOCK] = mmc_read_single_block;
		card->cmdProc[MMC_READ_MULTIPLE_BLOCK] = mmc_read_multiple_block;
	}
	if (ccc & CCC_STREAM_WRITE) {
		card->cmdProc[MMC_WRITE_DAT_UNTIL_STOP] = mmc_write_dat_until_stop;
	}
	if (ccc & (CCC_BLOCK_READ | CCC_BLOCK_WRITE)) {
		card->cmdProc[MMC_SET_BLOCK_COUNT] = mmc_set_block_count;
	}
	if (ccc & CCC_BLOCK_WRITE) {
		card->cmdProc[MMC_WRITE_SINGLE_BLOCK] = mmc_write_single_block;
		card->cmdProc[MMC_WRITE_MULTIPLE_BLOCK] = mmc_write_multiple_block;
		card->cmdProc[MMC_PROGRAM_CID] = mmc_program_cid;
		card->cmdProc[MMC_PROGRAM_CSD] = mmc_program_csd;
	}
	if (ccc & CCC_WRITE_PROT) {
		card->cmdProc[MMC_SET_WRITE_PROT] = mmc_set_write_prot;
		card->cmdProc[MMC_CLR_WRITE_PROT] = mmc_clr_write_prot;
		card->cmdProc[MMC_SEND_WRITE_PROT] = mmc_send_write_prot;
	}
	if (ccc & CCC_ERASE) {
		card->cmdProc[MMC_ERASE_GROUP_START] = mmc_erase_group_start;
		card->cmdProc[MMC_ERASE_GROUP_END] = mmc_erase_group_end;
		card->cmdProc[MMC_ERASE] = mmc_erase;
	}
	if (ccc & CCC_IO_MODE) {
		card->cmdProc[MMC_FAST_IO] = mmc_fast_io;
		card->cmdProc[MMC_GO_IRQ_STATE] = mmc_go_irq_state;
	}
	if (ccc & CCC_LOCK_CARD) {
		card->cmdProc[MMC_LOCK_UNLOCK] = mmc_lock_unlock;
	}
	if (ccc & CCC_APP_SPEC) {
		card->cmdProc[MMC_APP_CMD] = mmc_app_cmd;
		card->cmdProc[MMC_GEN_CMD] = mmc_gen_cmd;
	}
	card->cmdProc[MMC_READ_OCR] = mmc_read_ocr;
}

static void
SDCardInitCmds(MMCard * card)
{
	int ccc = GETBITS(card->csd, 84, 95);
	if (ccc & CCC_BASIC) {
		card->cmdProc[MMC_GO_IDLE_STATE] = mmc_go_idle;	/* CMD0 */
		card->cmdProc[MMC_ALL_SEND_CID] = mmc_all_send_cid;	/* CMD2 */
		card->cmdProc[SD_SEND_RELATIVE_ADDR] = sd_send_relative_addr;
		card->cmdProc[MMC_SET_DSR] = mmc_set_dsr;
		card->cmdProc[MMC_SEND_OP_COND] = mmc_send_op_cond;	/* CMD1 really available ? */
	}
	if (ccc & CCC_SWITCH) {
		card->cmdProc[MMC_SWITCH] = mmc_switch;
	}
	if (ccc & CCC_BASIC) {
		card->cmdProc[MMC_SELECT_CARD] = mmc_select_card;
		card->cmdProc[SD_SEND_INTERFACE_COND] = sd_send_interface_cond;
		card->cmdProc[MMC_SEND_CSD] = mmc_send_csd;
		card->cmdProc[MMC_SEND_CID] = mmc_send_cid;
		card->cmdProc[MMC_STOP_TRANSMISSION] = mmc_stop_transmission;
		card->cmdProc[MMC_SEND_STATUS] = mmc_send_status;
		card->cmdProc[MMC_GO_INACTIVE_STATE] = mmc_go_inactive_state;
		card->cmdProc[MMC_CRC_ON_OFF] = mmc_crc_on_off;
	}
	if (ccc & (CCC_BLOCK_READ | CCC_BLOCK_WRITE | CCC_LOCK_CARD)) {
		card->cmdProc[MMC_SET_BLOCKLEN] = mmc_set_blocklen;
	}
	if (ccc & CCC_BLOCK_READ) {
		card->cmdProc[MMC_READ_SINGLE_BLOCK] = mmc_read_single_block;
		card->cmdProc[MMC_READ_MULTIPLE_BLOCK] = mmc_read_multiple_block;
	}
	if (ccc & CCC_BLOCK_WRITE) {
		card->cmdProc[MMC_WRITE_SINGLE_BLOCK] = mmc_write_single_block;
		card->cmdProc[MMC_WRITE_MULTIPLE_BLOCK] = mmc_write_multiple_block;
		card->cmdProc[MMC_PROGRAM_CID] = mmc_program_cid;	/* reserved for manufacturer */
		card->cmdProc[MMC_PROGRAM_CSD] = mmc_program_csd;
	}
	if (ccc & CCC_WRITE_PROT) {
		card->cmdProc[MMC_SET_WRITE_PROT] = mmc_set_write_prot;
		card->cmdProc[MMC_CLR_WRITE_PROT] = mmc_clr_write_prot;
		card->cmdProc[MMC_SEND_WRITE_PROT] = mmc_send_write_prot;
	}
	if (ccc & CCC_ERASE) {
		card->cmdProc[MMC_ERASE_WR_BLK_START] = mmc_erase_wr_blk_start;
		card->cmdProc[MMC_ERASE_WR_BLK_END] = mmc_erase_wr_blk_end;
		card->cmdProc[MMC_ERASE] = mmc_erase;
	}
	if (ccc & CCC_SWITCH) {
		card->cmdProc[MMC_ERASE_GROUP_START] = mmc_erase_group_start;
		card->cmdProc[MMC_ERASE_GROUP_END] = mmc_erase_group_end;
	}
	if (ccc & CCC_LOCK_CARD) {
		card->cmdProc[MMC_LOCK_UNLOCK] = mmc_lock_unlock;
	}
	if (ccc & CCC_APP_SPEC) {
		card->cmdProc[MMC_APP_CMD] = mmc_app_cmd;
		card->cmdProc[MMC_GEN_CMD] = mmc_gen_cmd;
	}
	/* CMD58 is not mentioned in the CCC specification */
	card->cmdProc[MMC_READ_OCR] = mmc_read_ocr;
	/* The APP cmds */
	if (ccc & CCC_APP_SPEC) {
		card->appCmdProc[SD_APP_SET_BUS_WIDTH] = sd_app_set_bus_width;
		card->appCmdProc[SD_APP_SEND_STATUS] = sd_app_send_status;
		card->appCmdProc[SD_APP_OP_COND] = sd_app_op_cond;
		card->appCmdProc[SD_APP_SEND_NUM_WR_BLKS] = sd_app_send_num_wr_blks;
		card->appCmdProc[SD_APP_SET_WR_BLK_ERASE_COUNT] = sd_app_set_wr_blk_erase_count;
		card->appCmdProc[SD_APP_SET_CLR_CARD_DETECT] = sd_app_set_clr_card_detect;
		card->appCmdProc[SD_APP_SEND_SCR] = sd_app_send_scr;
		/* 
		 * currently I have no hint about the condition under which the secure commands are
		 * available 
		 */
		card->appCmdProc[SD_APP_SECURE_READ_MULTI_BLOCK] = sd_app_secure_read_multi_block;
		card->appCmdProc[SD_APP_SECURE_WRITE_MULT_BLOCK] = sd_app_secure_write_mult_block;
		card->appCmdProc[SD_APP_SECURE_WRITE_MKB] = sd_app_secure_write_mkb;
		card->appCmdProc[SD_APP_SECURE_ERASE] = sd_app_secure_erase;
		card->appCmdProc[SD_APP_GET_MKB] = sd_app_get_mkb;
		card->appCmdProc[SD_APP_GET_MID] = sd_app_get_mid;
		card->appCmdProc[SD_APP_SET_CER_RN1] = sd_app_set_cer_rn1;
		card->appCmdProc[SD_APP_GET_CER_RN2] = sd_app_get_cer_rn2;
		card->appCmdProc[SD_APP_SET_CER_RES2] = sd_app_set_cer_res2;
		card->appCmdProc[SD_APP_GET_CER_RES1] = sd_app_get_cer_res1;
		card->appCmdProc[SD_APP_CHANGE_SECURE_AREA] = sd_app_change_secure_area;
	}

}

void
MMCard_GotoSpi(MMCDev * mmcdev)
{
	MMCard *card = container_of(mmcdev, MMCard, mmcdev);
	int ccc = GETBITS(card->csd, 84, 95);
	int i;
	card->spimode = 1;
	card->crc_on = 0;
	for (i = 0; i < 64; i++) {
		card->cmdProc[i] = 0;
		card->appCmdProc[i] = 0;
	}
	if (ccc & CCC_BASIC) {
		card->cmdProc[SD_SEND_INTERFACE_COND] = spi_send_interface_cond;
		card->cmdProc[MMC_SEND_OP_COND] = spi_send_op_cond;	/* CMD1 really available ? */
		card->cmdProc[MMC_GO_IDLE_STATE] = spi_go_idle;
		card->cmdProc[MMC_SEND_STATUS] = spi_send_status;
		card->cmdProc[MMC_SEND_CSD] = spi_send_csd;
		card->cmdProc[MMC_SEND_CID] = spi_send_cid;
		/* Don't know if this is in the right group */
		card->cmdProc[MMC_CRC_ON_OFF] = spi_crc_on_off;
	}
	if (ccc & CCC_SWITCH) {
	}
	if (ccc & CCC_BASIC) {
		card->cmdProc[MMC_STOP_TRANSMISSION] = spi_stop_transmission;
	}
	if (ccc & (CCC_BLOCK_READ | CCC_BLOCK_WRITE | CCC_LOCK_CARD)) {
		card->cmdProc[MMC_SET_BLOCKLEN] = spi_set_blocklen;
	}
	if (ccc & CCC_BLOCK_READ) {
		card->cmdProc[MMC_READ_SINGLE_BLOCK] = spi_read_single_block;
		card->cmdProc[MMC_READ_MULTIPLE_BLOCK] = spi_read_multiple_block;
	}
	if (ccc & CCC_BLOCK_WRITE) {
		card->cmdProc[MMC_WRITE_SINGLE_BLOCK] = spi_write_single_block;
		card->cmdProc[MMC_WRITE_MULTIPLE_BLOCK] = spi_write_multiple_block;
	}
	if (ccc & CCC_WRITE_PROT) {
	}
	if (ccc & CCC_ERASE) {
		card->cmdProc[MMC_ERASE_WR_BLK_START] = spi_erase_wr_blk_start;
		card->cmdProc[MMC_ERASE_WR_BLK_END] = spi_erase_wr_blk_end;
		card->cmdProc[MMC_ERASE] = spi_erase;
	}
	if (ccc & CCC_SWITCH) {
	}
	if (ccc & CCC_LOCK_CARD) {
	}
	if (ccc & CCC_APP_SPEC) {
		card->cmdProc[MMC_APP_CMD] = spi_app_cmd;
		card->appCmdProc[SD_APP_OP_COND] = spi_app_op_cond;
		card->appCmdProc[SD_APP_SEND_NUM_WR_BLKS] = spi_app_send_num_wr_blks;
		card->appCmdProc[SD_APP_SEND_SCR] = spi_app_send_scr;
		/* 
		 * spi_app_send_status
		 * Seems not to exist and fall back to the non APP version 
		 * Test with sandisk, Toshiba and samsung card 
		 * card->appCmdProc[SD_APP_SEND_STATUS] = spi_app_send_status;
		 */
	}
	/* CMD58 is not mentioned in the CCC specification */
	card->cmdProc[MMC_READ_OCR] = spi_read_ocr;
	/* The APP cmds */
	if (ccc & CCC_APP_SPEC) {
	}
}

/*
 * ----------------------------------------------------------------------------------
 * MMCard_DoCmd
 *	Search a command Proc in the cmd/appCmd Table and execute it
 * ----------------------------------------------------------------------------------
 */
int
MMCard_DoCmd(MMCDev * mmcdev, uint32_t cmd, uint32_t arg, MMCResponse * resp)
{
	int result = MMC_ERR_TIMEOUT;
	MMCard *card = container_of(mmcdev, MMCard, mmcdev);
	MMCCmdProc *cmdProc = NULL;
	if (!card) {
		return MMC_ERR_TIMEOUT;
	}
	if (cmd > 63) {
		fprintf(stderr, "MMCard: Illegal cmd opcode 0x%02x\n", cmd);
		return MMC_ERR_TIMEOUT;
	}
	dbgprintf("MMCard CMD%d ,arg 0x%08x app %d\n", cmd, arg, card->is_app_cmd);
	resp->len = 0;
	memset(resp->data, 0xff, MMCARD_MAX_RESPLEN);
	if (card->is_app_cmd) {
		cmdProc = card->appCmdProc[cmd];
		card->is_app_cmd = 0;
	}
	/* If app cmd does not exist the regular command is used */
	if (cmdProc == NULL) {
		cmdProc = card->cmdProc[cmd];
	}
	if (cmdProc) {
		card->cmdcount++;
		result = cmdProc(card, cmd, arg, resp);
	} else {
		fprintf(stderr, "MMCard CMD%d not implemented\n", cmd);
	}
	if (resp->len > array_size(resp->data)) {
		fprintf(stderr, "Emulator: invalid response len %d\n", resp->len);
	}
	return result;
}

static uint32_t
hash_string(const char *s)
{
	uint32_t hash = 0;
	while (*s) {
		hash = *s + (hash << 6) + (hash << 16) - hash;
		s++;
	}
	return hash;
}

/*
 * -----------------------------------------------------------------
 * Fill the card registers with values from a template
 * identified by cardtype
 * -----------------------------------------------------------------
 */
static int
init_from_template(MMCard * card, const char *cardtype)
{
	uint8_t default_ssr[] = {
		0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
		0x02, 0x02, 0x90, 0x02, 0x00, 0xaa, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	int nr_types = sizeof(cardspec) / sizeof(MMCardSpec);
	int i;
	int ssr_ok = 0;
	MMCardSpec *spec;
	for (i = 0; i < nr_types; i++) {
		spec = &cardspec[i];
		if (strcmp(cardtype, spec->producttype) == 0) {
			break;
		}
	}
	if (i == nr_types) {
		return -1;
	}
	card->type = spec->type;
	card->usec_reset = spec->usec_reset;
	card->ocr = spec->ocr &= ~OCR_NOTBUSY;
	for (i = 0; i < 8; i++) {
		card->scr[i] = spec->scr[i];
	}
	for (i = 0; i < sizeof(card->cid); i++) {
		card->cid[i] = spec->cid[i];
		card->csd[i] = spec->csd[i];
	}
	for (i = 0; i < sizeof(card->ssr); i++) {
		card->ssr[i] = spec->ssr[i];
		if (card->ssr[i]) {
			ssr_ok = 1;
		}
	}
	/* For cards where I don't have the ssr contents */
	if (!ssr_ok) {
		memcpy(card->ssr, default_ssr, sizeof(card->ssr));
	}
	card->initial_rca = spec->rca;
	return 0;
}

/*
 * ---------------------------------------------
 * Create a card name (from capacity)
 * ---------------------------------------------
 */
static void
create_cardname(MMCard * card, const char *prefix)
{
	uint64_t capacity = card->capacity;
	if (strlen(prefix) > 1) {
		card->cid[3] = prefix[0];
		card->cid[4] = prefix[1];
	}
	if (capacity >= 1 << 30) {
		int giga = capacity >> 30;
		card->cid[5] = '0' + (giga / 10);
		card->cid[6] = '0' + (giga % 10);
		card->cid[7] = 'G';
	} else if (capacity >= 1 << 20) {
		int mega = capacity >> 20;
		card->cid[5] = '0' + (mega / 100);
		card->cid[6] = '0' + ((mega / 10) % 10);
		card->cid[7] = '0' + (mega % 10);
	}
}

/*
 * ---------------------------------------------------------------------
 * csd2_0_set_capacity
 * 	Set the Card Capacity of a SD card with CSD format 2.0
 *	(SD High Capacity)
 * ---------------------------------------------------------------------
 */

static int
csd2_0_set_capacity(MMCard * card, uint64_t capacity)
{
	uint32_t c_size = (capacity >> 19) - 1;
	COPYBITS(c_size, card->csd, 48, 69);
	COPYBITS(9, card->csd, 80, 83);
	card->capacity = capacity;
	card->blocklen = 512;
	if (capacity & 0x7ffff) {
		fprintf(stderr, "Capacity must be multiple of 512k\n");
		return -1;
	}
	create_cardname(card, "HC");
	return 0;
}

/*
 * -----------------------------------------------------------------------
 * csd1_0_set_capacity
 *	Set the Card Capacity of a SD card with CSD format 1.0
 *	Capacity is < 4GB with 2048byte sectors and < 1GB with 
 *	512 Byte sectors. The algorithm chooses 512 Byte
 *	sector size if possible. 
 * -----------------------------------------------------------------------
 */
static int
csd1_0_set_capacity(MMCard * card, uint64_t capacity)
{

	int nr_sectors;
	int shift = 0;
	int c_size_mult, c_size;
	int blkbits = 9;
	nr_sectors = capacity >> blkbits;
	if (nr_sectors > 4194304) {
		nr_sectors >>= 2;
		blkbits += 2;
	} else if (nr_sectors > 2097152) {
		nr_sectors >>= 1;
		blkbits++;
	}
	while ((nr_sectors == ((nr_sectors >> 1) << 1)) && (shift != 9)) {
		shift++;
		nr_sectors >>= 1;
	}
	if (shift < 2) {
		fprintf(stderr, "Illegal MM/SD-Card Size\n");
		return -1;
	}
	if (nr_sectors > 4096) {
		fprintf(stderr, "MM/SD-Card size can not be matched in CSD\n");
		return -1;
	}
	c_size = nr_sectors - 1;
	c_size_mult = shift - 2;
	COPYBITS(c_size, card->csd, 62, 73);
	COPYBITS(blkbits & 0xf, card->csd, 80, 83);
	COPYBITS(c_size_mult & 0x7, card->csd, 47, 49);

	card->blocklen = 1 << blkbits;
	card->capacity = card->blocklen * ((c_size + 1) << (c_size_mult + 2));
	if (card->type == CARD_TYPE_MMC) {
		create_cardname(card, "MM");
	} else if (card->type == CARD_TYPE_SD) {
		create_cardname(card, "SD");
	}
	//fprintf(stderr,"blkbits %02x,csd[5]: %02x\n",blkbits,card->csd[5]);
	//exit(1);
	return 0;
}

/*
 * ------------------------------------------------------------
 * csd_set_capacity
 * 	Set the card capacity into the CSD register.
 *	Calls CSD structure version dependent set function
 * ------------------------------------------------------------
 */
static int
csd_set_capacity(MMCard * card, uint64_t capacity)
{

	int csd_structure;
	csd_structure = getbits(card->csd, 16, 126, 127);
	switch (csd_structure) {
	    case 0:
		    return csd1_0_set_capacity(card, capacity);
	    case 1:
		    return csd2_0_set_capacity(card, capacity);
	    default:
		    fprintf(stderr, "Unknown CSD structure version\n");
		    exit(1);

	}
}

/*
 * -------------------------------------------------------------------------
 * For emulation of card where you have only a diskimage but no CID  
 * and CSD register values the values are taken from a template
 * and the size bits in CSD is calculated from filesize
 * -------------------------------------------------------------------------
 */
static void
init_auto_card_from_filesize(MMCard * card, char *filename)
{
	int fd;
	struct stat stat_buf;
	fd = open(filename, O_RDONLY | O_LARGEFILE);
	if (fd < 0) {
		fprintf(stderr, "MMC/SD card auto type requires an existing diskimage \"%s\"\n",
			filename);
		perror("");
		exit(1);
	}
	fstat(fd, &stat_buf);
	close(fd);
	fprintf(stderr, "MM/SD Card: Using filesize of \"%s\"\n", basename(filename));
	if (csd_set_capacity(card, stat_buf.st_size) < 0) {
		fprintf(stderr, "Can not set size of MM/SD-Card %s\n", filename);
		exit(1);
	}
	return;
}

int
MMCard_GetType(MMCard * card)
{
	if (card) {
		return card->type;
	} else {
		return 0;
	}
}

void
MMCard_Delete(MMCDev * mmcdev)
{
	MMCard *card = container_of(mmcdev, MMCard, mmcdev);
	DiskImage_Close(card->disk_image);
	card->disk_image = NULL;
	free(card);
}

/*
 * ---------------------------------------------------------------------
 * MMCard_New
 * Constructor for MMC/SD Cards
 * ---------------------------------------------------------------------
 */
MMCDev *
MMCard_New(const char *name)
{
	MMCard *card = sg_new(MMCard);
	char *imgdirname;
	char *filename;
	char *producttype;
	uint32_t psn = hash_string(name);
	int autotype = 0;
	producttype = Config_ReadVar(name, "type");
	filename = Config_ReadVar(name, "file");

	if (!producttype) {
		fprintf(stderr, "MMC Card: No product type configured for \"%s\". Skipped.\n",
			name);
		free(card);
		return NULL;
	}
	if (strcmp(producttype, "none") == 0) {
		fprintf(stderr, "MMC Card: \"%s\". type \"none\" configured Skipped.\n", name);
		free(card);
		return NULL;
	}
	if (strcmp(producttype, "auto_sdhc") == 0) {
		if (init_from_template(card, "Toshiba4G_HC") < 0) {
			fprintf(stderr, "Emulator bug, SDHC-Card template not found\n");
			exit(1);
		}
		autotype = 1;
	} else if (strcmp(producttype, "auto_sd") == 0) {
		if (init_from_template(card, "Toshiba32M") < 0) {
			fprintf(stderr, "Emulator bug, SD-Card template not found\n");
			exit(1);
		}
		autotype = 1;
	} else if (strcmp(producttype, "auto_mmc") == 0) {
		if (init_from_template(card, "ExtremeMemory128M") < 0) {
			fprintf(stderr, "Emulator bug, MM-Card template not found\n");
			exit(1);
		}
		autotype = 1;
		card->type = CARD_TYPE_MMC;
	} else {
		if (init_from_template(card, producttype) < 0) {
			fprintf(stderr,
				"MMCard Product \"%s\" not found. Please fix configfile !\n",
				producttype);
			dump_cardtypes();
			free(card);
			exit(1);
		}
		card->capacity = csd_get_capacity(card->csd, card->type);
		card->blocklen = csd_get_blocklen(card->csd, card->type);
	}
	COPYBITS(psn, card->cid, 24, 55);
	card->card_status = STATUS_READY_FOR_DATA;
	card->reset_state = RST_NOT_STARTED;
	card->clk = Clock_New("%s.clk", name);
	Clock_SetFreq(card->clk, 16 * 1000 * 1000);	/* Bad, the clock should come from controller */
	CycleTimer_Init(&card->transmissionTimer, MMCard_DoTransmission, &card->mmcdev);
	imgdirname = Config_ReadVar("global", "imagedir");
	if (imgdirname) {
		char *imagename;
		if (!filename) {
			imagename = alloca(strlen(imgdirname) + strlen(name) + 20);
			sprintf(imagename, "%s/%s.img", imgdirname, name);
		} else {
			imagename = alloca(strlen(imgdirname) + strlen(filename) + 20);
			sprintf(imagename, "%s/%s", imgdirname, filename);
		}
		if (autotype) {
			init_auto_card_from_filesize(card, imagename);
		}
		card->disk_image =
		    DiskImage_Open(imagename, card->capacity, DI_RDWR | DI_CREAT_FF | DI_SPARSE);
		if (!card->disk_image) {
			fprintf(stderr, "Failed to open disk_image \"%s\"\n", imagename);
			perror("msg");
			exit(1);
		}
	} else {
		fprintf(stderr, "No diskimage given for SD/MMC Card \"%s\"\n", name);
		exit(1);
	}
	if (card->type == CARD_TYPE_SD) {
		SDCardInitCmds(card);
	} else if (card->type == CARD_TYPE_MMC) {
		MMCardInitCmds(card);
	} else if (card->type == CARD_TYPE_MMCPLUS) {
		MMPlusCardInitCmds(card);
	}
	card->crc_on = 1;
	fprintf(stderr, "MM/SD-card \"%s\" of type \"%s\" cap. %llub blksize %d\n", name,
		producttype, (unsigned long long)card->capacity, card->blocklen);
	MMCDev_Init(&card->mmcdev, name);
	card->mmcdev.doCmd = MMCard_DoCmd;
	card->mmcdev.write = MMCard_Write;
	card->mmcdev.read = MMCard_Read;
	card->mmcdev.del = MMCard_Delete;
	card->mmcdev.gotoSpi = MMCard_GotoSpi;

	return &card->mmcdev;
}
#else
#include <stdlib.h>
#include "mmcdev.h"
MMCDev *
MMCard_New(const char *name)
{
    return NULL;
}
#endif
