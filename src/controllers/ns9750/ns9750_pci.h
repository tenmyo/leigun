#include <pci.h>
#define PCI_CONFIG_ADDR                   0xA0100000
#define PCI_CONFIG_DATA                   0xA0200000

#define  PCI_HEADER_TYPE_NORMAL 0
#define  PCI_HEADER_TYPE_BRIDGE 1
#define  PCI_HEADER_TYPE_CARDBUS 2

#define NS9xxx_PARBBASE (0xA0300000)

#define PARB_PARBCFG	(0)
#define PARB_PARBINT	(0x04)
#define PARB_PARBINTEN	(0x08)
#define PARB_PMISC	(0x0C)
#define		PMISC_INTA2PCI	(1<<0)
#define		PMISC_ENBAR0	(1<<4)
#define		PMISC_ENBAR1	(1<<5)
#define		PMISC_ENBAR2	(1<<6)
#define		PMISC_ENBAR3	(1<<7)
#define		PMISC_ENBAR4	(1<<8)
#define		PMISC_ENBAR5	(1<<9)
#define PARB_PCFG_0	(0x10)
#define PARB_PCFG_1	(0x14)
#define PARB_PCFG_2	(0x18)
#define PARB_PCFG_3	(0x1C)
#define PARB_PAHBCFG	(0x20)
#define PARB_PAHBERR	(0x24)
#define PARB_PCIERR	(0x28)
#define PARB_PINTR	(0x2C)
#define PARB_PINTEN	(0x30)
#define PARB_PALTMEM0	(0x34)
#define PARB_PALTMEM1	(0x38)
#define PARB_PALTIO	(0x3C)
#define PARB_PMALT0	(0x40)
#define PARB_PMALT1	(0x44)
#define PARB_PALTCTL	(0x48)
#define PARB_CMISC	(0x4C)
#define PARB_CSKTEV	(0x1000)
#define PARB_CSKMSK	(0x1004)
#define PARB_CSKTPST	(0x1008)
#define PARB_CSKTFEV	(0x100C)
#define PARB_CSKTCTL	(0x1010)
PCI_Function *NS9750_PciInit(const char *devname, int dev_nr);
