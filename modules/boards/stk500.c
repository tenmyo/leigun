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
#include "avr8/avr8_cpu.h"

// Leigun Core Headers
#include "core/device.h"
#include "initializer.h"

// External headers

// System headers


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
static const char *BOARD_NAME = "STK500";
static const char *BOARD_DESCRIPTION = "STK500 AVR8 development Board";
static const char *BOARD_DEFAULTCONFIG = 
"[global]\n"
"cpu_clock: 20000000\n"
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
static Device_Board_t *
create(void)
{
	AVR8_Init("avr");
	return 0;
}

static int
run(Device_Board_t *board)
{
	AVR8_Run();
	return 0;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
INITIALIZER(init) {
    Device_RegisterBoard(BOARD_NAME, BOARD_DESCRIPTION, &create, BOARD_DEFAULTCONFIG);
}
