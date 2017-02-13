//===-- core/exithandler.c - Generic Cleanup Facilities -----------*- C -*-===//
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
/// This file contains the definitions of generic cleanup facilities.
///
//===----------------------------------------------------------------------===//

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header
#include "core/exithandler.h"

// Local/Private Headers
#include "core/list.h"
#include "core/logging.h"

// External headers
#include <uv.h> // for mutex

// System headers
#include <errno.h>
#include <inttypes.h> // for uintptr_t
#include <stdbool.h>
#include <stdint.h> // for intptr_t
#include <stdlib.h> // for atexit, malloc
#include <string.h> // for strerror
#ifdef __unix__
#include <sched.h>
#define yield() sched_yield()
#elif defined(_MSC_VER)
#include <windows.h>
#define yield() SwitchToThread()
#else
#define yield()
#endif


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
static const char *MOD_NAME = "ExitHandler";


//==============================================================================
//= Types
//==============================================================================
typedef struct ExitHandler_List_s {
    List_Element_t liste;
    ExitHandler_Callback_cb cb;
    void *data;
} ExitHandler_t;


//==============================================================================
//= Function declarations(static)
//==============================================================================
static void ExitHandler_call(ExitHandler_t *handler);
static void ExitHandler_callAll(void);
static void ExitHandler_onExit(void);
static void ExitHandler_onSIGINT(uv_signal_t *handle, int signum);
static void ExitHandler_onAsync(uv_async_t *handle);
static void ExitHandler_thread(void *arg);
static int ExitHandler_compare(const ExitHandler_t *a, const ExitHandler_t *b);


//==============================================================================
//= Variables
//==============================================================================
static struct {
    List_t list;
    uv_mutex_t list_mutex;
    struct {
        uv_thread_t id;
        uv_loop_t loop;
        uv_signal_t sigint;
        uv_async_t async;
    } thread;
    bool exitting;
} ExitHandler_handlers;


//==============================================================================
//= Function definitions(static)
//==============================================================================
static void ExitHandler_call(ExitHandler_t *handler) {
    handler->cb(handler->data);
    free(handler);
}


static void ExitHandler_callAll(void) {
    LOG_Debug(MOD_NAME, "Call registerd handler...");
    uv_mutex_lock(&ExitHandler_handlers.list_mutex);
    List_PopEach(&ExitHandler_handlers.list, (List_Proc_cb)&ExitHandler_call);
    uv_mutex_unlock(&ExitHandler_handlers.list_mutex);
    yield();
}


static void ExitHandler_onExit(void) {
    LOG_Debug(MOD_NAME, "Catch EXIT");
    ExitHandler_handlers.exitting = true;
    if (uv_is_active((uv_handle_t *)&ExitHandler_handlers.thread.async)) {
        uv_async_send(&ExitHandler_handlers.thread.async);
    }
    uv_thread_join(&ExitHandler_handlers.thread.id);
}


static void ExitHandler_onSIGINT(uv_signal_t *handle, int signum) {
    LOG_Debug(MOD_NAME, "Catch SIGINT");
    ExitHandler_callAll();
    uv_stop(&ExitHandler_handlers.thread.loop);
}


static void ExitHandler_onAsync(uv_async_t *handle) {
    LOG_Debug(MOD_NAME, "Receive QUIT Message");
    ExitHandler_callAll();
    uv_stop(&ExitHandler_handlers.thread.loop);
}


static void ExitHandler_thread(void *arg) {
    LOG_Debug(MOD_NAME, "Thread start");
    uv_run(&ExitHandler_handlers.thread.loop, UV_RUN_DEFAULT);
    uv_signal_stop(&ExitHandler_handlers.thread.sigint);
    uv_close((uv_handle_t *)&ExitHandler_handlers.thread.sigint, NULL);
    uv_close((uv_handle_t *)&ExitHandler_handlers.thread.async, NULL);
    uv_run(&ExitHandler_handlers.thread.loop, UV_RUN_NOWAIT);
    uv_loop_close(&ExitHandler_handlers.thread.loop);
    LOG_Debug(MOD_NAME, "Thread stop");
    if (!ExitHandler_handlers.exitting) {
        exit(0);
    }
}


