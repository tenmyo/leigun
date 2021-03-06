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
/// @file
/// This file contains the definition of the Sheard library load functions,
/// which provides a loading shared libraries.
///
//===----------------------------------------------------------------------===//

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header
#include "lib.h"

// Local/Private Headers
#include "exithandler.h"
#include "leigun.h"
#include "list.h"
#include "logging.h"
#include "str.h"

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
struct Lib_listMember_s;
typedef struct Lib_listMember_s {
    List_Element_t liste;
    uv_lib_t lib;
    char *path;
} Lib_listMember_t;
typedef struct Lib_list_s { List_t list; } Lib_list_t;

//==============================================================================
//= Function declarations(static)
//==============================================================================
static void Lib_close(Lib_listMember_t *lib);
static void Lib_onExit(Lib_list_t *libs);


//==============================================================================
//= Variables
//==============================================================================
static Lib_list_t Lib_loadedLibs;


//==============================================================================
//= Function definitions(static)
//==============================================================================
//===----------------------------------------------------------------------===//
/// Close loaded librarie.
//===----------------------------------------------------------------------===//
static void Lib_close(Lib_listMember_t *lib) {
    // DO NOT UNLOAD for debugging (e.g. valgrind)
    // LOG_Debug("Lib", "Unload %s", lib->path);
    // uv_dlclose(&lib->lib);
    free(lib->path);
    free(lib);
}

//===----------------------------------------------------------------------===//
/// Close loaded libraries.
//===----------------------------------------------------------------------===//
static void Lib_onExit(Lib_list_t *libs) {
    LOG_Verbose("Lib", "Lib_onExit");
    List_Map(&libs->list, (List_Proc_cb)&Lib_close);
    List_Init(&libs->list);
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
/// @return same as libuv. imply an error if negative.
//===----------------------------------------------------------------------===//
int Lib_Init(void) {
    FILE *fp;
    char path[FILENAME_MAX];
    size_t size = sizeof(path);
    int err;
    Lib_listMember_t *lib;

    List_Init(&Lib_loadedLibs.list);
    err = ExitHandler_Register((ExitHandler_Callback_cb)&Lib_onExit,
                               &Lib_loadedLibs);
    if (err < 0) {
        LOG_Warn("Lib", "register exit handler failed. %d", err);
        return err;
    }
    LOG_Info("Lib", "Load libraries...");
    err = uv_os_homedir(path, &size);
    if (err < 0) {
        LOG_Warn("Lib", "uv_os_homedir failed. %s: %s", uv_err_name(err),
                 uv_strerror(err));
        return err;
    }
    LOG_Verbose("Lib", "homedir: %s", path);
    strcat(path, "/.leigun/libs.txt");
    LOG_Verbose("Lib", "list file: %s", path);
    fp = fopen(path, "r");
    if (!fp) {
        LOG_Info("Lib", "fopen failed. %s", strerror(errno));
        return 0;
    }
    while (fgets(path, sizeof(path), fp)) {
        STR_StripL(path);
        STR_StripSharpComment(path);
        STR_StripR(path);
        if (strlen(path) == 0) {
            continue;
        }
        LOG_Info("Lib", "Load %s", path);
        lib = LEIGUN_NEW(lib);
        err = uv_dlopen(path, &lib->lib);
        if (err < 0) {
            LOG_Warn("Lib", "dlopen(%s) failed. %s", path,
                     uv_dlerror(&lib->lib));
            uv_dlclose(&lib->lib);
            free(lib);
            continue;
        }
        lib->path = strdup(path);
        List_Push(&Lib_loadedLibs.list, &lib->liste);
    }
    fclose(fp);
    return 0;
}
