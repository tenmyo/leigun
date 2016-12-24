/*
 *************************************************************************************************
 *
 * File IO  
 *	Modified version  from Jochen Karrers 
 * 	xy_tools Event-IO System. It uses a
 * 	separate IO-Thread which sends signals
 * 	to the main thread, because the main thread 
 * 	is busy with emulating the CPU. 
 *
 * SIGIO implementation was substituted by a dual
 * thread implementation because SIGIO
 * did not work on Windows and also not on all Linux versions 
 * because kernel or glibc was broken in SUSE Linux 9.1 
 * with kernel 2.6.5 
 *
 * Copyright 2002 2004 2006 Jochen Karrer. All rights reserved.
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
#include "compiler_extensions.h"
#include "fio.h"

#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef __unix__
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <netdb.h>
#include <pthread.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "wsock32.lib")
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#define read(a,b,c) recv(a,b,c,0)
#define close(fd) closesocket(fd)
#define write(a,b,c) send(a,b,c,0)
#define clock_gettime(clk_id, tp) timespec_get(tp, TIME_UTC)
#include <thr/threads.h>
#define pthread_t thrd_t
#define pthread_mutex_t mtx_t
#define pthread_cond_t cnd_t
#define pthread_create(tid, attr, func, arg) thrd_create(tid, func, arg)
#define pthread_mutex_lock(mtx) mtx_lock(mtx)
#define pthread_mutex_unlock(mtx) mtx_unlock(mtx)
#define pthread_cond_wait(cond, mtx) cnd_wait(cond, mtx)
#define pthread_cond_signal(cond) cnd_signal(cond)
#define pselect(fds, rfds, wfds, efds, timeout, set) select(fds, rfds, wfds, efds, timeout)
#endif


#ifndef NO_ARM9
#include "arm9cpu.h"
#endif
#include "mainloop_events.h"
#include "xy_tree.h"

typedef struct XYFdSets {
	fd_set rfds;
	fd_set wfds;
} XYFdSets;

#ifdef __CYGWIN__
#define USE_DUMMYPIPE 1
#else
#define USE_DUMMYPIPE 0
#endif

static int maxfd = 0;
static XYFdSets g_fdsets;
static FIO_FileHandler *g_currentFH = NULL;
static FIO_FileHandler *fileh_head = NULL;
#if USE_DUMMYPIPE == 1
static int pipefd[2];
#endif

static pthread_t iothread;
#ifdef __unix__
static pthread_mutex_t io_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t io_done = PTHREAD_COND_INITIALIZER;
#else
static pthread_mutex_t io_mutex;
static pthread_cond_t io_done;
#endif
static XY_Tree host_timer_tree;
static xy_node *first_host_timer_node = NULL;
static pthread_mutex_t timer_mutex;

#ifdef __unix
static void
init_recursive_mutex(pthread_mutex_t * mutex)
{
	pthread_mutexattr_t attr;
	int result;
	result = pthread_mutexattr_init(&attr);
	if (result != 0) {
		fprintf(stderr, "pthread_mutexattr_init: %s\n", strerror(result));
		exit(1);
	}
	result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (result != 0) {
		fprintf(stderr, "pthread_mutexattr_settype: %s\n", strerror(result));
		exit(1);
	}
	pthread_mutex_init(mutex, &attr);
	pthread_mutexattr_destroy(&attr);
}
#endif

static inline void
get_timer_lock(void)
{
	pthread_mutex_lock(&timer_mutex);
}

static inline void
put_timer_lock(void)
{
	pthread_mutex_unlock(&timer_mutex);
}

/* -----------------------------------------------
 * Helper functions for timers in host domain
 * -----------------------------------------------
 */
static int
_is_later(const void *t1, const void *t2)
{
	struct timespec *time1 = (struct timespec *)t1;
	struct timespec *time2 = (struct timespec *)t2;
	if (time2->tv_sec < time1->tv_sec) {
		return 1;
	} else if (time2->tv_sec == time1->tv_sec) {
		if (time2->tv_nsec < time1->tv_nsec) {
			return 1;
		}
	}
	return 0;
}

static inline int
is_later(struct timespec *time1, struct timespec *time2)
{
	return _is_later(time1, time2);
}

static inline int
is_timeouted(HostTimer * th, struct timespec *currenttime)
{
	return is_later(currenttime, &th->timeout);
}

/*
 * ---------------------------------------------------
 * Calculate the time until timer times out.
 * If the timer already is timeouted the remaining
 * time is set to 0
 * ---------------------------------------------------
 */
