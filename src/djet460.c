/*
 *************************************************************************************************
 *
 * Emulation of a HP DeskJet 460 USB printer 
 * (C) 2006 Jochen Karrer
 *
 * State:
 *	Detected by linux kernel, can be redirected to a real printer or 
 *	print into a pnm/png
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

/*
 * ---------------------------------------------------------------------------------
 * Dump from real device:
 *	T:  Bus=05 Lev=01 Prnt=01 Port=01 Cnt=01 Dev#=  5 Spd=12  MxCh= 0
 *	D:  Ver= 2.00 Cls=00(>ifc ) Sub=00 Prot=00 MxPS= 8 #Cfgs=  1
 *	P:  Vendor=03f0 ProdID=1312 Rev= 1.00
 *	S:  Manufacturer=HP
 *	S:  Product=Deskjet 460
 *	S:  SerialNumber=MY62K4Z1JQ
 *	C:* #Ifs= 2 Cfg#= 1 Atr=c0 MxPwr=  2mA
 *	I:  If#= 0 Alt= 0 #EPs= 3 Cls=07(print) Sub=01 Prot=02 Driver=usblp
 *	E:  Ad=01(O) Atr=02(Bulk) MxPS=  64 Ivl=0ms
 *	E:  Ad=81(I) Atr=02(Bulk) MxPS=  64 Ivl=0ms
 *	E:  Ad=82(I) Atr=03(Int.) MxPS=   8 Ivl=10ms
 *	I:  If#= 1 Alt= 0 #EPs= 2 Cls=08(stor.) Sub=06 Prot=50 Driver=usb-storage
 *	E:  Ad=05(O) Atr=02(Bulk) MxPS=  64 Ivl=0ms
 *	E:  Ad=85(I) Atr=02(Bulk) MxPS=  64 Ivl=0ms
 *
 * 	No response DT_DEVICE_QUALIFIER and DT_OTHER_SPEED_CONFIG
 *
 *	Full speed only. Device has no high speed:
 *	---------------------------------------------
 *	Dev Descr 
 * 	12 01 00 02 00 00 00 08 f0 03 12 13 00 01 01 02 03 01
 *	Config descr:
 *	09 02 3e 00 02 01 00 c0 01 09 04 00 00 03 07 01
 *	02 00 07 05 01 02 40 00 00 07 05 81 02 40 00 00
 *	07 05 82 03 08 00 0a 09 04 01 00 02 08 06 50 00
 *	07 05 05 02 40 00 00 07 05 85 02 40 00 00
 *
 * ---------------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sgstring.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <usbdevice.h>
#include <usbstdrq.h>
#include <djet460.h>
#include <configfile.h>
#include <cycletimer.h>
#include <dj460interp.h>
#include <signode.h>
#if 0
#define dbgprintf(x...) fprintf(stderr,x)
#else
#define dbgprintf(x...)
#endif

static const usb_device_descriptor dev_descr_tmpl = {
        0x12,           	/* uint8_t  bLength;                            */
        USB_DT_DEVICE,
        host16_to_le(0x0200),	/* le16 bcdUSB Version 1.1                      */
        0x00,           	/* Class code defined at interface level        */
        0x00,           	/* SubClass;                                    */
        0x00,           	/* DeviceProtocol                               */
        0x08,           	/* MaxPacketSize0                               */
	host16_to_le(0x03f0),	/* idVendor Hewlett Packard			*/
        host16_to_le(0x1312),  	/* le16 idProduct				*/
        host16_to_le(0x0100),  	/* le16 bcdDevice				*/
        0x01,           	/* uint8_t  iManufacturer;                      */
        0x02,           	/* uint8_t  iProduct;                           */
        0x03,           	/* uint8_t  iSerialNumber;                      */
        0x01,           	/* bNumConfigurations, eventually patched       */
};

