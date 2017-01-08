/*
  Copyright 2017 TENMYO Masakazu. All rights reserved.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */
 // include self header
#include "asyncmanager.h"

// include system header
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// include library header
#include <uv.h>

// include user header
#include "initializer.h"

#define UV_ERRCHECK(err, failed) \
  if (err < 0) { \
    fprintf(stderr, "ERROR: %s: %s: %s[%d] %s\n", uv_err_name(err), uv_strerror(err), __FILE__, __LINE__, __func__); \
    failed; \
  }

enum req_type {
  REQ_QUIT,
  REQ_LISTEN,
  REQ_CLOSE,
  REQ_WRITE,
  REQ_READ_START,
  REQ_READ_STOP,
  REQ_NUM,
};
typedef enum req_type req_type;

struct req_data {
  uv_async_t async; // button(inheritance)
  req_type type;
  uv_sem_t bsem;
  void *data;
};
typedef struct req_data req_data;

struct AsyncManager {
  uv_loop_t *loop;
  uv_thread_t tid;
  req_data *req[REQ_NUM];
};
typedef struct AsyncManager AsyncManager;

static AsyncManager *g_singleton;
static uv_once_t init_guard = UV_ONCE_INIT;

static void init(void);
static void init_once(void);

static void server_thread(void *arg);
static int send_req(req_type type, void *data);
static void on_wakeup(uv_async_t *handle);

static void free_data(uv_handle_t *handle);
static void close_all(uv_handle_t *handle, void *arg);
static void on_exit(void);

// -----------------------------------------------------

struct TcpServer {
  uv_tcp_t super; // button(inheritance)
  AsyncManager_accept_cb on_connect;
  void *clientdata;
  struct sockaddr_in addr;
  int backlog;
};
typedef struct TcpServer TcpServer;

struct TcpHandle_t {
  uv_tcp_t super; // button(inheritance)
  TcpServer *server; // listening server handle, if super is that handle connected to client
  AsyncManager_read_cb read_cb;
  void *clientdata;
};

struct write_req_t {
  uv_write_t super; // button(inheritance)
  uv_buf_t buf;
  AsyncManager_write_cb write_cb;
  void *clientdata;
};

struct close_req_t {
  struct TcpHandle_t *handle;
  AsyncManager_close_cb close_cb;
  void *clientdata;
};

static void on_connect(uv_stream_t *server, int status);
static int listen_tcp(TcpServer *svr);

static void on_writed(uv_write_t *req, int status);
static int write_stream(struct write_req_t *wr);

static void on_closed(uv_handle_t *handle);
static int close_handle(struct close_req_t *cr);

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
static int read_start_stream(struct TcpHandle_t *handle);
static int read_stop_stream(struct TcpHandle_t *handle);
// -----------------------------------------------------

static void init(void) {
  int err = 0;
  int reqno;
  uv_barrier_t blocker;
  setbuf(stderr, NULL);
  fprintf(stderr, "AsyncManager init.\n");
  g_singleton = malloc(sizeof(*g_singleton));
  err = (g_singleton) ? 0 : UV_EAI_MEMORY;
  UV_ERRCHECK(err, return);
  // prepare server loop
  g_singleton->loop = uv_default_loop();
  // prepare async event notifier
  for (reqno = 0; reqno < REQ_NUM; ++reqno) {
    g_singleton->req[reqno] = malloc(sizeof(*g_singleton->req[reqno]));
    g_singleton->req[reqno]->type = reqno;
    err = uv_sem_init(&g_singleton->req[reqno]->bsem, 1);
    UV_ERRCHECK(err, free(g_singleton->req[reqno]);  goto ERR_ASYNC_INITED);
    err = uv_async_init(g_singleton->loop, &g_singleton->req[reqno]->async, &on_wakeup);
    UV_ERRCHECK(err, uv_sem_destroy(&g_singleton->req[reqno]->bsem); free(g_singleton->req[reqno]); goto ERR_ASYNC_INITED);
    g_singleton->req[reqno]->async.data = g_singleton->req[reqno];
  }
  // start server loop in the new thread
  err = uv_barrier_init(&blocker, 2);
  UV_ERRCHECK(err, goto ERR_ASYNC_INITED);
  g_singleton->loop->data = &blocker;
  err = uv_thread_create(&g_singleton->tid, &server_thread, g_singleton->loop);
  UV_ERRCHECK(err, goto ERR_ASYNC_INITED);
  uv_barrier_wait(&blocker);
  uv_barrier_destroy(&blocker);
  // register resource release
  atexit(&on_exit);
  return;

  // error handlers
ERR_ASYNC_INITED:
  for (; reqno > 0; --reqno) {
    uv_close((uv_handle_t *)&g_singleton->req[reqno - 1]->async, NULL);
    uv_sem_destroy(&g_singleton->req[reqno - 1]->bsem);
    free(g_singleton->req[reqno - 1]);
  }
  while (uv_loop_close(g_singleton->loop));
  free(g_singleton);
  g_singleton = NULL;
  abort();
}

