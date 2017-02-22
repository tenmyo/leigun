//===-- core/asyncmanager.c - AsynchronousIO management functions -*- C -*-===//
//
//              The Leigun Embedded System Simulator Platform
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
///
/// @file
/// This file contains the definition of the AsynchronousIO management
/// functions, which is light wrapper of the libuv.
///
//===----------------------------------------------------------------------===//

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header
#include "asyncmanager.h"

// Local/Private Headers
#include "exithandler.h"
#include "leigun.h"
#include "logging.h"

// External headers
#include <uv.h>

// System headers
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
enum req_type {
    REQ_LISTEN,
    REQ_CLOSE,
    REQ_WRITE,
    REQ_READ_START,
    REQ_READ_STOP,
    REQ_POLL_START,
    REQ_POLL_STOP,
    REQ_NUM,
};

enum sreq_type {
    SREQ_WRITE,
    SREQ_POLL_INIT,
    SREQ_NUM,
};


//==============================================================================
//= Types
//==============================================================================

// -----------------------------------------------------
// Handle Types(libuv)    AsyncManager
//   uv_handle_t          <- Handle_t
//      |-- uv_stream_t     <- StreamHandle_t
//      |     `-- uv_tcp_t    <- TcpStreamHandle_t
//      |                         `-- TcpServerStreamHandle_t
//      |                         `-- TcpClientStreamHandle_t
//      `-- uv_poll_t       <- PollHandle_t
struct PollHandle_t {
    union {
        union uv_any_handle any;
        uv_handle_t handle;
        uv_poll_t poll;
    } uv; // button(inheritance)
    AsyncManager_poll_cb poll_cb;
    void *poll_clientdata;
    int events;
};

struct TcpServerStreamHandle_t {
    struct {
        union {
            union uv_any_handle any;
            uv_handle_t handle;
            uv_stream_t stream;
            uv_tcp_t tcp;
        } uv; // button(inheritance)
        AsyncManager_read_cb read_cb;
        void *read_clientdata;
    };
    AsyncManager_connection_cb connection_cb;
    void *connection_clientdata;
    struct sockaddr_in addr;
    int backlog;
    int nodelay;
};
typedef struct TcpServerStreamHandle_t TcpServerStreamHandle_t;

struct TcpClientStreamHandle_t {
    struct {
        union {
            union uv_any_handle any;
            uv_handle_t handle;
            uv_stream_t stream;
            uv_tcp_t tcp;
        } uv; // button(inheritance)
        AsyncManager_read_cb read_cb;
        void *read_clientdata;
    };
    TcpServerStreamHandle_t *server; // server handle
};
typedef struct TcpClientStreamHandle_t TcpClientStreamHandle_t;

struct TcpStreamHandle_t {
    union {
        struct {
            union {
                union uv_any_handle any;
                uv_handle_t handle;
                uv_stream_t stream;
                uv_tcp_t tcp;
            } uv; // button(inheritance)
            AsyncManager_read_cb read_cb;
            void *read_clientdata;
        };
        TcpServerStreamHandle_t server;
        TcpClientStreamHandle_t client;
    };
};
typedef struct TcpStreamHandle_t TcpStreamHandle_t;

struct StreamHandle_t {
    union {
        struct {
            union {
                union uv_any_handle any;
                uv_handle_t handle;
                uv_stream_t stream;
            } uv; // button(inheritance)
            AsyncManager_read_cb read_cb;
            void *read_clientdata;
        };
        TcpStreamHandle_t tcp;
    };
};

struct Handle_t {
    union {
        union {
            union uv_any_handle any;
            uv_handle_t handle;
        } uv; // button(inheritance)
        StreamHandle_t stream;
        PollHandle_t poll;
    };
};

// -----------------------------------------------------
// Request Types
struct write_req_t {
    uv_write_t super; // button(inheritance)
    StreamHandle_t *stream;
    uv_buf_t buf;
    AsyncManager_write_cb write_cb;
    void *write_clientdata;
};

struct close_req_t {
    Handle_t *handle;
    AsyncManager_close_cb close_cb;
    void *close_clientdata;
};

