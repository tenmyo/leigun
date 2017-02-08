//===-- core/device.h - Leigun Device Management Facilities -------*- C -*-===//
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
/// This file contains the declaration of device management facilities.
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
typedef struct Device_Board_s Device_Board_t;
typedef int (*Device_RunBoard_cb)(Device_Board_t *board);
/// Device_Board_t is board instance data.
struct Device_Board_s {
    Device_RunBoard_cb run;
    void *data;
};

typedef Device_Board_t *(*Device_CreateBoard_cb)(void);


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Macros
//==============================================================================


//==============================================================================
//= Functions
//==============================================================================
int Device_Init(void);
int Device_RegisterBoard(const char *name, const char *description,
                         Device_CreateBoard_cb create,
                         const char *defaultconfig);
int Device_UnregisterBoard(const char *name);
Device_Board_t *Device_CreateBoard(const char *name);


#ifdef __cplusplus
}
#endif
