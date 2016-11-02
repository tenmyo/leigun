/*
 **************************************************************************************************
 *
 * TCP socket based CAN message forwarding
 * for CAN-chip emulators 
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include <socket_can.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include "fio.h"
#include "configfile.h"
#include "sgstring.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/tcp.h>

#define RX_FIFO_SIZE		(16)
#define RX_FIFO_WP(contr)	((contr)->rx_fifo_wp % RX_FIFO_SIZE)
#define RX_FIFO_RP(contr)	((contr)->rx_fifo_rp % RX_FIFO_SIZE)
#define RX_FIFO_CNT(contr)	((contr)->rx_fifo_wp - (contr)->rx_fifo_rp)

struct CanController {
        CanChipOperations *cops;
        void *clientData;
        int listen_fd;
        FIO_TcpServer tserv;
	CAN_MSG rx_fifo[RX_FIFO_SIZE];
	uint32_t rx_fifo_wp;
	uint32_t rx_fifo_rp;
	int rx_enabled;
	int rx_started;
        struct Connection *con_list;
};

typedef struct Connection {
        int sockfd;
        FIO_FileHandler rfh;    // read event file Handler
        int rfh_is_active;
        FIO_FileHandler wfh;    // write event file Handler
        int wfh_is_active;
        struct CanController *canController;
        struct Connection *next;
        CAN_MSG imsg;
        int ibuf_wp;
} Connection;

static void
close_connection(Connection *con)
{
	Connection *cursor,*prev;
	CanController *contr=con->canController;
        close(con->sockfd);
	if(con->rfh_is_active) {
		FIO_RemoveFileHandler(&con->rfh);
		con->rfh_is_active=0;
	}
        if(con->wfh_is_active) {
                FIO_RemoveFileHandler(&con->wfh);
                con->wfh_is_active=0;
        }
	for(prev=NULL,cursor=contr->con_list;cursor;prev=cursor,cursor=cursor->next)
        {
                if(cursor==con)     {
                        if(prev) {
                                prev->next=cursor->next;
                        } else {
                                contr->con_list=cursor->next;
                        }
			break;
                }
        }
        free(con);
	//fprintf(stderr,"CAN-Bus socket connection closed\n");
        return;
}

/*
 * ------------------------------------------------------
 * Can Send: Interface function for the Chip Emulator,
 *	sends a CAN-Message to all sockets connected 
 *	a CAN Controller
 * ------------------------------------------------------
 */ 

void
CanSend(CanController *contr,CAN_MSG *msg) {
	/* To lazy for buffering and Writefilehandlers, may be added later */
	Connection *con,*next;
	int result;
	if(!contr) {
		return;
	}
	for(con=contr->con_list;con; con=next) {
		next=con->next;
		fcntl(con->sockfd,F_SETFL,0);
		result = write(con->sockfd,msg,sizeof(*msg));
		if(result < 0) {
			if(errno==EAGAIN) {
				fprintf(stderr,"Tcp Buffer overrun, skiping CAN-Message\n");	
			} else {
				close_connection(con);
			}
		} else if (result==0) {
			close_connection(con);
		}
		fcntl(con->sockfd,F_SETFL,O_NONBLOCK);
	}
	return;
}

static int take_from_rx_fifo(CanController *contr,CAN_MSG *msg);
static void add_to_rx_fifo(CanController *contr,CAN_MSG *msg);

static void
do_receive(CanController *contr) {
	CAN_MSG msg;
	while(contr->rx_started) {
		if(take_from_rx_fifo(contr,&msg) == 0) {
			return;
		}
		contr->cops->receive(contr->clientData,&msg);
	}
}

static void 
read_from_sock(void *cd,int mask)
{
	Connection *con = cd;
	CanController *contr=con->canController;
	int result,count=0;
	char *cbuf=(char*)&con->imsg;
	while(contr->rx_enabled) {
		result = read(con->sockfd,cbuf+con->ibuf_wp,sizeof(CAN_MSG)-con->ibuf_wp);
		if(result > 0) {
			count=result;
		} else if(result == 0) {
			close_connection(con);
			return;
		} else {
			if(errno==EAGAIN) {
				return;
			} else {
				close_connection(con);
				return;
			}
		}
		con->ibuf_wp+=count;
		if(con->ibuf_wp >= sizeof(CAN_MSG)) {
			con->ibuf_wp=0;	
			add_to_rx_fifo(contr,&con->imsg);
			do_receive(contr);
		}
	}
	return;
	
}

