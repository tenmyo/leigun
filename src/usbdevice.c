/*
 **************************************************************************************************
 *
 * Emulation of an USB device 
 *
 * State:
 *	Basically working with DJ-460 Printer emulation
 *
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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

#include <usbdevice.h>
#include <usbproto.h>
#include <usbstdrq.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <xy_hash.h>
#include <compiler_extensions.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sgstring.h>

#if 0
#define dbgprintf(x...) fprintf(stderr,x)
#else
#define dbgprintf(x...)
#endif

#define STATE_IDLE	    (0)
#define STATE_DO_OUT        (1)
#define STATE_DO_IN         (2)
#define STATE_DO_ISOCHO     (3)
#define STATE_DO_BCINTO     (4)
#define STATE_HS_BCO        (5)
#define STATE_DO_ISOCHI     (6)
#define STATE_DO_BCINTI     (7)
#define STATE_HSPING        (8)

#define	STATE_BCINTO_DEV_WAIT_ODATA 	(4)
#define STATE_BCINTI_WAIT_DEV_RESP	(11)	

/*
 * -------------------------------------------------------------------------------------------
 * UsbDev_RegisterEndpoint
 * 	Is called by the device emulators during initialization.
 *	IN and OUT endpoints have to be registered seperately
 *	Control endpoints register IN and OUT enpoint in one call
 * -------------------------------------------------------------------------------------------
 */

UsbEndpoint *
UsbDev_RegisterEndpoint(UsbDevice *udev,int epaddr,int eptype,int maxpacket,UsbTransactionProc *proc) 
{
	UsbEndpoint *ep;
	int epnum = epaddr & 0xf; 
	int in =  !!(epaddr & 0x80);
	ep = sg_new(UsbEndpoint);
	memset(ep,0,sizeof(UsbEndpoint));
	ep->status = 0;
	ep->doTransaction = proc;
	ep->type = eptype;
	ep->maxpacket = maxpacket;
	ep->epaddr = epaddr;
	ep->usbdev = udev;
	if((eptype == EPNT_TYPE_CONTROL)) {
		udev->in_endpnt[epnum] = ep;	
		udev->out_endpnt[epnum] = ep;	
		ep->setup_buf = sg_new(*ep->setup_buf);
		XY_InitHashTable(&ep->requestHash,XY_ONE_WORD_KEYS,32);
        	XY_InitHashTable(&ep->descriptorHash,XY_ONE_WORD_KEYS,32);
	} else if(in) {
		udev->in_endpnt[epnum] = ep;	
	} else {
		udev->out_endpnt[epnum] = ep;	
	}
	return ep;
}

void
UsbDev_UnregisterEndpoint(UsbDevice *udev,UsbEndpoint *ep) 
{
	int in =  !!(ep->epaddr & 0x80);
	int eptype = ep->type;
	int epnum = ep->epaddr & 0xf;
	if((eptype == EPNT_TYPE_CONTROL)) {
		if((udev->in_endpnt[epnum] == ep) && (udev->out_endpnt[epnum] == ep)) {
			udev->in_endpnt[epnum] = NULL;	
			udev->out_endpnt[epnum] = NULL;	
			sg_free(ep->setup_buf);
			XY_ClearHashTable(&ep->requestHash);
			XY_ClearHashTable(&ep->descriptorHash);
		} else {
			fprintf(stderr,"trying to unregister wrong enpoint\n");
		}
	} else if(in) {
		if(udev->in_endpnt[epnum] == ep) {
			udev->in_endpnt[epnum] = NULL;	
		} else {
			fprintf(stderr,"trying to unregister wrong enpoint\n");
		}
	} else {
		if(udev->out_endpnt[epnum] == ep) {
			udev->out_endpnt[epnum] = NULL;	
		} else {
			fprintf(stderr,"trying unregister wrong enpoint\n");
		}
	}
	
}