static void init_once(void) {
  uv_once(&init_guard, init);
}

static void server_thread(void *arg) {
  uv_loop_t *loop = arg;
  fprintf(stderr, "AsyncManager start.\n");
  uv_barrier_wait((uv_barrier_t *)loop->data);
  uv_run(loop, UV_RUN_DEFAULT);
  fprintf(stderr, "AsyncManager stop.\n");
}

static int send_req(req_type type, void *data) {
  uv_sem_wait(&g_singleton->req[type]->bsem);
  g_singleton->req[type]->data = data;
  return uv_async_send(&g_singleton->req[type]->async);
}

static void on_wakeup(uv_async_t *handle) {
  req_data *req = handle->data;
  switch (req->type) {
  case REQ_QUIT:
    uv_walk(handle->loop, &close_all, NULL);
    break;
  case REQ_LISTEN:
    listen_tcp(req->data);
    break;
  case REQ_CLOSE:
    close_handle(req->data);
    break;
  case REQ_WRITE:
    write_stream(req->data);
    break;
  case REQ_READ_START:
    read_start_stream(req->data);
    break;
  case REQ_READ_STOP:
    read_stop_stream(req->data);
    break;
  default:
    break;
  }
  uv_sem_post(&req->bsem);
}

static void free_data(uv_handle_t *handle) {
  free(handle->data);
  handle->data = NULL;
}

static void close_all(uv_handle_t *handle, void *arg) {
  // FIXME: leak req->bsem
  if (!uv_is_closing(handle)) {
    uv_close(handle, &free_data);
  }
}

static void on_exit(void) {
  if (!g_singleton) {
    return;
  }
  fprintf(stderr, "AsyncManager stopping...\n");
  if (uv_loop_alive(g_singleton->loop)) {
    send_req(REQ_QUIT, NULL);
  }
  uv_thread_join(&g_singleton->tid);
  uv_loop_close(g_singleton->loop);
  free(g_singleton);
  g_singleton = NULL;
  fprintf(stderr, "AsyncManager fin.\n");
}


// -------------------------------------------------------------------------------------------------------------

static void on_connect(uv_stream_t *server, int status) {
  int err = status;
  TcpHandle_t *client;
  TcpServer *svr = server->data;
  const char *host = NULL;
  int port = 0;
  char client_address[INET_ADDRSTRLEN +1] = { 0 };
  struct sockaddr_in addr;
  int addrlen = sizeof(addr);

  fprintf(stderr, "%s[%d] %s\n", __FILE__, __LINE__, __func__);
  UV_ERRCHECK(err, goto EMIT);
  client = malloc(sizeof(*client));
  err = (client) ? 0 : UV_EAI_MEMORY;
  UV_ERRCHECK(err, goto EMIT);
  err = uv_tcp_init(server->loop, &client->super);
  UV_ERRCHECK(err, goto FREE_EMIT);
  client->super.data = client;
  client->server = svr;
  err = uv_accept(server, (uv_stream_t *)client);
  UV_ERRCHECK(err, goto CLOSE_EMIT);

  err = uv_tcp_getpeername(&client->super, (struct sockaddr *)&addr, &addrlen);
  UV_ERRCHECK(err, goto CLOSE_EMIT);
  port = addr.sin_port;
  err = uv_ip4_name(&addr, client_address, sizeof(client_address) - 1);
  UV_ERRCHECK(err, goto CLOSE_EMIT);
  host = client_address;
  goto EMIT;
  
CLOSE_EMIT:
  uv_close((uv_handle_t *)client, &free_data);
  client = NULL;
FREE_EMIT:
  free(client);
  client = NULL;
EMIT:
  if (svr->on_connect) {
    svr->on_connect(err, client, host, port, svr->clientdata);
  }
}

static int listen_tcp(TcpServer *svr) {
  int err;
  fprintf(stderr, "%s[%d] %s\n", __FILE__, __LINE__, __func__);
  err = uv_tcp_init(g_singleton->loop, &svr->super);
  UV_ERRCHECK(err, free(svr); return err);
  svr->super.data = svr;
  err = uv_tcp_bind(&svr->super, (const struct sockaddr *)&svr->addr, 0);
  UV_ERRCHECK(err, free_data((uv_handle_t *)&svr->super); return err);
  err = uv_listen((uv_stream_t*)&svr->super, svr->backlog, on_connect);
  UV_ERRCHECK(err, uv_close((uv_handle_t *)&svr->super, &free_data); return err);
  return 0;
}

int AsyncServer_InitTcpServer(const char *ip, int port, int backlog, AsyncManager_accept_cb cb, void *clientdata) {
  int err;
  init_once();
  fprintf(stderr, "%s[%d] %s\n", __FILE__, __LINE__, __func__);
  TcpServer *svr = malloc(sizeof(*svr));
  err = (svr) ? 0 : UV_EAI_MEMORY;
  UV_ERRCHECK(err, return err);
  err = uv_ip4_addr(ip, port, &svr->addr);
  UV_ERRCHECK(err, free(svr); return err);
  svr->backlog = backlog;
  svr->on_connect = cb;
  svr->clientdata = clientdata;
  uv_thread_t tid = uv_thread_self();
  if (uv_thread_equal(&g_singleton->tid, &tid)) {
    err = listen_tcp(svr);
  } else {
    err = send_req(REQ_LISTEN, svr);
  }
  return err;
}

