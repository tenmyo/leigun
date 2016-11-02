#include <stdint.h>
#include <stdbool.h>
typedef void ExitCallback(void *eventData);
void ExitHandler_Register(ExitCallback *proc, void *eventData);
bool ExitHandler_Unregister(ExitCallback *proc, void *eventData);