/*
 * ---------------------------------------------------------------
 * issue_handshake
 * 	Send a handshake to the USB Controller emulator.
 * 	Has to be called with PID_ACK or PID_NACK as argument	
 * ---------------------------------------------------------------
 */
static void
issue_handshake(UsbDevice *udev,int pid)
{
	UsbPacket pkt;
	pkt.pid = pid;
	if(udev->hostsink) {
		udev->hostsink(udev->usbhost,&pkt);
	}
}

/* 
 * -----------------------------------------------------------------
 * pid out
 *	send data to the device 
 * -----------------------------------------------------------------
 */
static void
pid_out(UsbDevice *udev,const UsbPacket *packet) 
{

	UsbEndpoint *epnt;
	UsbToken *token = &udev->transaction.token;
	uint8_t addr = packet->addr;
	uint8_t epnum = packet->epnum & 0xf;
	uint8_t pid = packet->pid;
	if(addr != udev->addr) {
		return;	
	} 
	epnt = udev->out_endpnt[epnum];
	if(!epnt) {
		fprintf(stderr,"Accessing nonexisting endpoint %d\n",epnum);
		return;
	}
	if(udev->ta_state != STATE_IDLE) {
		fprintf(stderr,"epnum %d Outpid received in wrong transaction state %d\n",epnum,udev->ta_state);
		return;
	}
	if(epnt->type == EPNT_TYPE_ISO) {
		udev->ta_state = STATE_DO_ISOCHO;
	} else if((udev->speed != USB_SPEED_HIGH) && 
		((epnt->type == EPNT_TYPE_BULK) || (epnt->type == EPNT_TYPE_CONTROL))) {
		udev->ta_state = STATE_DO_BCINTO;
	} else if((udev->speed == USB_SPEED_HIGH) &&
		((epnt->type == EPNT_TYPE_BULK) || (epnt->type == EPNT_TYPE_CONTROL))) {
		udev->ta_state = STATE_HS_BCO;
	} else if(epnt->type == EPNT_TYPE_INT) {
		udev->ta_state = STATE_DO_BCINTO;
	} else {
		fprintf(stderr,"usbdevice: Unhandled case in device state machine\n");
	}	
	token->pid = pid;
	token->addr = addr;
	token->epnum = epnum;
}

/*
 * -----------------------------------------------------------------------
 *
 * Send a PID IN to the device. 
 * -----------------------------------------------------------------------
 */
static void
pid_in(UsbDevice *udev,const UsbPacket *packet) 
{
	UsbEndpoint *epnt;
	UsbPacket reply;
	UsbTransaction *ta = &udev->transaction;
	UsbToken *token = &ta->token;
	uint8_t addr = packet->addr & 0x7f;
	uint8_t epnum = packet->epnum & 0xf;
	uint8_t pid = packet->pid;
	int result;

	/* Is this for me ? */	
	if(addr != udev->addr) {
		return;	
	} 
	epnt = udev->in_endpnt[epnum];
	if(!epnt) {
		fprintf(stderr,"USB device: accessing nonexisting endpoint %d\n",epnum);
		return;
	}
	if(udev->ta_state != STATE_IDLE) {
		fprintf(stderr,"Inpid received in wrong transaction state %d\n",udev->ta_state);
		return;
	}
	if(epnt->type == EPNT_TYPE_ISO) {
		udev->ta_state = STATE_DO_ISOCHI;
	} else {
		udev->ta_state = STATE_DO_BCINTI;
	}
	token->pid = pid;
	token->addr = addr;
	token->epnum = epnum;
	result = epnt->doTransaction(udev,epnt,ta,&reply);
	if((reply.pid == USB_PID_DATA0 || reply.pid == USB_PID_DATA1) ) {
		/* Timeout currently missing. Will wait forever here */
		udev->ta_state = STATE_BCINTI_WAIT_DEV_RESP;
	} else if(reply.pid == USB_PID_NAK) {
		udev->ta_state = STATE_IDLE;
	} else if(reply.pid == USB_PID_STALL) {
		udev->ta_state = STATE_IDLE;
	} else {
		fprintf(stderr,"Usb doTransaction: Unknown reply pid %d \n",reply.pid);
		return;
	}	 
	/*
	 * This may currently cause a Ack with no delay. 
 	 * Will be fixed later when packet delay based on length is added 
 	 */
	if(udev->hostsink) {
		udev->hostsink(udev->usbhost,&reply);
	}
} 

