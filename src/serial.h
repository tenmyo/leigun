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
#include <termios.h>


typedef uint16_t UartChar;

struct UartPort;

typedef void UartRxEventProc(void *dev);
typedef void UartTxEventProc(void *dev);
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
	int		(*uart_cmd)(struct SerialDevice *,UartCmd *cmd);
        void            (*stop_tx)(struct SerialDevice *);
        void            (*start_tx)(struct SerialDevice *);
        void            (*start_rx)(struct SerialDevice *);
        void            (*stop_rx)(struct SerialDevice *);

	int		(*write)(struct SerialDevice *,const UartChar *buf,int count);
	int		(*read)(struct SerialDevice *,UartChar *buf,int count);
} SerialDevice;

struct UartPort {
	SerialDevice *serial_device;
	void *owner;
	UartRxEventProc *rxEventProc;
	UartTxEventProc *txEventProc;
	UartStatChgProc *statProc;
};


/*
 * --------------------------------------------------------------------
 * The Uart_XYEventProcs are called by a serial device emulator
 * whenever there is some message to the Uart emulator
 * --------------------------------------------------------------------
 */
static inline void
Uart_RxEvent(SerialDevice *serdev) {
 	if(serdev->uart->rxEventProc) {
                serdev->uart->rxEventProc(serdev->uart->owner);
        }
}

static inline void
Uart_TxEvent(SerialDevice *serdev) {
 	if(serdev->uart->txEventProc) {
                serdev->uart->txEventProc(serdev->uart->owner);
        }
}

/*
 * -----------------------------------------------------------------------
 * Uart Start TX allows the uart emulator to transmit data and to call
 * the port->srcProc of the Uart Port when it requires data for sending
 * -----------------------------------------------------------------------
 */
static inline void 
SerialDevice_StartTx(UartPort *port) 
{
	//fprintf(stderr,"Uart StartTx\n");
	if(port->serial_device && port->serial_device->start_tx) {
		port->serial_device->start_tx(port->serial_device);
	}
}

/*
 * -----------------------------------------------------------------------
 * Uart_StopTx disallows the uart emulator to transmit data. It will not
 * call port->srcProc after this call
 * -----------------------------------------------------------------------
 */
static inline void 
SerialDevice_StopTx(UartPort *port)
{
	if(port->serial_device && port->serial_device->stop_tx) {
		port->serial_device->stop_tx(port->serial_device);
	}
}

/*
 * -----------------------------------------------------------------------
 * Uart_StartRx allows the Uart emulator to receive data and to call
 * the uart->sinkProc when data are available. 
 * -----------------------------------------------------------------------
 */
static inline void 
SerialDevice_StartRx(UartPort *port) 
{
	if(port->serial_device && port->serial_device->start_rx) {
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
SerialDevice_StopRx(UartPort *port) 
{
	if(port->serial_device && port->serial_device->stop_rx) {
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
static inline int
SerialDevice_Cmd(UartPort *port,UartCmd *cmd) 
{
	if(port->serial_device && port->serial_device->uart_cmd) {
		return port->serial_device->uart_cmd(port->serial_device,cmd);
	} else {
		return -1;
	}
}

/*
 * -------------------------------------------------------------
 * Read the received data from the UART. Typically called
 * in the RX-Event handler. It returns the number of bytes
 * written or <0 on error
 * -------------------------------------------------------------
 */

static inline int 
SerialDevice_Read(UartPort *port,UartChar *buf,int len)
{
	if(port->serial_device && port->serial_device->read) {
		return port->serial_device->read(port->serial_device,buf,len);
	} else {
		return -1;
	}
}

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
SerialDevice_Write(UartPort *port,const UartChar *buf,int len)
{
	if(port->serial_device && port->serial_device->write) {
		return port->serial_device->write(port->serial_device,buf,len);
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
UartPort *
Uart_New(const char *uart_name,UartRxEventProc *rxproc,UartTxEventProc *txproc,UartStatChgProc *statproc,void *owner);
/*
 * -------------------------------------------------------------------
 * Register new Serial Device emulator modules
 * -------------------------------------------------------------------
 */
void SerialModule_Register(const char *type,SerialDevice_Constructor *newSerdev);
#endif
