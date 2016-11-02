/*
 **************************************************************************************************
 *
 * Softgun builtin Web Server. 
 *
 * State: Not working
 *
 * Copyright 2013 Jochen Karrer. All rights reserved.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include "sgstring.h"
#include "configfile.h"
#include "fio.h"
#include "webserv.h"
#include "strhash.h"

#define MAX_CONNECTIONS	20

#if 0
typedef void HttpRequestProc(HttpCon * htc, void *eventData);
#endif

typedef struct HttpRequestHandler {
	char *path;		/* Uri striped by name of the registered component */
	void *eventData;
} HttpRequestHandler;

typedef struct WebServ {
	FIO_TcpServer tcpserv;
	uint32_t nr_connections;
	int sock_fd;
	SHashTable uriHash;
} WebServ;

#define MAX_URI_ARGC	(5)

typedef struct WebCon {
	WebServ *webserv;
	struct WebCon *next;
	FIO_FileHandler rfh;
	FIO_FileHandler wfh;
	int sockfd;
	char *argv[MAX_URI_ARGC];
	unsigned int argc;
	char uri[128];
	uint8_t buf[512];
	uint16_t uri_len;
	uint16_t rqhdrfieldlen;
	uint32_t buf_wp;
	uint32_t buf_rp;
	uint8_t method;
	uint8_t state;
} WebCon;

#define METHOD_GET	(1)
#define METHOD_PUT	(2)

/*
 *****************************************************************
 * HTTP status codes from rfc2616 section 10
 *****************************************************************
 */
#define HTS_CONTINUE			(100)
#define HTS_SWITCHING_PROTOCOLS		(101)
#define	HTS_OK				(200)
#define HTS_CREATED			(201)
#define	HTS_ACCEPTED			(202)
#define	HTS_NON_AUTHORITATIVE		(203)
#define	HTS_NO_CONTENT			(204)
#define HTS_RESET_CONTENT		(205)
#define	HTS_PARTIAL_CONTENT		(206)
#define	HTS_MULTIPLE_CHOICES		(300)
#define	HTS_MOVED_PERMANENTLY		(301)
#define	HTS_FOUND			(302)
#define	HTS_SEE_OTHER			(303)
#define	HTS_NOT_MODIFIED		(304)
#define	HTS_USE_PROXY			(305)
#define	HTS_TEMPORARY_REDIRECT		(307)
#define	HTS_BAD_REQUEST			(400)
#define	HTS_UNAUTHORIZED		(401)
#define	HTS_PAYMENT_REQUIRED		(402)
#define	HTS_FORBIDDEN			(403)
#define	HTS_NOT_FOUND			(404)
#define	HTS_METHOD_NOT_ALLOWED		(405)
#define HTS_NOT_ACCEPTABLE		(406)
#define	HTS_PROXY_AUTH_REQUIRED		(407)
#define	HTS_REQUEST_TIMEOUT		(408)
#define	HTS_CONFLICT			(409)
#define	HTS_GONE			(410)
#define	HTS_LENGTH_REQUIRED		(411)
#define HTS_PRECONDITION_FAILED		(412)
#define	HTS_REQUEST_ENTITY_TOO_LARGE	(413)
#define	HTS_REQUEST_URI_TOO_LONG	(414)
#define	HTS_UNSUPPORTED_MEDIA_TYPE	(415)
#define HTS_REQUEST_RANGE_NOT_SATISFIABLE	(416)
#define HTS_EXPECTATION_FAILED		(417)
#define	HTS_INTERNAL_SERVER_ERROR	(500)
#define	HTS_NOT_IMPLEMENTED		(501)
#define HTS_BAD_GATEWAY			(502)
#define HTS_SERVICE_UNAVAILABLE		(503)
#define HTS_GATEWAY_TIMEOUT		(504)
#define HTS_VERSION_NOT_SUPPORTED	(505)

