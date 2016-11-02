/*
 *************************************************************************************************
 *
 * Memory and IO-Memory Access for 64 Bit bus with
 * IO-Handler registration and Translation Tables from Targets Physical
 * address (TPA) to hosts virtual Address (HVA)
 *
 *  Status:
 *	not implemented
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
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
#define PG_TRACED		(1)
#define PG_TRANS_FINISHED	(2)
#define PG_TYPE_MASK		(3)

typedef struct PageDirectory {
	void *pte[16484];
	int use_count;
} PageDirectory;

static int level_shift[4] = { 50, 36, 22, 10 };
static uint32_t level_mask[4] = { 0x3fff,0x3fff,0x3fff,0x0fff};
static uint32_t level_nr_entries[4] = { 0x4000,0x4000,0x4000,0x1000};

static uint8_t *flvl[16384]; 

void *
Bus64ToHVA(uint64_t addr) {
	int i;
	int shift;
	unsigned long pg_type;
	uint32_t mask;	
	uint8_t **ptr = flvl;	
	for(i=0;i<4;i++) {
		shift = level_shift[i];	
		mask = level_mask[i];	
		int index = (addr >> shift) & mask;
		ptr = (uint8_t **)ptr[index];		
		pg_type = ((unsigned long) ptr) & PG_TYPE_MASK;
		if(pg_type & PG_TRANS_FINISHED) {
			ptr-=pg_type;
			mask = ((1<<shift) - 1);
				
			return ptr + (addr & mask);
		}	
	}
	/* Bug if reached */
	return NULL;	
}

/* 
 * ---------------------------------------------------------------------------------- 
 *
 * ---------------------------------------------------------------------------------- 
 */
void
Bus64_MapMem(uint64_t map_addr,uint8_t *start_mem,uint64_t devsize,uint64_t mapsize) 
{
	uint64_t addr;
	for(addr=map_addr;addr < mapsize;addr += pagesize) {
		
	}
}
