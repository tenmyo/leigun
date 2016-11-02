#include <stdint.h>
#include <stdbool.h>
typedef int Elf_LoadCallback(uint64_t addr, uint8_t * buf, int64_t len, void *clientData);
int64_t Elf_LoadFile(const char *filename, Elf_LoadCallback *cbProc, void *cbData);
bool Elf_CheckElf(const char *filename);

