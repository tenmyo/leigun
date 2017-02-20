//===-- boards/at91sam7a3_ek.c ------------------------------------*- C -*-===//
//
//              The Leigun Embedded System Simulator Platform : modules
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
/// Compose a AT91_SAM7A3-EK Development Board
///
//===----------------------------------------------------------------------===//

/*
 * -----------------------------------------------------------------------------
 *  Compose a AT91_SAM7A3-EK Development Board
 *
 * (C) 2009 Jochen Karrer
 *   Author: Jochen Karrer
 *
 * -----------------------------------------------------------------------------
 */

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header

// Local/Private Headers
#include "arm/mmu_arm9.h"
#include "controllers/at91/at91sam_efc.h"

// Leigun Core Headers

// External headers

// System headers
#include "bus.h"
#include "core/device.h"
#include "sram.h"


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
#define BOARD_NAME "AT91SAM7A3_EK"
#define BOARD_DESCRIPTION "Atmel SAM7A3 Evaluation Kit"
#define BOARD_DEFAULTCONFIG                                                    \
    "[global]\n"                                                               \
    "start_address: 0\n"                                                       \
    "\n"                                                                       \
    "[iram]\n"                                                                 \
    "size: 32k\n"                                                              \
    "\n"                                                                       \
    "[loader]\n"                                                               \
    "load_address: 0x0\n"                                                      \
    "\n"                                                                       \
    "[iflash]\n"                                                               \
    "size: 256k\n"


//==============================================================================
//= Types
//==============================================================================
typedef struct board_s {
    Device_Board_t base;
    Device_MPU_t *mpu;
} board_t;


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Function declarations(static)
//==============================================================================
static Device_Board_t *create(void);


//==============================================================================
//= Function definitions(static)
//==============================================================================
static Device_Board_t *create(void) {
    //      ArmCoprocessor *copro;
    BusDevice *dev, *flash, *efc;
    board_t *board = calloc(1, sizeof(*board));
    board->base.base.self = board;

    Bus_Init(MMU_InvalidateTlb, 4 * 1024);
    board->mpu = Device_CreateMPU("ARM9");
    dev = SRam_New("iram");
    Mem_AreaAddMapping(dev, 0x00200000, 1024 * 1024,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    // Mem_AreaAddMapping(dev,0x00000000,1024*1024,MEM_FLAG_WRITABLE |
    // MEM_FLAG_READABLE);
    AT91SAM7_EfcNew(&flash, &efc, "efc", "iflash");

    /* Before remap only */
    Mem_AreaAddMapping(flash, 0x00000000, 1024 * 1024,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    Mem_AreaAddMapping(flash, 0x00100000, 1024 * 1024,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    return &board->base;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
DEVICE_REGISTER_BOARD(BOARD_NAME, BOARD_DESCRIPTION, &create,
                      BOARD_DEFAULTCONFIG);
