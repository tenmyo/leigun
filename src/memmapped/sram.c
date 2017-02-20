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
// clang-format off
/*
 **************************************************************************************************
 * SRAM Emulation
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */
// clang-format on

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header

// Local/Private Headers
#include "bus.h"
#include "core/device.h"
#include "core/logging.h"

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
    dev->host_mem = malloc(dev->size);
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
    SRAM_t *dev = calloc(1, sizeof(*dev));
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
