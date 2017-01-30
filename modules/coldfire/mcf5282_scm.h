#include <bus.h>

#define CSM_CS0		(0)
#define CSM_CS1		(1)
#define CSM_CS2		(2)
#define CSM_CS3		(3)
#define CSM_CS4		(4)
#define CSM_CS5		(5)
#define CSM_CS6		(6)

typedef struct Scm MCF5282ScmCsm;
MCF5282ScmCsm *MCF5282_ScmCsmNew(const char *name);
void MCF5282Scm_RegisterIpsbarDevice(MCF5282ScmCsm * scm, BusDevice * bdev, uint32_t ipsbar_offset);
void MCF5282Csm_RegisterDevice(MCF5282ScmCsm *, BusDevice * dev, unsigned int cs_nr);