static int
calculate_remaining(HostTimer * th, struct timespec *remaining)
{
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	if (time.tv_nsec > th->timeout.tv_nsec) {
		remaining->tv_nsec = (1000000000 + th->timeout.tv_nsec) - time.tv_nsec;
		if ((time.tv_sec + 1) <= th->timeout.tv_sec) {
			remaining->tv_sec = th->timeout.tv_sec - time.tv_sec - 1;
		} else {
			remaining->tv_sec = 0;
			remaining->tv_nsec = 0;
			return 0;
		}
	} else {
		remaining->tv_nsec = (th->timeout.tv_nsec - time.tv_nsec);
		if (time.tv_sec <= th->timeout.tv_sec) {
			remaining->tv_sec = th->timeout.tv_sec - time.tv_sec;
		} else {
			remaining->tv_sec = 0;
			remaining->tv_nsec = 0;
			return 0;
		}
	}
	return 1;
}

/* 
 * --------------------------------------
 * Handle SIGUSR1 does nothing,
 * the signal is only needed to wake up 
 * the select system call of the iothread
 * when the fds changes; 
 * --------------------------------------
 */
static void
handle_sigusr1()
{
#if USE_DUMMYPIPE == 1
	char c = 0;
	write(pipefd[1], &c, 1);
#endif
}

/*
 * ----------------------------------------------------------
 * run_iothread
 *	The IO-thread waits for IO using select. 
 *	The main thread will get an event using the
 *	variable mainloop_event_pending, and a hint
 *	that it was an IO-event in the variable 
 *      mainloop_event_io.
 *
 * 	The IO-thread waits until the mainloop handled all IO. 
 *	Whenever the filedescriptor set changes the iothread
 * 	is waked up using SIGUSR1, which does nothing
 *	but interrupting select and calling it again with
 *	a new file descritor set.
 *	This function does not return.
 *
 *  pselect:
 *		 http://lwn.net/Articles/176911/ 
 * ----------------------------------------------------------
 */

void *
run_iothread(void *cd)
{
	fd_set rfds, wfds;
	int result;
	struct timespec timeout;
#ifndef NO_SIGNAL
	struct sigaction sa;
#if USE_DUMMYPIPE == 1
	if (pipe(pipefd) < 0) {
		fprintf(stderr, "FIO: Dummy Pipe creation failed\n");
		exit(1);
	}
	if (pipefd[0] > maxfd) {
		maxfd = pipefd[0];
	}
	FD_SET(pipefd[0], &g_fdsets.rfds);
	fcntl(pipefd[0], F_SETFL, O_NONBLOCK);	/* pipie might be not readable after pselect. Example: timeout */
#endif
	sa.sa_handler = handle_sigusr1;	/* Establish signal handler */
	sa.sa_flags = 0;
	sigset_t emptyset, blockset;
	sigemptyset(&blockset);	/* Block SIGUSR1 */
	sigemptyset(&emptyset);
	sigaddset(&blockset, SIGUSR1);
	sigprocmask(SIG_BLOCK, &blockset, NULL);
	sigaction(SIGUSR1, &sa, NULL);	// fd change notification
#endif
	while (1) {
#if USE_DUMMYPIPE == 1
		uint8_t c;
#endif
		rfds = g_fdsets.rfds;
		wfds = g_fdsets.wfds;
		get_timer_lock();
		if (first_host_timer_node) {
			HostTimer *first_timer = XY_NodeValue(first_host_timer_node);
			calculate_remaining(first_timer, &timeout);
			put_timer_lock();
			/* whoever can change timers here has to send a signal */
			result = pselect(maxfd + 1, &rfds, &wfds, NULL, &timeout, &emptyset);
		} else {
			put_timer_lock();
			/* whoever can change timers here has to send a signal */
			result = pselect(maxfd + 1, &rfds, &wfds, NULL, NULL, &emptyset);
		}
#if USE_DUMMYPIPE == 1
		read(pipefd[0], &c, 1);
#endif
		if (result > 0) {
			pthread_mutex_lock(&io_mutex);
			mainloop_event_io = 1;
			mainloop_event_pending = 1;

			/* Let the IO thread sleep until IO is done */
			pthread_cond_wait(&io_done, &io_mutex);
			pthread_mutex_unlock(&io_mutex);
		}
	}
	return NULL;
}

/*
 * -----------------------------------------------------------------
 * Handle SigIO is called from the mainloop of the CPU whenever
 * the main loop of the CPU finds the flag mainloop_event_io 
 * It finds the cause of the signal using select 
 * and invokes a handler. 
 * -----------------------------------------------------------------
 */
