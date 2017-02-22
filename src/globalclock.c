//===-- core/globalclock.h ----------------------------------------*- C -*-===//
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
///
//===----------------------------------------------------------------------===//

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header
#include "globalclock.h"

// Local/Private Headers
#include "leigun.h"
#include "list.h"
#include "logging.h"

// External headers
#include <uv.h> // for mutex

// System headers
#include <inttypes.h> // for PRId64
#include <stdbool.h>
#include <stdlib.h> // for malloc
#include <string.h> // for strerror


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
static const char *MOD_NAME = "GlobalClock";


//==============================================================================
//= Types
//==============================================================================
struct GlobalClock_LocalClock_s {
    List_Element_t liste;
    uv_thread_t tid;
    GlobalClock_Proc_cb proc;
    void *data;
    uint64_t hz;
    uint64_t period_cnt;
    uint_fast16_t period_cnt_reminder;
    uint64_t rest_cnt;
    uint_fast16_t rest_fraction;
};


//==============================================================================
//= Variables
//==============================================================================
static struct {
    List_t list;
    uint32_t list_num;
    uv_mutex_t list_mutex;
    uint32_t period_ms;
    uv_barrier_t barrier;
    bool running;
} GlobalClock_clock;


//==============================================================================
//= Function declarations(static)
//==============================================================================
static void GlobalClock_thread(void *arg);
static void GlobalClock_createThread(GlobalClock_LocalClock_t *clk);
static void GlobalClock_setFrequency(GlobalClock_LocalClock_t *clk,
                                     uint64_t hz);

//==============================================================================
//= Function definitions(static)
//==============================================================================
static void GlobalClock_thread(void *arg) {
    GlobalClock_LocalClock_t *clk = arg;
    clk->proc(clk, clk->data);
}

static void GlobalClock_createThread(GlobalClock_LocalClock_t *clk) {
    uv_thread_create(&clk->tid, &GlobalClock_thread, clk);
}


static void GlobalClock_setFrequency(GlobalClock_LocalClock_t *clk,
                                     uint64_t hz) {
    clk->hz = hz;
    clk->period_cnt = (hz * GlobalClock_clock.period_ms) / 1000;
    clk->period_cnt_reminder = (hz * GlobalClock_clock.period_ms) % 1000;
    clk->rest_fraction = 0;
    // Don't care rest_cnt
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
int GlobalClock_Init(uint32_t period_ms) {
    int err;
    LOG_Debug(MOD_NAME, "Init...");
    List_Init(&GlobalClock_clock.list);
    err = uv_mutex_init(&GlobalClock_clock.list_mutex);
    if (err < 0) {
        LOG_Error(MOD_NAME, "uv_mutex_init failed. %s %s", uv_err_name(err),
                  uv_strerror(err));
        return err;
    }
    GlobalClock_clock.period_ms = period_ms;
    GlobalClock_clock.running = false;
    GlobalClock_clock.list_num = 0;
    return 0;
}


int GlobalClock_Start(void) {
    int err = 0;
    uint64_t prev;
    uint64_t now;
    LOG_Info(MOD_NAME, "Start global clock");
    if (GlobalClock_clock.list_num == 0) {
        LOG_Warn(MOD_NAME, "no have registered proc");
        err = UV_ENOENT;
        goto END;
    }
    GlobalClock_clock.running = true;
    err = uv_barrier_init(&GlobalClock_clock.barrier,
                          GlobalClock_clock.list_num + 1);
    if (err < 0) {
        LOG_Error(MOD_NAME, "uv_barrier_init failed. %s %s", uv_err_name(err),
                  uv_strerror(err));
        err = 0;
        goto END;
    }
    uv_mutex_lock(&GlobalClock_clock.list_mutex);
    List_Map(&GlobalClock_clock.list, (List_Proc_cb)&GlobalClock_createThread);
    uv_mutex_unlock(&GlobalClock_clock.list_mutex);
    prev = uv_hrtime();
    for (;;) {
        uv_barrier_wait(&GlobalClock_clock.barrier);
        now = uv_hrtime();
        LOG_Debug(MOD_NAME, "exp[ms]: %" PRId32 "\treal[ms]: %.3lf",
                  GlobalClock_clock.period_ms, (now - prev) * 1e-6);
        prev = now;
    }
END:
    return err;
}


int GlobalClock_Registor(GlobalClock_Proc_cb proc, void *data, uint64_t hz) {
    LOG_Debug(MOD_NAME, "Register %08zX:%p", (uintptr_t)proc, data);
    if (GlobalClock_clock.running) {
        LOG_Error(MOD_NAME, "GlobalClock alreay running");
        return UV_EALREADY;
    }
    GlobalClock_LocalClock_t *clk = LEIGUN_NEW(clk);
    if (!clk) {
        LOG_Error(MOD_NAME, "malloc failed %s", strerror(errno));
        return UV_EAI_MEMORY;
    }
    List_InitElement(&clk->liste);
    clk->proc = proc;
    clk->data = data;
    clk->rest_cnt = 0;
    GlobalClock_setFrequency(clk, hz);
    uv_mutex_lock(&GlobalClock_clock.list_mutex);
    List_Push(&GlobalClock_clock.list, &clk->liste);
    GlobalClock_clock.list_num++;
    uv_mutex_unlock(&GlobalClock_clock.list_mutex);
    return 0;
}


void GlobalClock_ChangeFrequency(GlobalClock_LocalClock_t *clk, uint64_t hz) {
    if (clk->hz == hz) {
        return;
    }
    LOG_Info(MOD_NAME, "Change clock frequency %" PRId64 "->%" PRId64, clk->hz,
             hz);
    clk->rest_cnt = (clk->rest_cnt * hz) / clk->hz;
    GlobalClock_setFrequency(clk, hz);
    return;
}


void GlobalClock_ConsumeCycle(GlobalClock_LocalClock_t *clk, uint32_t cnt) {
    while (clk->rest_cnt < cnt) {
        LOG_Verbose(MOD_NAME, "Wait %08zX:%p", (uintptr_t)clk->proc, clk->data);
        uv_barrier_wait(&GlobalClock_clock.barrier);
        clk->rest_cnt += clk->period_cnt;
        clk->rest_fraction += clk->period_cnt_reminder;
        if (clk->rest_fraction >= 1000) {
            LOG_Verbose(MOD_NAME, "Add fraction %08zX:%p", (uintptr_t)clk->proc,
                        clk->data);
            clk->rest_cnt++;
            clk->rest_fraction -= 1000;
        }
    }
    clk->rest_cnt -= cnt;
}
