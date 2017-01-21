/*
 *************************************************************************************************
 *
 * Emulation of the ARM PrimeCell Multi Port 
 * Memory Controller PL172 documented in DDI0215D
 *
 *  State: working
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "compiler_extensions.h"
#include "configfile.h"
#include "arm9cpu.h"
#include "ns9750_mem.h"
#include "bus.h"
#include "signode.h"
#include "sgstring.h"

/*
 * ------------------------------------------------------------
 * Build a memory map from the eight memory Areas
 * Has to be called whenever a write to a register changes
 * memory Mapping
 * ------------------------------------------------------------
 */
void
NS9750_rebuild_map(NS9750_MemController * memco)
{
	BusDevice *bdev;
	int i, j;
	uint32_t base;
	uint32_t mask;
	uint32_t mapsize;
	for (i = 0; i < 8; i++) {
		bdev = memco->bdev[i];
		if (bdev) {
			Mem_AreaDeleteMappings(bdev);
		}
	}
	for (i = 0; i < 8; i++) {
		j = i;
		if (((i == 4) || (i == 0)) && (memco->mctrl & MCTRL_ADDM)) {
			j = 1;
		}
		base = memco->cs_base[i];
		mask = memco->cs_mask[j];
		mapsize = ~mask + 1;
		bdev = memco->bdev[j];
		if (!bdev)
			continue;
		if (!(memco->mctrl & MCTRL_MCEN)) {	// enabled  
			Mem_AreaAddMapping(bdev, base, mapsize, 0);
		} else {
			Mem_AreaAddMapping(bdev, base, mapsize,
					   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		}

	}
}

/*
 * -----------------------------------------
 * Memory Configuration Register
 * -----------------------------------------
 */
static void
mctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	if (value != memco->mctrl) {
		memco->mctrl = value;
		NS9750_rebuild_map(memco);
	}
	if (!(memco->mctrl & MCTRL_MCEN)) {
		fprintf(stderr, "Disabling memory controller at %08x\n", ARM_GET_NNIA - 8);
	}
	return;
}

static uint32_t
mctrl_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mctrl;
}

/*
 * ------------------------------------
 * Access to Chip-Select Base and Mask
 * ------------------------------------
 */
static void
sys_cs_base_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	uint32_t cs = ((address & ~3UL) - SYS_MCS4B) / 8;
	cs = (cs + 4) & 7;
	memco->cs_base[cs] = value;
	NS9750_rebuild_map(memco);
	return;
}

static uint32_t
sys_cs_base_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	uint32_t cs = ((address & ~3UL) - SYS_MCS4B) / 8;
	cs = (cs + 4) & 7;
	return memco->cs_base[cs];
}

static void
sys_cs_mask_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	uint32_t cs = ((address & ~3) - SYS_MCS4M) / 8;
	cs = (cs + 4) & 7;
	memco->cs_mask[cs] = value;
	NS9750_rebuild_map(memco);
	return;
}

static uint32_t
sys_cs_mask_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	uint32_t cs = ((address & ~3UL) - SYS_MCS4M) / 8;
	cs = (cs + 4) & 7;
	return memco->cs_mask[cs];
}

static void
mstatus_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MSTATUS is not writable\n");
	return;
}

static uint32_t
mstatus_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	// never busy, always empty buffers
	if (memco->mdynctrl & MDYNCTRL_SR) {
		return MSTATUS_SA;
	} else {
		return 0;
	}
}

/*
 * ----------------------------------------------------------------
 * The lowest bit of the MCONFIG Register (Endian Mode)
 * Changes the sequence of accesses to an external memory !
 * For example 16 Bit flash with 12 34 56 78
 * will read 56 78 12 34 when 
 * ----------------------------------------------------------------
 */
static void
mconfig_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	fprintf(stderr, "MCONFIG not fully implemented!\n");
#if 1
	memco->mconfig = value;
	if (memco->mconfig & MCONFIG_END) {
		fprintf(stderr, "Memco: Go to big endian\n");
		SigNode_Set(memco->big_endianNode, SIG_HIGH);
	} else {
		fprintf(stderr, "Memco: Go to little endian\n");
		SigNode_Set(memco->big_endianNode, SIG_LOW);
	}
	return;
#endif
}

