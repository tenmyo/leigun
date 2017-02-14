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
/// @file
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
typedef Device_Board_t *(*Device_CreateBoard_cb)(void);
typedef int (*Device_RunBoard_cb)(Device_Board_t *dev);
/// Device_Board_t is board instance data.
struct Device_Board_s {
    Device_RunBoard_cb run;
    void *data;
};

typedef struct Device_MPU_s Device_MPU_t;
typedef Device_MPU_t *(*Device_CreateMPU_cb)(void);
typedef int (*Device_RunMPU_cb)(Device_MPU_t *dev);
/// Device_MPU_t is MPU instance data.
struct Device_MPU_s {
    Device_RunMPU_cb run;
    void *data;
};


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
void Device_DumpBoards(void);

int Device_RegisterMPU(const char *name, const char *description,
                       Device_CreateMPU_cb create, const char *defaultconfig);
int Device_UnregisterMPU(const char *name);
Device_MPU_t *Device_CreateMPU(const char *name);
void Device_DumpMPUs(void);


#ifdef __cplusplus
}
#endif
