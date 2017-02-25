//===-- bus32/abstructbus32.c -------------------------------------*- C -*-===//
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
#define DEVICE_NAME "Abstruct Bus32"
#define DEVICE_DESCRIPTION "Abstruct Bus32"

typedef enum AbstructBus32_mapType_e {
    MT_MEM,
    // MT_FUNC,
} AbstructBus32_mapType_t;


//==============================================================================
//= Types
//==============================================================================
typedef struct AbstructBus32_map_s {
    AbstructBus32_mapType_t type;
    int prot;
    size_t length;
    union {
        struct {
            uintptr_t host_mem;
        } mem;
        // struct {} func;
    } u;
} AbstructBus32_map_t;


typedef struct AbstructBus32_s {
    Device_Bus32_t bus;
    Leigun_ByteAddr32_t addr_mask;
    Leigun_ByteAddr32_t block_mask;
    AbstructBus32_map_t **map;
} AbstructBus32_t;


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Function declarations(static)
//==============================================================================
// For DeviceManager
static Device_Bus32_t *AbstructBus32_create(const Device_DrvBus32_t *drv,
                                            int all_addr_bits, int block_bits,
                                            Device_DrvBus32_Error_cb error_cb);
static int AbstructBus32_prepare(void *self);
static int AbstructBus32_release(void *self);
static int AbstructBus32_setOpt(void *self, Device_OptReq_t req,
                                const void *optval, size_t optlen);
static int AbstructBus32_getOpt(void *self, Device_OptReq_t req, void *optval,
                                size_t *optlen);

// For Device
static int AbstructBus32_map(void *self, Device_MemMapped_t *mmd,
                             Leigun_ByteAddr32_t addr, uint32_t length,
                             int prot);

static void AbstructBus32_read(AbstructBus32_t *self, Leigun_ByteAddr32_t addr,
                               void *dst, size_t len);
static uint8_t AbstructBus32_read8(void *self, Leigun_ByteAddr32_t addr);
static uint16_t AbstructBus32_read16(void *self, Leigun_ByteAddr32_t addr);
static uint32_t AbstructBus32_read32(void *self, Leigun_ByteAddr32_t addr);
static uint64_t AbstructBus32_read64(void *self, Leigun_ByteAddr32_t addr);

static void AbstructBus32_write(AbstructBus32_t *dev, Leigun_ByteAddr32_t addr,
                                void *src, size_t len);
static void AbstructBus32_write8(void *self, Leigun_ByteAddr32_t addr,
                                 uint8_t val);
static void AbstructBus32_write16(void *self, Leigun_ByteAddr32_t addr,
                                  uint16_t val);
static void AbstructBus32_write32(void *self, Leigun_ByteAddr32_t addr,
                                  uint32_t val);
static void AbstructBus32_write64(void *self, Leigun_ByteAddr32_t addr,
                                  uint64_t val);

// For MemMapped
static int AbstructBus32_map_mem(void *self, Leigun_ByteAddr32_t addr,
                                 uint32_t length, int prot, void *mem);


//==============================================================================
//= Function definitions(static)
//==============================================================================
// For DeviceManager
static Device_Bus32_t *AbstructBus32_create(const Device_DrvBus32_t *drv,
                                            int all_addr_bits, int block_bits,
                                            Device_DrvBus32_Error_cb error_cb) {
    LOG_Info(DEVICE_NAME, "create addr_bits: %d, block_bits: %d", all_addr_bits,
             block_bits);
    AbstructBus32_t *dev = LEIGUN_NEW(dev);
    dev->bus.self = dev;
    dev->bus.drv = drv;
    dev->bus.all_addr_bits = all_addr_bits;
    dev->bus.block_bits = block_bits;
    dev->bus.error_cb = error_cb;
    dev->addr_mask = (1 << (all_addr_bits - 1)) - 1;
    dev->block_mask = (1 << (block_bits - 1)) - 1;
    return &dev->bus;
}


static int AbstructBus32_prepare(void *self) {
    AbstructBus32_t *dev = self;
    LOG_Debug(DEVICE_NAME, "prepare(%s)", dev->bus.drv->base.name);
    dev->map = LEIGUN_NEW_ARRAY(1 << (dev->bus.all_addr_bits - 1),
                                AbstructBus32_map_t *);
    if (!dev->map) {
        LOG_Error(DEVICE_NAME, "malloc failed %s", strerror(errno));
        return UV_EAI_MEMORY;
    }
    return 0;
}


static int AbstructBus32_release(void *self) {
    AbstructBus32_t *dev = self;
    LOG_Debug(DEVICE_NAME, "release(%s)", dev->bus.drv->base.name);
    free(dev->map);
    dev->map = NULL;
    return 0;
}


static int AbstructBus32_setOpt(void *self, Device_OptReq_t req,
                                const void *optval, size_t optlen) {
    int ret;
    AbstructBus32_t *dev = self;
    switch (req) {
    default:
        ret = UV_EINVAL;
        break;
    }
    return ret;
}


static int AbstructBus32_getOpt(void *self, Device_OptReq_t req, void *optval,
                                size_t *optlen) {
    int ret;
    AbstructBus32_t *dev = self;
    switch (req) {
    case OR_SIZE:
    default:
        ret = UV_EINVAL;
        break;
    }
    return ret;
}


// For Device
static int AbstructBus32_map(void *self, Device_MemMapped_t *mmd,
                             Leigun_ByteAddr32_t addr, uint32_t length,
                             int prot) {
    LOG_Debug(DEVICE_NAME, "map addr: %d, length: %zd, prot: %d", addr, length,
              prot);
    AbstructBus32_t *dev = self;
    return mmd->drv->map32(mmd->self, &dev->bus, addr & dev->addr_mask, length,
                           prot);
}


