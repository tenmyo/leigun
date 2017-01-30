
/*
 * ------------------------------------------------------------------
 * Standard requests, for the bRequest field of a SETUP packet.
 * These requests are valid only for Type 0 Requests
 * (bmRequestType bits 5+6).
 * See table 9-4. in USB 2.0 spec
 * ------------------------------------------------------------------
 */
#define USBRQ_GET_STATUS              0x00
#define USBRQ_CLEAR_FEATURE           0x01
#define USBRQ_SET_FEATURE             0x03
#define USBRQ_SET_ADDRESS             0x05
#define USBRQ_GET_DESCRIPTOR          0x06
#define USBRQ_SET_DESCRIPTOR          0x07
#define USBRQ_GET_CONFIGURATION       0x08
#define USBRQ_SET_CONFIGURATION       0x09
#define USBRQ_GET_INTERFACE           0x0A
#define USBRQ_SET_INTERFACE           0x0B
#define USBRQ_SYNCH_FRAME             0x0C

#define USBRQ_SET_ENCRYPTION          0x0D	/* Wireless USB */
#define USBRQ_GET_ENCRYPTION          0x0E
#define USBRQ_SET_HANDSHAKE           0x0F
#define USBRQ_GET_HANDSHAKE           0x10
#define USBRQ_SET_CONNECTION          0x11
#define USBRQ_SET_SECURITY_DATA       0x12
#define USBRQ_GET_SECURITY_DATA       0x13
#define USBRQ_SET_WUSB_DATA           0x14
#define USBRQ_LOOPBACK_DATA_WRITE     0x15
#define USBRQ_LOOPBACK_DATA_READ      0x16
#define USBRQ_SET_INTERFACE_DS        0x17

/* The Direction */
#define USB_RQT_DEVTOHOST	(0x80)	/* DevToHost */
#define USB_RQT_HOSTTODEV	(0x0)

/* The Type */
#define	USB_RQT_TSTD		(0x00)
#define USB_RQT_TCLASS		(1<<5)
#define	USB_RQT_TVENDOR		(2<<5)
#define	USB_RQT_TRESERVED	(3<<5)
/* The receiver */
#define USB_RQT_RDEVICE		(0)
#define USB_RQT_RINTFACE	(1)
#define USB_RQT_RENDPNT		(2)
#define	USB_RQT_ROTHER		(3)

#define USB_RQT_DEVINSTD	(USB_RQT_RDEVICE | USB_RQT_DIRIN  | USB_RQT_TSTD)
#define USB_RQT_DEVOUTSTD	(USB_RQT_RDEVICE | USB_RQT_DIROUT | USB_RQT_TSTD)

#if 0
#define USB_STDRQ_OK			(0)
#define USB_STDRQ_ERROR			(-1)
#define USB_STDRQ_ENOHANDLER		(-2)
#endif

//int Usb_DoStdRequest(UsbDevice *udev,UsbTransaction *ta,UsbPacket *reply);
UsbTaRes UsbStdRq_GetDescriptor(UsbDevice * udev, UsbTransaction * ta, UsbPacket * reply);
UsbTaRes UsbStdRq_SetDescriptor(UsbDevice * udev, UsbTransaction * ta, UsbPacket * reply);
