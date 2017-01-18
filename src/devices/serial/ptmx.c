/**
 ******************************************************
 * PTMX 
 ******************************************************
 */
#define _XOPEN_SOURCE
#define _BSD_SOURCE
// include self header
#include "compiler_extensions.h"
#include "ptmx.h"

// include system header
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

// include library header

// include user header
#include "sgstring.h"

#define OBUF_SIZE 2048
#define OBUF_RP(pi) ((pi)->obuf_rp % OBUF_SIZE)
#define OBUF_WP(pi) ((pi)->obuf_wp % OBUF_SIZE)
#define OBUF_LVL(pi) ((pi)->obuf_wp - (pi)->obuf_rp)

struct PtmxIface {
    PollHandle_t *rfh;
    int rfh_active;
    AsyncManager_poll_cb dataSinkProc;
    void *dataSinkEventData;

    AsyncManager_poll_cb dataSourceProc;
    void *dataSourceEventData;

    PollHandle_t *wfh;
    int wfh_active;
    uint32_t obuf_rp;
    uint32_t obuf_wp;
    uint8_t obuf[OBUF_SIZE];
    int ptmx_fd;
    char *linkname;
};

static void Ptmx_Reopen(PtmxIface * pi);

/**
 ************************************************************************************
 * \fn void Ptmx_SetDataSink(PtmxIface *pi,EV_FileProc *proc,void *eventData)
 * Configure the data sink for the ptmx file.
 ************************************************************************************
 */
void
Ptmx_SetDataSink(PtmxIface * pi, AsyncManager_poll_cb proc, void *eventData)
{
    pi->dataSinkProc = proc;
    pi->dataSinkEventData = eventData;
    if (pi->rfh_active) {
        AsyncManager_PollStart(pi->rfh, ASYNCMANAGER_EVENT_READABLE, proc, eventData);
    }
}

void
Ptmx_SetDataSource(PtmxIface * pi, AsyncManager_poll_cb proc, void *eventData)
{
    pi->dataSourceProc = proc;
    pi->dataSourceEventData = eventData;
    if (pi->wfh_active) {
        AsyncManager_PollStart(pi->wfh, ASYNCMANAGER_EVENT_WRITABLE, proc, eventData);
    }
}

/**
 *************************************************************************************
 * static void Ptmx_Reopen(PtmxIface *pi); 
 * Open/reopen the ptmx interface.
 *************************************************************************************
 */
static void
Ptmx_Reopen(PtmxIface * pi)
{
    int rfh_was_active = 0;
    int wfh_was_active = 0;
    struct termios termios;
    if (pi->rfh_active) {
        AsyncManager_PollStop(pi->rfh);
        pi->rfh_active = 0;
        rfh_was_active = 1;
    }
    if (pi->wfh_active) {
        AsyncManager_PollStop(pi->wfh);
        pi->wfh_active = 0;
        wfh_was_active = 1;
    }
    if (pi->ptmx_fd >= 0) {
        AsyncManager_Close(AsyncManager_Poll2Handle(pi->rfh), NULL, NULL);
        AsyncManager_Close(AsyncManager_Poll2Handle(pi->wfh), NULL, NULL);
        close(pi->ptmx_fd);
    }
    pi->ptmx_fd = open("/dev/ptmx", O_RDWR | O_NOCTTY);
    if (pi->ptmx_fd < 0) {
        perror("Failed to open ptmx");
        exit(4);
    }
    unlockpt(pi->ptmx_fd);
    //fprintf(stdout,"fd %d ptsname: %s\n",pi->ptmx_fd,ptsname(pi->ptmx_fd));
    unlink(pi->linkname);
    if (symlink(ptsname(pi->ptmx_fd), pi->linkname) < 0) {
        perror("can not link live events");
        exit(4);
    }
    pi->rfh = AsyncManager_PollInit(pi->ptmx_fd);
    pi->wfh = AsyncManager_PollInit(pi->ptmx_fd);
    if (rfh_was_active && pi->dataSinkProc) {
        AsyncManager_PollStart(pi->rfh, ASYNCMANAGER_EVENT_READABLE, pi->dataSinkProc,
          pi->dataSinkEventData);
        pi->rfh_active = 1;
    }
    if (wfh_was_active && pi->dataSourceProc) {
        AsyncManager_PollStart(pi->wfh, ASYNCMANAGER_EVENT_WRITABLE, pi->dataSourceProc,
          pi->dataSourceEventData);
        pi->wfh_active = 1;
    }
    if (tcgetattr(pi->ptmx_fd, &termios) < 0) {
        fprintf(stderr, "Can not  get terminal attributes\n");
        return;
    }
    cfmakeraw(&termios);
    if (tcsetattr(pi->ptmx_fd, TCSAFLUSH, &termios) < 0) {
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
Ptmx_SetInputEnable(PtmxIface * pi, bool on)
{
    if (on) {
        if (!pi->rfh_active && pi->dataSinkProc) {
            AsyncManager_PollStart(pi->rfh, ASYNCMANAGER_EVENT_READABLE, pi->dataSinkProc,
              pi->dataSinkEventData);
            pi->rfh_active = 1;
        }
    } else {
        if (pi->rfh_active) {
            AsyncManager_PollStop(pi->rfh);
            pi->rfh_active = 0;
        }
    }
}

void
Ptmx_SetOutputEnable(PtmxIface * pi, bool on)
{
    if (on) {
        if (!pi->wfh_active && pi->dataSourceProc) {
            AsyncManager_PollStart(pi->wfh, ASYNCMANAGER_EVENT_WRITABLE, pi->dataSourceProc,
              pi->dataSourceEventData);
            pi->wfh_active = 1;
        }
    } else {
        if (pi->wfh_active) {
            AsyncManager_PollStop(pi->wfh);
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
Ptmx_Write(PtmxIface * pi, void *vdata, int cnt)
{

    int result;
    result = write(pi->ptmx_fd, &pi->obuf[OBUF_RP(pi)], cnt);
    if (result < 0) {
        if (errno == EAGAIN) {
            return 0;
        } else {
            Ptmx_Reopen(pi);
            return 0;
        }
    } else if (result == 0) {
        fprintf(stderr, "EOF on tty\n");
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
Ptmx_Read(PtmxIface * pi, void *_buf, int cnt)
{
    int result;
    result = read(pi->ptmx_fd, _buf, cnt);
    if (result < 0) {
        if (errno == EAGAIN) {
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
Ptmx_Printf(PtmxIface * pi, const char *format, ...)
{
    char str[512];
    va_list ap;
    va_start(ap, format);
    vsnprintf(str, 512, format, ap);
    va_end(ap);
    Ptmx_SendString(pi, str);
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
    if (!pi->linkname) {
        fprintf(stderr, "Out of memory for ptmx link name copy\n");
        exit(4);
    }
    Ptmx_Reopen(pi);
    return pi;
}