static void AbstructBus32_read(AbstructBus32_t *dev, Leigun_ByteAddr32_t addr,
                               void *dst, size_t len) {
    unsigned block_index = (addr & dev->addr_mask) >> dev->bus.block_bits;
    unsigned block_offset = addr & dev->block_mask;
    AbstructBus32_map_t *map = dev->map[block_index];
    if (!map) {
        dev->bus.error_cb(addr, DEVICE_PROT_READ);
        return;
    }
    if ((map->prot & DEVICE_PROT_READ) != DEVICE_PROT_READ) {
        dev->bus.error_cb(addr, DEVICE_PROT_READ);
        return;
    }
    if (block_offset + len - 1 > map->length) {
        dev->bus.error_cb(addr, DEVICE_PROT_READ);
        return;
    }
    memcpy(dst, (void *)(map->u.mem.host_mem + block_offset), len);
}


static uint8_t AbstructBus32_read8(void *self, Leigun_ByteAddr32_t addr) {
    uint8_t ret;
    AbstructBus32_read(self, addr, &ret, sizeof(ret));
    return ret;
}


static uint16_t AbstructBus32_read16(void *self, Leigun_ByteAddr32_t addr) {
    uint16_t ret;
    AbstructBus32_read(self, addr, &ret, sizeof(ret));
    return ret;
}


static uint32_t AbstructBus32_read32(void *self, Leigun_ByteAddr32_t addr) {
    uint32_t ret;
    AbstructBus32_read(self, addr, &ret, sizeof(ret));
    return ret;
}


static uint64_t AbstructBus32_read64(void *self, Leigun_ByteAddr32_t addr) {
    uint64_t ret;
    AbstructBus32_read(self, addr, &ret, sizeof(ret));
    return ret;
}


static void AbstructBus32_write(AbstructBus32_t *dev, Leigun_ByteAddr32_t addr,
                                void *src, size_t len) {
    unsigned block_index = (addr & dev->addr_mask) >> dev->bus.block_bits;
    unsigned block_offset = addr & dev->block_mask;
    AbstructBus32_map_t *map = dev->map[block_index];
    if (!map) {
        dev->bus.error_cb(addr, DEVICE_PROT_WRITE);
        return;
    }
    if ((map->prot & DEVICE_PROT_WRITE) != DEVICE_PROT_WRITE) {
        dev->bus.error_cb(addr, DEVICE_PROT_WRITE);
        return;
    }
    if (block_offset + len - 1 > map->length) {
        dev->bus.error_cb(addr, DEVICE_PROT_WRITE);
        return;
    }
    memcpy((void *)(map->u.mem.host_mem + block_offset), src, len);
}


static void AbstructBus32_write8(void *self, Leigun_ByteAddr32_t addr,
                                 uint8_t val) {
    AbstructBus32_write(self, addr, &val, sizeof(val));
}


static void AbstructBus32_write16(void *self, Leigun_ByteAddr32_t addr,
                                  uint16_t val) {
    AbstructBus32_write(self, addr, &val, sizeof(val));
}


static void AbstructBus32_write32(void *self, Leigun_ByteAddr32_t addr,
                                  uint32_t val) {
    AbstructBus32_write(self, addr, &val, sizeof(val));
}


static void AbstructBus32_write64(void *self, Leigun_ByteAddr32_t addr,
                                  uint64_t val) {
    AbstructBus32_write(self, addr, &val, sizeof(val));
}


// For MemMapped
static int AbstructBus32_map_mem(void *self, Leigun_ByteAddr32_t addr,
                                 uint32_t length, int prot, void *mem) {
    LOG_Debug(DEVICE_NAME, "map_mem addr: %d, length: %zd, prot: %d, mem: %p",
              addr, length, prot, mem);
    AbstructBus32_t *dev = self;
    size_t block_size = (1 << (dev->bus.block_bits - 1));
    uint32_t block_begin = addr >> dev->bus.block_bits;
    uint32_t block_end = (addr + length) >> dev->bus.block_bits;
    uint32_t block;
    uintptr_t mem_addr = (uintptr_t)mem;
    // chack exists
    for (block = block_begin; block <= block_end; block++) {
        if (dev->map[block]) {
            LOG_Error(DEVICE_NAME, "already map exists.");
            return UV_EEXIST;
        }
    }
    // map
    for (block = block_begin; block <= block_end;
         block++, length -= block_size, mem_addr += block_size) {
        LOG_Verbose(DEVICE_NAME, "block: %d, mem: %p, len: %zd", block,
                    (void *)mem_addr, length & dev->block_mask);
        dev->map[block] = LEIGUN_NEW(dev->map[block]);
        dev->map[block]->type = MT_MEM;
        dev->map[block]->prot = prot;
        dev->map[block]->length = length & dev->block_mask;
        dev->map[block]->u.mem.host_mem = mem_addr;
    }
    return 0;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
DEVICE_REGISTER_BUS32(DEVICE_NAME, DEVICE_DESCRIPTION, &AbstructBus32_prepare,
                      &AbstructBus32_release, &AbstructBus32_setOpt,
                      &AbstructBus32_getOpt, NULL, &AbstructBus32_create,
                      &AbstructBus32_map, &AbstructBus32_read8,
                      &AbstructBus32_write8, &AbstructBus32_read16,
                      &AbstructBus32_write16, &AbstructBus32_read32,
                      &AbstructBus32_write32, &AbstructBus32_read64,
                      &AbstructBus32_write64, &AbstructBus32_map_mem, NULL)
