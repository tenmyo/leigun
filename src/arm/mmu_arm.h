/*
 * ----------------------------------------------------
 *
 * Definitions for the CP15 Memory Management Unit
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 * ----------------------------------------------------
 */

#include <bus.h>
#include <sys/time.h>
#include <time.h>

/*
 * ------------------------------------------------
 * Access types required for priviledge calculation
 * ------------------------------------------------
 */
#define MMU_ACCESS_IFETCH (0x1)
#define MMU_ACCESS_DATA_READ  (0)
#define MMU_ACCESS_DATA_WRITE (32)

/*
 * ---------------------------------------------------------
 * Translation Lookaside buffer
 *      caches Physical address for IO
 *      accesses and Host Virtual Address for faster
 *      memory access
 * ---------------------------------------------------------
 */
typedef struct TlbEntry {
        uint32_t cpu_mode;
        uint32_t va;    	// ARM Virtual Address
        uint32_t pa;    	// ARM Physical Address
        uint8_t * hva;     	// Host Virtual address
} TlbEntry;

extern TlbEntry tlbe_ifetch;
extern TlbEntry tlbe_read;

extern uint32_t mmu_enabled;

#define TLBE_IS_HVA(tlbe) ((tlbe).hva!=NULL)
#define TLBE_IS_PA(tlbe) ((tlbe).hva==NULL)

#define TLB_MATCH(tlbe,addr) ((((addr)&0xfffffc00)==(tlbe).va) && ((tlbe).cpu_mode==ARM_SIGNALING_MODE))
#define TLB_MATCH_HVA(tlbe,addr) ((((addr)&0xfffffc00)==(tlbe).va) && ((tlbe).cpu_mode==ARM_SIGNALING_MODE) && TLBE_IS_HVA(tlbe))

/* 
 * -----------------------------------------------
 * The second Level TLB cache
 * -----------------------------------------------
 */
#define STLB_SIZE (128)
#define STLB_INDEX(addr) (((addr)>>10) & (STLB_SIZE-1))

typedef struct STlbEntry {
        uint32_t version;
	uint32_t cpu_mode;
        uint32_t va;    	// ARM Virtual Address
        uint8_t * hva;     	// Host Virtual address
} STlbEntry;

extern STlbEntry stlb_ifetch[STLB_SIZE];
extern STlbEntry stlb_read[STLB_SIZE];
extern STlbEntry stlb_write[STLB_SIZE];

/* Invalidating the second level TLB is done by incrementing the stlb_version */
extern uint32_t stlb_version;

static inline uint8_t * 
STLB_MATCH_HVA(STlbEntry *stlb,uint32_t addr) 
{ 
		STlbEntry *stlbe=stlb+STLB_INDEX(addr);
		if (likely( 
		   (stlbe->version == stlb_version) && 
		   (((addr)&0xfffffc00) == stlbe->va) && 
	           (stlbe->cpu_mode == ARM_SIGNALING_MODE))) {

			return stlbe->hva + (addr&0x3ff);
		} else {
			return NULL;
		}
}

#define MMU_ARM926EJS	(0xa0310000)
#define MMU_ARM920T	(0xa0320000)
#define MMUV_NS9750	(0x2)
#define MMUV_IMX21	(0x3)

ArmCoprocessor *MMU_Create(const char *name,int endian,uint32_t type);
uint32_t MMU9_TranslateAddress(uint32_t addr,uint32_t access_type);
/*
 * ---------------------------------------------------------------
 * Enter the Virtual address of the Host System to into
 * the tlb (HVA)
 * ---------------------------------------------------------------
 */
static inline void
mmu_enter_hva_to_tlbe_ifetch(uint32_t va,uint8_t *hva) {
        tlbe_ifetch.va=va&0xfffffc00;
        tlbe_ifetch.hva=hva-(va&0x3ff);
        tlbe_ifetch.cpu_mode=ARM_SIGNALING_MODE;
}

static inline void
enter_hva_to_tlbe_read(uint32_t va,uint8_t *hva) {
        tlbe_read.va=va&0xfffffc00;
        tlbe_read.hva=hva-(va&0x3ff);
        tlbe_read.cpu_mode=ARM_SIGNALING_MODE;
}

static inline void
mmu_enter_hva_to_both_tlbe_ifetch(uint32_t va,uint8_t *hva)
{
        STlbEntry *stlbe;
        int index = STLB_INDEX(va);
        stlbe = stlb_ifetch+index;
        tlbe_ifetch.hva = stlbe->hva = hva-(va&0x3ff);
        tlbe_ifetch.va = stlbe->va = va & 0xfffffc00;
        tlbe_ifetch.cpu_mode = stlbe->cpu_mode = ARM_SIGNALING_MODE;
        stlbe->version = stlb_version;
}