static const usb_config_descriptor conf_descr_tmpl = {
	0x09,			/* uint8_t  bLength; */
	USB_DT_CONFIG,		/* uint8_t  bDescriptorType; 				*/
        host16_to_le(0x0009),	/* le16 TotalLength will be patched later 		*/ 
        0x01, 			/* uint8_t  bNumInterfaces;				*/
        0x01,			/* uint8_t  bConfigurationValue;			*/
        0x00, 			/* uint8_t  iConfiguration; (starts with 1)		*/
        0xC0, 			/* uint8_t  bmAttributes; Laserjet 2200 does not set D7 */ 
	0x01,			/* uint8_t  bMaxPower;					*/
};

static const usb_interface_descriptor if_descr_tmpl = {
	0x09,		/* uint8_t  bLength; 					*/
       	USB_DT_INTERFACE,
      	0x00,  		/* uint8_t  bInterfaceNumber is zerobased 		*/
        0x00, 		/* uint8_t  bAlternateSetting  (zerobased)		*/
       	0x03, 		/* uint8_t  bNumEndpoints; excluding ep0		*/
       	0x07, 		/* uint8_t  bInterfaceClass Printer			*/
        0x01,		/* uint8_t  bInterfaceSubClass Printers			*/
       	0x02, 		/* uint8_t  bInterfaceProtocol				*/
       	0x00 		/* uint8_t  iInterface; Don't describe it with a string	*/

};

static const usb_endpoint_descriptor bulkout_ep_descr_tmpl = {
	0x07,		/* uint8_t  bLength; printer class doku is shit ?	*/
        USB_DT_ENDPOINT,	
       	0x01, 		/* uint8_t  bEndpointAddress; 				*/
        0x02, 		/* uint8_t  bmAttributes;				*/
        0x40, 0x00, 	/* le16 wMaxPacketSize 64 Bytes ?			*/
       	0x00, 		/* uint8_t  bInterval is for isochronous only 		*/

};

static const usb_endpoint_descriptor bulkin_ep_descr_tmpl = {
	0x07,		/* uint8_t  bLength; printer class doku is shit ?	*/
        USB_DT_ENDPOINT,	
       	0x81, 		/* uint8_t  bEndpointAddress; 				*/
        0x02, 		/* uint8_t  bmAttributes;				*/
        0x40, 0x00, 	/* le16 wMaxPacketSize 64 Bytes ?			*/
       	0x00, 		/* uint8_t  bInterval is for isochronous only 		*/
};

static const usb_endpoint_descriptor int_ep_descr_tmpl = {
	0x07,		/* uint8_t  bLength; printer class doku is shit ?	*/
        USB_DT_ENDPOINT,	
       	0x82, 		/* uint8_t  bEndpointAddress; 				*/
        0x03, 		/* uint8_t  bmAttributes;				*/
        0x08, 0x00, 	/* le16 wMaxPacketSize 64 Bytes ?			*/
       	0x0a, 		/* uint8_t  bInterval is for iso and interrupt only 	*/
};


typedef struct DJet460 {
	UsbDevice *udev;
	Dj460Interp *interp;
	char *lp_devname;
	char *lp_outdir;
	int lp_devfd;
	CycleTimer lp_close_timer;
	usb_device_descriptor device_descriptor;
	usb_config_descriptor config_descriptor;
	usb_interface_descriptor interface_descriptor;
	usb_endpoint_descriptor out_endpoint_descriptor;
	usb_endpoint_descriptor in_endpoint_descriptor;
	usb_endpoint_descriptor int_endpoint_descriptor;

	SigNode *nPwrGreen;
	SigNode	*nPwrRed;
	SigNode *nBlackLed;
	SigNode *nColorLed;
	SigNode *nResumeLed;
	SigNode *nPowerButton;
	SigNode *nCancelButton;
	SigNode *nResumeButton;
	SigNode *nDoorDetect;
	/* LPDevice *lpdev */
} DJet460;

/*
 * --------------------------------------------------------
 * Convert a string into an USB descriptor (unicode)
 * --------------------------------------------------------
 */
static int 
strtodescr(uint8_t *dst,const char *src,unsigned int maxlen) 
{
    int  i;
    int len = strlen(src);
    uint8_t *d = dst;
    if(maxlen < 2) {
	return 0;
    }
    *d++ = 2+(len<<1);
    *d++ = USB_DT_STRING;
    for(i = 0; (i < len) && i < (maxlen-2); i++) {
        *d++ = src[i];
        *d++ = 0;
    }
    return d - dst;
}