struct req_data {
    uv_async_t async; // button(inheritance)
    enum req_type type;
    uv_sem_t bsem;
    void *data;
};

struct sreq_data {
    uv_async_t async; // button(inheritance)
    enum sreq_type type;
    uv_sem_t bsem;
    uv_sem_t respsem;
    void *reqdata;
    int status;
    void *respdata;
};

// -----------------------------------------------------
struct AsyncManager {
    uv_loop_t *loop;
    uv_thread_t tid;
    struct req_data *req[REQ_NUM];
    struct sreq_data *sreq[SREQ_NUM];
    uv_idle_t idle;
    bool quit;
};
typedef struct AsyncManager AsyncManager;


//==============================================================================
//= Function declarations(static)
//==============================================================================
#define UV_ERRCHECK(err, failed)                                               \
    if ((err) < 0) {                                                           \
        LOG_Error("AM", "ERROR: %s: %s: %s[%d] %s\n", uv_err_name(err),        \
                  uv_strerror(err), __FILE__, __LINE__, __func__);             \
        failed;                                                                \
    }


// -----------------------------------------------------
static struct req_data *new_req_data(enum req_type reqno);
static struct sreq_data *new_sreq_data(enum sreq_type reqno);

static void AsyncManager_onIdle(uv_idle_t *handle);
static void server_thread(void *arg);
static int send_req(enum req_type type, void *data);
static int send_sreq(enum sreq_type type, void *data, void *result);
static void on_wakeup_req(uv_async_t *handle);
static void on_wakeup_sreq(uv_async_t *handle);

static void free_data(uv_handle_t *handle);
static void AsyncManager_onExit(void *);

// -----------------------------------------------------
static void on_connection(uv_stream_t *server, int status);
static int listen_tcp(TcpServerStreamHandle_t *svr);

static void on_writed(uv_write_t *req, int status);
static int write_stream(struct write_req_t *wr);
static int writesync_stream(struct write_req_t *wr, int *err);

static void on_closed(uv_handle_t *handle);
static int close_handle(struct close_req_t *cr);

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size,
                         uv_buf_t *buf);
static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
static int read_start(StreamHandle_t *handle);
static int read_stop(StreamHandle_t *handle);

// -----------------------------------------------------
static int poll_init(int *fd, PollHandle_t **handle);

static void on_poll(uv_poll_t *handle, int status, int events);
static int poll_start(PollHandle_t *handle);
static int poll_stop(PollHandle_t *handle);


//==============================================================================
//= Variables
//==============================================================================
static AsyncManager g_singleton;


//==============================================================================
//= Function definitions(static)
//==============================================================================
// -----------------------------------------------------
static struct req_data *new_req_data(enum req_type reqno) {
    int ret;
    struct req_data *req = LEIGUN_NEW(req);
    ret = (req) ? 0 : UV_EAI_MEMORY;
    UV_ERRCHECK(ret, return NULL);
    req->type = reqno;
    ret = uv_sem_init(&req->bsem, 1);
    UV_ERRCHECK(ret, goto REQ_ALLOCED);
    ret = uv_async_init(g_singleton.loop, &req->async, &on_wakeup_req);
    UV_ERRCHECK(ret, goto REQ_SEM_INITED);
    req->async.data = req;
    return req;

REQ_SEM_INITED:
    uv_sem_destroy(&req->bsem);
REQ_ALLOCED:
    free(req);
    return NULL;
}


static struct sreq_data *new_sreq_data(enum sreq_type reqno) {
    int ret;
    struct sreq_data *req = LEIGUN_NEW(req);
    ret = (req) ? 0 : UV_EAI_MEMORY;
    UV_ERRCHECK(ret, return NULL);
    req->type = reqno;
    ret = uv_sem_init(&req->bsem, 1);
    UV_ERRCHECK(ret, goto REQ_ALLOCED);
    ret = uv_sem_init(&req->respsem, 0);
    UV_ERRCHECK(ret, goto REQ_BSEM_INITED);
    ret = uv_async_init(g_singleton.loop, &req->async, &on_wakeup_sreq);
    UV_ERRCHECK(ret, goto REQ_RESPSEM_INITED);
    req->async.data = req;
    return req;

REQ_RESPSEM_INITED:
    uv_sem_destroy(&req->respsem);
REQ_BSEM_INITED:
    uv_sem_destroy(&req->bsem);
REQ_ALLOCED:
    free(req);
    return NULL;
}

