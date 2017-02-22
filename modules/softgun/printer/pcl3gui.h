#include <printengine.h>
typedef struct PCL3GUI_Interp PCL3GUI_Interp;

PCL3GUI_Interp *PCL3GUIInterp_New(PrintEngine * pe);
int PCL3GUIInterp_Feed(PCL3GUI_Interp *, uint8_t * buf, int count, int *done);
void PCL3GUIInterp_Reset(PCL3GUI_Interp *);