/*
 * ------------------------------------------------------------------------------------
 * ------------------------------------------------------------------------------------
 */
static int 
interrupt_endpoint(UsbDevice *udev,UsbEndpoint *ep,UsbTransaction *ta,UsbPacket *reply) 
{
	fprintf(stderr,"No handler for Interrupt endpoint\n");
	return USBTA_NOTHANDLED;
}

static void
lp_close(void *dev) 
{
	DJet460 *dj = (DJet460 *)dev;
	if(dj->lp_devfd >= 0) {
		fprintf(stderr,"Closed unused Printer device\n");
		close(dj->lp_devfd);
		dj->lp_devfd = -1;	
	}
}

__UNUSED__ static void
dump_printdata(UsbTransaction *ta) 
{
	int i;
	for(i=0;i<ta->data_len;i++) {
		if(isprint(ta->data[i])) {
				fprintf(stderr,"%c",ta->data[i]);
		} else {
			fprintf(stderr,"0x%02x",ta->data[i]);
		}
	}
	fprintf(stderr,"\n");
}

/*
 * ----------------------------------------------------------------------------------
 * data_out_endpoint
 * 	Bulk Endpoint 1 
 *	Accepts all data and sends it to the Printer Language interpreter
 *	and/or to a real printer device
 * ----------------------------------------------------------------------------------
 */
static int 
data_out_endpoint(UsbDevice *udev,UsbEndpoint *ep,UsbTransaction *ta,UsbPacket *reply) 
{
	DJet460 *dj = (DJet460 *)udev->owner;
	dbgprintf("Bulk endpoint request\n");
	CycleTimer_Mod(&dj->lp_close_timer,MillisecondsToCycles(15000));
	switch(ta->token.pid) {
		case USB_PID_OUT:
			if((dj->lp_devfd < 0) && dj->lp_devname) {
				dj->lp_devfd = open(dj->lp_devname,O_RDWR);	
			}
			if(dj->lp_devfd >= 0) {
				int count;
				int result;
				for(count = 0; count < ta->data_len;count+=result) {
					result = write(dj->lp_devfd,ta->data+count,ta->data_len-count);
					if(result < 0) {
						fprintf(stderr,"Write to lp fd failed\n");
						close(dj->lp_devfd);
						dj->lp_devfd = -1;
						break;
					}
				}
					
			} else {
			
			}
			if(dj->interp != NULL) {
				Dj460Interp_Feed(dj->interp,(void*)ta->data,ta->data_len);
			}
			reply->pid = USB_PID_ACK;
			break;
		default:
			fprintf(stderr,"DJ460 bulk out: got unexpected token pid %d\n",ta->token.pid);
			return USBTA_NOTHANDLED;
	}
	return USBTA_OK;
}

/*
 * ----------------------------------------------------------------------------
 * data_in_endpoint
 * 	Bulk endpoint 0x81 replies with a zero length packet for IN PID
 *	and ignores the ACK's
 * ----------------------------------------------------------------------------
 */
static int 
data_in_endpoint(UsbDevice *udev,UsbEndpoint *ep,UsbTransaction *ta,UsbPacket *reply) 
{
	//DJet460 *dj = (DJet460 *)udev->owner;
	dbgprintf("Bulk endpoint request\n");
	switch(ta->token.pid) {
		case USB_PID_IN:
			fprintf(stderr,"IN PID: reply with len 0\n");
			reply->len = 0;
			reply->pid = USB_PID_DATA0; // toggle missing
			break;
		case USB_PID_ACK:
			/* fine, now switch to next buffer */
			break;
		default:
			fprintf(stderr,"DJ460 bulk in: got unexpected token pid %d\n",ta->token.pid);
			return USBTA_NOTHANDLED;
	}
	return USBTA_OK;
}

/*
 * -------------------------------------------------------------------------------
 * Get/SetDeviceDescriptor
 * -------------------------------------------------------------------------------
 */
