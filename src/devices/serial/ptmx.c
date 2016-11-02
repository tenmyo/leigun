/**
 ******************************************************
 * PTMX 
 ******************************************************
 */

#define _XOPEN_SOURCE
#define _BSD_SOURCE 
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <alloca.h>
#include <termios.h>
#include "fio.h"
#include "ptmx.h"
#include "sgstring.h"

#define OBUF_SIZE 2048 
#define OBUF_RP(pi) ((pi)->obuf_rp % OBUF_SIZE)
#define OBUF_WP(pi) ((pi)->obuf_wp % OBUF_SIZE)
#define OBUF_LVL(pi) ((pi)->obuf_wp - (pi)->obuf_rp)

struct PtmxIface {
	FIO_FileHandler rfh;
        int rfh_active;
	FIO_FileProc *dataSinkProc;
	void *dataSinkEventData;

	FIO_FileProc *dataSourceProc;
	void *dataSourceEventData;

        FIO_FileHandler wfh;
        int wfh_active;
        uint32_t obuf_rp;
        uint32_t obuf_wp;
        uint8_t obuf[OBUF_SIZE];
	int ptmx_fd;
	char *linkname;
};

static void Ptmx_Reopen(PtmxIface *pi); 

/**
 ************************************************************************************
 * \fn void Ptmx_SetDataSink(PtmxIface *pi,EV_FileProc *proc,void *eventData)
 * Configure the data sink for the ptmx file.
 ************************************************************************************
 */
void
Ptmx_SetDataSink(PtmxIface *pi,FIO_FileProc *proc,void *eventData)
{
	pi->dataSinkProc = proc;
	pi->dataSinkEventData = eventData;
	if(pi->rfh_active) {
        	FIO_RemoveFileHandler(&pi->rfh);
        	FIO_AddFileHandler(&pi->rfh,pi->ptmx_fd,FIO_READABLE,proc,eventData);
	}
}

void
Ptmx_SetDataSource(PtmxIface *pi,FIO_FileProc *proc,void *eventData)
{
	pi->dataSourceProc = proc;
	pi->dataSourceEventData = eventData;
	if(pi->wfh_active) {
        	FIO_RemoveFileHandler(&pi->wfh);
        	FIO_AddFileHandler(&pi->wfh,pi->ptmx_fd,FIO_WRITABLE,proc,eventData);
	}
}

/**
 *************************************************************************************
 * static void Ptmx_Reopen(PtmxIface *pi); 
 * Open/reopen the ptmx interface.
 *************************************************************************************
 */
static void
Ptmx_Reopen(PtmxIface *pi) {
	int rfh_was_active = 0;
	int wfh_was_active = 0;
	struct termios termios;
        if(pi->rfh_active) {
                FIO_RemoveFileHandler(&pi->rfh);
                pi->rfh_active = 0;
		rfh_was_active = 1;
        }
        if(pi->wfh_active) {
                FIO_RemoveFileHandler(&pi->wfh);
                pi->wfh_active = 0;
		wfh_was_active = 1;
        }
        if(pi->ptmx_fd >= 0) {
                close(pi->ptmx_fd);
        }
        pi->ptmx_fd = open("/dev/ptmx",O_RDWR | O_NOCTTY);
        if(pi->ptmx_fd < 0) {
                perror("Failed to open ptmx");
                exit(4);
        }
        unlockpt(pi->ptmx_fd);
        //fprintf(stdout,"fd %d ptsname: %s\n",pi->ptmx_fd,ptsname(pi->ptmx_fd));
        unlink(pi->linkname);
        if(symlink(ptsname(pi->ptmx_fd),pi->linkname) < 0) {
                perror("can not link live events");
                exit(4);
        }
        fcntl(pi->ptmx_fd,F_SETFL,O_NONBLOCK);
	if(rfh_was_active && pi->dataSinkProc) {
        	//FIO_RemoveFileHandler(&pi->rfh);
        	FIO_AddFileHandler(&pi->rfh,pi->ptmx_fd,FIO_READABLE,pi->dataSinkProc,pi->dataSinkEventData);
        	pi->rfh_active = 1;
	}
	if(wfh_was_active && pi->dataSourceProc) {
        	FIO_AddFileHandler(&pi->wfh,pi->ptmx_fd,FIO_WRITABLE,pi->dataSourceProc,pi->dataSourceEventData);
        	pi->wfh_active = 1;
	}
	if(tcgetattr(pi->ptmx_fd,&termios)<0) {
                fprintf(stderr,"Can not  get terminal attributes\n");
                return;
        }
	cfmakeraw(&termios);
        if(tcsetattr(pi->ptmx_fd,TCSAFLUSH,&termios)<0) {
                perror("can't set terminal settings");
                return;
        }
}

