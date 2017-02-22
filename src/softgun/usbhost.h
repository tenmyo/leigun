/*
 * -------------------------------------------------------------------------------
 * usbhost.h
 * 	The interface which must be implenented by a USB host if it
 * 	wants to receive USB packets
 * -------------------------------------------------------------------------------
 */
typedef void UsbHost_PktSink(void *dev, const UsbPacket * pkt);