static UsbTaRes
GetDeviceDescr(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	DJet460 *dj = (DJet460*) USBDEV(ep)->owner;
	dbgprintf("Called GetDeviceDescr with wLength %d\n",sb->wLength);
	fprintf(stderr,"Called GetDeviceDescr with wLength %d\n",sb->wLength);
	sb->data_len = sg_mincpy(sb->setup_data,&dj->device_descriptor,sizeof(dj->device_descriptor),sb->wLength);
	return USBTA_OK;
}

static UsbTaRes 
SetDeviceDescr(UsbEndpoint *ep,UsbSetupBuffer *sb) 
{
	fprintf(stderr,"DJ460: Set Device descriptor is not possible\n");
	return USBTA_NOTHANDLED;
}

/*
 * ----------------------------------------------------------------------------------
 * A configuration descriptor also contains the endpoint descriptors
 * ----------------------------------------------------------------------------------
 */
static UsbTaRes
GetConfigDescr(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	DJet460 *dj = (DJet460*) USBDEV(ep)->owner;
	dbgprintf("Called GetConfigDescr\n");
	//if(sb->wLength) 	
	int len_config = dj->config_descriptor.bLength;
	int len_if  = dj->interface_descriptor.bLength;
	int len_out = dj->out_endpoint_descriptor.bLength;
	int len_in = dj->in_endpoint_descriptor.bLength;
	int len_int = dj->int_endpoint_descriptor.bLength;
	int totallen = len_config + len_if + len_out + len_in + len_int;
	uint8_t buf[totallen];
	int count;
	memcpy(buf,&dj->config_descriptor,len_config);
	count = len_config;
	memcpy(buf+count,&dj->interface_descriptor,len_if);
	count += len_if;
	memcpy(buf+count,&dj->out_endpoint_descriptor,len_out);
	count += len_out; 
	memcpy(buf+count,&dj->in_endpoint_descriptor,len_in);
	count += len_in; 
	memcpy(buf+count,&dj->int_endpoint_descriptor,len_int);
	count += len_int; 

	/* Patch the tottallen field of config descriptor */
	buf[2] = totallen & 0xff;
	buf[3] = totallen >> 8;
	sb->data_len = sg_mincpy(sb->setup_data,buf,sb->wLength,count);		
	return USBTA_OK;
}

static UsbTaRes
SetConfigDescr(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	fprintf(stderr,"DJ460: Set config descriptor not possible\n");
	return USBTA_NOTHANDLED;
}

static UsbTaRes
GetStringDescr(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	int index = sb->wValue & 0xff;
	uint8_t *reply = sb->setup_data;
	int maxlen = sb->wLength;
	switch(index) {
		case 0: /* Special case: return Languages */
			sb->data_len = *reply++ = 4; 
			*reply++ = USB_DT_STRING;
			*reply++ = 0x09; *reply++ = 0x04; /* English USA */
			break;

		case 1: /* iManufacturer 	*/
			sb->data_len = strtodescr(reply,"HP",maxlen);
			break;

		case 2: /* iProductId		*/
			sb->data_len = strtodescr(reply,"Deskjet 460",maxlen);
			break;

		case 3:	/* iSerialNumber	*/
			sb->data_len = strtodescr(reply,"MY62K4Z1JQ",maxlen);
			break;

		case 4:	/* iConfiguration 	*/
			sb->data_len = strtodescr(reply,"BlahBlubb",maxlen);
			break;
		default:
			fprintf(stderr,"+++++++ No descriptor with index %d found\n",index);
			return USBTA_ERROR;
	}
	dbgprintf("Got string descriptor with len %d\n",sb->data_len);
	return USBTA_OK; 
}

static UsbTaRes
SetStringDescr(UsbEndpoint *ep,UsbSetupBuffer *sb) 
{
	fprintf(stderr,"DJ460: String descriptor can not be set\n");
	return USBTA_NOTHANDLED;
}

/*
 * ------------------------------------------------------------------
 * USB Printer class specific requests
 * ------------------------------------------------------------------
 */
#define PS_PAPER_EMPTY	(1<<5)
#define	PS_SELECT	(1<<4)
#define PS_NOT_ERROR 	(1<<3)

