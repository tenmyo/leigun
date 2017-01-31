/*
 ********************************************************************************************
 * Interface to USB devices 
 * (C) 2006 Jochen Karrer 
 ********************************************************************************************
 */

#ifndef __USBDEV_H
#define __USBDEV_H
#include <usbproto.h>
#include <usbhost.h>
#include <stdint.h>
#include <xy_hash.h>

/* use bitwise ? */
#define le16 uint16_t

#define EPNT_TYPE_CONTROL	(0)
#define EPNT_TYPE_ISO		(1)
#define EPNT_TYPE_BULK		(2)
#define EPNT_TYPE_INT		(5)

#define EPNT_DIR_IN		(1)
#define EPNT_DIR_OUT		(2)

/*
 * ---------------------------------------------------------------------
 * These are the return values of the USB request handler provided
 * by the USB device implementation. 
 * ---------------------------------------------------------------------
 */
#define USB_RET_NAK             (-1)
#define USB_RET_STALL           (-2)
#define USB_RET_NYET            (-3)

#define USBDEV(ep)	((ep)->usbdev)

struct UsbDevice;

typedef struct UsbToken {
	uint8_t pid;
	uint8_t addr;
	uint8_t epnum;
	/* uint16_t frmnumb ? */
} UsbToken;

typedef struct UsbSetupBuffer {
	/* 
	 * The following fileds are filled  with the 8 Byte Setup packet if the request 
	 * is a control  request
	 */
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;

	uint8_t *setup_data;
	int transfer_count;
	int data_len;
	int data_bufsize;
} UsbSetupBuffer;

/*
 * -----------------------------------------------------------
 * The UsbTransaction already converted to host byteorder
 * -----------------------------------------------------------
 */
typedef struct UsbTransaction {
	UsbToken token;
	const uint8_t *data;
	int data_len;
} UsbTransaction;

typedef struct UsbEndpoint UsbEndpoint;

typedef int UsbTransactionProc(struct UsbDevice *, UsbEndpoint *, UsbTransaction *,
			       UsbPacket * reply);

struct UsbEndpoint {
	struct UsbDevice *usbdev;
	int type;
	int maxpacket;
	int direction;
	unsigned int toggle;

	/* Out Endpoint is input of device */
	UsbTransactionProc *doTransaction;

	int status;
	int epaddr;

	/* Only Control endpoints */
	UsbSetupBuffer *setup_buf;
	XY_HashTable requestHash;
	XY_HashTable descriptorHash;
};

typedef enum usb_device_speed {
	USB_SPEED_UNKNOWN = 0,	/* enumerating */
	USB_SPEED_LOW, USB_SPEED_FULL,	/* */
	USB_SPEED_HIGH,		/* */
	USB_SPEED_VARIABLE,	/* wireless (usb 2.5) */
} usb_device_speed;

enum usb_device_state {
	/* NOTATTACHED isn't in the USB spec, and this state acts
	 * the same as ATTACHED ... but it's clearer this way.
	 */
	USB_STATE_NOTATTACHED = 0,
	/* the chapter 9 device states */
	USB_STATE_ATTACHED,
	USB_STATE_POWERED,
	USB_STATE_DEFAULT,	/* limited function */
	USB_STATE_ADDRESS,
	USB_STATE_CONFIGURED,	/* most functions */

	USB_STATE_POWERED_SUSPENDED,
	USB_STATE_DEFAULT_SUSPENDED,
	USB_STATE_ADDRESS_SUSPENDED,
	USB_STATE_CONFIGURED_SUSPENDED
};

typedef enum UsbTaRes {
	USBTA_OK = 0,
	USBTA_ERROR = -1,
	USBTA_IGNORED = -2,
	USBTA_NOTHANDLED = -3
} UsbTaRes;

/*
 * Descriptor types bits from USB 2.0 spec table 9.5
 */
#define USB_DT_DEVICE                   0x01
#define USB_DT_CONFIG                   0x02
#define USB_DT_STRING                   0x03
#define USB_DT_INTERFACE                0x04
#define USB_DT_ENDPOINT                 0x05
#define USB_DT_DEVICE_QUALIFIER         0x06
#define USB_DT_OTHER_SPEED_CONFIG       0x07
#define USB_DT_INTERFACE_POWER          0x08

