//===-- memmapped/sram.c ------------------------------------------*- C -*-===//
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

// Local/Private Headers
#include "bus.h"
#include "device.h"
#include "leigun.h"
#include "logging.h"

// External headers
#include <uv.h>

// System headers
#include <stdlib.h>
#include <string.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
#define DEVICE_NAME "SRAM"
#define DEVICE_DESCRIPTION "SRAM(Static Random Access Memory)"


//==============================================================================
//= Types
//==============================================================================
typedef struct SRAM_s {
    Device_MemMapped_t mmd;
    const char *name;
    size_t size;
    void *host_mem;
} SRAM_t;


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Function declarations(static)
//==============================================================================
static int SRAM_prepare(void *self);
static int SRAM_release(void *self);
static int SRAM_map32(void *self, uint32_t addr, size_t length, int prot, size_t offset);
static int SRAM_unmap32(void *self, uint32_t addr, size_t length);
static int SRAM_setOpt(void *self, Device_OptReq_t req, const void *optval, size_t optlen);
static int SRAM_getOpt(void *self, Device_OptReq_t req, void *optval, size_t *optlen);
static SigNode *SRAM_getSignode(void *self, const char *name);

Device_MemMapped_t *SRAM_create(const char *name);

//==============================================================================
//= Function definitions(static)
//==============================================================================
static int SRAM_prepare(void *self) {
    SRAM_t *dev = self;
    LOG_Debug(DEVICE_NAME, "prepare(%s, %zd)", dev->name, dev->size);
    dev->host_mem = LEIGUN_NEW_BUF(dev->size);
    if (!dev->host_mem) {
        LOG_Error(DEVICE_NAME, "malloc failed %s", strerror(errno));
        return UV_EAI_MEMORY;
    }
    memset(dev->host_mem, 0xCD, dev->size);
    return 0;
}


static int SRAM_release(void *self) {
    SRAM_t *dev = self;
    LOG_Debug(DEVICE_NAME, "release(%s)", dev->name);
    free(dev->host_mem);
    dev->host_mem = NULL;
    return 0;
}


static int SRAM_map32(void *self, uint32_t addr, size_t length, int prot, size_t offset) {
    SRAM_t *dev = self;
    LOG_Debug(DEVICE_NAME, "map32(%s)", dev->name);
    Mem_MapRange(addr, dev->host_mem, dev->size, length, prot);
    return 0;
}


static int SRAM_unmap32(void *self, uint32_t addr, size_t length) {
    SRAM_t *dev = self;
    LOG_Debug(DEVICE_NAME, "unmap32(%s)", dev->name);
    Mem_UnMapRange(addr, length);
    return 0;
}


static int SRAM_setOpt(void *self, Device_OptReq_t req, const void *optval, size_t optlen) {
    int ret;
    SRAM_t *dev = self;
    switch (req) {
    case OR_SIZE:
        if (optlen != sizeof(dev->size)) {
            ret = UV_EAGAIN;
            break;
        }
        dev->size = *((size_t *)optval);
        ret = 0;
        break;
    default:
        ret = UV_EINVAL;
        break;
    }
    return ret;
}


static int SRAM_getOpt(void *self, Device_OptReq_t req, void *optval, size_t *optlen) {
    int ret;
    SRAM_t *dev = self;
    switch (req) {
    case OR_SIZE:
        if (*optlen < sizeof(dev->size)) {
            *optlen = sizeof(dev->size);
            ret = UV_EAGAIN;
            break;
        }
        *optlen = sizeof(dev->size);
        *((size_t *)optval) = dev->size;
        ret = 0;
        break;
    default:
        ret = UV_EINVAL;
        break;
    }
    return ret;
}


static SigNode *SRAM_getSignode(void *self, const char *name) {
    return NULL;
}


Device_MemMapped_t *SRAM_create(const char *name) {
    LOG_Info(DEVICE_NAME, "create(%s)", name);
    SRAM_t *dev = LEIGUN_NEW(dev);
    dev->mmd = (Device_MemMapped_t){
        .base.self = dev,
        .base.prepare = &SRAM_prepare,
        .base.release = &SRAM_release,
        .base.set_opt = &SRAM_setOpt,
        .base.get_opt = &SRAM_getOpt,
        .base.get_signode = &SRAM_getSignode,
        .map32 = &SRAM_map32,
        .unmap32 = &SRAM_unmap32,
    };
    dev->name = strdup(name);
    dev->size = 0;
    dev->host_mem = NULL;
    return &dev->mmd;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
DEVICE_REGISTER_MEMMAPPED(DEVICE_NAME, DEVICE_DESCRIPTION, &SRAM_create)
