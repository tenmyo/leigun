//===-- core/lib.c - Sheard library load functions ----------------*- C -*-===//
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
/// This file contains the definition of the Sheard library load functions,
/// which provides a loading shared libraries.
///
//===----------------------------------------------------------------------===//

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header
#include "core/lib.h"

// Local/Private Headers
#include "core/lg-errno.h"
#include "core/list.h"
#include "core/logging.h"
#include "core/str.h"

// External headers
#include <uv.h>

// System headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================
typedef struct Lib_List_s Lib_List_t;
struct Lib_List_s {
    List_ElementMembesr(Lib_List_t);
    uv_lib_t lib;
    char *path;
};

//==============================================================================
//= Function declarations(static)
//==============================================================================
static void Lib_close(Lib_List_t *lib);
static void Lib_onExit(void);


//==============================================================================
//= Variables
//==============================================================================
static struct { List_Members(Lib_List_t) } g_libs;


//==============================================================================
//= Function definitions(static)
//==============================================================================
//===----------------------------------------------------------------------===//
/// Close loaded librarie.
//===----------------------------------------------------------------------===//
static void Lib_close(Lib_List_t *lib) {
    LOG_Verbose("Lib", "Close %s", lib->path);
    uv_dlclose(&lib->lib);
    free(lib->path);
    free(lib);
}

//===----------------------------------------------------------------------===//
/// Close loaded libraries.
//===----------------------------------------------------------------------===//
static void Lib_onExit(void) {
    LOG_Verbose("Lib", "Lib_onExit");
    List_Map(Lib_List_t, &g_libs, Lib_close);
    List_Init(&g_libs);
}


//==============================================================================
//= Function definitions(global)
//==============================================================================

//===----------------------------------------------------------------------===//
/// Init libraries.
///
/// Load and initialize the Leigun libraries from library list file.
/// The file location are ${HOME}/.leigun/libs.txt.
/// The list are described library file path on each line and allow '#' comment.
///
/// For each in lists:
/// - `dlopen(libpath)`
/// - `dlsym("initialize")` and call `void initialize(void)`
///
/// Typical list file:
/// @code
/// # This is library list file.
/// /opt/leigun/lib/ether-foo.so # Fast Ether Card foo
/// /opt/leigun/lib/video-bar.so
/// # /opt/leigun/lib/unused-lib.so
/// @endcode
///
/// @attention
/// Even if dlopen or dlsym fails, not occur error to return.
///
/// @retval LG_ESUCCESS     No Error
/// @retval LG_EENV         Can't get ENV['HOME']
//===----------------------------------------------------------------------===//
Lg_Errno_t Lib_Init(void) {
    FILE *fp;
    char path[FILENAME_MAX];
    size_t size = sizeof(path);
    int err;
    Lib_List_t *lib;
    void (*initfunc)(void);
    atexit(&Lib_onExit);
    LOG_Info("Lib", "Load libraries...");
    err = uv_os_homedir(path, &size);
    if (err < 0) {
        LOG_Warn("Lib", "uv_os_homedir failed. %s: %s", uv_err_name(err),
                 uv_strerror(err));
        return LG_EENV;
    }
    LOG_Verbose("Lib", "homedir: %s", path);
    strcat(path, "/.leigun/libs.txt");
    LOG_Verbose("Lib", "list file: %s", path);
    fp = fopen(path, "r");
    if (!fp) {
        LOG_Info("Lib", "fopen failed. %s", strerror(errno));
        return LG_ESUCCESS;
    }
    while (fgets(path, sizeof(path), fp)) {
        STR_StripL(path);
        STR_StripSharpComment(path);
        STR_StripR(path);
        if (strlen(path) == 0) {
            continue;
        }
        LOG_Info("Lib", "dlopen %s", path);
        lib = calloc(1, sizeof(*lib));
        err = uv_dlopen(path, &lib->lib);
        if (err < 0) {
            LOG_Warn("Lib", "dlopen(%s) failed. %s", path,
                     uv_dlerror(&lib->lib));
            uv_dlclose(&lib->lib);
            free(lib);
            continue;
        }
        LOG_Verbose("Lib", "dlsym(initialize)");
        err = uv_dlsym(&lib->lib, "initialize", (void **)&initfunc);
        if (err < 0) {
            LOG_Warn("Lib", "dlsym(initialize) failed. %s",
                     uv_dlerror(&lib->lib));
            uv_dlclose(&lib->lib);
            free(lib);
            continue;
        }
        lib->path = strdup(path);
        List_Push(&g_libs, lib);
        initfunc();
    }
    fclose(fp);
    return LG_ESUCCESS;
}
