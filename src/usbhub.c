/*
 *************************************************************************************************
 *
 * Emulation of an USB Hub 
 *
 * State:
 *      Detected by linux kernel, but does nothing useful
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "usbdevice.h"
#include "usbstdrq.h"
#include "usbhub.h.h"
#include "sgstring.h"

#define         HUBFEAT_C_HUB_LOCAL_POWER       (0)
#define         HUBFEAT_C_HUB_OVER_CURRENT      (1)

/* Destination Port */
#define         HUBFEAT_PORT_CONNECTION         (0)
#define         HUBFEAT_PORT_ENABLE             (1)
#define         HUBFEAT_PORT_SUSPEND            (2)
#define         HUBFEAT_PORT_OVER_CURRENT       (3)
#define         HUBFEAT_PORT_RESET              (4)
#define         HUBFEAT_PORT_POWER              (8)
#define         HUBFEAT_PORT_LOW_SPEED          (9)
#define         HUBFEAT_C_PORT_CONNECTION       (16)
#define         HUBFEAT_C_PORT_ENABLE           (17)
#define         HUBFEAT_C_PORT_SUSPEND          (18)
#define         HUBFEAT_C_PORT_OVER_CURRENT     (19)
#define         HUBFEAT_C_PORT_RESET            (20)
#define         HUBFEAT_PORT_TEST               (21)
#define         HUBFEAT_PORT_INDICATOR          (22)

#define USB_RQT_HUB        (USB_RQT_TCLASS | USB_RQT_RDEVICE)
#define USB_RQT_PORT       (USB_RQT_TCLASS | USB_RQT_ROTHER)

/*
 * The following requests are only valid for RQT_HUB or RQT_PORT
 */
#define	UHUBRQ_GET_STATUS	(0)
#define	UHUBRQ_CLEAR_FEATURE	(1)
#define UHUBRQ_SET_FEATURE	(3)
#define	UHUBRQ_GET_DESCRIPTOR	(6)
#define	UHUBRQ_SET_DESCRIPTOR	(7)
#define UHUBRQ_CLEAR_TT_BUFFER	(8)
#define	UHUBRQ_RESET_TT		(9)
#define	UHUBRQ_GET_TT_STATE	(10)
#define	UHUBRQ_STOP_TT		(11)

typedef struct UsbHub {
	UsbDevice *udev;
} UsbHub;

UsbDevice *
UsbHub_New(const char *name)
{
	UsbHub *hub = sg_new(UsbHub);
	UsbDevice *udev;
	int i;
	udev = hub->udev = UsbDev_New(hub, USB_SPEED_FULL);
	return udev;
}
