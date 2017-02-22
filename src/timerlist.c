//===-- core/timerlist.h ------------------------------------------*- C -*-===//
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
#include "timerlist.h"

// Local/Private Headers
#include "list.h"

// External headers
#include <uv.h>

// System headers
#include <stdlib.h> // for malloc


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================
typedef struct TimerList_Element_s {
    List_Element_t liste;
    uint64_t cnt;
    TimerList_cb cb;
    void *data;
} TimerList_Element_t;

struct TimerList_s {
    List_t list;
    uv_mutex_t list_mutex;
};


//==============================================================================
//= Function declarations(static)
//==============================================================================
static int TimerList_insert(TimerList_Element_t *e, TimerList_Element_t *cur);


//==============================================================================
//= Variables
//==============================================================================d


//==============================================================================
//= Function definitions(static)
//==============================================================================
//===----------------------------------------------------------------------===//
///
//===----------------------------------------------------------------------===//
static int TimerList_insert(TimerList_Element_t *e, TimerList_Element_t *cur) {
    if (!cur) {
        return -1;
    }
    if (e->cnt < cur->cnt) {
        cur->cnt -= e->cnt;
        return -1;
    }
    e->cnt -= cur->cnt;
    return 1;
}

//===----------------------------------------------------------------------===//
///
//===----------------------------------------------------------------------===//
static int TimerList_compare(TimerList_Element_t *e, TimerList_Element_t *cur) {
    return ((e->cb == cur->cb) && (e->data == cur->data));
}


//==============================================================================
//= Function definitions(global)
//==============================================================================

//===----------------------------------------------------------------------===//
///
//===----------------------------------------------------------------------===//
TimerList_t *TimerList_New(void) {
    TimerList_t *l = calloc(1, sizeof(*l));
    if (l) {
        List_Init(&l->list);
        uv_mutex_init(&l->list_mutex);
    }
    return l;
}


//===----------------------------------------------------------------------===//
///
//===----------------------------------------------------------------------===//
int TimerList_Insert(TimerList_t *l, uint64_t cnt, TimerList_cb cb,
                     void *data) {
    TimerList_Element_t *e = calloc(1, sizeof(*e));
    if (!e) {
        return UV_EAI_MEMORY;
    }
    List_InitElement(&e->liste);
    e->cnt = cnt;
    e->cb = cb;
    e->data = data;
    uv_mutex_lock(&l->list_mutex);
    List_Insert(&l->list, (List_Insert_cb)&TimerList_insert, &e->liste);
    uv_mutex_unlock(&l->list_mutex);
    return 0;
}


//===----------------------------------------------------------------------===//
///
//===----------------------------------------------------------------------===//
void TimerList_Remove(TimerList_t *l, TimerList_cb cb, void *data) {
    TimerList_Element_t e = {.cb = cb, .data = data};
    TimerList_Element_t *cur;
    uv_mutex_lock(&l->list_mutex);
    cur = List_PopBy(&l->list, (List_Compare_cb)&TimerList_compare, &e.liste);
    uv_mutex_unlock(&l->list_mutex);
    free(cur);
}


//===----------------------------------------------------------------------===//
///
//===----------------------------------------------------------------------===//
void TimerList_Fire(TimerList_t *l, uint64_t inc) {
    TimerList_Element_t e = {.cnt = inc};
    TimerList_Element_t *cur;
    int ret;
    for (;;) {
        uv_mutex_lock(&l->list_mutex);
        cur = List_Head(&l->list);
        ret = TimerList_insert(&e, cur);
        if (ret < 0) {
            uv_mutex_unlock(&l->list_mutex);
            break;
        }
        List_Pop(&l->list);
        uv_mutex_unlock(&l->list_mutex);
        if (cur->cb) {
            cur->cb(cur->data);
        }
        free(cur);
    }
}