static int
FIO_HandleIO(struct timespec *timeout)
{
	int result;
	XYFdSets fdsets;
	FIO_FileHandler *fh;
	int wakeup = 0;

	fdsets.rfds = g_fdsets.rfds;
	fdsets.wfds = g_fdsets.wfds;
	result = pselect(maxfd + 1, &fdsets.rfds, &fdsets.wfds, NULL, timeout, NULL);
	if (mainloop_event_io) {
		mainloop_event_io = 0;
		wakeup = 1;
	}
	if (result > 0) {
 restart:
		for (fh = fileh_head; fh; fh = fh->next) {
			int pendmask = 0;
			if (fh->busy) {
				continue;
			}
			if ((fh->mask & FIO_READABLE) && FD_ISSET(fh->fd, &fdsets.rfds)) {
				pendmask |= FIO_READABLE;
				FD_CLR(fh->fd, &fdsets.rfds);
			}
			if ((fh->mask & FIO_WRITABLE) && FD_ISSET(fh->fd, &fdsets.wfds)) {
				pendmask |= FIO_WRITABLE;
				FD_CLR(fh->fd, &fdsets.wfds);
			}
			if (pendmask) {
				FIO_FileHandler *save = g_currentFH;	// use stack for history
				g_currentFH = fh;
				fh->busy = 1;
				//fprintf(stderr,"Calling fh proc \n");
				fh->proc(fh->clientData, pendmask);
				if (g_currentFH == fh) {
					g_currentFH = save;
					fh->busy = 0;
				} else {
					// fprintf(stderr,"Modification of fh from handler%p\n",save);
					g_currentFH = save;
					goto restart;
				}
			}
		}
	} else if (result == 0) {
		struct timespec time;
		get_timer_lock();
		if (first_host_timer_node) {
			HostTimer *timer = XY_NodeValue(first_host_timer_node);
			clock_gettime(CLOCK_MONOTONIC, &time);
			if (is_timeouted(timer, &time)) {
				// do something
			}
		}
		put_timer_lock();
	}
	if (wakeup) {
		/* Now IO is handled, allow the IO-thread to run again */
		pthread_mutex_lock(&io_mutex);
		pthread_cond_signal(&io_done);
		pthread_mutex_unlock(&io_mutex);
	}
	return result;
}

/*
 * --------------------------------------------------
 * Do all outstanding IO-Events
 * --------------------------------------------------
 */
void
FIO_HandleInput()
{
	int result;
	struct timespec timeout;
	timeout.tv_nsec = timeout.tv_sec = 0;
//      do {    
	result = FIO_HandleIO(&timeout);
//      } while(result>0);
	return;
}

void
FIO_WaitEventTimeout(struct timespec *timeout)
{
	struct timespec remaining;
	get_timer_lock();
	if (first_host_timer_node) {

		HostTimer *first_timer = XY_NodeValue(first_host_timer_node);
		calculate_remaining(first_timer, &remaining);
	}
	put_timer_lock();
	FIO_HandleIO(timeout);
}

/*
 * ---------------------------------------------------------------------
 * Add a file Handler by updating the linked lists and the
 * global fd_set templates. When this is done wake up the IO Thread 
 * from his select systemcall with a signal
 * ---------------------------------------------------------------------
 */

void
FIO_AddFileHandler(FIO_FileHandler * fh, int fd, int mask, FIO_FileProc * proc, void *clientData)
{

	fh->fd = fd;
	fh->mask = mask;
	fh->proc = proc;
	fh->clientData = clientData;
	fh->busy = 0;

	if (fd > maxfd) {
		maxfd = fd;
	}
	fh->next = fileh_head;
	fh->prev = NULL;
	if (fileh_head) {
		fileh_head->prev = fh;
	}
	fileh_head = fh;
	if (fh->mask & FIO_READABLE) {
		FD_SET(fh->fd, &g_fdsets.rfds);
	}
	if (fh->mask & FIO_WRITABLE) {
		FD_SET(fh->fd, &g_fdsets.wfds);
	}
	/* 
	 * The IO-Thread which needs to re-call select with new fds
	 *  so interrupt the old select with a signal  
	 */
#ifndef NO_SIGNAL
	pthread_kill(iothread, SIGUSR1);
#endif
}

