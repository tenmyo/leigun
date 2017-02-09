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
#include "mcs51/cpu_mcs51.h"

// Leigun Core Headers
#include "core/device.h"
#include "initializer.h"

// External headers

// System headers


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
static const char *BOARD_NAME = "ALT9";
static const char *BOARD_DESCRIPTION = "Alt-9 8051 example board";
static const char *BOARD_DEFAULTCONFIG = 
"[global]\n"
"cpu_clock: 8000000\n"
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
static Device_Board_t *create(void);
static int run(Device_Board_t *board);


//==============================================================================
//= Function definitions(static)
//==============================================================================
static Device_Board_t *create(void) {
    Device_Board_t *board;
    board = malloc(sizeof(*board));
    board->run = &run;
    MCS51_Init("mcs51");
    return board;
}

static int run(Device_Board_t *board) {
    MCS51_Run();
    return 0;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
INITIALIZER(init) {
    Device_RegisterBoard(BOARD_NAME, BOARD_DESCRIPTION, &create, BOARD_DEFAULTCONFIG);
}