// -----------------------------------------------------
static void AsyncManager_onIdle(uv_idle_t *handle) {
    if (g_singleton.quit) {
        uv_stop(g_singleton.loop);
    }
}

static void server_thread(void *arg) {
    uv_loop_t *loop = arg;
    LOG_Debug("AM", "AsyncManager thread start");
    uv_barrier_wait((uv_barrier_t *)loop->data);
    uv_run(loop, UV_RUN_DEFAULT);
    LOG_Debug("AM", "AsyncManager thread end");
    /// @todo: stop&close all handle
    uv_loop_close(g_singleton.loop);
}

static int send_req(enum req_type type, void *data) {
    uv_sem_wait(&g_singleton.req[type]->bsem);
    LOG_Verbose("AM", "%s(%d)", __func__, type);
    g_singleton.req[type]->data = data;
    return uv_async_send(&g_singleton.req[type]->async);
}

static int send_sreq(enum sreq_type type, void *data, void *result) {
    int ret;
    uv_sem_wait(&g_singleton.sreq[type]->bsem);
    LOG_Verbose("AM", "%s(%d)", __func__, type);
    g_singleton.sreq[type]->reqdata = data;
    ret = uv_async_send(&g_singleton.sreq[type]->async);
    UV_ERRCHECK(ret, goto END);
    uv_sem_wait(&g_singleton.sreq[type]->respsem);
    ret = g_singleton.sreq[type]->status;
    (*(void **)result) = g_singleton.sreq[type]->respdata;
    UV_ERRCHECK(ret, );
END:
    uv_sem_post(&g_singleton.sreq[type]->bsem);
    return ret;
}

static void on_wakeup_req(uv_async_t *handle) {
    struct req_data *req = handle->data;
    int err = 0;
    switch (req->type) {
    case REQ_LISTEN:
        err = listen_tcp(req->data);
        break;
    case REQ_CLOSE:
        err = close_handle(req->data);
        break;
    case REQ_WRITE:
        err = write_stream(req->data);
        break;
    case REQ_READ_START:
        err = read_start(req->data);
        break;
    case REQ_READ_STOP:
        err = read_stop(req->data);
        break;
    case REQ_POLL_START:
        err = poll_start(req->data);
        break;
    case REQ_POLL_STOP:
        err = poll_stop(req->data);
        break;
    case REQ_NUM:
        assert(!"on_wakeup_req received unknown type");
        /* NOTREACHED */
        break;
    }
    UV_ERRCHECK(err, );
    uv_sem_post(&req->bsem);
}

static void on_wakeup_sreq(uv_async_t *handle) {
    struct sreq_data *req = handle->data;
    switch (req->type) {
    case SREQ_WRITE:
        req->status = writesync_stream(req->reqdata, (void *)&req->respdata);
        break;
    case SREQ_POLL_INIT:
        req->status = poll_init(req->reqdata, (void *)&req->respdata);
        break;
    case SREQ_NUM:
        assert(!"on_wakeup_sreq received unknown type");
        /* NOTREACHED */
        break;
    }
    uv_sem_post(&req->respsem);
}

static void free_data(uv_handle_t *handle) {
    void *buf = handle->data;
    handle->data = NULL;
    free(buf);
}

static void AsyncManager_onExit(void *data) {
    LOG_Debug("AM", "AsyncManager stopping...");
    g_singleton.quit = true;
}


