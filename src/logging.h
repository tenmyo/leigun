//===-- core/logging.h - Logging facility functions ---------------*- C -*-===//
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
/// This file contains the declaration of the Logging facility function(macro)s,
/// which provides a flexible event logging system for simulators.
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
#include <stdio.h> // for stderr, fprintf


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
typedef enum LOG_level {
    LOG_LEVEL_NONE,    ///< Nothing to out
    LOG_LEVEL_VERBOSE, ///< One by one tracing simulator behavior
    LOG_LEVEL_DEBUG,   ///< For simulator developper log
    LOG_LEVEL_INFO,    ///< For simulator user(nervous) log
    LOG_LEVEL_WARN,    ///< For simulator user(typical) log
    LOG_LEVEL_ERROR,   ///< For simulator user(uncareful) log
} LOG_level_t;


//==============================================================================
//= Types
//==============================================================================


//==============================================================================
//= Variables
//==============================================================================
extern int LOG_level;


//==============================================================================
//= Macros
//==============================================================================
#define LOG_Log(level, pri, tag, ...)                                          \
    do {                                                                       \
        if (LOG_level <= (level)) {                                            \
            fprintf(stderr, "[%-5s]\t%s\t", pri, tag);                         \
            fprintf(stderr, __VA_ARGS__);                                      \
            fprintf(stderr, "\n");                                             \
        }                                                                      \
    } while (0)

#define LOG_Verbose(tag, ...)                                                  \
    LOG_Log(LOG_LEVEL_VERBOSE, "\033[37mVERB\033[0m", tag, __VA_ARGS__)
#define LOG_Debug(tag, ...)                                                    \
    LOG_Log(LOG_LEVEL_DEBUG, "\033[1;34mDEBUG\033[0m", tag, __VA_ARGS__)
#define LOG_Info(tag, ...)                                                     \
    LOG_Log(LOG_LEVEL_INFO, "\033[1;33mINFO\033[0m", tag, __VA_ARGS__)
#define LOG_Warn(tag, ...)                                                     \
    LOG_Log(LOG_LEVEL_WARN, "\033[1;35mWARN\033[0m", tag, __VA_ARGS__)
#define LOG_Error(tag, ...)                                                    \
    LOG_Log(LOG_LEVEL_ERROR, "\033[1;31mERROR\033[0m", tag, __VA_ARGS__)


//==============================================================================
//= Functions
//==============================================================================


#ifdef __cplusplus
}
#endif
