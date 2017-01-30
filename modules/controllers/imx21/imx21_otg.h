#include <bus.h>
#include <usbdevice.h>
BusDevice *IMX21Otg_New(const char *name);
void IMX21Otg_Plug(BusDevice * bdev, UsbDevice * usbdev, unsigned int port);
void IMX21Otg_Unplug(BusDevice * bdev, unsigned int port);
