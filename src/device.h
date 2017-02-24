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
#include "initializer.h"
#include "list.h"
#include "signode.h"

// External headers

// System headers
#include <stdint.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
typedef enum Device_DrvKind_e {
    DK_BOARD,         /// Board composed of multiple processors, devices.
    DK_MPU,           /// Processor drived by program.
    DK_MEMORY_MAPPED, /// Memory mapped peripherals.
    DK_BUS32,         /// Bus(32bit address or lower) peripherals.
} Device_DrvKind_t;

typedef enum Device_OptReq_e {
    OR_SIZE,
} Device_OptReq_t;

#define DEVICE_PROT_NONE 0
#define DEVICE_PROT_READ (1 << 0)
#define DEVICE_PROT_WRITE (1 << 1)
#define DEVICE_PROT_EXEC (1 << 2)


//==============================================================================
//= Types
//==============================================================================
typedef struct Device_DrvBase_s Device_DrvBase_t;

typedef struct Device_DrvBoard_s Device_DrvBoard_t;
typedef struct Device_Board_s Device_Board_t;

typedef struct Device_DrvMPU_s Device_DrvMPU_t;
typedef struct Device_MPU_s Device_MPU_t;

typedef struct Device_DrvMemMapped_s Device_DrvMemMapped_t;
typedef struct Device_MemMapped_s Device_MemMapped_t;

typedef struct Device_DrvBus_s Device_DrvBus_t;
typedef struct Device_Bus_s Device_Bus_t;


// Device Instances

struct Device_Board_s {
    void *self;
    const Device_DrvBoard_t *drv;
};

struct Device_MPU_s {
    void *self;
    const Device_DrvMPU_t *drv;
};

struct Device_MemMapped_s {
    void *self;
    const Device_DrvMemMapped_t *drv;
};

struct Device_Bus_s {
    void *self;
    const Device_Bus_t *drv;
};


// Device Drivers
struct Device_DrvBase_s {
    List_Element_t liste;
    Device_DrvKind_t kind;
    const char *name;
    const char *description;
    int (*prepare)(void *dev);
    int (*release)(void *dev);
    int (*set_opt)(void *dev, Device_OptReq_t req, const void *optval,
                   size_t optlen);
    int (*get_opt)(void *dev, Device_OptReq_t req, void *optval,
                   size_t *optlen);
    SigNode *(*get_signode)(void *dev, const char *name);
};

struct Device_DrvBoard_s {
    Device_DrvBase_t base;
    const char *defaultconfig;
    Device_Board_t *(*create)(void);
};

struct Device_DrvMPU_s {
    Device_DrvBase_t base;
    const char *defaultconfig;
    Device_MPU_t *(*create)(void);
};

struct Device_DrvMemMapped_s {
    Device_DrvBase_t base;
    Device_MemMapped_t *(*create)(const Device_DrvMemMapped_t *drv,
                                  const char *name);
    // For Bus
    int (*map32)(void *self, uint32_t addr, size_t length, int prot,
                 size_t offset);
};

