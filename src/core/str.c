//===-- core/str.c - string manipulation functions ----------------*- C -*-===//
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
/// This file contains the definition of the String manipulation functions,
/// which provides a miscellaneous string manipulation.
///
//===----------------------------------------------------------------------===//

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header
#include "core/str.h"

// Local/Private Headers

// External headers

// System headers
#include <ctype.h>
#include <string.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================


//==============================================================================
//= Function declarations(static)
//==============================================================================


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Function definitions(static)
//==============================================================================


//==============================================================================
//= Function definitions(global)
//==============================================================================

//===----------------------------------------------------------------------===//
/// Delete leading spaces.
///
/// Delete leading space(`isspace(c)==true`) charactors.
/// First detected non-space charactor and following charactors move to head.
///
/// @param[in,out] str      target strings
//===----------------------------------------------------------------------===//
void STR_StripL(char *str) {
    size_t len = strlen(str);
    size_t i;
    for (i = 0; i < len; ++i) {
        if (!isspace(str[i])) {
            break;
        }
    }
    if (i != 0) {
        memmove(str, &str[i], i - len + 1);
    }
}


//===----------------------------------------------------------------------===//
/// Delete trailing spaces.
///
/// Delete trailing space(`isspace(c)==true`) charactors.
/// That replace to NUL('\0') charactor.
///
/// @param[in,out] str      target strings
//===----------------------------------------------------------------------===//
void STR_StripR(char *str) {
    size_t len = strlen(str);
    size_t i;
    for (i = 0; i < len; ++i) {
        if (isspace(str[len - i])) {
            str[len - i] = '\0';
        }
    }
}


//===----------------------------------------------------------------------===//
/// Delete sharp('#') and following charactors.
///
/// Delete sharp('#') and following charactors.
/// That replace first sharp('#') to NUL('\0') charactor.
///
/// @param[in,out] str      target strings
//===----------------------------------------------------------------------===//
void STR_StripSharpComment(char *str) {
    size_t len = strlen(str);
    size_t i;
    for (i = 0; i < len; ++i) {
        if (str[i] == '#') {
            str[i] = '\0';
        }
    }
}
