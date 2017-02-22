typedef struct PCL3_Interp PCL3_Interp;
PCL3_Interp *PCL3Interp_New(void);
int PCL3Interp_Feed(PCL3_Interp * interp, uint8_t * buf, int len, int *done);
void PCL3Interp_Reset(PCL3_Interp * interp);