static void
pid_setup(UsbDevice *udev,const UsbPacket *packet) 
{
	UsbEndpoint *epnt;
	UsbTransaction *ta = &udev->transaction;
	UsbToken *token = &ta->token;
	uint8_t addr = packet->addr & 0x7f;
	uint8_t epnum = packet->epnum & 0xf;
	uint8_t pid = packet->pid;
	if(addr != udev->addr) {
		fprintf(stderr,"Address does not match: addr %d, dev %d\n",addr,udev->addr);
		return;	
	} 
	dbgprintf("Setup token for endpoint %d\n",epnum);
	epnt = udev->out_endpnt[epnum];
	if(!epnt) {
		fprintf(stderr,"Accessing nonexisting endpoint %d\n",epnum);
		return;
	}
	if(udev->ta_state != STATE_IDLE) {
		fprintf(stderr,"UsbDevice: SETUP pid received in wrong state %d\n",udev->ta_state);
		return;
	}
	if(epnt->type != EPNT_TYPE_CONTROL) {
		fprintf(stderr,"UsbDevice: SETUP pid for non control endpoint %d\n",epnum);
		return;
	}
	if((udev->speed != USB_SPEED_HIGH)) {
		udev->ta_state = STATE_DO_BCINTO;
	} else if((udev->speed == USB_SPEED_HIGH) &&
		((epnt->type == EPNT_TYPE_BULK) || (epnt->type == EPNT_TYPE_CONTROL))) {
		udev->ta_state = STATE_HS_BCO;
	} else {
		fprintf(stderr,"UsbDevice: wrong SETUP pid packet\n");
	}
	token->pid = pid;
	token->addr = addr;
	token->epnum = epnum;
	dbgprintf("Setup token was accepted\n");
}

static void
pid_sof(UsbDevice *udev,const UsbPacket *packet) 
{
	// Synchronize something
}

/* 
 * To lazy in first version, return always true
 */
static int 
check_toggle(UsbEndpoint *epnt,int toggle) {
	return 1;
}

