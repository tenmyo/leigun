/*
 ****************************************************************************************
 * USB Protocol definiton header file
 *
 * (C) 2006 Jochen Karrer
 ****************************************************************************************
 */

#ifndef __USBPROTO_H
#define __USBPROTO_H
#include <stdint.h>

#define USB_PID_OUT             (1)
#define USB_PID_IN              (9)
#define USB_PID_SOF             (5)
#define USB_PID_SETUP           (0xd)

/* Data packets */
#define USB_PID_DATA0           (0x3)
#define USB_PID_DATA1           (0xb)
#define USB_PID_DATA2           (0x7)
#define USB_PID_MDATA           (0xf)

/* Handshake packets */
#define USB_PID_ACK             (0x2)
#define USB_PID_NAK             (0xa)
#define USB_PID_STALL           (0xe)
#define USB_PID_NYET            (0x6)

/* Special packets */
#define USB_PID_ERR             (0xc)	/* This is a handshake */
#define USB_PID_PRE             (0xc)	/* This is a token */
#define USB_PID_SPLIT           (0x8)	/* This is a token */
#define USB_PID_PING            (0x4)	/* This is a token */
#define USB_PID_RESERVED        (0)

/* PIDs from 0xf0 are used as special signaling (SE0 reset for example) */
#define USB_CTRLPID_RESET	(0xf0)
#define USB_CTRLPID_CONNECT_FS	(0xf1)
#define USB_CTRLPID_CONNECT_LS	(0xf2)
#define USB_CTRLPID_DISCONNECT	(0xf3)
#define USB_CTRLPID_HOST_ATACH	(0xf4)

#define USB_MAX_PKTLEN (1024)
typedef struct UsbPacket {
	uint8_t pid;
	uint8_t addr;		/* 7  Bits */
	uint8_t epnum;		/* 4  Bits */
	uint16_t fnumb;		/* 11 Bits, only in SOF */
	uint8_t data[USB_MAX_PKTLEN];
	int len;
	uint8_t token_crc;	/* 5 Bits */
	uint16_t crc16;
} UsbPacket;

#endif