static uint32_t
mconfig_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mconfig;
}

static void
mdynctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdynctrl = value;
	return;
}

static uint32_t
mdynctrl_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdynctrl;
}

static void
mrfrsh_tmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mrfrsh_tmr = value;
	//fprintf(stderr,"refresh timer %08x\n",value);
	return;
}

static uint32_t
mrfrsh_tmr_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mrfrsh_tmr;
}

static void
mreadconfig_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mreadconfig = value;
	return;
}

static uint32_t
mreadconfig_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mreadconfig;
}

static void
mprechrg_period_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mprechrg_period = value;
	return;
}

static uint32_t
mprechrg_period_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mprechrg_period;
}

static void
macttoprechrg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->macttoprechrg = value;
	return;
}

static uint32_t
macttoprechrg_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->macttoprechrg;
}

static void
msrfrsh_exittime_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->msrfrsh_exittime = value;
	return;
}

static uint32_t
msrfrsh_exittime_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->msrfrsh_exittime;
}

static void
mldo_to_act_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mldo_to_act = value;
	return;
}

static uint32_t
mldo_to_act_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mldo_to_act;
}

static void
mdata_to_act_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdata_to_act = value;
	return;
}

static uint32_t
mdata_to_act_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdata_to_act;
}

static void
mdyn_recover_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdyn_recover = value;
	return;
}

static uint32_t
mdyn_recover_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdyn_recover;
}

static void
mact_to_act_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mact_to_act = value;
	return;
}

static uint32_t
mact_to_act_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mact_to_act;
}

static void
marfrsh_period_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->marfrsh_period = value;
	return;
}

static uint32_t
marfrsh_period_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->marfrsh_period;
}

static void
msrfrsh_exit_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->msrfrsh_exit = value;
	return;
}

static uint32_t
msrfrsh_exit_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->msrfrsh_exit;
}

static void
macta_to_actb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->macta_to_actb = value;
	return;
}

static uint32_t
macta_to_actb_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->macta_to_actb;
}

static void
mlmod_to_act_cmd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mlmod_to_act_cmd = value;
	return;
}

static uint32_t
mlmod_to_act_cmd_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mlmod_to_act_cmd;
}

static void
mstat_ext_wait_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstat_ext_wait = value;
	return;
}

static uint32_t
mstat_ext_wait_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstat_ext_wait;
}

static void
mdyn0_config_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdyn0_config = value;
	return;
}

static uint32_t
mdyn0_config_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdyn0_config;
}

static void
mdyn1_config_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdyn1_config = value;
	return;
}

static uint32_t
mdyn1_config_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdyn1_config;
}

static void
mdyn2_config_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdyn2_config = value;
	return;
}

static uint32_t
mdyn2_config_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdyn2_config;
}

static void
mdyn3_config_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdyn3_config = value;
	return;
}

static uint32_t
mdyn3_config_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdyn3_config;
}

static void
mdyn0_rascas_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdyn0_rascas = value;
	return;
}

static uint32_t
mdyn0_rascas_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdyn0_rascas;
}

static void
mdyn1_rascas_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdyn1_rascas = value;
	return;
}

static uint32_t
mdyn1_rascas_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdyn1_rascas;
}

static void
mdyn2_rascas_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdyn2_rascas = value;
	return;
}

static uint32_t
mdyn2_rascas_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdyn2_rascas;
}

static void
mdyn3_rascas_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mdyn3_rascas = value;
	return;
}

static uint32_t
mdyn3_rascas_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mdyn3_rascas;
}

static void
mstt0_config_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt0_config = value;
	return;
}

static uint32_t
mstt0_config_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt0_config;
}

static void
mstt1_config_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt1_config = value;
	return;
}

static uint32_t
mstt1_config_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt1_config;
}

static void
mstt2_config_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt2_config = value;
	return;
}

static uint32_t
mstt2_config_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt2_config;
}

static void
mstt3_config_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt3_config = value;
	return;
}

static uint32_t
mstt3_config_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt3_config;
}

static void
mstt0_wwen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt0_wwen = value;
	return;
}

static uint32_t
mstt0_wwen_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt0_wwen;
}

static void
mstt1_wwen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt1_wwen = value;
	return;
}

static uint32_t
mstt1_wwen_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt1_wwen;
}