static void 
pid_data_0_1(UsbDevice *udev,const UsbPacket *packet,int toggle) 
{
	UsbEndpoint *epnt;
	UsbTransaction *ta = &udev->transaction;
	UsbToken *token = &ta->token;
	UsbPacket reply;
	int epnum = token->epnum & 0xf;
	int result;
	dbgprintf(stderr,"PID data_0_1\n");
	epnt = udev->out_endpnt[epnum];
	if(!epnt) {
		fprintf(stderr,"Data for nonexisting endpoint %d\n",epnum);
		return;
	}
	if((udev->ta_state == STATE_BCINTO_DEV_WAIT_ODATA) &&
	   (token->pid == USB_PID_OUT)) {
		/* check_crc16(packet); */
		if(!check_toggle(epnt,toggle)) {
			/* accept wrong toggle, pkt it is already received */
			fprintf(stderr,"Got already received packet (wrong toggle)\n");
			/* do this before handshake of bad timing of host */
			udev->ta_state = STATE_IDLE;
			issue_handshake(udev,USB_PID_ACK);
		} else {
			epnt->toggle ^= 1;
			ta->data = packet->data;
			ta->data_len = packet->len; 
			/* The device gets a transaction structure */
			result = epnt->doTransaction(udev,epnt,ta,&reply);
			/* do this before because of bad timing of host */
			udev->ta_state = STATE_IDLE;  
			if(result == USBTA_OK) {
				issue_handshake(udev,USB_PID_ACK);
			} else {
				fprintf(stderr,"Non OK USB transaction not implemented\n");
				issue_handshake(udev,USB_PID_NAK);
			}
#if 0
			if(result == NOSPACE) {
				issue_handshake(udev,USB_PID_NAK);
			} else if(result == TROUBLE) {
				issue_handshake(udev,USB_PID_STALL);
			} else {
				issue_handshake(udev,USB_PID_ACK);
			} 
#endif
		}
	} else if ((udev->ta_state == STATE_BCINTO_DEV_WAIT_ODATA) &&
	   (token->pid == USB_PID_SETUP)) {
		if(toggle == 1) {
			fprintf(stderr,"Setup data with wrong toggle 1\n");
			return;
		}
		if(packet->len != 8) {
			fprintf(stderr,"USB Setup transaction with wrong data packet size %d\n",packet->len);
			return;
		}
		ta->data = packet->data;
		ta->data_len = packet->len; 
		result = epnt->doTransaction(udev,epnt,ta,&reply);
		/* ack triggers currently next transaction with no delay !! recursion ! */
		issue_handshake(udev,USB_PID_ACK);
		udev->ta_state = STATE_IDLE;
	} else {
		fprintf(stderr,"pid data 0/1 got in illegal transaction state %d\n",udev->ta_state); 
	}
}

/*
 * --------------------------------------------------------------------
 * An ACK to the device tells the device that it can forget the data
 * which are sent to the host because the host has received them
 * --------------------------------------------------------------------
 */
static void
pid_ack(UsbDevice *udev,const UsbPacket *packet) 
{
	/* Page 225 */
	UsbTransaction *ta = &udev->transaction;
	UsbToken *token = &ta->token;
	int epnum = token->epnum & 0xf;
	UsbEndpoint *epnt = udev->in_endpnt[epnum];
	if(!epnt) {
		fprintf(stderr,"Bug: ack for nonexisting endpoint\n");
		return;
	}
	if(udev->ta_state == STATE_BCINTI_WAIT_DEV_RESP) {
		/* epnt is known from earlier packets, but pid changed */
		token->pid = USB_PID_ACK;
		epnt->doTransaction(udev,epnt,ta,NULL);
		//if(epnt->doNext) {
		//	epnt->doNext(epnt);	
		//}
		udev->ta_state = STATE_IDLE;
		//fprintf(stderr,"Ack: Endpoint %d ta_state now %d\n",epnum,udev->ta_state);
	} else {
		fprintf(stderr,"pid_ack: something is missing in state machine\n");
	}
}

static void
pid_nack(UsbDevice *udev,const UsbPacket *packet) 
{
	if(udev->ta_state == STATE_BCINTI_WAIT_DEV_RESP) {
		udev->ta_state = STATE_IDLE;
	}
}

static void
pid_stall(UsbDevice *udev,const UsbPacket *packet) 
{
	if(udev->ta_state == STATE_BCINTI_WAIT_DEV_RESP) {
		fprintf(stderr,"STALL shouldn't happen in state %d\n",udev->ta_state);
	}
}

/*
 * --------------------------------------------------------------------------------
 * RegisterPacketSink and UnregisterPacketSink are used by the host emulator
 * When an USB device is plugged
 * --------------------------------------------------------------------------------
 */
void
UsbDev_RegisterPacketSink(UsbDevice *dev,void *host,UsbHost_PktSink *hostsink) 
{
	if(dev->usbhost) {
		fprintf(stderr,"Warning, Device is already connected to an USB-Host\n");
		return;
	}
	dev->usbhost = host;
	dev->hostsink = hostsink;
}

