//===-- core/list.h - Generic list data structure manupulation ----*- C -*-===//
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
/// This file contains the declaration of the Generic list structure and
/// manupulation macros.
///
/// @attention
/// NOT REENTLANT. THREAD UNSAFE.
///
//===----------------------------------------------------------------------===//
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
//==============================================================================
//= Dependencies
//==============================================================================
// Local/Private Headers

// External headers

// System headers
#include <stddef.h> // for NULL


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================
typedef struct List_s List_t;
typedef struct List_Element_s List_Element_t;
struct List_s {
    List_Element_t *head;
};
struct List_Element_s {
    List_Element_t *next;
};

typedef void (*List_Proc_cb)(List_Element_t *e);
typedef int (*List_Compare_cb)(const List_Element_t *ea,
                               const List_Element_t *eb);
typedef int (*List_Insert_cb)(List_Element_t *ea, List_Element_t *eb);


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Macros
//==============================================================================

static inline void List_Init(List_t *l) {
    l->head = NULL;
}

static inline void List_InitElement(List_Element_t *e) {
    e->next = NULL;
}

static inline void *List_Head(List_t *l) {
    return l->head;
}

static inline void List_Push(List_t *l, List_Element_t *e) {
    e->next = l->head;
    l->head = e;
}

static inline void *List_Pop(List_t *l) {
    List_Element_t *e = l->head;
    if (e) {
        l->head = e->next;
    }
    return e;
}

/// @attention DO NOT CHANGE LIST in proc(e.g. Push, Pop...)
static inline void List_Map(List_t *l, List_Proc_cb proc) {
    List_Element_t *cur;
    List_Element_t *next;
    for (cur = l->head; cur; cur = next) {
        next = cur->next;
        proc(cur);
    }
}

/// @attention DO NOT CHANGE LIST in proc(e.g. Push, Pop...)
static inline void List_FilterMap(List_t *l, List_Compare_cb comparator,
                                  const List_Element_t *operand,
                                  List_Proc_cb proc) {
    List_Element_t *cur;
    List_Element_t *next;
    for (cur = l->head; cur; cur = next) {
        next = cur->next;
        if (comparator(operand, cur) == 0) {
            proc(cur);
        }
    }
}

/// @attention DO NOT CHANGE LIST in proc(e.g. Push, Pop...)
static inline void List_PopEach(List_t *l, List_Proc_cb proc) {
    List_Map(l, proc);
    List_Init(l);
}

static inline void *List_PopBy(List_t *l, List_Compare_cb comparator,
                               const List_Element_t *operand) {
    List_Element_t *cur;
    List_Element_t *next;
    List_Element_t **prev_next = &l->head;
    for (cur = *prev_next; cur; prev_next = &cur->next, cur = next) {
        next = cur->next;
        if (comparator(operand, cur) == 0) {
            *prev_next = cur->next;
            break;
        }
    }
    return cur;
}

static inline void *List_Find(List_t *l, List_Compare_cb comparator,
                              const List_Element_t *operand) {
    List_Element_t *cur;
    List_Element_t *next;
    List_Element_t **prev_next = &l->head;
    for (cur = *prev_next; cur; prev_next = &cur->next, cur = next) {
        next = cur->next;
        if (comparator(operand, cur) == 0) {
            break;
        }
    }
    return cur;
}

static inline void List_Insert(List_t *l, List_Insert_cb comparator,
                               List_Element_t *e) {
    List_Element_t *cur;
    List_Element_t *next;
    List_Element_t **prev_next = &l->head;
    for (cur = *prev_next; cur; prev_next = &cur->next, cur = next) {
        next = cur->next;
        if (comparator(e, cur) <= 0) {
            e->next = cur;
            *prev_next = e;
            return;
        }
    }
    e->next = NULL;
    *prev_next = e;
}


//==============================================================================
//= Functions
//==============================================================================

#ifdef __cplusplus
}
#endif
