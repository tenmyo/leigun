/*
 *************************************************************************************************
 * 
 * Event driven	telnet server 
 *
 *   State: working 
 *
 * Copyright 2004 2008 Jochen Karrer. All rights reserved.
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

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#include <fio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#define TELOPTS
#include <arpa/telnet.h>
#include <errno.h>
#include <channel.h>
#include "sgstring.h"

#include "telnetd.h"
#include "editor.h"

#define STATE_RCV  	(0)
#define STATE_IAC  	(1)
#define STATE_SUB  	(2)
#define STATE_OPTION  	(3)
#define STATE_CLOSED	(4)

typedef struct TelOpt {
	uint8_t state;
	uint8_t nego_started;
} TelOpt;

struct TelnetSession {
	void *owner;
	TelnetServer *tserv;
	char *submit_buf;
	unsigned int submit_buf_wp;
	unsigned int submit_buf_rp;

	ChannelHandler *datasink;
	ChannelHandler *datasource;

	Channel channel;
	int rfh_is_active;
	FIO_FileHandler rfh;
	TelOpt echo;
	TelOpt sga;
	TelOpt linemode;
	TelOpt lflow;
	int state;
	int telnet_cmd;
	int fd;
	int usecount;
	Editor *editor;
};

struct TelnetServer {
	Telnet_AcceptProc *accproc;
	void *clientData;
	FIO_TcpServer tcpserv;
};

/*
 *************************************************************************
 * Send some data to the telnet client in blocking mode (I'm to lazy for
 * buffering).
 *************************************************************************
 */
static void
telnet_output(int fd, const void *buf, int count)
{
	int result;
	fcntl(fd, F_SETFL, 0);
	while (count) {
		result = write(fd, buf, count);
		if (result <= 0) {
			/* Can not delete the session now */
			fprintf(stderr,"Write to socket failed %d errno %d failed\n",result,errno);
			return;
		}
		count -= result;
		buf += result;
	}
	fcntl(fd, F_SETFL, O_NONBLOCK);
}

/*
 *******************************************************************************
 * Send an "Interpret As Command" Sequence to the telnet client
 *******************************************************************************
 */
static void
send_iac(TelnetSession * ts, uint8_t cmd, uint8_t opt)
{
	char buf[3];
	buf[0] = IAC;
	buf[1] = cmd;
	buf[2] = opt;
	telnet_output(ts->fd, buf, 3);
}

static void telnet_input(void *cd, int mask);

static void
TS_StartRx(TelnetSession * ts)
{
	if (!ts->rfh_is_active) {
		ts->rfh_is_active = 1;
		FIO_AddFileHandler(&ts->rfh, ts->fd, FIO_READABLE, telnet_input, ts);
	}
}

static void
TS_StopRx(TelnetSession * ts)
{
	if (ts->rfh_is_active) {
		ts->rfh_is_active = 0;
		dbgprintf("Remove the filehandler\n");
		FIO_RemoveFileHandler(&ts->rfh);
	}
}

static void
TS_Del(TelnetSession * ts)
{
	free(ts);
}

/*
 ***********************************************************************
 * Disable a telnet session before deletion
 ***********************************************************************
 */
static void
TS_Disable(TelnetSession * ts)
{
	uint8_t c;
	TS_StopRx(ts);
	/* Don't close stdout stderr */
	shutdown(ts->fd, SHUT_RDWR);
	while (read(ts->fd, &c, 1) > 0) {
		dbgprintf("READEMPTY %02x\n", c);
	}
	if (ts->fd > 2) {
		dbgprintf("close %d\n", ts->fd);
		if (close(ts->fd) < 0) {
			perror("close fd failed");
		} else {
			ts->fd = -1;
		}
	} else {
		dbgprintf("Do not close %d\n", ts->fd);
	}
	ts->state = STATE_CLOSED;
}

/* From RFC 854 */

/*
 **************************************************************************************
 * Accept "DO" Command sequences. If it is an answer to a negotiation started by this
 * server don't reply with "WILL"  
 **************************************************************************************
 */
