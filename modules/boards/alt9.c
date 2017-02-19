//===-- boards/alt9.c ---------------------------------------------*- C -*-===//
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
/// Compose a Alt-9 8051 example board
///
//===----------------------------------------------------------------------===//

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header

// Local/Private Headers

// Leigun Core Headers
#include "core/device.h"

// External headers

// System headers
#include <stdlib.h> // for malloc


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
#define BOARD_NAME "ALT9"
#define BOARD_DESCRIPTION "Alt-9 8051 example board"
#define BOARD_DEFAULTCONFIG                                                    \
    "[global]\n"                                                               \
    "cpu_clock: 8000000\n"


//==============================================================================
//= Types
//==============================================================================
typedef struct board_s { Device_MPU_t *mpu; } board_t;


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
    Device_Board_t *board = malloc(sizeof(*board));
    board_t *self = malloc(sizeof(*self));
    board->self = board;
    self->mpu = Device_CreateMPU("MCS51");
    return board;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
DEVICE_REGISTER_BOARD(BOARD_NAME, BOARD_DESCRIPTION, &create,
                      BOARD_DEFAULTCONFIG);