void
UsbDev_UnregisterPacketSink(UsbDevice *dev,void *host,UsbHost_PktSink *hostsink) 
{
	if((dev->usbhost != host) && (dev->hostsink != hostsink)) {
		fprintf(stderr,"Warning: Unregistering foreign USB Host\n");
	}
	dev->usbhost = NULL;
	dev->hostsink = NULL;
}
/*
 * --------------------------------------------------------------------------------
 * UsbDev_Feed
 * 	Feed an USB device with raw packets. 
 * --------------------------------------------------------------------------------
 */
void
UsbDev_Feed(void *dev,const UsbPacket *packet) 
{
	uint8_t pid = packet->pid;
	UsbDevice *udev = (UsbDevice *)dev;
	//dbgprintf(stderr,"USB-Device got pid %d\n",pid);
	dbgprintf("USB-Device ep %d got pid %d in state %d\n",packet->epnum,pid,udev->ta_state);
	switch(pid) {
		/* Token packets */
		case USB_PID_OUT:
			pid_out(udev,packet);
			break;

		case USB_PID_IN:
			pid_in(udev,packet);
			break;

		case USB_PID_SOF:
			pid_sof(udev,packet);
			break;

		case USB_PID_SETUP:
			pid_setup(udev,packet);
			break;

		/* Data packets */
		case USB_PID_DATA0:
			pid_data_0_1(udev,packet,0);
			break;

		case USB_PID_DATA1:
			pid_data_0_1(udev,packet,1);
			break;

		case USB_PID_DATA2:
			break;

		case USB_PID_MDATA:
			break;

		/* Handshake packets */
		case USB_PID_ACK:
			pid_ack(udev,packet);
			break;

		case USB_PID_NAK:
			pid_nack(udev,packet);
			break;

		case USB_PID_STALL:
			pid_stall(udev,packet);
			break;

		case USB_PID_NYET:
			break;

		/* case USB_PID_PRE: shared with pid_err 	*/
		/*	break; 		*/

		/* Special packets */
		case USB_PID_ERR:
			break;

		case USB_PID_SPLIT:
			break;

		case USB_PID_PING:
			break;

		case USB_PID_RESERVED:
			break;

		case USB_CTRLPID_RESET:
			udev->state = STATE_IDLE;
			udev->addr = 0;

	} 
}  

UsbDevice *
UsbDev_New(void *owner,usb_device_speed speed) 
{
	UsbDevice *udev = sg_new(UsbDevice);
	if(!udev) {
		fprintf(stderr,"Out of memory allocating usb device\n");
		exit(1);
	}
	memset(udev,0,sizeof(*udev));
	udev->state = USB_STATE_NOTATTACHED;
	udev->owner = owner;
	udev->speed = speed;
	/* Register the default handlers for get and set descriptor */
	return udev;
}

/*
 * --------------------------------------------------------------------------------
 * The USB Standard device requests from Chapter 9.4 of USB2.0 spec
 * ---------------------------------------------------------------------------------
 */

#if 0
static int
udrq_get_status(UsbDevice *dev,UsbTransaction *urq) 
{
	int epnr = urq->wIndex & 0xf;
	UsbEndpoint *epnt = dev->endpnt[epnr];
	if(urq->wValue !=0) {
		fprintf(stderr,"Warning: wValue !=0\n");
	}
	if(!(urq->wIndex & 0x80)) {
		fprintf(stderr,"Wrong direction in get_status\n");
		return USBTA_ERROR;
	}
	if(urq->wLength != 2) {
		fprintf(stderr,"usbdev: get status with wrong length of %d\n",urq->wLength);
	}
	if(dev->state == USB_STATE_ADDRESS) {
		return USBTA_ERROR; 
	}
	if(epnr > 0) {
		dev->reply_buf[0] = epnt->status & 0xff;
		dev->reply_buf[1] = 0;	
	} else {
		dev->reply_buf[0] = 1; /* Self powered, not remote wakeup */
		dev->reply_buf[1] = 0;	
	}
	dev->reply_wp = 2;
	return 2;
}
#endif

