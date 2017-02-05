//===-- core/exithandler.h - Generic cleanup facilities -----------*- C -*-===//
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
/// This file contains the declaration of Generic cleanup facilities.
///
//===----------------------------------------------------------------------===//
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
//==============================================================================
//= Dependencies
//==============================================================================
#include "core/lg-errno.h"
// Local/Private Headers

// External headers

// System headers


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================
typedef void (*ExitHandler_Callback_cb)(void *data);


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Macros
//==============================================================================


//==============================================================================
//= Functions
//==============================================================================
Lg_Errno_t ExitHandler_Init(void);
Lg_Errno_t ExitHandler_Register(ExitHandler_Callback_cb proc, void *data);
Lg_Errno_t ExitHandler_Unregister(ExitHandler_Callback_cb proc, void *data);


#ifdef __cplusplus
}
#endif
