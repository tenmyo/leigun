#include <stdint.h>
#include <stdbool.h>
typedef int XY_SRecCallback(uint32_t addr, uint8_t * buf, int len, void *clientData);
int64_t XY_LoadSRecordFile(char *filename, XY_SRecCallback * cb, void *clientData);
bool SRecord_FileIsSRecord(char *filename);