#define WC_STATE_IDLE	(0)
#define WC_STATE_G	(1)
#define WC_STATE_E	(2)
#define WC_STATE_T	(3)
#define WC_STATE_P	(4)
#define WC_STATE_O	(5)
#define WC_STATE_S	(6)
#define WC_STATE_T2	(7)
#define WC_STATE_SPC1	(8)
#define WC_STATE_URI	(9)
#define WC_STATE_SPC2	(10)
#define WC_STATE_HTTP		(11)
#define WC_STATE_LF		(12)
#define WC_STATE_RQHDRFIELD	(13)
#define WC_STATE_LF2		(14)
#define WC_STATE_ERROR		(0xff)

/*
 ********************************************************************
 * Separate arguments from URI.
 * Input example: "bla.cgi?x=5&z=7&hello"
 ********************************************************************
 */
static int
split_args(char *line, int maxargc, char *argv[])
{
	int i, argc = 1;
	argv[0] = line;
	for (i = 0; line[i]; i++) {
		if ((line[i] == ' ')) {
			line[i] = 0;
			return argc;
		}
		if ((argc == 1) && (line[i] == '?')) {
			line[i] = 0;
			argv[argc] = line + i + 1;
			argc++;
		} else if ((argc > 1) && (line[i] == '&')) {
			line[i] = 0;
			if (argc < maxargc) {
				argv[argc] = line + i + 1;
				argc++;
			}
		}
	}
	return argc;
}

/**
 ****************************************************
 * Find a request handler identified by the URI 
 ****************************************************
 */
static HttpRequestHandler *
FindRequestHandler(WebServ * ws, char *uri, size_t uribuflen)
{
	SHashEntry *hashEntry;
	size_t urilen;
	unsigned int i;

	HttpRequestHandler *rqh;
	hashEntry = SHash_FindEntry(&ws->uriHash, uri);
	if (hashEntry) {
		rqh = SHash_GetValue(hashEntry);
		rqh->path = uri;
		return rqh;
	}
	urilen = strlen(uri);
	for (i = 0; i < urilen; i++) {
		if (isalnum((int)uri[i])) {
			continue;
		}
		if ((uri[i] == '/') && (uri[i + 1] != '/')) {
			continue;
		}
		if ((uri[i] == '.') && (uri[i + 1] != '.')) {
			continue;
		}
		break;
	}
	if (i != urilen) {
		fprintf(stderr, "URI security check failed: \"%s\"\n", uri);
		return NULL;
	}
	for (i = urilen; i > 0; i--) {
		if (uri[i] == '/') {
			char tmp = uri[i + 1];
			uri[i + 1] = 0;
			hashEntry = SHash_FindEntry(&ws->uriHash, uri);
			uri[i + 1] = tmp;
			uri[i] = 0;
			if (hashEntry) {
				rqh = SHash_GetValue(hashEntry);
				if (rqh) {
					rqh->path = uri + i + 1;
				}
				return rqh;
			}
		}
	}
}

#if 0
static void
WebServ_RegisterHttpRequestProc(WebServ * ws, char *uri, HttpRequestProc * proc, void *cbData)
{

}
#endif