/*
 * ----------------------------------------------------------------------
 * GetDeviceId
 * ----------------------------------------------------------------------
 */
UsbTaRes
UsbPrn_GetDeviceId(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	int copylen,replylen;
	char *str = "MFG:HP;MDL:Deskjet 460;CMD:MLC,PCL,PML,DW-PCL,DESKJET,DYN;CLS:PRINTER;DES:Deskjet 460;SN:MY62K4Z1JQ;S:04800080840010210040002c1781062c2881062;Z:0102,0503cbe8015548,0600;BT:000000000000,,0000008F,60;t";
	int required = strlen(str) + 2;
	dbgprintf("GET DEVICE ID WITH max wLength %d\n",sb->wLength);
	if(sb->wLength < 2) {
		return USBTA_NOTHANDLED;
	} else if(required > sb->wLength) {
		copylen = sb->wLength-2;
		replylen = sb->wLength;
	} else {
		copylen = strlen(str);
		replylen = copylen + 2;
	}	
	memcpy(sb->setup_data+2,str,copylen);
	/* Contains the length of the string in big endian in the first two bytes */
	sb->setup_data[0] = replylen >> 8;
	sb->setup_data[1] = replylen & 0xff;
	sb->data_len = replylen; 
	return USBTA_OK;
}

/*
 * ----------------------------------------------------------------------
 * UsbPrn_GetPortStatus
 * ----------------------------------------------------------------------
 */
UsbTaRes
UsbPrn_GetPortStatus(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	if(sb->wLength < 1) {
		fprintf(stderr,"GetPortStatus with datasize < 1\n");
		return USBTA_NOTHANDLED;
	}
	sb->data_len = 1;
	sb->setup_data[0] = PS_NOT_ERROR | PS_SELECT;
	return USBTA_OK;
}

UsbTaRes
UsbPrn_SoftReset(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	fprintf(stderr,"Djet460 Soft reset not implemented\n");
	return USBTA_NOTHANDLED;
}
/*
 * ----------------------------------------------------------------------
 * Usb Device Requests
 * ----------------------------------------------------------------------
 */

UsbTaRes
usbrq_get_status(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	fprintf(stderr,"USB get status not implemented %d\n",USBDEV(ep)->addr);
	return USBTA_NOTHANDLED;
}

UsbTaRes
usbrq_clear_feature(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	int feature = sb->wValue;
	fprintf(stderr,"USB clear feature not implemented %d\n",USBDEV(ep)->addr);
	sleep(1);
	exit(1);
	switch(feature) {	
		default:
			return USBTA_NOTHANDLED;
	}
}

UsbTaRes
usbrq_get_configuration(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	fprintf(stderr,"USB get configuration not implemented %d\n",USBDEV(ep)->addr);
	exit(1);
	return USBTA_NOTHANDLED;
}

UsbTaRes
usbrq_get_interface(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	fprintf(stderr,"USB get interface not implemented %d\n",USBDEV(ep)->addr);
	return USBTA_NOTHANDLED;
}

UsbTaRes
usbrq_set_address(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	USBDEV(ep)->addr = sb->wValue;
	fprintf(stderr,"USB Set address %d\n",USBDEV(ep)->addr);
	return USBTA_OK;
}

UsbTaRes
UsbDev_SetConfiguration(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	int value = sb->wValue & 0xff; /* Upper byte is reserved */ 
	if(value == 0) {
		fprintf(stderr,"SetConfiguration 0 goto address state is missing\n");
		return USBTA_OK;
	} else if(value == 1) {
		fprintf(stderr,"SetConfiguration 1 was successful\n");
		return USBTA_OK;
	} else {
		fprintf(stderr,"Can not set configuration %d\n",value);
	}
	return USBTA_NOTHANDLED;
}

UsbTaRes
usbrq_set_feature(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	int feature = sb->wValue;
	fprintf(stderr,"USB Set feature not implemented %d\n",USBDEV(ep)->addr);
	switch(feature) {	
		default:
			return USBTA_NOTHANDLED;
	}
	return USBTA_NOTHANDLED;
}

