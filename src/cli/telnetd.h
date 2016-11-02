#include <stdint.h>
#include <channel.h>
typedef struct TelnetSession TelnetSession;
typedef struct TelnetServer TelnetServer;
typedef void Telnet_AcceptProc(void *clientData, Channel * channel, char *hostName, int port);
TelnetServer *TelnetServer_New(char *host, int port, Telnet_AcceptProc * accproc, void *cd);
void Telnet_CloseSession(TelnetSession * ts);
