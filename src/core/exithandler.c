//===-- core/exithandler.h - Generic Cleanup Facilities -----------*- C -*-===//
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
/// \file
/// This file contains the declaration of generic cleanup facilities.
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
#include <signal.h>
#include <stddef.h> // for ptrdiff_t
#include <stdlib.h> // for atexit, malloc
#include <string.h> // for strerror


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================
typedef struct ExitHandler_List_s ExitHandler_t;
struct ExitHandler_List_s {
    List_ElementMembesr(ExitHandler_t);
    ExitHandler_Callback_cb cb;
    void *data;
};


//==============================================================================
//= Function declarations(static)
//==============================================================================
static void ExitHandler_call(ExitHandler_t *handler);
static void ExitHandler_onExit(void);
static void ExitHandler_onSIGINT(int signal);
static int ExitHandler_compare(const ExitHandler_t *a, const ExitHandler_t *b);


//==============================================================================
//= Variables
//==============================================================================
static struct {
    List_Members(ExitHandler_t);
    uv_mutex_t mutex;
    sighandler_t sighandler;
} ExitHandler_handlers;


//==============================================================================
//= Function definitions(static)
//==============================================================================
static void ExitHandler_call(ExitHandler_t *handler) {
    handler->cb(handler->data);
    free(handler);
}


static void ExitHandler_onExit(void) {
    LOG_Debug("ExitHandler", "Call registerd handler...");
    List_Map(&ExitHandler_handlers, ExitHandler_call);
    uv_mutex_destroy(&ExitHandler_handlers.mutex);
    List_Init(&ExitHandler_handlers);
}


static void ExitHandler_onSIGINT(int signal) {
    LOG_Debug("ExitHandler", "Catch SIGINT");
    ExitHandler_onExit();
    if (ExitHandler_handlers.sighandler) {
        LOG_Debug("ExitHandler", "Call original SIGINT handler");
        ExitHandler_handlers.sighandler(signal);
    }
}


static int ExitHandler_compare(const ExitHandler_t *a, const ExitHandler_t *b) {
    ptrdiff_t diff;
    diff = b->cb - a->cb;
    if (diff) {
        return (diff < 0) ? -1 : 1;
    }
    diff = b->data - a->data;
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
/// @return imply an error if negative
//===----------------------------------------------------------------------===//
Lg_Errno_t ExitHandler_Init(void) {
    int err;
    sighandler_t sighandler;
    LOG_Debug("ExitHandler", "Init...");
    List_Init(&ExitHandler_handlers);
    err = uv_mutex_init(&ExitHandler_handlers.mutex);
    if (err < 0) {
        LOG_Error("ExitHandler", "uv_mutex_init failed. %s %s",
                  uv_err_name(err), uv_strerror(err));
        return LG_EUV_MUTEX;
    }
    err = atexit(&ExitHandler_onExit);
    if (err) {
        LOG_Error("ExitHandler", "atexit failed %s", strerror(err));
        return LG_EATEXIT;
    }
    sighandler = signal(SIGINT, &ExitHandler_onSIGINT);
    LOG_Verbose("ExitHandler", "original SIGINT handler:%p", sighandler);
    if (sighandler == SIG_ERR) {
        LOG_Error("ExitHandler", "signal(SIGINT) failed %s", strerror(errno));
        return LG_ESIGNAL;
    }
    ExitHandler_handlers.sighandler = sighandler;
    return LG_ESUCCESS;
}


//===----------------------------------------------------------------------===//
/// Register exit handler with any data.
///
/// @param[in]      proc        handler, calling at the leigun terminate.
/// @param[in]      data        data for handler calling. allow NULL.
///
/// @return imply an error if negative
//===----------------------------------------------------------------------===//
Lg_Errno_t ExitHandler_Register(ExitHandler_Callback_cb proc, void *data) {
    LOG_Debug("ExitHandler", "Register %p:%p", proc, data);
    ExitHandler_t *handler = malloc(sizeof(*handler));
    if (!handler) {
        LOG_Error("ExitHandler", "malloc failed %s", strerror(errno));
        return LG_ENOMEM;
    }
    List_InitElement(handler);
    handler->cb = proc;
    handler->data = data;
    uv_mutex_lock(&ExitHandler_handlers.mutex);
    List_Push(&ExitHandler_handlers, handler);
    uv_mutex_unlock(&ExitHandler_handlers.mutex);
    return LG_ESUCCESS;
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
/// @return imply an error if negative
//===----------------------------------------------------------------------===//
Lg_Errno_t ExitHandler_Unregister(ExitHandler_Callback_cb proc, void *data) {
    LOG_Debug("ExitHandler", "Unregister %p:%p", proc, data);
    const ExitHandler_t handler = {.cb = proc, .data = data};
    ExitHandler_t *result;
    uv_mutex_lock(&ExitHandler_handlers.mutex);
    List_PopBy(&ExitHandler_handlers, ExitHandler_compare, &handler, result);
    free(result);
    uv_mutex_unlock(&ExitHandler_handlers.mutex);

    return LG_ESUCCESS;
}