static void
do_option(TelnetSession * ts, int option)
{
	dbgprintf("Do %d\n", option);
	switch (option) {
	    case TELOPT_ECHO:
		    if (!ts->echo.nego_started) {
			    fprintf(stderr, "Telnet Client does want an echo\n");
			    send_iac(ts, WILL, option);
		    } else {
			    dbgprintf("Telnet Client accepts my echo\n");
			    ts->echo.nego_started = 0;
		    }
		    ts->echo.state = 1;
		    break;

	    case TELOPT_SGA:
		    if (!ts->sga.nego_started) {
			    fprintf(stderr, "Telnet Client does want suppression of GA\n");
			    send_iac(ts, WILL, option);
		    } else {
			    dbgprintf("Telnet Client accepts suppression of GA\n");
			    ts->sga.nego_started = 0;
		    }
		    ts->sga.state = 1;
		    break;

	    case TELOPT_BINARY:
		    fprintf(stderr, "DO Binary\n");
		    break;

	    case TELOPT_LFLOW:
		    if (!ts->lflow.nego_started) {
			    fprintf(stderr, "Telnet Client does want Flow control\n");
			    send_iac(ts, WILL, option);
		    } else {
			    dbgprintf("Telnet Client accepts Flow control\n");
			    ts->lflow.nego_started = 0;
		    }
		    ts->lflow.state = 1;
		    break;

	    case TELOPT_LINEMODE:
		    if (!ts->linemode.nego_started) {
			    fprintf(stderr, "Telnet Client does want linemode\n");
			    send_iac(ts, WILL, option);
		    } else {
			    dbgprintf("Telnet Client accepts linemode\n");
			    ts->linemode.nego_started = 0;
		    }
		    ts->linemode.state = 1;
		    break;
	    default:
		    dbgprintf("refuse 0x%02x\n", option);
		    send_iac(ts, WONT, option);
		    break;
	}
}

/*
 **************************************************************************************
 * Accept "DONT" Command sequences. If it is an answer to a negotiation started by this
 * server don't reply with "WONT"  
 **************************************************************************************
 */
static void
dont_option(TelnetSession * ts, int option)
{
	dbgprintf("Don't %d\n", option);
	switch (option) {
	    case TELOPT_ECHO:
		    if (!ts->echo.nego_started) {
			    fprintf(stderr, "Telnet client does not like my echo\n");
			    send_iac(ts, WONT, option);
		    } else {
			    dbgprintf("Telnet client accepts that i don't echo\n");
			    ts->echo.nego_started = 0;
		    }
		    ts->echo.state = 0;
		    break;

	    case TELOPT_SGA:
		    if (!ts->sga.nego_started) {
			    fprintf(stderr, "Telnet client does not like suppress GA\n");
			    send_iac(ts, WONT, option);
		    } else {
			    dbgprintf("Telnet client accepts not to suppress GA\n");
			    ts->sga.nego_started = 0;
		    }
		    ts->sga.state = 0;
		    break;

	    case TELOPT_LFLOW:
		    if (!ts->lflow.nego_started) {
			    fprintf(stderr, "Telnet client does not like flow control\n");
			    send_iac(ts, WONT, option);
		    } else {
			    dbgprintf("Telnet client accepts disabling flow control\n");
			    ts->lflow.nego_started = 0;
		    }
		    ts->lflow.state = 0;
		    break;

	    case TELOPT_LINEMODE:
		    if (!ts->linemode.nego_started) {
			    fprintf(stderr, "Telnet client does not like linemode\n");
			    send_iac(ts, WONT, option);
		    } else {
			    dbgprintf("Telnet client accepts disabling linemode\n");
			    ts->linemode.nego_started = 0;
		    }
		    ts->linemode.state = 0;
		    break;

	    default:
		    send_iac(ts, WONT, option);
	}
}

/*
 **************************************************************************************
 * Accept "WILL" Command sequences. If it is an answer to a negotiation started by this
 * server don't reply with "DO"  
 **************************************************************************************
 */
