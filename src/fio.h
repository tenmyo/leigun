#ifndef FIO_H
#define FIO_H
#include <time.h>
#ifdef _POSIX_C_SOURCE
#include <sys/time.h>
#endif
#include "xy_tree.h"

void FIO_HandleInput(void);
void FIO_WaitEventTimeout(struct timespec *timeout);

typedef void FIO_FileProc(void *clientData, int mask);

/* define the event types */
#define FIO_READABLE (1)
#define FIO_WRITABLE (2)
/* 
 * ----------------------------------------------------------
 * The filehandler 
 * 	proc is invoked with clientData and event mask
 *	as argument whenever an event from mask happens on fd
 * ----------------------------------------------------------
 */
typedef struct FIO_FileHandler {
	int fd;
	FIO_FileProc *proc;
	void *clientData;
	struct FIO_FileHandler *next;
	struct FIO_FileHandler *prev;
	char mask;
	char busy;
} FIO_FileHandler;

typedef void FIO_Accept(int fd, char *host, unsigned short port, void *clientData);
typedef struct FIO_TcpServer {
	int sock;
	FIO_FileHandler acc_fh;
	FIO_Accept *proc;
	void *clientData;
} FIO_TcpServer;

/* 
 * ---------------------------------------------------------------
 * Host timer is runing in the host domain. 
 * This means it uses the time from the host machine and not
 * the time in the emulated target (which is typically slower)
 * POSIX.1b monotonic timers are used for this
 * ---------------------------------------------------------------
 */
typedef struct HostTimer {
	struct timespec timeout;
	xy_node node;
} HostTimer;

/* 
 * ----------------------------------------------------------
 * AddFileHandler 
 *	register a proc which is invoked when an event from
 *	mask happens.
 * ----------------------------------------------------------
 */
void FIO_AddFileHandler(FIO_FileHandler * fh, int fd, int mask, FIO_FileProc * proc,
			void *clientData);
void FIO_RemoveFileHandler(FIO_FileHandler * fh);
/* 
 * --------------------------------------------------------------
 *  InitTcpServer
 *	Create a TCP-server socket and register a accept-proc 
 *	which is invoked whenever someone tries to connect the 
 *	TCP-socket.
 * -------------------------------------------------------------
 */
int FIO_InitTcpServer(FIO_TcpServer *, FIO_Accept * proc, void *clientData,
		      char *host, unsigned short port);
void FIO_CloseTcpServer(FIO_TcpServer * tserv);

#define FIO_OPEN_ASYNC (1)
#define FIO_OPEN_SYNC  (0)

void FIO_Init();
#endif