/* these are from a minor usb 2.0 revision (ECN) */
#define USB_DT_OTG                      0x09
#define USB_DT_DEBUG                    0x0a
#define USB_DT_INTERFACE_ASSOCIATION    0x0b

/**
 * Some values can only be used at device level, others only at inteface level
 */
#define USB_CLASS_INTERFACE     (0x00)	/* Defined at interface level */
#define USB_CLASS_AUDIO         (0x01)	/* I */
#define USB_CLASS_CDC           (0x02)	/* ID */
#define USB_CLASS_HID           (0x03)	/* I */
#define USB_CLASS_PHYSICAL      (0x05)	/* I */
#define USB_CLASS_IMAGE         (0x06)	/* I */
#define USB_CLASS_PRINTER       (0x07)	/* I */
#define USB_CLASS_STORAGE       (0x08)	/* I */
#define USB_CLASS_HUB           (0x09)	/* D */
#define USB_CLASS_CDCDATA       (0x0a)	/* I */
#define USB_CLASS_SMARTCARD     (0x0b)	/* I */
#define USB_CLASS_CONTSEC       (0x0d)	/* I */
#define USB_CLASS_VIDEO         (0x0e)	/* I */
#define USB_CLASS_PHEALTHCARE   (0x0f)	/* I */
#define USB_CLASS_AUDIOVIDEO    (0x10)	/* I */
#define USB_CLASS_DIAGNOSTIC    (0xDC)	/* ID */
#define USB_CLASS_WIRELESS      (0xE0)	/* I */
#define USB_CLASS_MISC          (0xEF)	/* ID */
#define USB_CLASS_VENDOR_SPEC   (0xFF)	/* ID */
/* 
 * Wireless USB spec 
 */
#define USB_DT_SECURITY                 0x0c
#define USB_DT_KEY                      0x0d
#define USB_DT_ENCRYPTION_TYPE          0x0e
#define USB_DT_BOS                      0x0f
#define USB_DT_DEVICE_CAPABILITY        0x10
#define USB_DT_WIRELESS_ENDPOINT_COMP   0x11

/*
 * -------------------------------------------------------
 * The usb device descriptor
 * -------------------------------------------------------
 */

typedef struct usb_device_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	le16 bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	le16 idVendor;
	le16 idProduct;
	le16 bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
} __attribute__ ((packed)) usb_device_descriptor;

typedef struct usb_config_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	le16 wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
} __attribute__ ((packed)) usb_config_descriptor;

typedef struct usb_string_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	le16 wData[1];		/* UTF-16LE encoded */
} __attribute__ ((packed)) usb_string_descriptor;

/*
 * ---------------------------------------------------------------
 * Interface descriptor is returned as part of config descriptor
 * and can not directly accesses with GetDescr, SetDescr
 * ---------------------------------------------------------------
 */
typedef struct usb_interface_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} __attribute__ ((packed)) usb_interface_descriptor;

/* USB_DT_ENDPOINT: Endpoint descriptor */
typedef struct usb_endpoint_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	le16 wMaxPacketSize;
	uint8_t bInterval;

	/* NOTE:  these two are _only_ in audio endpoints. */
	/* use USB_DT_ENDPOINT*_SIZE in bLength, not sizeof. */
	uint8_t bRefresh;
	uint8_t bSynchAddress;
} __attribute__ ((packed)) usb_endpoint_descriptor;

#define USB_DT_ENDPOINT_SIZE            7
#define USB_DT_ENDPOINT_AUDIO_SIZE      9	/* Audio extension */

/* USB_DT_DEVICE_QUALIFIER: Device Qualifier descriptor */
typedef struct usb_qualifier_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	le16 bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint8_t bNumConfigurations;
	uint8_t bRESERVED;
} __attribute__ ((packed)) usb_qualifier_descriptor;

/* USB_DT_OTG (from OTG 1.0a supplement) */
typedef struct usb_otg_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	uint8_t bmAttributes;	/* support for HNP, SRP, etc */
} __attribute__ ((packed)) usb_otg_descriptor;