static void
will_option(TelnetSession * ts, int option)
{
	dbgprintf("Will %d\n", option);
	switch (option) {
	    case TELOPT_ECHO:
		    if (!ts->echo.nego_started) {
			    fprintf(stderr, "Telnet client wants to echo himself\n");
			    send_iac(ts, DO, option);
		    } else {
			    dbgprintf("Telnet client accepts to echo himself\n");
			    ts->echo.nego_started = 0;
		    }
		    ts->echo.state = 0;	/* Don't echo also */
		    break;

	    case TELOPT_SGA:
		    if (!ts->sga.nego_started) {
			    fprintf(stderr, "Telnet client wants suppress GA\n");
			    send_iac(ts, DO, option);
		    } else {
			    dbgprintf("Telnet client accepts to suppress GA\n");
			    ts->sga.nego_started = 0;
		    }
		    ts->sga.state = 1;
		    break;

	    case TELOPT_LFLOW:
		    if (!ts->lflow.nego_started) {
			    fprintf(stderr, "Telnet client wants control flow\n");
			    send_iac(ts, DO, option);
		    } else {
			    dbgprintf("Telnet client accepts flow control\n");
			    ts->lflow.nego_started = 0;
		    }
		    ts->lflow.state = 1;
		    break;

	    case TELOPT_LINEMODE:
		    if (!ts->linemode.nego_started) {
			    fprintf(stderr, "Telnet client wants use linemode\n");
			    send_iac(ts, DO, option);
		    } else {
			    dbgprintf("Telnet client accepts linemode\n");
			    ts->linemode.nego_started = 0;
		    }
		    ts->linemode.state = 1;
		    break;
	}
}

/*
 **************************************************************************************
 * Accept "WONT" Command sequences. If it is an answer to a negotiation started by this
 * server don't reply with "DONT"  
 **************************************************************************************
 */
static void
wont_option(TelnetSession * ts, int option)
{
	fprintf(stderr, "Wont %d\n", option);
	switch (option) {
	    case TELOPT_ECHO:
		    if (!ts->echo.nego_started) {
			    fprintf(stderr, "Telnet client doesn't want to echo\n");
			    send_iac(ts, DONT, option);
		    } else {
			    fprintf(stderr, "Telnet client accepts not to echo\n");
			    ts->echo.nego_started = 0;
		    }
		    ts->echo.state = 1;	/* We do the echo */
		    break;

	    case TELOPT_SGA:
		    if (!ts->sga.nego_started) {
			    fprintf(stderr, "Telnet client doesn't want to suppress GA\n");
			    send_iac(ts, DONT, option);
		    } else {
			    fprintf(stderr, "Telnet accepts not to suppress GA\n");
			    ts->sga.nego_started = 0;
		    }
		    ts->sga.state = 0;
		    break;

	    case TELOPT_LFLOW:
		    if (!ts->lflow.nego_started) {
			    fprintf(stderr, "Telnet client doen't want control flow\n");
			    send_iac(ts, DONT, option);
		    } else {
			    dbgprintf("Telnet accepts No flowcontrol\n");
			    ts->lflow.nego_started = 0;
		    }
		    ts->lflow.state = 0;
		    break;

	    case TELOPT_LINEMODE:
		    if (!ts->linemode.nego_started) {
			    fprintf(stderr, "Telnet client doesn't want to use linemode\n");
			    send_iac(ts, DONT, option);
		    } else {
			    dbgprintf("Telnet client accepts Non-linemode\n");
			    ts->linemode.nego_started = 0;
		    }
		    ts->linemode.state = 0;
		    break;
	}
}

static void
TSess_Submit(void *clientData, void *buf, int len)
{
	TelnetSession *ts = (TelnetSession *) clientData;
	ChannelHandler *ch;
	if (len) {
		if (ts->submit_buf) {
			sg_free(ts->submit_buf);
		}
		ts->submit_buf = sg_strdup(buf);
	}
	ts->submit_buf_wp = len;
	ts->submit_buf_rp = 0;
	/* Stop the input until lines is used up */
	TS_StopRx(ts);
	ch = ts->datasink;
	if (ch && ch->notifyProc) {
		ch->notifyProc(ch->clientData, CHAN_READABLE);
	}
}

/*
 ********************************************************
 * The main telnet state machine
 ********************************************************
 */