static void on_connection(uv_stream_t *server, int status) {
    int err = status;
    TcpClientStreamHandle_t *client = NULL;
    TcpServerStreamHandle_t *svr = server->data;
    const char *host = NULL;
    int port = 0;
    char client_address[INET_ADDRSTRLEN + 1] = {0};
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);

    LOG_Info("AM", "%s", __func__);
    UV_ERRCHECK(err, goto EMIT);
    client = LEIGUN_NEW(client);
    err = (client) ? 0 : UV_EAI_MEMORY;
    UV_ERRCHECK(err, goto EMIT);
    err = uv_tcp_init(server->loop, &client->uv.tcp);
    UV_ERRCHECK(err, goto FREE_EMIT);
    client->uv.stream.data = client;
    client->server = svr;
    err = uv_tcp_nodelay(&client->uv.tcp, svr->nodelay);
    UV_ERRCHECK(err, goto FREE_EMIT);
    err = uv_accept(server, &client->uv.stream);
    UV_ERRCHECK(err, goto CLOSE_EMIT);

    err =
        uv_tcp_getpeername(&client->uv.tcp, (struct sockaddr *)&addr, &addrlen);
    UV_ERRCHECK(err, goto CLOSE_EMIT);
    port = ntohs(addr.sin_port);
    err = uv_ip4_name(&addr, client_address, sizeof(client_address) - 1);
    UV_ERRCHECK(err, goto CLOSE_EMIT);
    host = client_address;
    LOG_Info("AM", "connected port:%d, peer:%s, peerport:%d",
             ntohs(svr->addr.sin_port), host, port);
    goto EMIT;

CLOSE_EMIT:
    uv_close(&client->uv.handle, &free_data);
    client = NULL;
FREE_EMIT:
    free(client);
    client = NULL;
EMIT:
    if (svr->connection_cb) {
        svr->connection_cb(err, (StreamHandle_t *)client, host, port,
                           svr->connection_clientdata);
    }
}

static int listen_tcp(TcpServerStreamHandle_t *svr) {
    int err;
    err = uv_tcp_init(g_singleton.loop, &svr->uv.tcp);
    UV_ERRCHECK(err, free(svr); return err);
    svr->uv.tcp.data = svr;
    err = uv_tcp_bind(&svr->uv.tcp, (const struct sockaddr *)&svr->addr, 0);
    UV_ERRCHECK(err, free_data(&svr->uv.handle); return err);
    err = uv_listen(&svr->uv.stream, svr->backlog, &on_connection);
    UV_ERRCHECK(err, uv_close(&svr->uv.handle, &free_data); return err);
    return 0;
}

static void on_writed(uv_write_t *req, int status) {
    struct write_req_t *wr = req->data;
    LOG_Verbose("AM", "%s(req:%p, handle:%p)", __func__, req, req->handle);
    UV_ERRCHECK(status, );
    if (wr->write_cb) {
        wr->write_cb(status, wr->stream, wr->write_clientdata);
    }
    free(wr);
}

static int write_stream(struct write_req_t *wr) {
    wr->super.data = wr;
    LOG_Verbose("AM", "%s(req:%p, handle:%p)", __func__, wr,
                &wr->stream->uv.stream);
    return uv_write(&wr->super, &wr->stream->uv.stream, &wr->buf, 1,
                    &on_writed);
}

static int writesync_stream(struct write_req_t *wr, int *err) {
    int ret;
    LOG_Verbose("AM", "%s", __func__);
    ret = uv_stream_set_blocking(&wr->stream->uv.stream, 1);
    UV_ERRCHECK(ret, );
    wr->super.data = wr;
    do {
        ret = uv_try_write(&wr->stream->uv.stream, &wr->buf, 1);
    } while (ret == UV_EAGAIN);
    UV_ERRCHECK(ret, );
    ret = uv_stream_set_blocking(&wr->stream->uv.stream, 0);
    UV_ERRCHECK(ret, );
    *err = ret;
    free(wr);
    return ret;
}

static void on_closed(uv_handle_t *handle) {
    if (handle->data) {
        struct close_req_t *cr = handle->data;
        if (cr->close_cb) {
            cr->close_cb(cr->handle, cr->close_clientdata);
        }
        free(cr);
    }
    free(handle);
}

static int close_handle(struct close_req_t *cr) {
    if (uv_is_closing(&cr->handle->uv.handle)) {
        free(cr);
    } else {
        cr->handle->uv.handle.data = cr;
        uv_close(&cr->handle->uv.handle, &on_closed);
    }
    return 0;
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size,
                         uv_buf_t *buf) {
    buf->base = LEIGUN_NEW_BUF(suggested_size);
    buf->len = suggested_size;
}

