/*
 *************************************************************************************************
 *
 * Emulation of the ARM Coprocessor cp15 
 * which is responsible for Memory Management
 *
 * Copyright 2004 2006 Jochen Karrer. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include "arm9cpu.h"
#include "cycletimer.h"
#include "mmu_arm.h"
#include "bus.h"
#include "compiler_extensions.h"
#include "fio.h"
#include "mainloop_events.h"
#include "sgstring.h"

#ifdef DEBUG
#define dbgprintf(x...) { if(unlikely(debugflags & DEBUG_MMU)) { fprintf(stderr,x); } }
#else 
#define dbgprintf(x...) 
#endif

/* Enter a Physical address to the Tlb */
TlbEntry tlbe_read;
TlbEntry tlbe_write;
TlbEntry tlbe_ifetch;
STlbEntry stlb_ifetch[STLB_SIZE];
STlbEntry stlb_read[STLB_SIZE];
STlbEntry stlb_write[STLB_SIZE];
uint32_t stlb_version;

static void
stlb_init(void) {
	int i;
	for(i=0;i<STLB_SIZE;i++) {
		stlb_ifetch[i].version=0;
		stlb_read[i].version=0;
		stlb_write[i].version=0;
	}
}

/*
 * -------------------------------------------------------
 * enter_hva_to_both_tlbe
 *
 * Enter HVAs to the first and second level TLB Cache
 * -------------------------------------------------------
 */
void
enter_hva_to_both_tlbe_read(uint32_t va,uint8_t *hva) 
{
	STlbEntry *stlbe;
	int index = STLB_INDEX(va); 
	stlbe = stlb_read+index;	
	tlbe_read.hva = stlbe->hva = hva-(va&0x3ff);
	tlbe_read.va = stlbe->va = va & 0xfffffc00;
	tlbe_read.cpu_mode = stlbe->cpu_mode = ARM_SIGNALING_MODE;
	stlbe->version = stlb_version;
}
void
enter_hva_to_both_tlbe_write(uint32_t va,uint8_t *hva) 
{
	STlbEntry *stlbe;
	int index = STLB_INDEX(va); 
	stlbe = stlb_write+index;	
	tlbe_write.hva = stlbe->hva = hva-(va&0x3ff);
	tlbe_write.va = stlbe->va = va & 0xfffffc00;
	tlbe_write.cpu_mode = stlbe->cpu_mode = ARM_SIGNALING_MODE;
	stlbe->version = stlb_version;
}
/*
 * -----------------------------------------
 * First level tlb  Physical address
 * -----------------------------------------
 */

static inline void
enter_pa_to_tlbe_read(uint32_t va,uint32_t pa) {
	tlbe_read.va=va&0xfffffc00;
	tlbe_read.pa=pa&0xfffffc00;
	tlbe_read.cpu_mode=ARM_SIGNALING_MODE;
	tlbe_read.hva=NULL;
}

static inline void
enter_pa_to_tlbe_write(uint32_t va,uint32_t pa) {
	tlbe_write.va=va&0xfffffc00;
	tlbe_write.pa=pa&0xfffffc00;
	tlbe_write.cpu_mode=ARM_SIGNALING_MODE;
	tlbe_write.hva=NULL;
}

/*
 * ---------------------------------------------------------------
 * Enter the Virtual address of the Host System to into
 * the tlb (HVA)
 * ---------------------------------------------------------------
 */

static inline void
enter_hva_to_tlbe_write(uint32_t va,uint8_t *hva) {
	tlbe_write.va=va&0xfffffc00;
	tlbe_write.hva=hva-(va&0x3ff);
	tlbe_write.cpu_mode=ARM_SIGNALING_MODE;
}


static inline void
invalidate_stlb() {
	stlb_version++;
	if(unlikely(!stlb_version)) {
		stlb_init();
		stlb_version++;
	}
}

static inline void
invalidate_tlb() {
	tlbe_ifetch.cpu_mode=~0;
	tlbe_write.cpu_mode=~0;
	tlbe_read.cpu_mode=~0;
	invalidate_stlb();
}
/*
 * -------------------------------------------------------------------
 * Invalidate TLB
 * 	for external access (Reset,Memory Controller reconfiguration)
 * -------------------------------------------------------------------
 */
void 
MMU_InvalidateTlb() {
	invalidate_tlb();
}

#define FLPD_TYPE_FAULT   (0)
#define FLPD_TYPE_COARSE  (1)
#define FLPD_TYPE_SECTION (2)
#define FLPD_TYPE_FINE	  (3)

#define SLPD_TYPE_FAULT  (0)
#define SLPD_TYPE_LARGE  (1)
#define SLPD_TYPE_SMALL  (2)
#define SLPD_TYPE_TINY   (3)

uint32_t
_MMU_Read32(uint32_t addr) {
	uint32_t taddr;
	uint8_t *hva;
	/* HVA match is alredy done in inline part !!! */
	if(likely(TLB_MATCH(tlbe_read,addr))) {
		taddr=tlbe_read.pa | ((addr) &0x3ff);	
	} else {  
		taddr=MMU9_TranslateAddress(addr,MMU_ACCESS_DATA_READ);
		hva=Bus_GetHVARead(taddr);
		if(hva) {
			enter_hva_to_both_tlbe_read(addr,hva);
			return HMemRead32(hva);
		} else {
			enter_pa_to_tlbe_read(addr,taddr);
		}
	}
	return IO_Read32(taddr);	
}

