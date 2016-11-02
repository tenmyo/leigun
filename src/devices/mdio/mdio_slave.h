#include <stdint.h>
#include "mdiodev.h"
typedef struct MDIO_Slave MDIO_Slave;
MDIO_Slave *MDIOSlave_New(const char *name);
void MDIOSlave_RegisterDev(MDIO_Slave *ms,MDIO_Device *phy,uint8_t devAddr,uint8_t devType);