static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    StreamHandle_t *handle = (StreamHandle_t *)stream;
    UV_ERRCHECK((int)nread, );
    if (nread == 0) {
        // EAGAIN or EWOULDBLOCK
        return;
    }
    if (handle->read_cb) {
        handle->read_cb(handle, buf->base, ((nread == UV_EOF) ? 0 : nread),
                        handle->read_clientdata);
    }
    free(buf->base);
    if (nread < 0) {
        // ERROR
        if (!uv_is_closing(&handle->uv.handle)) {
            stream->data = stream;
            uv_close(&handle->uv.handle, &free_data);
        }
    }
}

static int read_start(StreamHandle_t *handle) {
    return uv_read_start(&handle->uv.stream, &alloc_buffer, &on_read);
}

static int read_stop(StreamHandle_t *handle) {
    return uv_read_stop(&handle->uv.stream);
}

static int poll_init(int *fd, PollHandle_t **handle) {
    int ret;
    PollHandle_t *p = LEIGUN_NEW(p);
    *handle = NULL;
    ret = (p) ? 0 : UV_EAI_MEMORY;
    UV_ERRCHECK(ret, return ret);
    ret = uv_poll_init(g_singleton.loop, &p->uv.poll, *fd);
    UV_ERRCHECK(ret, free(p); return ret);
    p->uv.poll.data = p;
    *handle = p;
    return ret;
}

static void on_poll(uv_poll_t *handle, int status, int events) {
    PollHandle_t *handle_ = handle->data;
    int events_ = 0;
    if (handle_->poll_cb) {
        events_ |= (events & UV_READABLE) ? ASYNCMANAGER_EVENT_READABLE : 0;
        events_ |= (events & UV_WRITABLE) ? ASYNCMANAGER_EVENT_WRITABLE : 0;
        handle_->poll_cb(handle_, status, events_, handle_->poll_clientdata);
    }
}

static int poll_start(PollHandle_t *handle) {
    return uv_poll_start(&handle->uv.poll, handle->events, &on_poll);
}

static int poll_stop(PollHandle_t *handle) {
    return uv_poll_stop(&handle->uv.poll);
}


//==============================================================================
//= Function definitions(global)
//==============================================================================

//===----------------------------------------------------------------------===//
/// Init handlers, And register The module handler.
///
/// - Initialize handler lists
/// - Register The module handler to exit
/// - Register The module handler to signal SIGINT
///
/// The module handler will be calling handlers,
/// which registered by ExitHandler_Register at the leigun terminate.
///
/// @return same as libuv. imply an error if negative.
//===----------------------------------------------------------------------===//
int AsyncManager_Init(void) {
    int err = 0;
    int reqno;
    uv_barrier_t blocker;
    LOG_Info("AM", "AsyncManager init");
    // prepare server loop
    g_singleton.loop = uv_default_loop();
    // prepare idle
    g_singleton.quit = false;
    err = uv_idle_init(g_singleton.loop, &g_singleton.idle);
    UV_ERRCHECK(err, return err);
    err = uv_idle_start(&g_singleton.idle, &AsyncManager_onIdle);
    UV_ERRCHECK(err, return err);
    // prepare async event notifier
    for (reqno = 0; reqno < REQ_NUM; ++reqno) {
        g_singleton.req[reqno] = new_req_data((enum req_type)reqno);
        if (!g_singleton.req[reqno]) {
            goto ERR_REQ_INIT;
        }
    }
    for (reqno = 0; reqno < SREQ_NUM; ++reqno) {
        g_singleton.sreq[reqno] = new_sreq_data((enum sreq_type)reqno);
        if (!g_singleton.sreq[reqno]) {
            goto ERR_SREQ_INIT;
        }
    }
    // start server loop in the new thread
    err = uv_barrier_init(&blocker, 2);
    UV_ERRCHECK(err, goto ERR_SREQ_INITED);
    g_singleton.loop->data = &blocker;
    err = uv_thread_create(&g_singleton.tid, &server_thread, g_singleton.loop);
    UV_ERRCHECK(err, goto ERR_SREQ_INITED);
    uv_barrier_wait(&blocker);
    uv_barrier_destroy(&blocker);
    // register resource release
    ExitHandler_Register(&AsyncManager_onExit, &g_singleton);
    return 0;

// error handlers
ERR_SREQ_INITED:
ERR_SREQ_INIT:
    for (reqno = 0; (reqno < SREQ_NUM) && g_singleton.sreq[reqno]; ++reqno) {
        uv_close((uv_handle_t *)&g_singleton.sreq[reqno]->async, NULL);
        uv_sem_destroy(&g_singleton.sreq[reqno]->bsem);
        uv_sem_destroy(&g_singleton.sreq[reqno]->respsem);
        free(g_singleton.sreq[reqno]);
    }
ERR_REQ_INIT:
    for (reqno = 0; (reqno < REQ_NUM) && g_singleton.req[reqno]; ++reqno) {
        uv_close((uv_handle_t *)&g_singleton.req[reqno]->async, NULL);
        uv_sem_destroy(&g_singleton.req[reqno]->bsem);
        free(g_singleton.req[reqno]);
    }
    while (uv_loop_close(g_singleton.loop)) {
        ;
    }

    return err;
}