/* USB_DT_DEBUG:  for special highspeed devices, replacing serial console */
typedef struct usb_debug_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	/* bulk endpoints with 8 byte maxpacket */
	uint8_t bDebugInEndpoint;
	uint8_t bDebugOutEndpoint;
} usb_debug_descriptor;

/* USB_DT_INTERFACE_ASSOCIATION: groups interfaces */
typedef struct usb_interface_assoc_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	uint8_t bFirstInterface;
	uint8_t bInterfaceCount;
	uint8_t bFunctionClass;
	uint8_t bFunctionSubClass;
	uint8_t bFunctionProtocol;
	uint8_t iFunction;
} __attribute__ ((packed)) usb_interface_assoc_descriptor;

typedef struct usb_security_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	le16 wTotalLength;
	uint8_t bNumEncryptionTypes;
} usb_security_descriptor;

/* USB_DT_KEY:  used with {GET,SET}_SECURITY_DATA; only public keys
 * may be retrieved.
 */
typedef struct usb_key_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	uint8_t tTKID[3];
	uint8_t bReserved;
	uint8_t bKeyData[0];
} usb_key_descriptor;

/* USB_DT_ENCRYPTION_TYPE:  bundled in DT_SECURITY groups */
typedef struct usb_encryption_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	uint8_t bEncryptionType;
#define USB_ENC_TYPE_UNSECURE           0
#define USB_ENC_TYPE_WIRED              1	/* non-wireless mode */
#define USB_ENC_TYPE_CCM_1              2	/* aes128/cbc session */
#define USB_ENC_TYPE_RSA_1              3	/* rsa3072/sha1 auth */
	uint8_t bEncryptionValue;	/* use in SET_ENCRYPTION */
	uint8_t bAuthKeyIndex;
} usb_encryption_descriptor;

/* USB_DT_BOS:  group of wireless capabilities */
typedef struct usb_bos_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	le16 wTotalLength;
	uint8_t bNumDeviceCaps;
} usb_bos_descriptor;

/* USB_DT_DEVICE_CAPABILITY:  grouped with BOS */
typedef struct usb_dev_cap_header {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDevCapabilityType;
} usb_dev_cap_header;

typedef struct usb_wireless_cap_descriptor {	/* Ultra Wide Band */
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDevCapabilityType;

	uint8_t bmAttributes;
#define USB_WIRELESS_P2P_DRD            (1 << 1)
#define USB_WIRELESS_BEACON_MASK        (3 << 2)
#define USB_WIRELESS_BEACON_SELF        (1 << 2)
#define USB_WIRELESS_BEACON_DIRECTED    (2 << 2)
#define USB_WIRELESS_BEACON_NONE        (3 << 2)
	le16 wPHYRates;		/* bit rates, Mbps */
#define USB_WIRELESS_PHY_53             (1 << 0)	/* always set */
#define USB_WIRELESS_PHY_80             (1 << 1)
#define USB_WIRELESS_PHY_107            (1 << 2)	/* always set */
#define USB_WIRELESS_PHY_160            (1 << 3)
#define USB_WIRELESS_PHY_200            (1 << 4)	/* always set */
#define USB_WIRELESS_PHY_320            (1 << 5)
#define USB_WIRELESS_PHY_400            (1 << 6)
#define USB_WIRELESS_PHY_480            (1 << 7)
	uint8_t bmTFITXPowerInfo;	/* TFI power levels */
	uint8_t bmFFITXPowerInfo;	/* FFI power levels */
	le16 bmBandGroup;
	uint8_t bReserved;
} usb_wireless_cap_descriptor;

/* USB_DT_WIRELESS_ENDPOINT_COMP:  companion descriptor associated with
 * each endpoint descriptor for a wireless device
 */
typedef struct usb_wireless_ep_comp_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;

	uint8_t bMaxBurst;
	uint8_t bMaxSequence;
	le16 wMaxStreamDelay;
	le16 wOverTheAirPacketSize;
	uint8_t bOverTheAirInterval;
	uint8_t bmCompAttributes;