static void
mstt2_wwen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt2_wwen = value;
	return;
}

static uint32_t
mstt2_wwen_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt2_wwen;
}

static void
mstt3_wwen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt3_wwen = value;
	return;
}

static uint32_t
mstt3_wwen_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt3_wwen;
}

static void
mstt0_wwoen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt0_wwoen = value;
	return;
}

static uint32_t
mstt0_wwoen_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt0_wwoen;
}

static void
mstt1_wwoen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt1_wwoen = value;
	return;
}

static uint32_t
mstt1_wwoen_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt1_wwoen;
}

static void
mstt2_wwoen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt2_wwoen = value;
	return;
}

static uint32_t
mstt2_wwoen_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt2_wwoen;
}

static void
mstt3_wwoen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt3_wwoen = value;
	return;
}

static uint32_t
mstt3_wwoen_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt3_wwoen;
}

static void
mstt0_wtrd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt0_wtrd = value;
	return;
}

static uint32_t
mstt0_wtrd_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt0_wtrd;
}

static void
mstt1_wtrd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt1_wtrd = value;
	return;
}

static uint32_t
mstt1_wtrd_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt1_wtrd;
}

static void
mstt2_wtrd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt2_wtrd = value;
	return;
}

static uint32_t
mstt2_wtrd_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt2_wtrd;
}

static void
mstt3_wtrd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt3_wtrd = value;
	return;
}

static uint32_t
mstt3_wtrd_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt3_wtrd;
}

static void
mstt0_wtpg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt0_wtpg = value;
	return;
}

static uint32_t
mstt0_wtpg_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt0_wtpg;
}

static void
mstt1_wtpg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt1_wtpg = value;
	return;
}

static uint32_t
mstt1_wtpg_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt1_wtpg;
}

static void
mstt2_wtpg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt2_wtpg = value;
	return;
}

static uint32_t
mstt2_wtpg_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt2_wtpg;
}

static void
mstt3_wtpg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt3_wtpg = value;
	return;
}

static uint32_t
mstt3_wtpg_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt3_wtpg;
}

static void
mstt0_wtwr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt0_wtwr = value;
	return;
}

static uint32_t
mstt0_wtwr_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt0_wtwr;
}

static void
mstt1_wtwr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt1_wtwr = value;
	return;
}

static uint32_t
mstt1_wtwr_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt1_wtwr;
}

static void
mstt2_wtwr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt2_wtwr = value;
	return;
}

static uint32_t
mstt2_wtwr_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt2_wtwr;
}

static void
mstt3_wtwr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt3_wtwr = value;
	return;
}

static uint32_t
mstt3_wtwr_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt3_wtwr;
}

static void
mstt0_wttn_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt0_wttn = value;
	return;
}

static uint32_t
mstt0_wttn_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt0_wttn;
}

static void
mstt1_wttn_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt1_wttn = value;
	return;
}

static uint32_t
mstt1_wttn_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt1_wttn;
}

static void
mstt2_wttn_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt2_wttn = value;
	return;
}

static uint32_t
mstt2_wttn_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt2_wttn;
}

static void
mstt3_wttn_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	memco->mstt3_wttn = value;
	return;
}

static uint32_t
mstt3_wttn_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750_MemController *memco = clientData;
	return memco->mstt3_wttn;
}

static uint32_t
periph_id4_read(void *clientData, uint32_t address, int rqlen)
{
	return 0x33;		// 4 buffers 4 ports
}

static uint32_t
periph_id5_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;		// reserved
}

static uint32_t
periph_id6_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;		// reserved
}

static uint32_t
periph_id7_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;		// reserved
}

static uint32_t
periph_id0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0x72;		// part number 0
}

static uint32_t
periph_id1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0x11;		// part number 1, designer0
}

static uint32_t
periph_id2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0x14;		// designer1, revision = 1 
}

static uint32_t
periph_id3_read(void *clientData, uint32_t address, int rqlen)
{
	return 7;		// 32 Bit, TIC, Data buffers, static
}

static uint32_t
pcell_id0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0xd;
}

static uint32_t
pcell_id1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0xf0;
}

static uint32_t
pcell_id2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0x5;
}

static uint32_t
pcell_id3_read(void *clientData, uint32_t address, int rqlen)
{
	return 0xb1;
}