static void
WebCon_FeedSM(WebCon * wc, uint8_t c)
{
	switch (wc->state) {
	    case WC_STATE_IDLE:
		    if (c == 'G') {
			    wc->state = WC_STATE_G;
		    } else if (c == 'P') {
			    wc->state = WC_STATE_P;
		    } else {
			    wc->state = WC_STATE_ERROR;
		    }
		    break;
	    case WC_STATE_G:
		    if (c == 'E') {
			    wc->state = WC_STATE_E;
		    } else {
			    wc->state = WC_STATE_ERROR;
		    }
		    break;

	    case WC_STATE_E:
		    if (c == 'T') {
			    wc->state = WC_STATE_T;
		    } else {
			    wc->state = WC_STATE_ERROR;
		    }
		    break;

	    case WC_STATE_T:
		    if (c == ' ') {
			    wc->method = METHOD_GET;
			    wc->state = WC_STATE_URI;
		    } else {
			    wc->state = WC_STATE_ERROR;
		    }
		    break;

	    case WC_STATE_URI:
		    if (c == ' ') {
			    wc->uri[wc->uri_len] = 0;
			    wc->state = WC_STATE_HTTP;
			    fprintf(stderr, "URI is %s\n", wc->uri);
		    } else if ((wc->uri_len + 1) < array_size(wc->uri)) {
			    wc->uri[wc->uri_len++] = c;
		    } else {
			    fprintf(stderr, "URI to long\n");
			    wc->state = WC_STATE_ERROR;
		    }
		    break;

	    case WC_STATE_HTTP:
		    if (c == '\r') {
			    wc->state = WC_STATE_LF;
		    }
		    break;

	    case WC_STATE_LF:
		    if (c == '\n') {
			    wc->state = WC_STATE_RQHDRFIELD;
			    wc->rqhdrfieldlen = 0;
		    } else {
			    wc->state = WC_STATE_ERROR;
		    }
		    break;

	    case WC_STATE_RQHDRFIELD:
		    if (c == '\r') {
			    if (wc->rqhdrfieldlen == 0) {
				    wc->state = WC_STATE_LF2;
			    } else {
				    wc->state = WC_STATE_LF;
			    }
		    } else {
			    wc->rqhdrfieldlen++;
		    }
		    break;

	    case WC_STATE_LF2:
		    if (c == '\n') {
			    wc->state = WC_STATE_IDLE;
			    fprintf(stderr, "Got request for URI %s\n", wc->uri);
			    split_args(wc->uri, MAX_URI_ARGC, wc->argv);
			    fprintf(stderr, "split %s\n", wc->argv[0]);
			    wc->rqhdrfieldlen = 0;
		    }
		    break;
	}
	fprintf(stderr, "State %u c \"%c\"\n", wc->state, c);
}

/**
 ******************************************************************************
 * \fn static void WebCon_Sink(void *eventData,int mask)
 * Feed the data comming through the socket into the connection state machine
 ******************************************************************************
 */
static void
WebCon_Sink(void *eventData, int mask)
{
	WebCon *wc = eventData;
	uint8_t data[8];
	uint8_t i;
	int cnt;
	while ((cnt = read(wc->sockfd, &data, sizeof(data))) > 0) {
		for (i = 0; i < cnt; i++) {
			WebCon_FeedSM(wc, data[i]);
		}
	}
	return;
}

static void
WebServ_Accept(int sockfd, char *host, unsigned short port, void *cd)
{
	WebServ *ws = cd;
	WebCon *wc;
	int flag = 1;
	if (ws->nr_connections >= MAX_CONNECTIONS) {
		fprintf(stderr, "Too many Web connections\n");
		close(sockfd);
		return;
	}
	wc = sg_new(WebCon);
	ws->nr_connections++;
	wc->state = WC_STATE_IDLE;
	wc->sockfd = sockfd;
	wc->webserv = ws;
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
	FIO_AddFileHandler(&wc->rfh, sockfd, FIO_READABLE, WebCon_Sink, wc);
}

/**
 ******************************************************************************
 * Create a new webserver if configured in the config file.
 ******************************************************************************
 */
void
WebServ_New(const char *name)
{
	int fd;
	WebServ *ws;
	char *host = NULL;
	int32_t port = 0;
	host = Config_ReadVar(name, "host");
	if (!host) {
		fprintf(stderr, "No webserver configured\n");
		return;
	}
	if (Config_ReadInt32(&port, name, "port") < 0) {
		fprintf(stderr, "No port for webserver \"%s\" in configfile\n", name);
		exit(1);
	}
	ws = sg_new(WebServ);
	if ((fd = FIO_InitTcpServer(&ws->tcpserv, WebServ_Accept, ws, host, port)) < 0) {
		sg_free(ws);
		fprintf(stderr, "Can not create TCP server for Web Server \"%s\"\n", name);
		exit(1);
	}
	SHash_InitTable(&ws->uriHash);
	fprintf(stderr, "WebServ Listening on \"%s:%u\"\n", host, port);
	return;
}
