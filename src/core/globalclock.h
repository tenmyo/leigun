//===-- core/globalclock.h ----------------------------------------*- C -*-===//
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
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
//==============================================================================
//= Dependencies
//==============================================================================
// Local/Private Headers

// External headers

// System headers
#include <stdint.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================
typedef struct GlobalClock_LocalClock_s GlobalClock_LocalClock_t;
typedef void (*GlobalClock_Proc_cb)(GlobalClock_LocalClock_t *clk, void *data);


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Macros
//==============================================================================


//==============================================================================
//= Functions
//==============================================================================
int GlobalClock_Init(uint32_t period_ms);
int GlobalClock_Start(void);

int GlobalClock_Registor(GlobalClock_Proc_cb proc, void *data, uint64_t hz);
void GlobalClock_ChangeFrequency(GlobalClock_LocalClock_t *clk, uint64_t hz);
void GlobalClock_ConsumeCycle(GlobalClock_LocalClock_t *clk, uint32_t cnt);

#ifdef __cplusplus
}
#endif