#if 0
static int
udrq_clear_feature(UsbDevice *dev,UsbTransaction *udrq) 
{
	if(dev->state == USB_STATE_DEFAULT) {
		fprintf(stderr,"Error: ClearFeature not defined in default state\n");
		return USBTA_ERROR;
	}
	if(dev->state == USB_STATE_ADDRESS) {
		if(udrq->wIndex != 0) {
			fprintf(stderr,"Error, illegal ClearFeature in address state\n");
			return USBTA_ERROR;
		}
	}
	if((dev->state == USB_STATE_CONFIGURED)  || (dev->state == USB_STATE_ADDRESS)) {
		//int epnt = ?;
		dev->reply_wp = 0;
		return 0;	
	}
	return USBTA_NOTHANDLED;
}
#endif

#if 0
static int
udrq_set_feature(UsbDevice *dev,UsbTransaction *udrq) 
{
	if(dev->state == USB_STATE_DEFAULT) {
		fprintf(stderr,"Error: ClearFeature not defined in default state\n");
		return USBTA_ERROR;
	}
	if(dev->state == USB_STATE_ADDRESS) {
		if(udrq->wIndex != 0) {
			fprintf(stderr,"Error, illegal ClearFeature in address state\n");
			return USBTA_ERROR;
		}
	}
	if((dev->state == USB_STATE_CONFIGURED)  || (dev->state == USB_STATE_ADDRESS)) {
		//int epnt = ?;
		dev->reply_wp = 0;
		return 0;	
	}
	return USBTA_NOTHANDLED;
}
#endif


static UsbDescriptorHandler *
find_descriptor_handler(UsbEndpoint *ep,UsbSetupBuffer *sb) 
{
	unsigned long key = sb->wValue;
	XY_HashEntry *entry;
	entry = XY_FindHashEntry(&ep->descriptorHash,(void*)key);
	if(entry) {
		return XY_GetHashValue(entry);
	}
	return NULL;
}

UsbTaRes
UsbDev_GetDescriptor(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	UsbDescriptorHandler *dh = find_descriptor_handler(ep,sb);
	if(!dh) {
		fprintf(stderr,"* No handler found for descriptor %d\n",sb->wValue);
		return USBTA_NOTHANDLED;
	}
	if(!dh->getDescriptor) {
		fprintf(stderr,"* No get proc for descriptor %d\n",sb->wValue);
		return USBTA_NOTHANDLED;
	}
	dbgprintf("Get the descriptor\n");
	return dh->getDescriptor(ep,sb);
}

UsbTaRes
UsbDev_SetDescriptor(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	UsbDescriptorHandler *dh = find_descriptor_handler(ep,sb);
	if(!dh || !dh->setDescriptor) {
		fprintf(stderr,"* No handler found for descriptor %d\n",sb->wValue);
		return USBTA_NOTHANDLED;
	}
	if(!dh->setDescriptor) {
		fprintf(stderr,"* No set proc descriptor %d\n",sb->wValue);
		return USBTA_NOTHANDLED;
	}
	return dh->setDescriptor(ep,sb);
}

/*
 * --------------------------------------------------------------------------------------------------
 * Register procedures for setting/getting a descriptor
 * --------------------------------------------------------------------------------------------------
 */
void 
UsbDev_RegisterDescriptorHandler(UsbEndpoint *ep,int dt,int index,UsbDescriptorProc *get,UsbDescriptorProc *set)
{
	XY_HashEntry *entry;
	unsigned long key = (dt << 8) | index;
	UsbDescriptorHandler *dh = (UsbDescriptorHandler *)sg_new(*dh);
	int isnew;
	dh->getDescriptor = get; dh->setDescriptor = set;
	if(!dh) {
		fprintf(stderr,"Out of memory allocating DescriptorHandler\n");
		exit(1);
	}
	entry = XY_CreateHashEntry(&ep->descriptorHash,(void*)key,&isnew);
	if(!isnew) {
		fprintf(stderr,"Warning: Overwriting existing descriptor handler\n");
	}
	XY_SetHashValue(entry,dh);
}

