#include <stdint.h>
#include <stdbool.h>

int Config_ReadFile(char *filename);
void Config_AddString(const char *cfgstr);
bool Config_StrStrVar(const char *section, const char *name, const char *tststr);
char *Config_ReadVar(const char *section, const char *name);
int Config_ReadInt32(int32_t * result, const char *section, const char *name);
int Config_ReadUInt32(uint32_t * result, const char *section, const char *name);
int Config_ReadUInt64(uint64_t * retval, const char *section, const char *name);
int Config_ReadFloat32(float *result, const char *section, const char *name);
/* Read a space or comma separated list */
int Config_ReadList(const char *section, const char *name, char **argvp[]);