UsbTaRes
usbrq_set_interface(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	int interface;
	int altsetting;
	altsetting = sb->wValue;
	interface = sb->wIndex;
	fprintf(stderr,"Missing check if interface and altsetting exist\n");
	return USBTA_OK;
}

UsbTaRes
usbrq_synch_frame(UsbEndpoint *ep,UsbSetupBuffer *sb)
{
	fprintf(stderr,"USB synch frame not implemented %d\n",USBDEV(ep)->addr);
	return USBTA_NOTHANDLED;
}

/*
 * -------------------------------------------------------------
 * The printer panel button and door open event handlers
 * -------------------------------------------------------------
 */

static void 
PowerPress(struct SigNode * node,int value, void *clientData)
{
	if(value == SIG_LOW) {
		fprintf(stderr,"DJ460 Power Button pressed\n");
	} else {
		fprintf(stderr,"DJ460 Power Button released\n");
	}
}

static void 
CancelPress(struct SigNode * node,int value, void *clientData)
{
	if(value == SIG_LOW) {
		fprintf(stderr,"DJ460 Cancel Button pressed\n");
	} else {
		fprintf(stderr,"DJ460 Cancel Button released\n");
	}
}

static void 
ResumePress(struct SigNode * node,int value, void *clientData)
{
	if(value == SIG_LOW) {
		fprintf(stderr,"DJ460 Resume Button pressed\n");
	} else {
		fprintf(stderr,"DJ460 Resume Button released\n");
	}
}

static void 
DoorDetect(struct SigNode * node,int value, void *clientData)
{
	if(value == SIG_LOW) {
		fprintf(stderr,"DJ460 Door opened\n");
	} else {
		fprintf(stderr,"DJ460 Door closed\n");
	}
}

/*
 * ----------------------------------------------------------------------
 * Constructor for a new printer
 * 	Returns a pointer to an USB device 
 * ----------------------------------------------------------------------
 */