void
FIO_RemoveFileHandler(FIO_FileHandler * fh)
{
	FIO_FileHandler *cursor;
	if (fh->mask & FIO_READABLE) {
		FD_CLR(fh->fd, &g_fdsets.rfds);
	}
	if (fh->mask & FIO_WRITABLE) {
		FD_CLR(fh->fd, &g_fdsets.wfds);
	}
	if (fh->next)
		fh->next->prev = fh->prev;
	if (fh->prev) {
		fh->prev->next = fh->next;
	} else {
		fileh_head = fh->next;
	}

	for (cursor = fileh_head; cursor; cursor = cursor->next) {
		if (fh->fd != cursor->fd) {
			continue;
		}
		if (cursor->mask & FIO_READABLE) {
			FD_SET(cursor->fd, &g_fdsets.rfds);
		}
		if (cursor->mask & FIO_WRITABLE) {
			FD_SET(cursor->fd, &g_fdsets.wfds);
		}
	}
	if (g_currentFH == fh) {
		g_currentFH = NULL;
	}
	return;			// not found
}

/*
 * -------------------------------------------------------------------
 * TcpAccept 
 * 	Called by event handler of Tcp server socket when a 
 *      new connection is detected 
 * -------------------------------------------------------------------
 */

static void
TcpAccept(void *cd, int mask)
{
	FIO_TcpServer *tserv = (FIO_TcpServer *) cd;
	int sfd;
	unsigned short port;
	char *host;
	unsigned long hostl;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in con;
	//printf("accept mask %08x\n",mask);
	sfd = accept(tserv->sock, (struct sockaddr *)&con, &addrlen);
	if (sfd < 0) {
		//perror("accept failed errno %d\n",errno);
		return;
	}
	port = ntohs(con.sin_port);
	hostl = ntohl(con.sin_addr.s_addr);
	//printf("connect from %s\n",inet_ntoa(con.sin_addr));
	host = inet_ntoa(con.sin_addr);
	if (sfd < 0)
		return;
	if (tserv->proc) {
		// printf("Call connect proc fd %d\n",sfd);
		tserv->proc(sfd, host, port, tserv->clientData);
	}
	return;
}

/*
 * ----------------------------------------------------------------------------
 * FIO_InitTcpServer
 * 	Setup a TCP server. Listen on a port and setup a handler which 
 * 	is invoked for new connections
 * ----------------------------------------------------------------------------
 */
int
FIO_InitTcpServer(FIO_TcpServer * tserv, FIO_Accept * proc, void *clientData,
		  char *host, unsigned short port)
{
	struct sockaddr_in sa;
	int optval;
	int sock;
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		perror("can't create socket");
		return -1;
	}
	optval = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	tserv->sock = sock;
#ifdef __unix__
	fcntl(sock, F_SETFD, FD_CLOEXEC);
	fcntl(sock, F_SETFL, O_NONBLOCK);
#endif
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(host);
	if (bind(sock, (struct sockaddr *)&sa, sizeof(struct sockaddr)) < 0) {
		perror("can't bind");
		close(sock);
		return -1;
	}
	if (listen(sock, 50) < 0) {
		perror("can't listen");
		close(sock);
		return -1;
	}
	tserv->proc = proc;
	tserv->clientData = clientData;
	FIO_AddFileHandler(&tserv->acc_fh, sock, FIO_READABLE, TcpAccept, tserv);
	return sock;
}

/*
 * -----------------------------------------------------------
 * File descriptor IO init
 * 	Create a thread watching for IO with select.
 * -----------------------------------------------------------
 */
void
FIO_Init()
{
	int result;
#ifndef NO_SIGNAL
	sigset_t blockset;
#endif
	XY_InitTree(&host_timer_tree, _is_later, NULL, NULL, NULL);
#ifdef __unix__
	init_recursive_mutex(&timer_mutex);
#else
	mtx_init(&io_mutex, mtx_plain);
	cnd_init(&io_done);
	mtx_init(&timer_mutex, mtx_plain | mtx_recursive);
#endif
	FD_ZERO(&g_fdsets.rfds);
	FD_ZERO(&g_fdsets.wfds);
#ifdef __unix__
#else
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 0), &wsaData);
#endif
	/* 
	 * ------------------------------------------------------
	 * The IO-Thread uses SIGUSR1. Old pthreads library  
	 * sometimes sends signals to wrong threads, so it is
	 * better to block them in main thread
	 * ------------------------------------------------------
	 */
#ifndef NO_SIGNAL
	sigemptyset(&blockset);
	sigaddset(&blockset, SIGUSR1);
	sigprocmask(SIG_BLOCK, &blockset, NULL);
#endif
	result = pthread_create(&iothread, NULL, run_iothread, NULL);
	if (result < 0) {
		perror("IO-Thread creation failed\n");
		exit(1);
	}
	fprintf(stderr, "IO-Thread started\n");
}