/**
 ***********************************************************************
 * \fn void Ptmx_SetInputEnable(PtmxIface *pi,bool on) 
 * Enable / disable the input.
 ***********************************************************************
 */
void 
Ptmx_SetInputEnable(PtmxIface *pi,bool on) 
{
	if(on) {
		if(!pi->rfh_active && pi->dataSinkProc) {
			FIO_AddFileHandler(&pi->rfh,pi->ptmx_fd,FIO_READABLE,pi->dataSinkProc,pi->dataSinkEventData);
			pi->rfh_active = 1;
		}
	} else {
		if(pi->rfh_active) {
			FIO_RemoveFileHandler(&pi->rfh);
			pi->rfh_active = 0;
		}
	}
}
void 
Ptmx_SetOutputEnable(PtmxIface *pi,bool on) 
{
	if(on) {
		if(!pi->wfh_active && pi->dataSourceProc) {
			FIO_AddFileHandler(&pi->wfh,pi->ptmx_fd,FIO_WRITABLE,pi->dataSourceProc,pi->dataSourceEventData);
			pi->wfh_active = 1;
		}
	} else {
		if(pi->wfh_active) {
			FIO_RemoveFileHandler(&pi->wfh);
			pi->wfh_active = 0;
		}
	}
}


/**
 ********************************************************************************
 * \fn static void Ptmx_Write(PtmxIface *pi,uint8_t *data,int cnt); 
 ********************************************************************************
 */
int
Ptmx_Write(PtmxIface *pi,void *vdata,int cnt) {

	int result;
	result = write(pi->ptmx_fd,&pi->obuf[OBUF_RP(pi)],cnt);
	if(result < 0) {
		if(errno == EAGAIN) {
			return 0;
		} else {
			Ptmx_Reopen(pi);
			return 0;
		}
	} else if(result == 0) {
		fprintf(stderr,"EOF on tty\n");
		exit(4);
	} 
	return result;
}

/**
 *************************************************************************
 * Read from the Ptmx interface
 *************************************************************************
 */
int
Ptmx_Read(PtmxIface *pi,void *_buf,int cnt) 
{
	int result; 
	result = read(pi->ptmx_fd,_buf,cnt);
	if(result < 0) {
		if(errno == EAGAIN) {
			return 0;
		} else {
			Ptmx_Reopen(pi);
			return 0;
		}
	}
	return result;
}

/**
 ********************************************************************
 * \fn void Ptmx_Printf(PtmxIface *pi,const char *format,...) 
 ********************************************************************
 */
void
Ptmx_Printf(PtmxIface *pi,const char *format,...) {
        char str[512];
        va_list ap;
        va_start (ap, format);
        vsnprintf(str,512,format,ap);
        va_end(ap);
	Ptmx_SendString(pi,str);
}

/**
 ****************************************************************************
 * PtmxIface * Ptmx_New(const char *linkname) 
 ****************************************************************************
 */

PtmxIface * 
PtmxIface_New(const char *linkname) 
{
	PtmxIface *pi = sg_new(PtmxIface);
	pi->ptmx_fd = -1;
	pi->linkname = strdup(linkname);
	if(!pi->linkname) {
		fprintf(stderr,"Out of memory for ptmx link name copy\n");
		exit(4);
	}
	Ptmx_Reopen(pi);
	return pi;	
}
