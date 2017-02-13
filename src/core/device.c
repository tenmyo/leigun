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
#include "core/device.h"

// Local/Private Headers
#include "configfile.h"
#include "core/exithandler.h"
#include "core/list.h"
#include "core/logging.h"

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
typedef enum Device_kind_e {
    //    DK_SYSTEM,              /// System composed of multiple boards.
    DK_BOARD,             /// Board composed of multiple processors, devices.
    DK_SOC,               /// SOC contains processor, memory, I/O peripherals.
    DK_PROCESSOR,         /// Processor drived by program.
    DK_MEMORY_BUS_DEVICE, /// Memory mapped peripherals.
    //    DK_OTHER,               /// Other peripherals.
} Device_kind_t;

typedef struct Device_board_s Device_board_t;
struct Device_board_s {
    List_Element_t liste;
    Device_kind_t kind;
    const char *name;
    const char *description;
    const char *defaultconfig;
    Device_CreateBoard_cb create;
};

typedef struct Devices_s {
    List_t list;
    uv_mutex_t list_mutex;
} Devices_t;


//==============================================================================
//= Variables
//==============================================================================
static Devices_t Device_devices;


//==============================================================================
//= Function declarations(static)
//==============================================================================
static void Device_free(Device_board_t *device);
static void Device_onExit(void *data);
static int Device_compareBoard(const Device_board_t *a,
                               const Device_board_t *b);


//==============================================================================
//= Function definitions(static)
//==============================================================================
//===----------------------------------------------------------------------===//
/// Free a device list memory.
//===----------------------------------------------------------------------===//
static void Device_free(Device_board_t *device) {
    LOG_Debug(MOD_NAME, "free %s", device->name);
    free((void *)device->name);
    free((void *)device->description);
    free((void *)device->defaultconfig);
    free(device);
}

//===----------------------------------------------------------------------===//
/// Free device list.
//===----------------------------------------------------------------------===//
static void Device_onExit(void *data) {
    Devices_t *devices = data;
    LOG_Debug(MOD_NAME, "Device_onExit");
    List_PopEach(&devices->list, (List_Proc_cb)&Device_free);
    uv_mutex_destroy(&devices->list_mutex);
}

static int Device_compareBoard(const Device_board_t *a,
                               const Device_board_t *b) {
    return strcmp(a->name, b->name);
}

//===----------------------------------------------------------------------===//
/// Free device list.
//===----------------------------------------------------------------------===//
static void Device_printBoard(Device_board_t *device) {
    printf("%s: %s\n", device->name, device->description);
    printf("%s\n\n", device->defaultconfig);
}


//==============================================================================
//= Function definitions(global)
//==============================================================================

//===----------------------------------------------------------------------===//
/// Initialize devices.
///
/// - Initialize device list
/// - Register list cleanup handler
///
/// @return same as libuv. imply an error if negative.
//===----------------------------------------------------------------------===//
int Device_Init(void) {
    int err;
    LOG_Info(MOD_NAME, "Init");
    List_Init(&Device_devices.list);
    err = uv_mutex_init(&Device_devices.list_mutex);
    if (err < 0) {
        LOG_Error(MOD_NAME, "uv_mutex_init failed. %s %s", uv_err_name(err),
                  uv_strerror(err));
        return err;
    }

    err = ExitHandler_Register(&Device_onExit, &Device_devices);
    return 0;
}


//===----------------------------------------------------------------------===//
/// Register board.
///
/// @param[in]      name            board name
/// @param[in]      description     board description
/// @param[in]      create          board instantiation function(constructor)
/// @param[in]      defaultconfig   board defaut config
///
/// @return imply an error if negative
//===----------------------------------------------------------------------===//
int Device_RegisterBoard(const char *name, const char *description,
                         Device_CreateBoard_cb create,
                         const char *defaultconfig) {
    int err = 0;
    Device_board_t device = {.kind = DK_BOARD,
                             .name = strdup(name),
                             .description = strdup(description),
                             .defaultconfig = strdup(defaultconfig),
                             .create = create};
    Device_board_t *devicep;

    LOG_Info(MOD_NAME, "Register board %s", name);
    List_InitElement(&device.liste);

    uv_mutex_lock(&Device_devices.list_mutex);
    do {
        devicep =
            List_Find(&Device_devices.list,
                      (List_Compare_cb)&Device_compareBoard, &device.liste);
        if (devicep) {
            LOG_Warn(MOD_NAME, "Board %s already registered", name);
            err = UV_EEXIST;
            break;
        }

        devicep = malloc(sizeof(*devicep));
        if (!devicep) {
            LOG_Error(MOD_NAME, "malloc failed %s", strerror(errno));
            err = UV_EAI_MEMORY;
            break;
        }
        *devicep = device;
        List_Push(&Device_devices.list, &devicep->liste);
    } while (0);
    uv_mutex_unlock(&Device_devices.list_mutex);
    return err;
}


//===----------------------------------------------------------------------===//
/// Unregister board.
///
/// @param[in]      name        board name
///
/// @attention if not registered, it's not an error.
///
/// @return imply an error if negative
//===----------------------------------------------------------------------===//
int Device_UnregisterBoard(const char *name) {
    LOG_Info(MOD_NAME, "Unregister board %s", name);
    const Device_board_t device = {.kind = DK_BOARD, .name = name};
    Device_board_t *result;
    uv_mutex_lock(&Device_devices.list_mutex);
    result = List_PopBy(&Device_devices.list,
                        (List_Compare_cb)&Device_compareBoard, &device.liste);
    free(result);
    uv_mutex_unlock(&Device_devices.list_mutex);
    return 0;
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
    const Device_board_t device = {.kind = DK_BOARD, .name = name};
    Device_board_t *result;
    uv_mutex_lock(&Device_devices.list_mutex);
    result = List_Find(&Device_devices.list,
                       (List_Compare_cb)&Device_compareBoard, &device.liste);
    uv_mutex_unlock(&Device_devices.list_mutex);
    if (!result) {
        LOG_Warn(MOD_NAME, "Board %s not found", name);
        return NULL;
    }
    LOG_Info(MOD_NAME, "defaultconfig %s", result->defaultconfig);
    Config_AddString(result->defaultconfig);
    return result->create();
}


//===----------------------------------------------------------------------===//
/// Dump board info.
//===----------------------------------------------------------------------===//
void Device_DumpBoards(void) {
    printf("-- Registered bords are:\n");
    uv_mutex_lock(&Device_devices.list_mutex);
    List_Map(&Device_devices.list, (List_Proc_cb)&Device_printBoard);
    uv_mutex_unlock(&Device_devices.list_mutex);
    printf("--\n");
}