typedef void (*Device_DrvBus32_Error_cb)(uint32_t addr, int op);
struct Device_DrvBus32_s {
    Device_DrvBase_t base;
    Device_Bus_t *(*create)(int addr_bits);
    int (*map)(void *self, Device_DrvMemMapped_t *dev, uint32_t addr,
               size_t length, int prot);
    uint8_t (*read8)(uint32_t addr);
    void (*write8)(uint32_t addr, uint8_t val);
    uint16_t (*read16)(uint32_t addr);
    void (*write16)(uint32_t addr, uint16_t val);
    uint32_t (*read32)(uint32_t addr);
    void (*write32)(uint32_t addr, uint32_t val);
};


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Macros(Register)
//==============================================================================
#define DEVICE_REGISTER_BOARD(name_, description_, prepare_, release_,         \
                              set_opt_, get_opt_, get_signode_, create_,       \
                              defaultconfig_)                                  \
    static Device_DrvBoard_t Device_boardDrv_##name_ = {                       \
        .base.kind = DK_BOARD,                                                 \
        .base.name = (name_),                                                  \
        .base.description = (description_),                                    \
        .base.prepare = (prepare_) ? (prepare_) : &Device_DefaultPrepare,      \
        .base.release = (release_) ? (release_) : &Device_DefaultRelease,      \
        .base.set_opt = (set_opt_) ? (set_opt_) : &Device_DefaultSetOpt,       \
        .base.get_opt = (get_opt_) ? (get_opt_) : &Device_DefaultGetOpt,       \
        .base.get_signode =                                                    \
            (get_signode_) ? (get_signode_) : &Device_DefaultGetSignode,       \
        .defaultconfig = (defaultconfig_),                                     \
        .create = (create_)};                                                  \
    INITIALIZER(Device_boardDrvRegister_##name_) {                             \
        Device_Register(&Device_boardDrv_##name_.base);                        \
    }

#define DEVICE_REGISTER_MPU(name_, description_, prepare_, release_, set_opt_, \
                            get_opt_, get_signode_, create_, defaultconfig_)   \
    static Device_DrvMPU_t Device_mpuDrv_##name_ = {                           \
        .base.kind = DK_MPU,                                                   \
        .base.name = (name_),                                                  \
        .base.description = (description_),                                    \
        .base.prepare = (prepare_) ? (prepare_) : &Device_DefaultPrepare,      \
        .base.release = (release_) ? (release_) : &Device_DefaultRelease,      \
        .base.set_opt = (set_opt_) ? (set_opt_) : &Device_DefaultSetOpt,       \
        .base.get_opt = (get_opt_) ? (get_opt_) : &Device_DefaultGetOpt,       \
        .base.get_signode =                                                    \
            (get_signode_) ? (get_signode_) : &Device_DefaultGetSignode,       \
        .defaultconfig = (defaultconfig_),                                     \
        .create = (create_)};                                                  \
    INITIALIZER(Device_mpuRegister_##name_) {                                  \
        Device_Register(&Device_mpuDrv_##name_.base);                          \
    }

#define DEVICE_REGISTER_MEMMAPPED(name_, description_, prepare_, release_,     \
                                  set_opt_, get_opt_, get_signode_, create_,   \
                                  map32_)                                      \
    static Device_DrvMemMapped_t Device_memMappedDrv_##name_ = {               \
        .base.kind = DK_MEMORY_MAPPED,                                         \
        .base.name = (name_),                                                  \
        .base.description = (description_),                                    \
        .base.prepare = (prepare_) ? (prepare_) : &Device_DefaultPrepare,      \
        .base.release = (release_) ? (release_) : &Device_DefaultRelease,      \
        .base.set_opt = (set_opt_) ? (set_opt_) : &Device_DefaultSetOpt,       \
        .base.get_opt = (get_opt_) ? (get_opt_) : &Device_DefaultGetOpt,       \
        .base.get_signode =                                                    \
            (get_signode_) ? (get_signode_) : &Device_DefaultGetSignode,       \
        .create = (create_),                                                   \
        .map32 = (map32_)};                                                    \
    INITIALIZER(Device_memMappedDrvRegister_##name_) {                         \
        Device_Register(&Device_memMappedDrv_##name_.base);                    \
    }


//==============================================================================
//= Functions
//==============================================================================
void Device_Register(Device_DrvBase_t *drv);

Device_Board_t *Device_CreateBoard(const char *name);
Device_MPU_t *Device_CreateMPU(const char *name);

int Device_DefaultPrepare(void *dev);
int Device_DefaultRelease(void *dev);
int Device_DefaultSetOpt(void *dev, Device_OptReq_t req, const void *optval,
                         size_t optlen);
int Device_DefaultGetOpt(void *dev, Device_OptReq_t req, void *optval,
                         size_t *optlen);
SigNode *Device_DefaultGetSignode(void *dev, const char *name);


#ifdef __cplusplus
}
#endif
