/**
 *************************************************************************************************
 * Atexit handler linked list for clean up on exit. 
 * This file is part of the embedded system simulator "softgun"
 *
 * Copyright 2014 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "sgstring.h"
#include "cleanup.h"

typedef struct ExitHandler {
    ExitCallback *cbProc;
    void *eventData;
    struct ExitHandler *next;
} ExitHandler; 

static ExitHandler *exitHandlerHead = NULL;
static bool gInitialized = false;

static void
AtExitHandler(void)
{
    ExitHandler *eh;
    for (eh = exitHandlerHead; eh; eh = eh->next) {
       eh->cbProc(eh->eventData); 
    }
}

/**
 **********************************************************************************
 * \fn void ExitHandler_Unregister(ExitCallback *proc, void *eventData)
 **********************************************************************************
 */
bool
ExitHandler_Unregister(ExitCallback *proc, void *eventData)
{
    ExitHandler *eh, *prev;
    for (prev = NULL, eh = exitHandlerHead; eh; eh = eh->next) {
        if ((eh->cbProc == proc) && (eh->eventData == eventData)) {
            if(prev) {
                prev->next = eh->next;
            } else {
                exitHandlerHead = eh->next;
            }
            return true;
        }
    }
    return false;
}

void
ExitHandler_Register(ExitCallback *proc, void *eventData)
{
    ExitHandler *eh;
    if (!proc) {
        fprintf(stderr, "Bug, Exit handler callback proc is missing\n");
        return;
    }
    eh = sg_new(ExitHandler);
    eh->eventData = eventData;
    eh->cbProc = proc;
    eh->next = exitHandlerHead;
    exitHandlerHead = eh;
    if (gInitialized == false) {
        atexit(AtExitHandler);
        gInitialized = true;
    } 
    return;
}
