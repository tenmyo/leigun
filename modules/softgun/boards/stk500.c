//===-- boards/stk500.c -------------------------------------------*- C -*-===//
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
/// Compose a STK500 AVR8 development Board
///
//===----------------------------------------------------------------------===//

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header

// Local/Private Headers

// Leigun Core Headers
#include "leigun/leigun.h"
#include "leigun/device.h"

// External headers

// System headers
#include <stdlib.h> // for malloc


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
#define BOARD_NAME "STK500"
#define BOARD_DESCRIPTION "STK500 AVR8 development Board"
#define BOARD_DEFAULTCONFIG                                                    \
    "[global]\n"                                                               \
    "cpu_clock: 20000000\n"


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
    board_t *board = LEIGUN_NEW(board);
    board->base.self = board;
    board->mpu = Device_CreateMPU("AVR8");
    return &board->base;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
DEVICE_REGISTER_BOARD(BOARD_NAME, BOARD_DESCRIPTION, NULL, NULL, NULL, NULL,
                      NULL, &create, BOARD_DEFAULTCONFIG);
