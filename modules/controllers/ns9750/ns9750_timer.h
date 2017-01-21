int Sysco_PostIrq(int nr);
void Sysco_UnPostIrq(int nr);
void NS9750_TimerInit(const char *name);

#define SYS_AHB_GCFG	(0xA0900000)
#define SYS_AHB_BRC0	(0xA0900004)
#define SYS_AHB_BRC1	(0xA0900008)
#define SYS_AHB_BRC2	(0xA090000c)
#define SYS_AHB_BRC3	(0xA0900010)

/* Table 49 on Page 125 */
#define		AHB_MASTER_ARM		(0)
#define		AHB_MASTER_ETHRX	(1)
#define 	AHB_MASTER_ETHTX	(2)
#define		AHB_MASTER_PCI		(4)
#define		AHB_MASTER_BBUS		(5)
#define		AHB_MASTER_LCD		(6)

#define SYS_AHB_ARBTOUT 	(0xA0900014)
#define SYS_AHB_ERRSTAT1	(0xA0900018)
#define SYS_AHB_ERRSTAT2 	(0xA090001c)
#define SYS_AHB_ERRMON  	(0xA0900020)

#define SYS_TRCV(n)     (0xA0900044+((n)<<2))
#define SYS_TRR(n)      (0xA0900084+((n)<<2))

#define SYS_IVARV(n)    (0xA09000C4+((n)<<2))
#define SYS_ICFG(n)	(0xA0900144+((n)^3))
/* Interrupt Service routine Address */
#define SYS_ISRA          (0xA0900164)
/* Shows Interruptrequests which are not masked */
#define SYS_ISA          (0xA0900168)

/* Shows all interruptrequests */
#define SYS_ISRAW	(0xA090016C)

#define SYS_TIS         (0xA0900170)
#define SYS_TIS_MASK    (0xffffUL)
#define SYS_SWDGCFG	(0xA0900174)

#define		SWDG_SWWE	(1<<7)
#define		SWDG_SWWI	(1<<5)
#define		SWDG_SWWIC	(1<<4)
#define		SWDG_SWTCS(x)	((x)&0x7)
#define SYS_SWDGTMR	(0xA0900178)
#define SYS_CLKCFG	(0xA090017c)
#define SYS_MODRST	(0xA0900180)
#define SYS_MISCCFG	(0xA0900184)
#define		MISCCFG_REV_SHIFT	(24)
#define		MISCCFG_PCIA		(1<<13)
#define		MISCCFG_BMM		(1<<11)
#define		MISCCFG_CS1DB		(1<<10)
#define		MISCCFG_CS1DW_SHIFT	(8)
#define		MISCCFG_CS1DW_MASK	(3<<8)
#define		MISCCFG_MCCM		(1<<7)
#define		MISCCFG_PMMS		(1<<6)
#define		MISCCFG_CS1P		(1<<5)
#define 	MISCCFG_ENDM		(1<<3)
#define		MISCCFG_MBAR		(1<<2)
#define		MISCCFG_IRAM0		(1<<0)
#define SYS_PLLCFG	(0xA0900188)
#define SYS_INTID	(0xA090018C)
#define SYS_TCR(n)	(0xA0900190+((n)<<2))

#define SYS_GENID	(0xA0900210)
#define SYS_EXTINT0	(0xA0900214)
#define SYS_EXTINT1	(0xA0900218)
#define SYS_EXTINT2	(0xA090021c)
#define SYS_EXTINT3	(0xA0900220)
#define 	EXTINT_STS	(1<<3)
#define		EXTINT_CLR	(1<<2)
#define		EXTINT_PLTY	(1<<1)
#define		EXTINT_LVEDG	(1<<0)

/* Interrupt Vectors */
/* Interrupt Configuration */

#define SYS_ICFG_BASE	(0xA0900144)
#define SYS_ICFG_IT_BIT                 (5)
#define SYS_ICFG_INV                    (1<<6)
#define SYS_ICFG_IT_FIQ			(1<<5)
#define SYS_ICFG_IT_MASK                (1<<5)
#define SYS_ICFG_IE_BIT                 (7)
#define SYS_ICFG_IE_MASK                (1<<7)
#define SYS_ICFG_ISD_MASK               (0x1f<<0)

#define SYS_TCR_REN                     (1UL<<0)
#define SYS_TCR_TSZ                     (1UL<<1)
#define SYS_TCR_UDS                     (1UL<<2)
#define SYS_TCR_INTS                    (1UL<<3)
#define SYS_TCR_TM                      (3UL<<4)
#define SYS_TCR_TM_SHIFT                (4)
#define SYS_TCR_TLCS                    (7UL<<6)
#define SYS_TCR_TLCS_SHIFT              (6)
#define SYS_TCR_INTC                    (1UL<<9)
#define SYS_TCR_TEN                     (1UL<<15)

/*
 * ----------------------------------
 * AHB Interrupt sources 
 * ----------------------------------
 */

#define IRQ_WATCHDOG_TIMER      (0)
#define IRQ_AHB_BUS_ERROR       1
#define IRQ_BBUS_AGGREGATE      2
#define IRQ_RESERVED_0          3
#define IRQ_ETH_RECEIVE         4
#define IRQ_ETH_TRANSMIT        5
#define IRQ_ETH_PHY             6
#define IRQ_LCD_MODULE          7
#define IRQ_PCI_BRIDGE          8
#define IRQ_PCI_ARBITER         9
#define IRQ_PCI_EXTERNAL_0      10
#define IRQ_PCI_EXTERNAL_1      11
#define IRQ_PCI_EXTERNAL_2      12
#define IRQ_PCI_EXTERNAL_3      13
#define IRQ_I2C                 14
#define IRQ_BBUS_DMA            15
#define IRQ_TIMER_0             16
#define IRQ_TIMER_1             17
#define IRQ_TIMER_2             18
#define IRQ_TIMER_3             19
#define IRQ_TIMER_4             20
#define IRQ_TIMER_5             21
#define IRQ_TIMER_6             22
#define IRQ_TIMER_7             23
#define IRQ_TIMER_8_9           24
#define IRQ_TIMER_10_11         25
#define IRQ_TIMER_12_13         26
#define IRQ_TIMER_14_15         27
#define IRQ_EXTERNAL_0          28
#define IRQ_EXTERNAL_1          29
#define IRQ_EXTERNAL_2          30
#define IRQ_EXTERNAL_3          31