static int ExitHandler_compare(const ExitHandler_t *a, const ExitHandler_t *b) {
    intptr_t diff;
    diff = (intptr_t)b->cb - (intptr_t)a->cb;
    if (diff) {
        return (diff < 0) ? -1 : 1;
    }
    diff = (intptr_t)b->data - (intptr_t)a->data;
    if (diff == 0) {
        return 0;
    }
    return (diff < 0) ? -1 : 1;
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
int ExitHandler_Init(void) {
    int err;
    LOG_Debug(MOD_NAME, "Init...");
    List_Init(&ExitHandler_handlers.list);
    ExitHandler_handlers.exitting = false;
    err = uv_mutex_init(&ExitHandler_handlers.list_mutex);
    if (err < 0) {
        LOG_Error(MOD_NAME, "uv_mutex_init failed. %s %s", uv_err_name(err),
                  uv_strerror(err));
        return err;
    }
    err = uv_loop_init(&ExitHandler_handlers.thread.loop);
    if (err < 0) {
        LOG_Error(MOD_NAME, "uv_thread_create failed. %s %s", uv_err_name(err),
                  uv_strerror(err));
        return err;
    }
    err =
        uv_async_init(&ExitHandler_handlers.thread.loop,
                      &ExitHandler_handlers.thread.async, &ExitHandler_onAsync);
    if (err < 0) {
        LOG_Error(MOD_NAME, "uv_async_init failed. %s %s", uv_err_name(err),
                  uv_strerror(err));
        goto ERR_LOOP;
    }
    err = uv_signal_init(&ExitHandler_handlers.thread.loop,
                         &ExitHandler_handlers.thread.sigint);
    if (err < 0) {
        LOG_Error(MOD_NAME, "uv_signal_init failed. %s %s", uv_err_name(err),
                  uv_strerror(err));
        goto ERR_LOOP;
    }
    err = uv_signal_start(&ExitHandler_handlers.thread.sigint,
                          &ExitHandler_onSIGINT, SIGINT);
    if (err < 0) {
        LOG_Error(MOD_NAME, "uv_signal_start failed. %s %s", uv_err_name(err),
                  uv_strerror(err));
        goto ERR_LOOP;
    }
    err = uv_thread_create(&ExitHandler_handlers.thread.id, &ExitHandler_thread,
                           NULL);
    if (err < 0) {
        LOG_Error(MOD_NAME, "uv_thread_create failed. %s %s", uv_err_name(err),
                  uv_strerror(err));
        goto ERR_LOOP;
    }
    err = atexit(&ExitHandler_onExit);
    if (err) {
        LOG_Error(MOD_NAME, "atexit failed %s", strerror(err));
        return uv_translate_sys_error(err);
    }
    return 0;

ERR_LOOP:
    uv_loop_close(&ExitHandler_handlers.thread.loop);
    return err;
}


//===----------------------------------------------------------------------===//
/// Register exit handler with any data.
///
/// @param[in]      proc        handler, calling at the leigun terminate.
/// @param[in]      data        data for handler calling. allow NULL.
///
/// @return same as libuv. imply an error if negative.
//===----------------------------------------------------------------------===//
int ExitHandler_Register(ExitHandler_Callback_cb proc, void *data) {
    LOG_Debug(MOD_NAME, "Register %08zX:%p", (uintptr_t)proc, data);
    ExitHandler_t *handler = malloc(sizeof(*handler));
    if (!handler) {
        LOG_Error(MOD_NAME, "malloc failed %s", strerror(errno));
        return UV_EAI_MEMORY;
    }
    List_InitElement(&handler->liste);
    handler->cb = proc;
    handler->data = data;
    uv_mutex_lock(&ExitHandler_handlers.list_mutex);
    List_Push(&ExitHandler_handlers.list, &handler->liste);
    uv_mutex_unlock(&ExitHandler_handlers.list_mutex);
    return 0;
}


//===----------------------------------------------------------------------===//
/// Unregister registered exit handler with data.
///
/// @param[in]      proc        handler.
/// @param[in]      data        data.
///
/// @attention if not registered, it's not an error.
/// @attention if duplicated, unregister only a last registered handler.
///
/// @return same as libuv. imply an error if negative.
//===----------------------------------------------------------------------===//
int ExitHandler_Unregister(ExitHandler_Callback_cb proc, void *data) {
    LOG_Debug(MOD_NAME, "Unregister %08zX:%p", (uintptr_t)proc, data);
    const ExitHandler_t handler = {.cb = proc, .data = data};
    ExitHandler_t *result;
    uv_mutex_lock(&ExitHandler_handlers.list_mutex);
    result = List_PopBy(&ExitHandler_handlers.list,
                        (List_Compare_cb)&ExitHandler_compare, &handler.liste);
    free(result);
    uv_mutex_unlock(&ExitHandler_handlers.list_mutex);

    return 0;
}
