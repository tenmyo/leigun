//===-- core/device.c - Leigun Device Management Facilities -------*- C -*-===//
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
/// This file contains the definitions of device management facilities.
///
//===----------------------------------------------------------------------===//

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header
#include "device.h"

// Local/Private Headers
#include "configfile.h"
#include "exithandler.h"
#include "list.h"
#include "logging.h"

// External headers
#include <uv.h>

// System headers
#include <stdlib.h>
#include <string.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
static const char *MOD_NAME = "Device";


//==============================================================================
//= Types
//==============================================================================
typedef struct Devices_s { List_t devices; } Devices_t;


//==============================================================================
//= Variables
//==============================================================================
static Devices_t Device_devices;


//==============================================================================
//= Function declarations(static)
//==============================================================================
static int Device_compare(const Device_DrvBase_t *a, const Device_DrvBase_t *b);


//==============================================================================
//= Function definitions(static)
//==============================================================================
//===----------------------------------------------------------------------===//
/// Compare device.
//===----------------------------------------------------------------------===//
static int Device_compare(const Device_DrvBase_t *a,
                          const Device_DrvBase_t *b) {
    return !(a->kind == b->kind) || strcmp(a->name, b->name);
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
//===----------------------------------------------------------------------===//
/// Register device driver. called from DEVICE_REGISTER_xxx.
///
/// @param[in]      drv     driver
//===----------------------------------------------------------------------===//
void Device_Register(Device_DrvBase_t *drv) {
    LOG_Info(MOD_NAME, "Register [%d] %s", drv->kind, drv->name);
    List_InitElement(&drv->liste);
    if (List_Find(&Device_devices.devices, (List_Compare_cb)&Device_compare,
                  &drv->liste)) {
        LOG_Warn(MOD_NAME, "Board %s already registered", drv->name);
        return;
    }
    List_Push(&Device_devices.devices, &drv->liste);
}


//===----------------------------------------------------------------------===//
/// Create board.
///
/// @param[in]      name        board name
///
/// @attention if not registered, it's not an error.
///
/// @return created board. NULL if error
//===----------------------------------------------------------------------===//
Device_Board_t *Device_CreateBoard(const char *name) {
    LOG_Info(MOD_NAME, "Create board %s", name);
    const Device_DrvBase_t device = {.kind = DK_BOARD, .name = name};
    Device_DrvBoard_t *result;
    result = List_Find(&Device_devices.devices,
                       (List_Compare_cb)&Device_compare, &device.liste);
    if (!result) {
        LOG_Warn(MOD_NAME, "Board %s not found", name);
        return NULL;
    }
    LOG_Info(MOD_NAME, "defaultconfig %s", result->defaultconfig);
    Config_AddString(result->defaultconfig);
    return result->create();
}


//===----------------------------------------------------------------------===//
/// Create MPU.
///
/// @param[in]      name        name
///
/// @attention if not registered, it's not an error.
///
/// @return created MPU. NULL if error
//===----------------------------------------------------------------------===//
Device_MPU_t *Device_CreateMPU(const char *name) {
    LOG_Info(MOD_NAME, "Create MPU %s", name);
    const Device_DrvBase_t device = {.kind = DK_MPU, .name = name};
    Device_DrvMPU_t *result;
    result = List_Find(&Device_devices.devices,
                       (List_Compare_cb)&Device_compare, &device.liste);
    if (!result) {
        LOG_Warn(MOD_NAME, "MPU %s not found", name);
        return NULL;
    }
    LOG_Info(MOD_NAME, "defaultconfig %s", result->defaultconfig);
    Config_AddString(result->defaultconfig);
    return result->create();
}


int Device_DefaultPrepare(void *dev) {
    return 0;
}


int Device_DefaultRelease(void *dev) {
    return 0;
}


int Device_DefaultSetOpt(void *dev, Device_OptReq_t req, const void *optval,
                         size_t optlen) {
    return UV_EINVAL;
}


int Device_DefaultGetOpt(void *dev, Device_OptReq_t req, void *optval,
                         size_t *optlen) {
    return UV_EINVAL;
}


SigNode *Device_DefaultGetSignode(void *dev, const char *name) {
    return NULL;
}
