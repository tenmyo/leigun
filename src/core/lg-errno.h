//===-- core/lg-errno.h - Error constants -------------------------*- C -*-===//
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
/// \file
/// This file contains the declaration of the Error constants.
///
//===----------------------------------------------------------------------===//
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
//==============================================================================
//= Dependencies
//==============================================================================


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
/* Expand this list if necessary. */
#define LG_ERRNO_MAP(XX)                                                       \
    XX(ESUCCESS, 0, "Success")                                                 \
    XX(EENV, -1, "Can't get environment")

typedef enum Lg_Errno_e {
#define XX(code, num, _) LG_##code = (num),
    LG_ERRNO_MAP(XX)
#undef XX
} Lg_Errno_t;


//==============================================================================
//= Types
//==============================================================================


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Functions
//==============================================================================

#ifdef __cplusplus
}
#endif
