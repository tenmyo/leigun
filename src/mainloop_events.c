/*
 *************************************************************************************************
 * MainLoopSignals
 *	Signal events to the (CPU-) mainloop 
 *	using global variables
 *
 * Status:
 *	Used by ARM9 CPU and MMU and by Infineon C161 emulation
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
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

#include <stdint.h>
#include "mainloop_events.h"

/*
 * ----------------------------------------------------
 * mainloop_job_pending
 *	Set this whenever you want that the mainloop  
 *	checks for events. Can be set from
 *	other than main-thread. May only be cleared
 *	from the thread which handles the events.
 *	Every CPU implementation is required to check
 *	this variable regularly.
 *
 *	Normaly this variable will be set after 
 *	mainloop_job_io is set or after an unmasked 
 *      interrupt is signaled to the cpu.
 * ----------------------------------------------------
 */
uint32_t mainloop_event_pending=0;

/*
 * ---------------------------------------------------
 * mainloop_job_io
 *	Set this whenever you want the mainloop
 *	to process some IO.
 * ---------------------------------------------------
 */
uint32_t mainloop_event_io=0;
