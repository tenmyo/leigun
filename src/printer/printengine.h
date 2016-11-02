#ifndef _PRINT_ENGINE_H
#define _PRINT_ENGINE_H

#define PE_MEDIA_SIZE_LETTER	(2)
#define PE_MEDIA_SIZE_LEGAL	(3)
#define	PE_MEDIA_SIZE_A5	(4)
#define PE_MEDIA_SIZE_A4	(1)
#define PE_MEDIA_SIZE_A3	(26)
#define	PE_MEDIA_SIZE_A3P	(227)
#define PE_MEDIA_SIZE_B5	(45)
#define PE_MEDIA_SIZE_B4	(46)

typedef struct PrintEngine PrintEngine;
PrintEngine * PEng_New(const char *basefilename);
void PEng_EjectPage(PrintEngine *peng);
void PEng_SetMediaSize(PrintEngine *peng,int msize);
void PEng_SendRasterData(PrintEngine *peng,int xdpi,int ydpi,void *data,int pixels);

#endif