/*
 * ------------------------------------------------------------------
 * MMU_IFetch
 *      Fetch an Instruction code from Memory.
 *      No Caching of Physical addresses for MMU-IFetch, Only HVA
 *      is cached. If somebody executes from IO (for example
 *	flash in iomapped state) then it will be slow. 
 * ------------------------------------------------------------------
 */
static inline uint32_t
MMU_IFetch(uint32_t addr) {
	uint32_t taddr;
	uint8_t *hva;
	if(likely(TLB_MATCH(tlbe_ifetch,addr))) {
		hva=tlbe_ifetch.hva+(addr & 0x3ff);
		return HMemRead32(hva);
	} else if((hva=STLB_MATCH_HVA(stlb_ifetch,addr))) {
                mmu_enter_hva_to_tlbe_ifetch(addr,hva);
                return HMemRead32(hva);
	} else {
		taddr=MMU9_TranslateAddress(addr,MMU_ACCESS_IFETCH|MMU_ACCESS_DATA_READ);
		hva=Bus_GetHVARead(taddr);
		if(likely(hva)) {
                	mmu_enter_hva_to_both_tlbe_ifetch(addr,hva);
			return HMemRead32(hva);
		} else {
			/* Instruction from IO (For example io-mapped flash) */
			return IO_Read32(taddr); 
		}
	}
}
/*
 * -----------------------------------------------------------
 * 16 Bit instruction code fetch for Thumb mode
 * -----------------------------------------------------------
 */
static inline uint16_t
MMU_IFetch16(uint32_t addr) {
	uint32_t taddr;
	uint8_t *hva;
	if(likely(TLB_MATCH(tlbe_ifetch,addr))) {
		hva = tlbe_ifetch.hva + (addr & 0x3ff);
		return HMemRead16(hva);
	} else if((hva = STLB_MATCH_HVA(stlb_ifetch,addr))) {
                mmu_enter_hva_to_tlbe_ifetch(addr,hva);
                return HMemRead16(hva);
	} else {
		taddr=MMU9_TranslateAddress(addr,MMU_ACCESS_IFETCH|MMU_ACCESS_DATA_READ);
		hva=Bus_GetHVARead(taddr);
		if(likely(hva)) {
                	mmu_enter_hva_to_both_tlbe_ifetch(addr,hva);
			return HMemRead16(hva);
		} else {
			/* Instruction from IO (For example io-mapped flash) */
			return IO_Read16(taddr); 
		}
	}
}

uint32_t _MMU_Read32(uint32_t addr);  /* second part of above */
extern uint32_t mmu_byte_addr_xor;
extern uint32_t mmu_word_addr_xor;

static inline uint32_t 
MMU_Read32(uint32_t addr) {
        uint8_t *hva;
        if(likely(TLB_MATCH_HVA(tlbe_read,addr))) {
		hva=tlbe_read.hva+(addr&0x3ff);
		return HMemRead32(hva);
	} else if((hva=STLB_MATCH_HVA(stlb_read,addr))) {
                enter_hva_to_tlbe_read(addr,hva);
                return HMemRead32(hva);
	} else {
		return _MMU_Read32(addr);
	}
}

uint16_t _MMU_Read16(uint32_t addr);
static inline uint16_t 
MMU_Read16(uint32_t addr) {
        uint8_t *hva;
	addr ^= mmu_word_addr_xor;
        if(likely(TLB_MATCH_HVA(tlbe_read,addr))) {
		hva=tlbe_read.hva+(addr&0x3ff);
		return HMemRead16(hva);
	} else if((hva=STLB_MATCH_HVA(stlb_read,addr))) {
                enter_hva_to_tlbe_read(addr,hva);
                return HMemRead16(hva);
	} else {
		return _MMU_Read16(addr);
	}
}
uint8_t  _MMU_Read8(uint32_t addr); 
static inline uint8_t 
MMU_Read8(uint32_t addr) {
        uint8_t *hva;
	addr ^= mmu_byte_addr_xor;
        if(likely(TLB_MATCH_HVA(tlbe_read,addr))) {
		hva=tlbe_read.hva+(addr&0x3ff);
		return HMemRead8(hva);
	} else if((hva=STLB_MATCH_HVA(stlb_read,addr))) {
                enter_hva_to_tlbe_read(addr,hva);
                return HMemRead8(hva);
	} else {
		return _MMU_Read8(addr);
	}
}
void MMU_Write32(uint32_t value,uint32_t addr);
void MMU_Write16(uint16_t value,uint32_t addr);
void MMU_Write8(uint8_t value,uint32_t addr);
void MMU_AlignmentException(uint32_t far);
void MMU_InvalidateTlb(void); 
void MMU_SetDebugMode(int val);
int MMU_Byteorder();
void MMU_ArmInit(void);
