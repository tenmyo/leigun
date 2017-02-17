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
#include <stdint.h> // for uint64_t


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================
typedef void (*TimerList_cb)(void *data);
typedef struct TimerList_s TimerList_t;


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Macros
//==============================================================================


//==============================================================================
//= Functions
//==============================================================================
TimerList_t *TimerList_New(void);
int TimerList_Insert(TimerList_t *l, uint64_t cnt, TimerList_cb cb, void *data);
void TimerList_Remove(TimerList_t *l, TimerList_cb cb, void *data);
void TimerList_Fire(TimerList_t *l, uint64_t inc);

#ifdef __cplusplus
}
#endif
