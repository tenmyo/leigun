/*
 **********************************************************************************
 * Interface between Serial chip emulator and real terminal
 * 
 * (C) 2006 Jochen Karrer 
 *
 * State: not implemented
 **********************************************************************************
 */

#ifndef _SERIAL_H
#define _SERIAL_H
#include <stdint.h>
#include <stdbool.h>
#include <termios.h>
#include "cycletimer.h"

typedef uint16_t UartChar;

struct UartPort;

typedef void UartRxEventProc(void *dev, UartChar c);
typedef bool UartFetchTxCharProc(void *dev, UartChar * c);
typedef void UartStatChgProc(void *dev);

#define UART_OPC_SET_BAUDRATE	(1)
#define UART_OPC_SET_CSIZE	(2)
#define UART_OPC_CRTSCTS	(3)
#define		UART_RTS_ACT	(1)
#define		UART_RTS_INACT	(0)
#define UART_OPC_PAREN		(4)
#define UART_OPC_PARODD		(5)
#define UART_OPC_SET_RTS	(6)
#define UART_OPC_SET_DTR	(7)
#define UART_OPC_GET_RTS	(6)
#define UART_OPC_GET_DTR	(7)
#define UART_OPC_GET_DCD	(8)
#define UART_OPC_GET_RI		(9)
#define UART_OPC_GET_DSR	(10)
#define UART_OPC_GET_CTS	(11)

typedef struct UartCmd {
	int opcode;
	int flush;
	uint32_t arg;
	uint32_t retval;
} UartCmd;

typedef struct UartPort UartPort;
/*
 * ------------------------------------------------------------
 * This is what a SerialDevice Emulator must implement
 * ------------------------------------------------------------
 */
typedef struct SerialDevice {
	void *owner;
	UartPort *uart;

	int (*uart_cmd) (struct SerialDevice *, UartCmd * cmd);

	void (*start_rx) (struct SerialDevice *);
	void (*stop_rx) (struct SerialDevice *);

	int (*write) (struct SerialDevice *, const UartChar * buf, int count);
	int (*read) (struct SerialDevice *, UartChar * buf, int count);

	CycleTimer  rxTimer;
	bool rx_enabled;
} SerialDevice;

struct UartPort {
	SerialDevice *serial_device;
	void *owner;
	uint32_t rx_baudrate;
	uint32_t nsPerTxChar;
	uint32_t tx_baudrate;
	uint32_t nsPerRxChar;
	uint16_t tx_csize_mask;
	uint16_t rx_csize_mask;
	uint8_t tx_csize;
	uint8_t rx_csize;
	uint8_t halfstopbits;
	CycleTimer txTimer;
	UartRxEventProc *rxEventProc;
	UartFetchTxCharProc *txFetchChar;
	UartStatChgProc *statProc;
	bool rx_enabled;
	bool tx_enabled;
};

/*
 * --------------------------------------------------------------------
 * The Uart_XYEventProcs are called by a serial device emulator
 * whenever there is some message to the Uart emulator
 * --------------------------------------------------------------------
 */
static inline void
Uart_RxEvent(SerialDevice * serdev)
{
	int result;
	if (serdev->uart->rxEventProc) {
		UartChar c;
		if (serdev->uart 
		    && serdev->read) {
			result =
			    serdev->read(serdev->uart->serial_device, &c, 1);
		} else {
			return;
		}
		if (result == 1) {
			serdev->uart->rxEventProc(serdev->uart->owner, c);
		}
	}
}

#if 0
static inline void
Uart_TxEvent(SerialDevice * serdev)
{
	if (serdev->uart->txEventProc) {
		serdev->uart->txEventProc(serdev->uart->owner);
	}
}
#endif

/*
 * -----------------------------------------------------------------------
 * Uart Start TX allows the uart emulator to transmit data and to call
 * the port->srcProc of the Uart Port when it requires data for sending
 * -----------------------------------------------------------------------
 */
void SerialDevice_StartTx(UartPort * port);

/*
 * -----------------------------------------------------------------------
 * Uart_StopTx disallows the uart emulator to transmit data. It will not
 * call port->srcProc after this call
 * -----------------------------------------------------------------------
 */
static inline void
SerialDevice_StopTx(UartPort * port)
{
	port->tx_enabled = false;
}

/*
 * -----------------------------------------------------------------------
 * Uart_StartRx allows the Uart emulator to receive data and to call
 * the uart->sinkProc when data are available. 
 * -----------------------------------------------------------------------
 */
static inline void
SerialDevice_StartRx(UartPort * port)
{
	port->rx_enabled = true;
	if (port->serial_device && port->serial_device->start_rx) {
		port->serial_device->start_rx(port->serial_device);
	}
}

/*
 * ---------------------------------------------------------------------------------------
 * Uart_StopRx dissallows the Uart emulator to receive data (deinstalls the event handler
 * for incoming data). the uart->sinkProc will not be called again
 * ---------------------------------------------------------------------------------------
 */
static inline void
SerialDevice_StopRx(UartPort * port)
{
	port->rx_enabled = false;
	if (port->serial_device && port->serial_device->stop_rx) {
		port->serial_device->stop_rx(port->serial_device);
	}
}

/*
 * -------------------------------------------------------------------------------
 * Uart_Cmd controls the UART  with one of the UART_OPC* defined above
 * Its intention is to control baudrate, set Modem control lines, character
 * sizes and parity 
 * -------------------------------------------------------------------------------
 */
int SerialDevice_Cmd(UartPort * port, UartCmd * cmd);

/*
 * -------------------------------------------------------------
 * Write data for tranmission by the UART. The first time
 * this is called when there is some data to send. Next time
 * it is called from the  TX-Event handler (which tells that
 * there is room in the output fifo). It returns the number
 * of bytes which was written or < 0 on error
 * -------------------------------------------------------------
 */
static inline int
SerialDevice_Write(UartPort * port, const UartChar * buf, int len)
{
	if (port->serial_device && port->serial_device->write) {
		return port->serial_device->write(port->serial_device, buf, len);
	} else {
		return -1;
	}
}

typedef SerialDevice *SerialDevice_Constructor(const char *name);
/*
 * --------------------------------------------------------------------------------
 * Uart_New
 * 	Constructor for a new Uart. 
 *	rxproc is the handler which is called when data is availabe
 *	txproc is the handler which is called when a transmission is done
 *	statproc is called when a status change is detected
 * --------------------------------------------------------------------------------
 */
UartPort *Uart_New(const char *uart_name, UartRxEventProc * rxproc, UartFetchTxCharProc * txproc,
		   UartStatChgProc * statproc, void *owner);
/*
 * -------------------------------------------------------------------
 * Register new Serial Device emulator modules
 * -------------------------------------------------------------------
 */
void SerialModule_Register(const char *type, SerialDevice_Constructor * newSerdev);
#endif
