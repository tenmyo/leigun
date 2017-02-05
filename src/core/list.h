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
/// \file
/// This file contains the declaration of the Generic list structure and
/// manupulation macros.
///
/// \ attention
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


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================
typedef struct List_s List_t;
typedef struct List_Element_s List_Element_t;
struct List_s {
    List_Element_t *l_head;
};
struct List_Element_s {
    List_Element_t *l_next;
};


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Macros
//==============================================================================
#define List_Members(member_t) member_t *l_head;

#define List_ElementMembesr(member_t) member_t *l_next;

#define List_Init(l)                                                           \
    do {                                                                       \
        (l)->l_head = NULL;                                                    \
    } while (0)

#define List_InitElement(e)                                                    \
    do {                                                                       \
        (e)->l_next = NULL;                                                    \
    } while (0)

#define List_Push(l, e)                                                        \
    do {                                                                       \
        (e)->l_next = (l)->l_head;                                             \
        (l)->l_head = e;                                                       \
    } while (0)

/// \ attention DO NOT CHANGE LIST at proc(e.g. Push, Pop...)
#define List_Map(l, proc)                                                      \
    do {                                                                       \
        List_Element_t *e, *next;                                              \
        for (e = (List_Element_t *)(l)->l_head; e; e = next) {                 \
            next = e->l_next;                                                  \
            proc((void *)e);                                                   \
        }                                                                      \
    } while (0)

/// \ attention DO NOT CHANGE LIST at proc(e.g. Push...)
#define List_PopEach(l, proc)                                                  \
    do {                                                                       \
        List_Element_t *e, *next;                                              \
        for (e = (List_Element_t *)(l)->l_head; e; e = next) {                 \
            next = e->l_next;                                                  \
            proc((void *)e);                                                   \
        }                                                                      \
        List_Init(l);                                                          \
    } while (0)

#define List_PopBy(l, comparator, operand_element, result_element)             \
    do {                                                                       \
        List_Element_t *e, *next;                                              \
        List_Element_t **prev_next = (List_Element_t **)(&(l)->l_head);        \
        for (e = *prev_next; e; prev_next = &e->l_next, e = next) {            \
            next = e->l_next;                                                  \
            if (!comparator((void *)e, operand_element)) {                     \
                *prev_next = e->l_next;                                        \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        (result_element) = (void *)e;                                          \
    } while (0)

#define List_Find(l, comparator, operand_element, result_element)              \
    do {                                                                       \
        List_Element_t *e, *next;                                              \
        for (e = (List_Element_t *)(l)->l_head; e; e = next) {                 \
            next = e->l_next;                                                  \
            if (!comparator((void *)e, operand_element)) {                     \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        (result_element) = (void *)e;                                          \
    } while (0)


//==============================================================================
//= Functions
//==============================================================================

#ifdef __cplusplus
}
#endif