static void
tcp_connect(int sockfd,char *host,unsigned short port,void *cd)
{
	Connection *con = sg_new(Connection);	
	CanController *contr=cd;
	int bufsize = 2048;
        int flag = 1;

	con->canController=contr;
	con->sockfd=sockfd;
	fcntl(sockfd,F_SETFL,O_NONBLOCK);
	if(contr->con_list) {
		con->next=contr->con_list;
	} else {
		con->next=NULL;
	} 
	contr->con_list=con;
	if(contr->rx_enabled) {
		FIO_AddFileHandler(&con->rfh,sockfd,FIO_READABLE,read_from_sock,con);
		con->rfh_is_active=1;
	}
	setsockopt(sockfd,SOL_SOCKET,SO_SNDBUF,&bufsize,sizeof(bufsize));
	setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&bufsize,sizeof(bufsize));
        setsockopt(sockfd,SOL_TCP,TCP_NODELAY,&flag,sizeof(flag));
	//fprintf(stderr,"New socket connection to emulated CAN-Controller\n");
}

/*
 *******************************************************************
 * Create a new Cancontroller with a Listening TCP-Socket
 *******************************************************************
 */
CanController * 
CanSocketInterface_New(CanChipOperations *cops,const char *name,void *clientData) 
{
	CanController *contr = sg_new(CanController);	
	int32_t port;
	char *host=Config_ReadVar(name,"host");
        if(!host) {
		host = "127.0.0.1";
        }
	if(Config_ReadInt32(&port,name,"port")<0) {
		free(contr);
		return 0;
	}
	contr->cops=cops;	
	contr->clientData=clientData;
	contr->rx_enabled = 1;
	contr->listen_fd=FIO_InitTcpServer(&contr->tserv,tcp_connect,contr,host,port);
	fprintf(stderr,"%s: listening on %s:%d\n",name,host,port);
	if(contr->listen_fd<0) {
		fprintf(stderr,"Can not open TCP Listening Port %d for CAN-Emulator: ",port);
		perror("");
		free(contr);
		return NULL;
	}
	return contr;
}

static void
disable_rx_handlers(CanController *contr) {
	Connection *cursor;
	if(contr->rx_enabled == 0) {
		return;
	}
	contr->rx_enabled = 0;
	for(cursor=contr->con_list;cursor;cursor=cursor->next)
        {
		if(cursor->rfh_is_active) {
			cursor->rfh_is_active=0;
			FIO_RemoveFileHandler(&cursor->rfh);
		}
        }
}

static void
enable_rx_handlers(CanController *contr) {
	Connection *cursor;
	if(contr->rx_enabled == 1) {
		return;
	}
	contr->rx_enabled = 1;
	for(cursor=contr->con_list;cursor;cursor=cursor->next)
        {
		if(!cursor->rfh_is_active) {
			cursor->rfh_is_active=1;
			FIO_AddFileHandler(&cursor->rfh,cursor->sockfd,FIO_READABLE,read_from_sock,cursor);
		}
        }
}

static void 
add_to_rx_fifo(CanController *contr,CAN_MSG *msg) 
{
	CAN_MSG *dst;
	if(RX_FIFO_CNT(contr) == RX_FIFO_SIZE) {
		fprintf(stderr,"Should not happen\n");
	}
	dst = &contr->rx_fifo[RX_FIFO_WP(contr)];
	*dst = *msg;
	//fprintf(stderr,"add msg %d %04x %02x %02x %02x\n",msg->can_dlc,CAN_ID(msg),msg->data[0],msg->data[1],msg->data[2]);
	contr->rx_fifo_wp++;
	if(RX_FIFO_CNT(contr) >= RX_FIFO_SIZE) {
		disable_rx_handlers(contr);
	}
}

static int 
take_from_rx_fifo(CanController *contr,CAN_MSG *msg) 
{
	CAN_MSG *src;
	if(RX_FIFO_CNT(contr) == 0) {
		return 0;
	}
	src = &contr->rx_fifo[RX_FIFO_RP(contr)];
	contr->rx_fifo_rp++;
	*msg = *src;	
	//fprintf(stderr,"take msg %d %04x %02x %02x %02x\n",msg->can_dlc,CAN_ID(msg),msg->data[0],msg->data[1],msg->data[2]);
	enable_rx_handlers(contr);
	return 1;
}


void 
CanStopRx(CanController *contr) {
	if(!contr) {
		return;
	}
	contr->rx_started = 0;
}

void 
CanStartRx(CanController *contr) {
	if(!contr) {
		return;
	}
	contr->rx_started = 1;
	/* don't know if recursion is good */
	do_receive(contr);
}
