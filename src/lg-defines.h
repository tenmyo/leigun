//===-- core/lg-defines.h -----------------------------------------*- C -*-===//
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


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Macros
//==============================================================================
// __has_builtin
#ifndef __has_builtin
#define __has_builtin(x) defined(x)
#endif

// __attribute__
#if !__has_builtin(__attribute__) && !defined(__GNUC__) &&                     \
    !defined(__INTEL_COMPILER)
#define __attribute__(x)
#endif

// __builtin_expect
#if !__has_builtin(__builtin_expect) && !defined(__GNUC__) &&                  \
    !defined(__INTEL_COMPILER)
#define __builtin_expect(x) (x)
#endif

// likely
#define LG_LIKELY(x) __builtin_expect(!!(x), 1)
#define LG_UNLIKELY(x) __builtin_expect(!!(x), 0)


//==============================================================================
//= Functions
//==============================================================================


#ifdef __cplusplus
}
#endif
