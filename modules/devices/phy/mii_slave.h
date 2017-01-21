#include "phy.h"
typedef struct MII_Slave MII_Slave;
MII_Slave *MIISlave_New(const char *name);
void MIISlave_RegisterPhy(MII_Slave *ms,PHY_Device *phy,unsigned int addr);