static UsbRequestHandler *
find_request_handler(UsbEndpoint *ep,UsbSetupBuffer *sb) 
{
	unsigned long key = sb->bRequest | (sb->bmRequestType << 8);
	XY_HashEntry *entry;
	entry = XY_FindHashEntry(&ep->requestHash,(void*)key);
	if(entry) {
		return XY_GetHashValue(entry);
	}
	return NULL;
}

__UNUSED__ void
dump_setup_buffer(UsbSetupBuffer *sb) 
{
	int i;
	fprintf(stderr,"rqt %02x rq %02x wVal %04x wIdx %04x wLength %04x \n",sb->bmRequestType,
		sb->bRequest,sb->wValue,sb->wIndex,sb->wLength);
	
	
	for(i=0;i<sb->data_len;i++) {
		fprintf(stderr,"%02x ",sb->setup_data[i]);
	}
	fprintf(stderr,"\n");
	
}

/*
 * -----------------------------------------------------------------------
 * UsbDev_DoRequest
 *	The Usb Ctrl Endpoint handler calls this proc when it has
 *	assembled a complete setup buffer containing an USB Ctrl request. 
 *	This proc searches if a handler is registered for this Request
 *	and invokes it or returns NOTHANDLED
 * -----------------------------------------------------------------------
 */
static UsbTaRes
UsbDev_DoRequest(UsbEndpoint *ep,UsbSetupBuffer *sb) 
{
	UsbRequestHandler *handler = find_request_handler(ep,sb);
	UsbTaRes result;
	if(handler) {
		dbgprintf("Found a handler for the request %02x\n",sb->bRequest);
		result = handler(ep,sb);
		/* dump_setup_buffer(sb); */
		return result;
	} else {
		fprintf(stderr,"No Handler for Setup request %d type 0x%04x\n",sb->bRequest,sb->bmRequestType);
		return USBTA_NOTHANDLED;
	}
}

/*
 * ------------------------------------------------------------------------------------------------
 * UsbDev_RegisterRequest
 *	A control endpoint which uses the UsbDev default control endpoint implementation
 * 	as a library can Register request handlers here. 
 * ------------------------------------------------------------------------------------------------
 */
void 
UsbDev_RegisterRequest(UsbEndpoint *ep,uint8_t rq,uint8_t rqt,UsbRequestHandler *proc)
{
	unsigned long key = rq | (rqt << 8); 
	int isnew;
	XY_HashEntry *entry;
	if(ep->type != EPNT_TYPE_CONTROL) {
		fprintf(stderr,"Emulator bug: Register request for non control enpoint\n");
		exit(1);
	}
	entry = XY_CreateHashEntry(&ep->requestHash,(void *)key,&isnew);
	if(!entry) {
		fprintf(stderr,"Out of memory allocating request Hash entry\n");
		exit(1);
	}
	if(!isnew) {
		fprintf(stderr,"Overwriting old requestHandler\n");
	}
	XY_SetHashValue(entry,proc);
}

/*
 * --------------------------------------------------------------------
 * The UsbDevices have very similar Control Endpoint 0
 * This is the default implementation which will be used
 * by most device emulators
 * --------------------------------------------------------------------
 */
