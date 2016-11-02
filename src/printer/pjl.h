typedef struct PJL_Interp PJL_Interp;
int PJLInterp_Feed(PJL_Interp *interp,const uint8_t *buf,int len,int *newlang);
PJL_Interp *PJLInterp_New();
void PJLInterp_Reset(PJL_Interp *interp);
