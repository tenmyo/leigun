//===-- boards/m5282lite.c ----------------------------------------*- C -*-===//
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
/// Compose a MCF5282LITE Board
///
//===----------------------------------------------------------------------===//

/*
 * -----------------------------------------------------------------------------
 *  Compose a MCF5282LITE Board
 *
 * (C) 2008 Jochen Karrer
 *   Author: Jochen Karrer
 *
 * -----------------------------------------------------------------------------
 */

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header

// Local/Private Headers
#include "amdflash.h"
#include "coldfire/mcf5282_csm.h"
#include "coldfire/mcf5282_scm.h"

// Leigun Core Headers
#include "bus.h"
#include "sram.h"
#include "leigun/leigun.h"
#include "leigun/device.h"

// External headers

// System headers


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
#define BOARD_NAME "M5282LITE"
#define BOARD_DESCRIPTION "M5282LITE"
#define BOARD_DEFAULTCONFIG                                                    \
    "[global]\n"                                                               \
    "start_address: 0x00000000\n"                                              \
    "cpu_clock: 66000000\n"                                                    \
    "\n"                                                                       \
    "[sram0]\n"                                                                \
    "size: 64k"                                                                \
    "\n"                                                                       \
    "[flash0]\n"                                                               \
    "type: AM29LV160BB\n"                                                      \
    "chips: 1\n"


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
static void create_clock_links(void);
static void create_signal_links(void);
static void create_i2c_devices(void);
static Device_Board_t *create(void);


//==============================================================================
//= Function definitions(static)
//==============================================================================

static void create_clock_links(void) {
    //      Clock_Link("st.slck","pmc.slck");
}

static void create_signal_links(void) {
}

static void create_i2c_devices(void) {
}

static Device_Board_t *create(void) {
    BusDevice *dev;
    MCF5282ScmCsm *scmcsm;
    Bus_Init(NULL, 4 * 1024);
    scmcsm = MCF5282_ScmCsmNew("scmcsm");
    dev = AMDFlashBank_New("flash0");
    MCF5282Csm_RegisterDevice(scmcsm, dev, CSM_CS0);
    board_t *board = LEIGUN_NEW(board);
    board->base.self = board;
    board->mpu = Device_CreateMPU("coldfire");

// dev = CFM_New("cfm");
#if 0
	if (dev) {
		Mem_AreaAddMapping(dev, 0xffe00000, 2 * 1024 * 1024,
				   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	}
#endif
    dev = SRam_New("sram0");
    if (dev) {
        Mem_AreaAddMapping(dev, 0x04000000, 64 * 1024,
                           MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    }
    create_i2c_devices();
    create_signal_links();
    create_clock_links();
    return &board->base;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
DEVICE_REGISTER_BOARD(BOARD_NAME, BOARD_DESCRIPTION, NULL, NULL, NULL, NULL,
                      NULL, &create, BOARD_DEFAULTCONFIG);
