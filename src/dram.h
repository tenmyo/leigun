#ifndef DRAM_H
#define DRAM_H
#include <bus.h>

BusDevice *DRam_New(char *name);

/* SDRAM Commands taken from qimonda HYE18L256160BFL-7.5 documentation */

#define SDRCMD_PREALL           (1)
#define SDRCMD_REFS             (2)
#define SDRCMD_REFSX            (3)
#define SDRCMD_REFA             (4)
#define SDRCMD_DPDS             (5)
#define SDRCMD_DPDSX            (6)
#define SDRCMD_CKEL             (7)
#define SDRCMD_CKEH             (8)
#define SDRCMD_READ             (9)
#define SDRCMD_READA            (10)
#define SDRCMD_WRITE            (11)
#define SDRCMD_WRITEA           (12)
#define SDRCMD_ACT              (13)
#define SDRCMD_PRE              (14)
#define SDRCMD_BST              (15)
#define SDRCMD_MRS		(16)

#define SDRCYC_UNDEFINED	(24)
#define SDRCYC_NORMAL		(25)
#define SDRCYC_PRECHRG		(26)
#define SDRCYC_AUTOREFRESH	(27)
#define SDRCYC_SETMODE		(28)
#define SDRCYC_MANSELFREFRESH	(29)

typedef struct DRam_SpecialCycle {
	int magic;		/* must be in first location */
	int cycletype;
} DRam_SpecialCycle_t;

#endif
