#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "fio.h"
typedef struct PtmxIface PtmxIface;
typedef void PtmxDataSinkProc(void *evData,uint8_t *buf,int cnt);
PtmxIface * PtmxIface_New(const char *linkname);
int Ptmx_Write(PtmxIface *pi,void *vdata,int cnt);
void Ptmx_Printf(PtmxIface *pi,const char *format,...)  __attribute__ ((format (printf, 2, 3))); 
void Ptmx_SetDataSink(PtmxIface *pi,FIO_FileProc *proc,void *eventData);
void Ptmx_SetDataSource(PtmxIface *pi,FIO_FileProc *proc,void *eventData);

void Ptmx_SetInputEnable(PtmxIface *pi,bool on);
void Ptmx_SetOutputEnable(PtmxIface *pi,bool on);
int Ptmx_Read(PtmxIface *pi,void *_buf,int cnt);



static inline void
Ptmx_SendString(PtmxIface *pi,char *str) {
	Ptmx_Write(pi,str,strlen(str));
}