static int
feed_state_machine(TelnetSession * ts, uint8_t c)
{
	dbgprintf("feed 0x%02x\n", c);
	switch (ts->state) {
	    case STATE_RCV:
		    if (c == IAC) {
			    ts->state = STATE_IAC;
		    } else {
			    switch (c) {
				case SLC_IP:
					dbgprintf("Interrupt\n");
					break;
				case 0:
				case SLC_SYNCH:
				case SLC_BRK:
				case SLC_AO:
				case SLC_AYT:
				case SLC_EOR:
				case SLC_ABORT:
				case SLC_EOF:
				case SLC_SUSP:
				case SLC_EL:
				case SLC_EW:
				case SLC_LNEXT:
				case SLC_XON:
				case SLC_XOFF:
				case SLC_FORW1:
				case SLC_FORW2:
					break;
				default:
					if (Editor_Feed(ts->editor, c) < 0) {
						if (ts->state == STATE_CLOSED) {
							return -1;
						}
					}
					break;

			    }
		    }
		    break;
	    case STATE_IAC:
		    switch (c) {
			case DONT:
			case DO:
			case WONT:
			case WILL:
				ts->telnet_cmd = c;
				ts->state = STATE_OPTION;
				break;

			case SB:
				ts->state = STATE_SUB;
				break;

			case GA:
				ts->state = STATE_RCV;
				break;
			case EL:
				ts->state = STATE_RCV;
				break;
			case EC:
				ts->state = STATE_RCV;
				break;
			case AYT:
				ts->state = STATE_RCV;
				break;
			case AO:
				ts->state = STATE_RCV;
				break;
			case IP:
				dbgprintf("Received Interrupt\n");
				ts->state = STATE_RCV;
				break;
			case BREAK:
				ts->state = STATE_RCV;
				break;
			case DM:
				ts->state = STATE_RCV;
				break;
			case NOP:
				ts->state = STATE_RCV;
				break;
			case SE:
				ts->state = STATE_RCV;
				break;
		    }
		    break;

	    case STATE_OPTION:
		    switch (ts->telnet_cmd) {
			case DO:
				do_option(ts, c);
				ts->state = STATE_RCV;
				break;

			case DONT:
				dont_option(ts, c);
				ts->state = STATE_RCV;
				break;
			case WONT:
				wont_option(ts, c);
				ts->state = STATE_RCV;
				break;
			case WILL:
				will_option(ts, c);
				ts->state = STATE_RCV;
				break;
			default:
				fprintf(stderr, "Not understanding 0x%02x\n", c);
				ts->state = STATE_RCV;
				break;
		    }
		    break;

	    case STATE_SUB:
		    if (c == SE) {
			    ts->state = STATE_RCV;
		    } else {
			    dbgprintf("sub %02x\n", c);
		    }
		    break;
	    default:
		    dbgprintf("unexpected state %d\n", ts->state);
		    break;
	}
	return 0;
}

/*
 *************************************************************
 * The handler for data coming from the terminal. 
 * It is called when the socket filedescriptor gets readable
 *************************************************************
 */
static void
telnet_input(void *cd, int mask)
{
	int result;
	int i;
	TelnetSession *ts = cd;
	uint8_t buf[16];
	ts->usecount++;
	errno = 0;
	while ((result = read(ts->fd, buf, sizeof(buf))) > 0) {
		for (i = 0; i < result; i++) {
			if (feed_state_machine(ts, buf[i]) < 0) {
				ts->usecount--;
				if (ts->usecount == 0) {
					TS_Del(ts);
				}
				return;
			}
		}
	}
	ts->usecount--;
	if ((result < 0) && (errno == EAGAIN)) {
		return;
	} else {
		dbgprintf("Connection lost, result %d errno %d\n",result,errno);
		ts->state = STATE_CLOSED;
		TS_Disable(ts);
		if (ts->usecount == 0) {
			TS_Del(ts);
		}
		return;
	}
}

static void
tsess_echo(void *clientData, void *buf, int len)
{
	TelnetSession *ts = (TelnetSession *) clientData;
	if (ts->echo.state) {
		telnet_output(ts->fd, buf, len);
	}
}

static TelnetSession *
TS_New(int fd)
{
	TelnetSession *ts = sg_new(TelnetSession);
	ts->editor = Editor_New(tsess_echo, TSess_Submit, ts);
	ts->fd = fd;
	dbgprintf("New Telnet session fd %d\n", fd);
	fcntl(ts->fd, F_SETFL, O_NONBLOCK);
	ts->rfh_is_active = 0;
	ts->sga.state = 1;
	ts->echo.state = 1;
	ts->lflow.state = 1;
	ts->linemode.state = 0;
	ts->usecount = 0;

	ts->sga.nego_started = 1;
	TS_StartRx(ts);
	send_iac(ts, WILL, TELOPT_SGA);

	ts->lflow.nego_started = 1;
	send_iac(ts, DO, TELOPT_LFLOW);	// remote flow control 

	ts->echo.nego_started = 1;
	send_iac(ts, WILL, TELOPT_ECHO);

	ts->linemode.nego_started = 1;
	send_iac(ts, DONT, TELOPT_LINEMODE);
	return ts;
}