int AsyncManager_Close(Handle_t *handle, AsyncManager_close_cb close_cb,
                       void *clientdata) {
    int ret;
    LOG_Info("AM", "%s(%p)", __func__, handle);
    // create close request
    struct close_req_t *cr = LEIGUN_NEW(cr);
    ret = (cr) ? 0 : UV_EAI_MEMORY;
    UV_ERRCHECK(ret, return ret);
    cr->handle = handle;
    cr->close_cb = close_cb;
    cr->close_clientdata = clientdata;
    // check context == libuv
    uv_thread_t tid = uv_thread_self();
    if (uv_thread_equal(&g_singleton.tid, &tid)) {
        ret = close_handle(cr);
    } else {
        ret = send_req(REQ_CLOSE, cr);
    }
    return ret;
}


int AsyncManager_BufferSizeSend(Handle_t *handle, int *value) {
    LOG_Info("AM", "%s(%d)", __func__, *value);
    return uv_send_buffer_size(&handle->uv.handle, value);
}


int AsyncManager_BufferSizeRecv(Handle_t *handle, int *value) {
    LOG_Info("AM", "%s(%d)", __func__, *value);
    return uv_recv_buffer_size(&handle->uv.handle, value);
}


int AsyncManager_Write(StreamHandle_t *handle, const void *base,
                       unsigned int len, AsyncManager_write_cb write_cb,
                       void *clientdata) {
    int ret;
    LOG_Debug("AM", "%s(handle:%p, base:%p, len:%d)", __func__, handle, base,
              len);
    // create write request
    struct write_req_t *wr = LEIGUN_NEW(wr);
    ret = (wr) ? 0 : UV_EAI_MEMORY;
    UV_ERRCHECK(ret, return ret);
    wr->stream = handle;
    wr->buf = uv_buf_init((char *)base, len);
    wr->write_cb = write_cb;
    wr->write_clientdata = clientdata;
    // check context == libuv
    uv_thread_t tid = uv_thread_self();
    if (uv_thread_equal(&g_singleton.tid, &tid)) {
        ret = write_stream(wr);
    } else {
        ret = send_req(REQ_WRITE, wr);
    }
    return ret;
}


int AsyncManager_WriteSync(StreamHandle_t *handle, const void *base,
                           unsigned int len) {
    int ret;
    int err;
    LOG_Debug("AM", "%s(handle:%p, base:%p, len:%d)", __func__, handle, base,
              len);
    // create write request
    struct write_req_t *wr = LEIGUN_NEW(wr);
    ret = (wr) ? 0 : UV_EAI_MEMORY;
    UV_ERRCHECK(ret, return ret);
    wr->stream = handle;
    wr->buf = uv_buf_init((char *)base, len);
    // check context == libuv
    uv_thread_t tid = uv_thread_self();
    if (uv_thread_equal(&g_singleton.tid, &tid)) {
        ret = writesync_stream(wr, &err);
    } else {
        ret = send_sreq(SREQ_WRITE, wr, &err);
    }
    UV_ERRCHECK(ret, return ret);
    return err;
}