void 
DJet460_New(const char *name,UsbDevice **udevRet /*, ParportPrinter *pprn */) 
{
	DJet460 *dj = sg_new(DJet460); 
	UsbDevice *udev;
	UsbEndpoint *ep0;
	int i;
	dj->lp_devname = Config_ReadVar(name,"lpdevice");
	dj->lp_devfd = -1;
	dj->lp_outdir = Config_ReadVar(name,"output_dir");
	if(dj->lp_outdir) {
		dj->interp = Dj460Interp_New(dj->lp_outdir); 
	}
	CycleTimer_Init(&dj->lp_close_timer,lp_close,dj);
	/* currently restrict to full speed */
	dj->udev = *udevRet = udev = UsbDev_New(dj,USB_SPEED_FULL);
	dj->device_descriptor = dev_descr_tmpl; // *((usb_device_descriptor *) dev_descr_template_fs);
	dj->config_descriptor = conf_descr_tmpl; //*((usb_config_descriptor *) conf_descr_template);
	dj->interface_descriptor = if_descr_tmpl; // *((usb_interface_descriptor *) if_descr_template);
	dj->out_endpoint_descriptor = bulkout_ep_descr_tmpl; // *((usb_endpoint_descriptor *) bulkout_ep_desc_template); 
	dj->in_endpoint_descriptor = bulkin_ep_descr_tmpl; // *((usb_endpoint_descriptor *) bulkin_ep_desc_template);
	dj->int_endpoint_descriptor = int_ep_descr_tmpl; // *((usb_endpoint_descriptor *) int_ep_desc_template);

	/* Register the endpoints */
	ep0 = UsbDev_RegisterEndpoint(udev,0x00,EPNT_TYPE_CONTROL,8,UsbDev_CtrlEp);
	UsbDev_RegisterEndpoint(udev,0x01,EPNT_TYPE_BULK,64,data_out_endpoint);
	UsbDev_RegisterEndpoint(udev,0x81,EPNT_TYPE_BULK,64,data_in_endpoint);
	UsbDev_RegisterEndpoint(udev,0x82,EPNT_TYPE_INT,8,interrupt_endpoint);

	/* Register the requests */
	UsbDev_RegisterRequest(ep0,USBRQ_GET_DESCRIPTOR,0x80,UsbDev_GetDescriptor);
	UsbDev_RegisterRequest(ep0,USBRQ_SET_DESCRIPTOR,0,UsbDev_SetDescriptor);

	UsbDev_RegisterRequest(ep0,USBRQ_CLEAR_FEATURE,0,usbrq_clear_feature);
	UsbDev_RegisterRequest(ep0,USBRQ_GET_CONFIGURATION,0x80,usbrq_get_configuration);
	UsbDev_RegisterRequest(ep0,USBRQ_GET_INTERFACE,0x81,usbrq_get_interface);
	UsbDev_RegisterRequest(ep0,USBRQ_GET_STATUS,0x80,usbrq_get_status);
	UsbDev_RegisterRequest(ep0,USBRQ_SET_ADDRESS,0,usbrq_set_address);
	UsbDev_RegisterRequest(ep0,USBRQ_SET_CONFIGURATION,0,UsbDev_SetConfiguration);
	UsbDev_RegisterRequest(ep0,USBRQ_SET_FEATURE,0,usbrq_set_feature);
	UsbDev_RegisterRequest(ep0,USBRQ_SET_INTERFACE,0x01,usbrq_set_interface);
	UsbDev_RegisterRequest(ep0,USBRQ_SYNCH_FRAME,0x82,usbrq_synch_frame);

	/* Printer class specific requests */
	UsbDev_RegisterRequest(ep0,0x00,0xa1,UsbPrn_GetDeviceId);
	UsbDev_RegisterRequest(ep0,0x01,0xa1,UsbPrn_GetPortStatus);
	UsbDev_RegisterRequest(ep0,0x02,0xa1,UsbPrn_SoftReset);

	/* Register the descriptors */
	UsbDev_RegisterDescriptorHandler(ep0,USB_DT_DEVICE,0,GetDeviceDescr,SetDeviceDescr);
	UsbDev_RegisterDescriptorHandler(ep0,USB_DT_CONFIG,0,GetConfigDescr,SetConfigDescr);
	for(i=0;i<5;i++) {
		UsbDev_RegisterDescriptorHandler(ep0,USB_DT_STRING,i,GetStringDescr,SetStringDescr);
	}
	dj->nPwrGreen = SigNode_New("%s.nPwrGreen",name);
	dj->nPwrRed  = SigNode_New("%s.nPwrRed",name);
	dj->nBlackLed = SigNode_New("%s.nBlackLed",name);
	dj->nColorLed = SigNode_New("%s.nColorLed",name);
	dj->nResumeLed = SigNode_New("%s.nResumeLed",name);
	dj->nPowerButton = SigNode_New("%s.nPowerButton",name);
	dj->nCancelButton = SigNode_New("%s.nCancelButton",name);
	dj->nResumeButton = SigNode_New("%s.nResumeButton",name);
	dj->nDoorDetect = SigNode_New("%s.nDoorDetect",name);
	if(!dj->nPwrGreen || !dj->nPwrRed || !dj->nBlackLed || !dj->nColorLed
		|| !dj->nPowerButton || !dj->nCancelButton || !dj->nResumeButton
		|| !dj->nDoorDetect) 
	{
		fprintf(stderr,"Can not create signal lines for DJ460 Printer Panel\n");
		exit(1);
	}
	SigNode_Set(dj->nBlackLed,SIG_LOW);
	SigNode_Set(dj->nColorLed,SIG_LOW);
	SigNode_Set(dj->nBlackLed,SIG_HIGH);
	SigNode_Set(dj->nColorLed,SIG_HIGH);
	SigNode_Trace(dj->nPowerButton,PowerPress,dj);
	SigNode_Trace(dj->nCancelButton,CancelPress,dj);
	SigNode_Trace(dj->nResumeButton,ResumePress,dj);
	SigNode_Trace(dj->nDoorDetect,DoorDetect,dj);
	SigNode_Set(dj->nPwrGreen,SIG_LOW); /* Start with printer enabled */
}