uint16_t
_MMU_Read16(uint32_t addr) {
	uint32_t taddr;
	uint8_t *hva;
	/* HVA case already done in inline part */
	if(likely(TLB_MATCH(tlbe_read,addr))) {
		taddr=tlbe_read.pa | ((addr) &0x3ff);	
	} else {  
		taddr=MMU9_TranslateAddress(addr,MMU_ACCESS_DATA_READ);
		hva=Bus_GetHVARead(taddr);
		if(hva) {
			enter_hva_to_both_tlbe_read(addr,hva);
			return HMemRead16(hva);
		} else {
			enter_pa_to_tlbe_read(addr,taddr);
		}
	}
	return IO_Read16(taddr);	
}

uint8_t
_MMU_Read8(uint32_t addr) {
	uint32_t taddr;
	/* HVA part already done in inline part */
	if(likely(TLB_MATCH(tlbe_read,addr))) {
		taddr=tlbe_read.pa | ((addr) &0x3ff);	
	} else {  
		uint8_t *hva;
		taddr=MMU9_TranslateAddress(addr,MMU_ACCESS_DATA_READ);
		hva=Bus_GetHVARead(taddr);
		if(hva) {
			enter_hva_to_both_tlbe_read(addr,hva);
			return HMemRead8(hva);
		} else {
			enter_pa_to_tlbe_read(addr,taddr);
		}
	}
	return IO_Read8(taddr);	
}

void
MMU_Write32(uint32_t value,uint32_t addr) {
	uint32_t taddr;
	uint8_t *hva;
	if(likely(TLB_MATCH(tlbe_write,addr))) {
		if(TLBE_IS_HVA(tlbe_write)) { 
			hva=tlbe_write.hva+(addr&0x3ff);
			HMemWrite32(value,hva);
			return;
		} else {
			taddr=tlbe_write.pa | ((addr) &0x3ff);	
		}
	} else if((hva=STLB_MATCH_HVA(stlb_write,addr))) {
		enter_hva_to_tlbe_write(addr,hva);
		HMemWrite32(value,hva);
		return;
	} else {  
		taddr=MMU9_TranslateAddress(addr,MMU_ACCESS_DATA_WRITE);
		hva=Bus_GetHVAWrite(taddr);
		if(hva) {
			enter_hva_to_both_tlbe_write(addr,hva);
			HMemWrite32(value,hva);
			return;
		} else {
			enter_pa_to_tlbe_write(addr,taddr);
		}
	}
	IO_Write32(value,taddr);
}

void
MMU_Write16(uint16_t value,uint32_t addr) {
	uint8_t *hva;
	uint32_t taddr;
	addr=addr^mmu_word_addr_xor;
	if(likely(TLB_MATCH(tlbe_write,addr))) {
		if(TLBE_IS_HVA(tlbe_write)) { 
			hva=tlbe_write.hva+(addr&0x3ff);
			HMemWrite16(value,hva);
			return;
		} else {
			taddr=tlbe_write.pa | ((addr) &0x3ff);	
		}
	} else if((hva=STLB_MATCH_HVA(stlb_write,addr))) {
		enter_hva_to_tlbe_write(addr,hva);
		HMemWrite16(value,hva);
		return;
	} else {  
		taddr=MMU9_TranslateAddress(addr,MMU_ACCESS_DATA_WRITE);
		hva=Bus_GetHVAWrite(taddr);
		if(hva) {
			enter_hva_to_both_tlbe_write(addr,hva);
			HMemWrite16(value,hva);
			return;
		} else {
			enter_pa_to_tlbe_write(addr,taddr);
		}
	}
	IO_Write16(value,taddr);	
}
void
MMU_Write8(uint8_t value,uint32_t addr) {
	uint8_t *hva;
	uint32_t taddr=addr;
	addr=addr^mmu_byte_addr_xor;
	if(likely(TLB_MATCH(tlbe_write,addr))) {
		if(TLBE_IS_HVA(tlbe_write)) { 
			hva=tlbe_write.hva+(addr&0x3ff);
			HMemWrite8(value,hva);
			return;
		} else {
			taddr=tlbe_write.pa | ((addr) &0x3ff);	
		}
	} else if((hva=STLB_MATCH_HVA(stlb_write,addr))) {
		enter_hva_to_tlbe_write(addr,hva);
		HMemWrite8(value,hva);
		return;
	} else {  
		taddr=MMU9_TranslateAddress(addr,MMU_ACCESS_DATA_WRITE);
		hva=Bus_GetHVAWrite(taddr);
		if(hva) {
			enter_hva_to_both_tlbe_write(addr,hva);
			HMemWrite8(value,hva);
			return;
		} else {
			enter_pa_to_tlbe_write(addr,taddr);
		}
	}
	IO_Write8(value,taddr);	
}

void
MMU_ArmInit(void) {
	stlb_init();
}
