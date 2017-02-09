//===-- boards/lite5200b.c ----------------------------------------*- C -*-===//
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
/// Compose a LITE5200B Board
///
//===----------------------------------------------------------------------===//

/*
 * -----------------------------------------------------------------------------
 *  Compose a LITE5200B Board
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
#include "ppc/cpu_ppc.h"

// Leigun Core Headers
#include "bus.h"
#include "core/device.h"
#include "dram.h"
#include "initializer.h"

// External headers

// System headers
#include <stdio.h>

//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
static const char *BOARD_NAME = "LITE5200B";
static const char *BOARD_DESCRIPTION = "LITE5200B";
static const char *BOARD_DEFAULTCONFIG = "[global]\n"
                                         "start_address: 0x00000000\n"
                                         "cpu_clock: 400000000\n"
                                         "\n"
                                         "[flash0]\n"
                                         "type: S29GL128NR2\n"
                                         "chips: 1\n"
                                         "\n"
                                         "[flash1]\n"
                                         "type: S29GL128NR2\n"
                                         "chips: 1\n"
                                         "\n"
                                         "[dram0]\n"
                                         "size: 128M\n"
                                         "\n"
                                         "[dram1]\n"
                                         "size: 128M\n"
                                         "\n";


//==============================================================================
//= Types
//==============================================================================


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Function declarations(static)
//==============================================================================
static void create_signal_links(void);
static void create_i2c_devices(void);
static Device_Board_t *create(void);
static int run(Device_Board_t *board);


//==============================================================================
//= Function definitions(static)
//==============================================================================

static int run(Device_Board_t *board) {
    PpcCpu_Run();
    return 0;
}

static Device_Board_t *create(void) {
    Device_Board_t *board;
    board = malloc(sizeof(*board));
    board->run = &run;

    // PpcCpu *cpu = PpcCpu_New(CPU_MPC866P,0);
    Bus_Init(NULL, 4 * 1024);
    BusDevice *dev;
    /* CS0 */
    dev = AMDFlashBank_New("flash0");
    /* CS1 */
    dev = AMDFlashBank_New("flash1");
    /* SDRAM_CS0 */
    dev = DRam_New("dram0");
    /* SDRAM_CS1 */
    dev = DRam_New("dram1");
    return board;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
INITIALIZER(init) {
    Device_RegisterBoard(BOARD_NAME, BOARD_DESCRIPTION, &create,
                         BOARD_DEFAULTCONFIG);
}