#define USB_ENDPOINT_SWITCH_MASK        0x03	/* in bmCompAttributes */
#define USB_ENDPOINT_SWITCH_NO          0
#define USB_ENDPOINT_SWITCH_SWITCH      1
#define USB_ENDPOINT_SWITCH_SCALE       2
} usb_wireless_ep_comp_descriptor;

/*
 * ---------------------------------------------------------
 * UsbDevice structure
 * 	Many fields stolen from linux kernel usb_device
 * ---------------------------------------------------------
 */
typedef struct UsbDevice {
	void *owner;
	void *usbhost;
	UsbHost_PktSink *hostsink;
	int addr;		/* Address on USB bus (0-127) */
	enum usb_device_state state;
	enum usb_device_speed speed;
	int ta_state;		/* Transaction state defined in figure 8.21 */
	UsbTransaction transaction;	/* Assembly buffer for the current request */
	UsbEndpoint *in_endpnt[16];
	UsbEndpoint *out_endpnt[16];
	uint8_t reply_buf[1024];	/* enough for one max sized paket */
	int reply_wp;		/* number of bytes in reply_buf */
} UsbDevice;

typedef UsbTaRes UsbDescriptorProc(UsbEndpoint *, UsbSetupBuffer *);

typedef struct UsbDescriptorHandler {
	UsbDescriptorProc *getDescriptor;
	UsbDescriptorProc *setDescriptor;
} UsbDescriptorHandler;

typedef UsbTaRes UsbRequestHandler(UsbEndpoint *, UsbSetupBuffer *);

/*
 * ------------------------------------------------------------------------------------
 * UsbDev_Feed
 * 	The Host Port Feeds its data with this proc into an USB device
 * ------------------------------------------------------------------------------------
 */
void UsbDev_Feed(void *dev, const UsbPacket * packet);

/*
 * ------------------------------------------------------------------------
 * An usb device uses the packet sink provided by the USB host.
 * The USB host has to register its electrical interface (the packet sink) 
 * at the USB device
 * ------------------------------------------------------------------------
 */
void UsbDev_RegisterPacketSink(UsbDevice * dev, void *host, UsbHost_PktSink *);
void UsbDev_UnregisterPacketSink(UsbDevice * dev, void *host, UsbHost_PktSink *);

/*
 * ------------------------------------------------------------------------
 * The Interface which is inherited by the device emulators
 * ------------------------------------------------------------------------
 */
UsbDevice *UsbDev_New(void *owner, usb_device_speed speed);

UsbEndpoint *UsbDev_RegisterEndpoint(UsbDevice * udev, int epaddr, int eptype, int maxpacket,
				     UsbTransactionProc *);

void UsbDev_RegisterRequest(UsbEndpoint *, uint8_t rq, uint8_t rqt, UsbRequestHandler * proc);

/*
 * ---------------------------------------------------------
 * Here the Library part starts
 * ---------------------------------------------------------
 */
/*
 * -----------------------------------------------------------------------------------------------
 * UsbDev_CtrlEp
 * A device emulator can use the default EP0 implementation instead of implementing an own
 * In case of use it has to register its requests with UsbDev_RegisterRequest
 * -----------------------------------------------------------------------------------------------
 */
int UsbDev_CtrlEp(UsbDevice * udev, UsbEndpoint * ep, UsbTransaction * ta, UsbPacket * reply);
/*
 * -------------------------------------------------------------------------------------
 * An USB device which calls UsbDev_GetDescriptor/SetDescriptor when it receives 
 * a GET/SET_DESCRIPTOR on endpoint 0 has to use UsbDev_RegisterDescriptorHandler  
 * to make its handlers known
 * -------------------------------------------------------------------------------------
 */
UsbTaRes UsbDev_GetDescriptor(UsbEndpoint * ep, UsbSetupBuffer * sb);
UsbTaRes UsbDev_SetDescriptor(UsbEndpoint * ep, UsbSetupBuffer * sb);
void UsbDev_RegisterDescriptorHandler(UsbEndpoint *, int dt, int index, UsbDescriptorProc * get,
				      UsbDescriptorProc * set);

//void UsbDevice_RegisterDescriptorHandler(UsbDevice *udev,int dt,int index,UsbDescriptorProc *get,UsbDescriptorProc *set);

#endif				// __USBDEV_H