NS9750_MemController *
NS9750_MemCoInit(const char *name)
{
	NS9750_MemController *memco;
	uint32_t i;
	int result;
	memco = sg_new(NS9750_MemController);
	/* Default register state */
	for (i = 0; i < 8; i++) {
		int cs = (i + 4) & 7;
		memco->cs_base[cs] = 0x10000000 * i;
		memco->cs_mask[cs] = 0xf0000000;
	}
	memco->mctrl = MCTRL_ADDM | MCTRL_MCEN;
	memco->mdynctrl = MDYNCTRL_SR | 2;
	memco->mrfrsh_tmr = 0;
	memco->mreadconfig = 0;
	memco->mprechrg_period = 0xf;
	memco->macttoprechrg = 0xf;
	memco->msrfrsh_exittime = 0xf;
	memco->mldo_to_act = 0xf;
	memco->mdata_to_act = 0xf;
	memco->mdyn_recover = 0xf;
	memco->mact_to_act = 0x1f;
	memco->marfrsh_period = 0x1f;
	memco->msrfrsh_exit = 0x1f;
	memco->macta_to_actb = 0xf;
	memco->mlmod_to_act_cmd = 0xf;
	memco->mstat_ext_wait = 0;	// from real cpu
	memco->mdyn0_config = 0;
	memco->mdyn1_config = 0;
	memco->mdyn2_config = 0;
	memco->mdyn3_config = 0;
	memco->mdyn0_rascas = 0x303;
	memco->mdyn1_rascas = 0x303;
	memco->mdyn2_rascas = 0x303;
	memco->mdyn3_rascas = 0x303;
	memco->mstt0_config = 0;
	memco->mstt1_config = 0;
	memco->mstt2_config = 0;
	memco->mstt3_config = 0;
	memco->mstt0_wwen = 0;
	memco->mstt1_wwen = 0;
	memco->mstt2_wwen = 0;
	memco->mstt3_wwen = 0;
	memco->mstt0_wwoen = 0;
	memco->mstt1_wwoen = 0;
	memco->mstt2_wwoen = 0;
	memco->mstt3_wwoen = 0;
	memco->mstt0_wtrd = 0x1f;
	memco->mstt1_wtrd = 0x1f;
	memco->mstt2_wtrd = 0x1f;
	memco->mstt3_wtrd = 0x1f;
	memco->mstt0_wtpg = 0x1f;
	memco->mstt1_wtpg = 0x1f;
	memco->mstt2_wtpg = 0x1f;
	memco->mstt3_wtpg = 0x1f;
	memco->mstt0_wtwr = 0x1f;
	memco->mstt1_wtwr = 0x1f;
	memco->mstt2_wtwr = 0x1f;
	memco->mstt3_wtwr = 0x1f;
	memco->mstt0_wttn = 0xf;
	memco->mstt1_wttn = 0xf;
	memco->mstt2_wttn = 0xf;
	memco->mstt3_wttn = 0xf;

	IOH_New32(SYS_MCTRL, mctrl_read, mctrl_write, memco);
	IOH_New32(SYS_MSTATUS, mstatus_read, mstatus_write, memco);
	IOH_New32(SYS_MCONFIG, mconfig_read, mconfig_write, memco);
	IOH_New32(SYS_MDYNCTRL, mdynctrl_read, mdynctrl_write, memco);
	IOH_New32(SYS_MRFRSH_TMR, mrfrsh_tmr_read, mrfrsh_tmr_write, memco);
	IOH_New32(SYS_MREADCONFIG, mreadconfig_read, mreadconfig_write, memco);
	IOH_New32(SYS_MPRECHRG_PERIOD, mprechrg_period_read, mprechrg_period_write, memco);
	IOH_New32(SYS_MACTTOPRECHRG, macttoprechrg_read, macttoprechrg_write, memco);
	IOH_New32(SYS_MSRFRSH_EXITTIME, msrfrsh_exittime_read, msrfrsh_exittime_write, memco);
	IOH_New32(SYS_MLDO_TO_ACT, mldo_to_act_read, mldo_to_act_write, memco);
	IOH_New32(SYS_MDATA_TO_ACT, mdata_to_act_read, mdata_to_act_write, memco);
	IOH_New32(SYS_MDYN_RECOVER, mdyn_recover_read, mdyn_recover_write, memco);
	IOH_New32(SYS_MACT_TO_ACT, mact_to_act_read, mact_to_act_write, memco);
	IOH_New32(SYS_MARFRSH_PERIOD, marfrsh_period_read, marfrsh_period_write, memco);
	IOH_New32(SYS_MSRFRSH_EXIT, msrfrsh_exit_read, msrfrsh_exit_write, memco);
	IOH_New32(SYS_MACTA_TO_ACTB, macta_to_actb_read, macta_to_actb_write, memco);
	IOH_New32(SYS_MLMOD_TO_ACT_CMD, mlmod_to_act_cmd_read, mlmod_to_act_cmd_write, memco);
	IOH_New32(SYS_MSTAT_EXT_WAIT, mstat_ext_wait_read, mstat_ext_wait_write, memco);
	IOH_New32(SYS_MDYN0_CONFIG, mdyn0_config_read, mdyn0_config_write, memco);
	IOH_New32(SYS_MDYN1_CONFIG, mdyn1_config_read, mdyn1_config_write, memco);
	IOH_New32(SYS_MDYN2_CONFIG, mdyn2_config_read, mdyn2_config_write, memco);
	IOH_New32(SYS_MDYN3_CONFIG, mdyn3_config_read, mdyn3_config_write, memco);
	IOH_New32(SYS_MDYN0_RASCAS, mdyn0_rascas_read, mdyn0_rascas_write, memco);
	IOH_New32(SYS_MDYN1_RASCAS, mdyn1_rascas_read, mdyn1_rascas_write, memco);
	IOH_New32(SYS_MDYN2_RASCAS, mdyn2_rascas_read, mdyn2_rascas_write, memco);
	IOH_New32(SYS_MDYN3_RASCAS, mdyn3_rascas_read, mdyn3_rascas_write, memco);
	IOH_New32(SYS_MSTT0_CONFIG, mstt0_config_read, mstt0_config_write, memco);
	IOH_New32(SYS_MSTT1_CONFIG, mstt1_config_read, mstt1_config_write, memco);
	IOH_New32(SYS_MSTT2_CONFIG, mstt2_config_read, mstt2_config_write, memco);
	IOH_New32(SYS_MSTT3_CONFIG, mstt3_config_read, mstt3_config_write, memco);
	IOH_New32(SYS_MSTT0_WWEN, mstt0_wwen_read, mstt0_wwen_write, memco);
	IOH_New32(SYS_MSTT1_WWEN, mstt1_wwen_read, mstt1_wwen_write, memco);
	IOH_New32(SYS_MSTT2_WWEN, mstt2_wwen_read, mstt2_wwen_write, memco);
	IOH_New32(SYS_MSTT3_WWEN, mstt3_wwen_read, mstt3_wwen_write, memco);
	IOH_New32(SYS_MSTT0_WWOEN, mstt0_wwoen_read, mstt0_wwoen_write, memco);
	IOH_New32(SYS_MSTT1_WWOEN, mstt1_wwoen_read, mstt1_wwoen_write, memco);
	IOH_New32(SYS_MSTT2_WWOEN, mstt2_wwoen_read, mstt2_wwoen_write, memco);
	IOH_New32(SYS_MSTT3_WWOEN, mstt3_wwoen_read, mstt3_wwoen_write, memco);
	IOH_New32(SYS_MSTT0_WTRD, mstt0_wtrd_read, mstt0_wtrd_write, memco);
	IOH_New32(SYS_MSTT1_WTRD, mstt1_wtrd_read, mstt1_wtrd_write, memco);
	IOH_New32(SYS_MSTT2_WTRD, mstt2_wtrd_read, mstt2_wtrd_write, memco);
	IOH_New32(SYS_MSTT3_WTRD, mstt3_wtrd_read, mstt3_wtrd_write, memco);
	IOH_New32(SYS_MSTT0_WTPG, mstt0_wtpg_read, mstt0_wtpg_write, memco);
	IOH_New32(SYS_MSTT1_WTPG, mstt1_wtpg_read, mstt1_wtpg_write, memco);
	IOH_New32(SYS_MSTT2_WTPG, mstt2_wtpg_read, mstt2_wtpg_write, memco);
	IOH_New32(SYS_MSTT3_WTPG, mstt3_wtpg_read, mstt3_wtpg_write, memco);
	IOH_New32(SYS_MSTT0_WTWR, mstt0_wtwr_read, mstt0_wtwr_write, memco);
	IOH_New32(SYS_MSTT1_WTWR, mstt1_wtwr_read, mstt1_wtwr_write, memco);
	IOH_New32(SYS_MSTT2_WTWR, mstt2_wtwr_read, mstt2_wtwr_write, memco);
	IOH_New32(SYS_MSTT3_WTWR, mstt3_wtwr_read, mstt3_wtwr_write, memco);
	IOH_New32(SYS_MSTT0_WTTN, mstt0_wttn_read, mstt0_wttn_write, memco);
	IOH_New32(SYS_MSTT1_WTTN, mstt1_wttn_read, mstt1_wttn_write, memco);
	IOH_New32(SYS_MSTT2_WTTN, mstt2_wttn_read, mstt2_wttn_write, memco);
	IOH_New32(SYS_MSTT3_WTTN, mstt3_wttn_read, mstt3_wttn_write, memco);
	IOH_New32(MPMCPerihpID4, periph_id4_read, NULL, memco);
	IOH_New32(MPMCPerihpID5, periph_id5_read, NULL, memco);
	IOH_New32(MPMCPerihpID6, periph_id6_read, NULL, memco);
	IOH_New32(MPMCPerihpID7, periph_id7_read, NULL, memco);
	IOH_New32(MPMCPerihpID0, periph_id0_read, NULL, memco);
	IOH_New32(MPMCPerihpID1, periph_id1_read, NULL, memco);
	IOH_New32(MPMCPerihpID2, periph_id2_read, NULL, memco);
	IOH_New32(MPMCPerihpID3, periph_id3_read, NULL, memco);
	IOH_New32(MPMCPCellID0, pcell_id0_read, NULL, memco);
	IOH_New32(MPMCPCellID1, pcell_id1_read, NULL, memco);
	IOH_New32(MPMCPCellID2, pcell_id2_read, NULL, memco);
	IOH_New32(MPMCPCellID3, pcell_id3_read, NULL, memco);

	for (i = 0; i < 8; i++) {
		IOH_New32(SYS_MCS4B + i * 8, sys_cs_base_read, sys_cs_base_write, memco);
		IOH_New32(SYS_MCS4M + i * 8, sys_cs_mask_read, sys_cs_mask_write, memco);
	}
	Config_ReadInt32(&result, "ns9750", "boot_strap_0");
	memco->mstt1_config |= (((result ^ 1) & 1) << 7);	// inverted

	Config_ReadInt32(&result, "ns9750", "boot_strap_3");
	memco->mstt1_config |= (((result ^ 1) & 1) << 0);	// inverted

	Config_ReadInt32(&result, "ns9750", "boot_strap_4");
	memco->mstt1_config |= (((result ^ 0) & 1) << 1);	// inverted

	Config_ReadInt32(&result, "ns9750", "gpio44");
	if (!result) {
		memco->mconfig |= 1;
	}
	memco->big_endianNode = SigNode_New("%s.big_endian", name);
	if (!memco->big_endianNode) {
		fprintf(stderr, "Can not create big_endian Node\n");
		exit(3427);
	}
	if (memco->mconfig & MCONFIG_END) {
		SigNode_Set(memco->big_endianNode, SIG_HIGH);
	} else {
		SigNode_Set(memco->big_endianNode, SIG_LOW);
	}

	printf("Initial mstt1_config is %08x\n", memco->mstt1_config);
	fprintf(stderr, "ARM PL172 Memory Controller Initialized\n");
	return memco;
}

void
NS9750_RegisterDevice(NS9750_MemController * memco, BusDevice * bdev, uint32_t cs)
{
	if (cs > 7) {
		fprintf(stderr, "Bug, only 8 Chipselects available but trying to set Nr. %d\n", cs);
		exit(4324);
	}
	if (memco->bdev[cs]) {
		fprintf(stderr,
			"NS9750_RegisterDevice warning: There is already a device for CS%d\n", cs);
	}
	memco->bdev[cs] = bdev;
	NS9750_rebuild_map(memco);

}
