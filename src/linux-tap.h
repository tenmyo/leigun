#ifdef __linux__
int Net_CreateInterface(const char *devname);
#else
#define Net_CreateInterface(x) (-1)
#endif
