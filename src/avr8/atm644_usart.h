#include <stdint.h>

typedef struct ATMUsartRegisterMap {
	int32_t addrBase;
	int32_t addrUCSRA;
	int32_t addrUCSRB;
	int32_t addrUCSRC;
	int32_t addrUBRRL;
	int32_t addrUBRRH;
	int32_t addrUDR;
} ATMUsartRegisterMap;
void ATM644_UsartNew(const char *name, ATMUsartRegisterMap *map);
