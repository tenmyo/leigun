#include "signode.h"
#include "cycletimer.h"
const char *SigNot_New(const char *outname,const char *inname);
void SigNand_New(const char *out,const char *in1, const char *in2);
void SigAnd_New(const char *out,const char *in1, const char *in2);
SigNode * SigDelay_New(const char *out,const char *in,
          CycleCounter_t negDel,CycleCounter_t posDel);