static void on_writed(uv_write_t *req, int status) {
  struct write_req_t *wr = (struct write_req_t *)req;
  if (wr->write_cb) {
    wr->write_cb(status, (TcpHandle_t *)req->handle, wr->clientdata);
  }
  free(wr);
}

static int write_stream(struct write_req_t *wr) {
  return uv_write((uv_write_t *)wr, wr->super.handle, &wr->buf, 1, on_writed);
}

int AsyncServer_Write(TcpHandle_t *handle, const void *base, size_t len, AsyncManager_write_cb write_cb, void *clientdata) {
  int ret;
  // create write request
  struct write_req_t *wr = malloc(sizeof(*wr));
  ret = (wr) ? 0 : UV_EAI_MEMORY;
  UV_ERRCHECK(ret, return ret);
  wr->super.handle = (uv_stream_t *)handle;
  wr->super.data = wr;
  wr->buf = uv_buf_init(base, len);
  wr->write_cb = write_cb;
  wr->clientdata = clientdata;
  // check context == libuv
  uv_thread_t tid = uv_thread_self();
  if (uv_thread_equal(&g_singleton->tid, &tid)) {
    ret = write_stream(wr);
  } else {
    ret = send_req(REQ_WRITE, wr);
  }
  return ret;
}

static void on_closed(uv_handle_t *handle) {
  if (handle->data) {
    struct close_req_t *cr = (struct close_req_t *)handle->data;
    assert(handle == (uv_handle_t *)&cr->handle->super);
    if (cr->close_cb) {
      cr->close_cb(cr->handle, cr->clientdata);
    }
    free(cr);
  }
  free(handle);
}

static int close_handle(struct close_req_t *cr) {
  cr->handle->super.data = cr;
  uv_close((uv_handle_t *)&cr->handle->super, &on_closed);
  return 0;
}

int AsyncServer_Close(TcpHandle_t *handle, AsyncManager_close_cb close_cb, void *clientdata) {
  int ret;
  fprintf(stderr, "%s[%d] %s\n", __FILE__, __LINE__, __func__);
  // create close request
  struct close_req_t *cr = malloc(sizeof(*cr));
  ret = (cr) ? 0 : UV_EAI_MEMORY;
  UV_ERRCHECK(ret, return ret);
  cr->handle = handle;
  cr->close_cb = close_cb;
  cr->clientdata = clientdata;
  // check context == libuv
  uv_thread_t tid = uv_thread_self();
  if (uv_thread_equal(&g_singleton->tid, &tid)) {
    ret = close_handle(cr);
  } else {
    ret = send_req(REQ_CLOSE, cr);
  }
  return ret;
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = (char*)malloc(suggested_size);
  buf->len = suggested_size;
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  struct TcpHandle_t *handle = (struct TcpHandle_t *)stream;
  if (nread < 0) {
    handle->read_cb(handle, NULL, nread, handle->clientdata);
    if (!uv_is_closing((uv_handle_t *)stream)) {
      stream->data = NULL;
      uv_close((uv_handle_t *)stream, &on_closed);
    }
  }
  if (nread > 0) {
    handle->read_cb(handle, buf->base, nread, handle->clientdata);
  }
  free(buf->base);
}

static int read_start_stream(struct TcpHandle_t *handle) {
  int ret;
  ret = uv_read_start((uv_stream_t *)&handle->super, &alloc_buffer, &on_read);
  return ret;
}

int AsyncServer_ReadStart(TcpHandle_t *handle, AsyncManager_read_cb read_cb, void *clientdata) {
  int ret;
  fprintf(stderr, "%s[%d] %s\n", __FILE__, __LINE__, __func__);
  // create read start request -> in TcpHandle
  handle->read_cb = read_cb;
  handle->clientdata = clientdata;
  // check context == libuv
  uv_thread_t tid = uv_thread_self();
  if (uv_thread_equal(&g_singleton->tid, &tid)) {
    ret = read_start_stream(handle);
  } else {
    ret = send_req(REQ_READ_START, handle);
  }
  return ret;
}

static int read_stop_stream(struct TcpHandle_t *handle) {
  return uv_read_stop((uv_stream_t *)&handle->super);
}

int AsyncServer_ReadStop(TcpHandle_t *handle) {
  int ret;
  fprintf(stderr, "%s[%d] %s\n", __FILE__, __LINE__, __func__);
  // create read stop request -> NONE
  // check context == libuv
  uv_thread_t tid = uv_thread_self();
  if (uv_thread_equal(&g_singleton->tid, &tid)) {
    ret = read_stop_stream(handle);
  } else {
    ret = send_req(REQ_READ_STOP, handle);
  }
  return ret;
}