int AsyncManager_ReadStart(StreamHandle_t *handle, AsyncManager_read_cb read_cb,
                           void *clientdata) {
    int ret;
    LOG_Info("AM", "%s(%p)", __func__, handle);
    // create read start request -> in TcpHandle
    handle->read_cb = read_cb;
    handle->read_clientdata = clientdata;
    // check context == libuv
    uv_thread_t tid = uv_thread_self();
    if (uv_thread_equal(&g_singleton.tid, &tid)) {
        ret = read_start(handle);
    } else {
        ret = send_req(REQ_READ_START, handle);
    }
    return ret;
}


int AsyncManager_ReadStop(StreamHandle_t *handle) {
    int ret;
    LOG_Info("AM", "%s(%p)", __func__, handle);
    // create read stop request -> NONE
    // check context == libuv
    uv_thread_t tid = uv_thread_self();
    if (uv_thread_equal(&g_singleton.tid, &tid)) {
        ret = read_stop(handle);
    } else {
        ret = send_req(REQ_READ_STOP, handle);
    }
    return ret;
}


int AsyncManager_InitTcpServer(const char *ip, int port, int backlog,
                               int nodelay, AsyncManager_connection_cb cb,
                               void *clientdata) {
    int err;
    LOG_Info("AM", "%s(ip:%s, port:%d)", __func__, ip, port);
    TcpServerStreamHandle_t *svr = LEIGUN_NEW(svr);
    err = (svr) ? 0 : UV_EAI_MEMORY;
    UV_ERRCHECK(err, return err);
    err = uv_ip4_addr(ip, port, &svr->addr);
    UV_ERRCHECK(err, free(svr); return err);
    svr->backlog = backlog;
    svr->nodelay = nodelay;
    svr->connection_cb = cb;
    svr->connection_clientdata = clientdata;
    uv_thread_t tid = uv_thread_self();
    if (uv_thread_equal(&g_singleton.tid, &tid)) {
        err = listen_tcp(svr);
    } else {
        err = send_req(REQ_LISTEN, svr);
    }
    return err;
}


PollHandle_t *AsyncManager_PollInit(int fd) {
    int ret;
    PollHandle_t *handle = NULL;
    LOG_Info("AM", "%s[%d] %s", __FILE__, __LINE__, __func__);
    // check context == libuv
    uv_thread_t tid = uv_thread_self();
    if (uv_thread_equal(&g_singleton.tid, &tid)) {
        ret = poll_init(&fd, &handle);
    } else {
        ret = send_sreq(SREQ_POLL_INIT, &fd, &handle);
    }
    UV_ERRCHECK(ret, return NULL);
    return handle;
}

int AsyncManager_PollStart(PollHandle_t *handle, int events,
                           AsyncManager_poll_cb cb, void *clientdata) {
    int ret;
    LOG_Info("AM", "%s[%d] %s", __FILE__, __LINE__, __func__);
    // create read start request -> in PollHandle
    handle->poll_cb = cb;
    handle->poll_clientdata = clientdata;
    handle->events = 0;
    handle->events |= (events & ASYNCMANAGER_EVENT_READABLE) ? UV_READABLE : 0;
    handle->events |= (events & ASYNCMANAGER_EVENT_WRITABLE) ? UV_WRITABLE : 0;
    // check context == libuv
    uv_thread_t tid = uv_thread_self();
    if (uv_thread_equal(&g_singleton.tid, &tid)) {
        ret = poll_start(handle);
    } else {
        ret = send_req(REQ_POLL_START, handle);
    }
    return ret;
}

int AsyncManager_PollStop(PollHandle_t *handle) {
    int ret;
    LOG_Info("AM", "%s[%d] %s", __FILE__, __LINE__, __func__);
    // create read start request -> already PollHandle
    // check context == libuv
    uv_thread_t tid = uv_thread_self();
    if (uv_thread_equal(&g_singleton.tid, &tid)) {
        ret = poll_stop(handle);
    } else {
        ret = send_req(REQ_POLL_STOP, handle);
    }
    return ret;
}
