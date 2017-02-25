//===-- leigun.h --------------------------------------------------*- C -*-===//
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
#include <stdlib.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================
typedef uint16_t Leigun_ByteAddr16_t;
typedef uint32_t Leigun_ByteAddr32_t;
typedef uint64_t Leigun_ByteAddr64_t;


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Macros
//==============================================================================
#define LEIGUN_NEW(ptr) calloc(1, sizeof(*(ptr)))
#define LEIGUN_NEW_ARRAY(num, type) calloc(num, sizeof(type))
#define LEIGUN_NEW_BUF(size) calloc(1, size)


//==============================================================================
//= Functions
//==============================================================================

#ifdef __cplusplus
}
#endif