int
UsbDev_CtrlEp(UsbDevice *udev,UsbEndpoint *ep,UsbTransaction *ta,UsbPacket *reply)
{
        UsbSetupBuffer *sb = ep->setup_buf;
        int result;
        dbgprintf("Device: Got control transaction token.pid %d\n",ta->token.pid);
        if(ta->token.pid == USB_PID_SETUP) {
                sb->bmRequestType = ta->data[0];
                sb->bRequest = ta->data[1];
                sb->wValue = ta->data[2] | (ta->data[3] << 8);
                sb->wIndex = ta->data[4] | (ta->data[5] << 8);
                sb->wLength = ta->data[6] | (ta->data[7] << 8);
                sb->data_len = sb->transfer_count = 0;
                /* realloc assembly buffer setup_data here */
                if(sb->wLength > sb->data_bufsize) {
                        sb->setup_data = realloc(sb->setup_data,sb->wLength);
                        if(sb->setup_data) {
                                sb->data_bufsize = sb->wLength;
                        } else {
                                fprintf(stderr,"Out of memory allocating setup buffer\n");
                                exit(1);
                        }
                }
                /* Device to host or no Data */
                if(sb->bmRequestType &  USB_RQT_DEVTOHOST) /*  DeviceToHost */ {
                        result = UsbDev_DoRequest(ep,sb);
                }
                reply->pid = USB_PID_ACK;
                return USBTA_OK;
	} else if(ta->token.pid == USB_PID_OUT) {
                /* Host to Device */
                if(sb->bmRequestType & USB_RQT_DEVTOHOST) {
                        dbgprintf("Status stage of DIRIN Setup packet\n");
                        if(sb->transfer_count < sb->data_len) {
                                fprintf(stderr,"Status stage before data complete\n");
                        }
                        /*  status stage: ignore the data (should be zlp) and reply with ack */
                        reply->pid = USB_PID_ACK;
                        return USBTA_OK;
                } else {
                        int len = ta->data_len;
                        if(len > (sb->wLength - sb->transfer_count)) {
 				fprintf(stderr,"Oversized setup request len %d, wLen %d, datalen %d\n",len,sb->wLength,sb->transfer_count);
                                len = sb->wLength - sb->transfer_count;
                        }
                        memcpy(sb->setup_data+sb->transfer_count,ta->data,len);
                        sb->transfer_count += len;
                        dbgprintf("Data stage of DIROUT SETUP packet size %d: reply with ACK\n",sb->transfer_count);
                        reply->pid = USB_PID_ACK;
                        return USBTA_OK;
                }
        } else if(ta->token.pid == USB_PID_IN) {
                if(sb->bmRequestType & USB_RQT_DEVTOHOST) {
			int i;
                        unsigned int len = sb->data_len - sb->transfer_count;
                        if(len > ep->maxpacket) {
                                len = ep->maxpacket;
                        }
                        memcpy(reply->data,sb->setup_data+sb->transfer_count,len);
                        dbgprintf("Setup IN data: reply %d bytes dl %d, tc %d\n",len,sb->data_len,sb->transfer_count);
			for(i=0;i<len;i++) {
				dbgprintf("%02x ",sb->setup_data[i+sb->transfer_count]);
			}
			dbgprintf("\n");

                        reply->len = len;
                        reply->pid = USB_PID_DATA0; /* should toggle ! */
                        sb->transfer_count += len;
                        return USBTA_OK;
                } else {
                        /*
                         * ------------------------------------
                         * Terminate a control write
                         * Table 8-7 Status Stage responses
                         * NAK STALL or ZLP (Ok)
                         * ------------------------------------
                         */
                        sb->data_len = sb->transfer_count;
                        result = UsbDev_DoRequest(ep,sb);
                        /* STALL is missing here */
                        dbgprintf(stderr,"Term Write with ZLP bmRequestType 0x%02x rq %02x\n",sb->bmRequestType,sb->bRequest);
                        reply->len = 0;
                        if(result == USBTA_OK) {
                                reply->pid = USB_PID_DATA1;
                        } else {
				fprintf(stderr,"Send a NAK because RQ result %d\n",result);
                                reply->pid = USB_PID_NAK;
                        }
                        return result;
                }
        } else if(ta->token.pid == USB_PID_PING) {
                dbgprintf(stderr,"Status stage by ping not implemented\n");
                // goto status stage for control reads (IN)
        }
        return USBTA_ERROR;
}