static void
tsess_close(Channel * ch, void *impl)
{
	TelnetSession *ts = (TelnetSession *) impl;
	TS_Disable(ts);
	if (ts->usecount == 0) {
		TS_Del(ts);
	}
}

static int
tsess_eof(void *impl)
{
	TelnetSession *ts = (TelnetSession *) impl;
	if (ts->state == STATE_CLOSED) {
		return 1;
	} else {
		return 0;
	}
}

static ssize_t
tsess_read(void *impl, void *_buf, size_t count)
{
	TelnetSession *ts = (TelnetSession *) impl;
	char *buf = (char *)_buf;
	int i;
	if (ts->state == STATE_CLOSED) {
		return -1;
	}
	//fprintf(stderr,"Read line %d: %s\n",ed->line_readpointer,line->data);
	for (i = 0; i < count; i++) {
		if (ts->submit_buf_rp < ts->submit_buf_wp) {
			buf[i] = ts->submit_buf[ts->submit_buf_rp++];
		} else {
			break;
		}
	}
	if (ts->submit_buf_rp == ts->submit_buf_wp) {
		TS_StartRx(ts);
	}
	return i;
}

static ssize_t
tsess_write(void *impl, const void *buf, size_t count)
{
	TelnetSession *ts = (TelnetSession *) impl;
	telnet_output(ts->fd, buf, count);
	return count;
}

/*
 *****************************************************************
 * Channel_AddHandler Implementation for the telnet server.
 * Only one read and one write handler is allowed per telnet
 * session. More makes no sense anyway.
 *****************************************************************
 */
static void
tsess_addHandler(ChannelHandler * ch, Channel * chan, void *impl, int mask, Channel_Proc * proc,
		 void *clientData)
{
	TelnetSession *ts = (TelnetSession *) impl;
	if (mask & CHAN_READABLE) {
		ch->notifyProc = proc;
		ch->clientData = clientData;
		ch->chan = chan;
		ts->datasink = ch;
	}
	if (mask & CHAN_WRITABLE) {
		ch->notifyProc = proc;
		ch->clientData = clientData;
		ch->chan = chan;
		ts->datasource = ch;
	}
	return;
}

static void
tsess_removeHandler(ChannelHandler * ch, void *impl)
{
	ch->notifyProc = NULL;
	ch->clientData = NULL;
	ch->chan = NULL;
	return;
}

/*
 ***********************************************************
 * Accept an incoming connection on the TCP-Server
 ***********************************************************
 */
static void
tserv_accept(int sockfd, char *host, unsigned short port, void *cd)
{
	TelnetServer *tserv = cd;
	Channel *sessionChannel;
	TelnetSession *ts;
	dbgprintf("Connection from %s port %d\n", host, port);
	ts = TS_New(sockfd);
	sessionChannel = &ts->channel;
	memset(sessionChannel, 0, sizeof(Channel));
	sessionChannel->implementor = ts;
	sessionChannel->read = tsess_read;
	sessionChannel->write = tsess_write;
	sessionChannel->addHandler = tsess_addHandler;
	sessionChannel->removeHandler = tsess_removeHandler;
	sessionChannel->eof = tsess_eof;
	sessionChannel->close = tsess_close;
	sessionChannel->busy = 0;
	ts->tserv = tserv;
	ts->usecount++;
	if (tserv->accproc) {
		tserv->accproc(tserv->clientData, sessionChannel, host, port);
	}
	ts->usecount--;
	return;
}

/*
 * -------------------------------------------------------------
 * Create a new Telnet server listening on a network socket 
 * -------------------------------------------------------------
 */
TelnetServer *
TelnetServer_New(char *host, int port, Telnet_AcceptProc * accproc, void *clientData)
{
	int fd;
	TelnetServer *tserv = sg_new(TelnetServer);

	if ((fd = FIO_InitTcpServer(&tserv->tcpserv, tserv_accept, tserv, host, port)) < 0) {
		sg_free(tserv);
		return NULL;
	}
	tserv->accproc = accproc;
	tserv->clientData = clientData;
	dbgprintf("Debug Server fd %d Listening on host \"%s\" port %d\n", fd, host, port);
	return tserv;
}
