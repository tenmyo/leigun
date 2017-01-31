/*
 ***********************************************************************************************
 *
 * Remote Frame buffer protocol server (vncserver)
 *
 * State:
 *	Works with RAW and ZRLE encoding with True color and
 *	8 Bit colormap. Recommended viewer is realvnc
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */
// include self header
#include "compiler_extensions.h"
#include "rfbserver.h"

// include system header
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// include library header
#include <uv.h>
#ifndef NO_ZLIB
#include <zlib.h>
#endif

// include user header
#include "core/byteorder.h"
#include "configfile.h"
#include "fbdisplay.h"
#include "sgstring.h"
#include "sglib.h"

#include "core/asyncmanager.h"

#ifndef NO_KEYBOARD
#include "keyboard.h"
#endif

#ifndef NO_MOUSE
#include "mouse.h"
#endif

#if 0
#define dbgprintf(...) { fprintf(stdout,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define IBUFSIZE 8192

#define CONSTAT_NEW		(0)
#define	CONSTAT_PROTO_WAIT	(1)
#define	CONSTAT_SHARED_WAIT	(2)
#define CONSTAT_IDLE		(3)

typedef struct RfbServer RfbServer;

 /*
  * -----------------------------------------------------------------------
  * PixelFormat
  *	Pixelformat is used to store the default
  *	pixelformat of the server and the required
  *	pixelformat for the rfb connection
  * ----------------------------------------------------------------------
  */

typedef struct PixelFormat {
  uint8_t bits_per_pixel;
  uint8_t bypp;
  uint8_t depth;
  uint8_t big_endian_flag;
  uint8_t true_color_flag;
  uint16_t red_max;
  uint8_t red_bits;	/* redundant, calculated from red_max */
  uint16_t blue_max;
  uint8_t blue_bits;	/* redundant, calculated from blue_max */
  uint16_t green_max;
  uint8_t green_bits;	/* redundant, calculated from green_max */
  uint8_t red_shift;
  uint8_t green_shift;
  uint8_t blue_shift;
  uint8_t padding[3];
} PixelFormat;

#define MAX_NAME_LEN (80)

typedef struct FrameBufferInfo {
  int fb_size;		/* Number of bytes allocated in *framebuffer */
  char *framebuffer;
  unsigned int fb_width;
  unsigned int fb_height;
  unsigned int fb_linebytes;
  PixelFormat pixfmt;
  uint32_t name_length;
  char name_string[MAX_NAME_LEN];
} FrameBufferInfo;

typedef struct UpdateRectangle {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
} UpdateRectangle;

typedef struct RLEncoder {
  uint32_t pixval;
  int run_length;
} RLEncoder;

#define UDRECT_FIFOSIZE (256)
/*
 * -----------------------------------------------------------------------------
 * RfbConnection structure contains the state information
 * for one client. It is inserted in a linked list of clients
 * with list head in RfbServer
 * ------------------------------------------------------------------------------
 */
typedef struct RfbConnection {
  int protoversion;	/* major in high 16 bit minor in lower 16 Bit */
  int state;
  int current_encoding;
  StreamHandle_t *handle;

  PixelFormat pixfmt;
  /* translation table belongs to the pixel format */
  uint32_t trans_red[256];
  uint32_t trans_green[256];
  uint32_t trans_blue[256];

  FrameBufferInfo *fbi;	/* points to fbi of RfbServer */
  struct RfbConnection *next;
  uint8_t ibuf[IBUFSIZE];
  int ibuf_wp;
  int ibuf_expected;
  RLEncoder rle;
#ifndef NO_ZLIB
  z_stream zs;
#endif
  RfbServer *rfbserv;
  int update_outstanding;
  UpdateRectangle udrect_fifo[UDRECT_FIFOSIZE];
  uint64_t udrect_wp;
  uint64_t udrect_rp;
  uint8_t *obuf;
  int obuf_wp;
  int obuf_size;
} RfbConnection;

struct RfbServer {
  FbDisplay display;
#ifndef NO_KEYBOARD
  Keyboard keyboard;
#endif
#ifndef NO_MOUSE
  Mouse mouse;
#endif
  int32_t propose_bpp;
#ifndef NO_STARTCMD
  pid_t viewerpid;
#endif
  RfbConnection *con_head;
  /* Servers native FBI (window info & Pixelformat */
  FrameBufferInfo fbi;
  uint32_t exit_on_close;
};

/*
 *******************************************************************
 * Update the bit count fields when the maxval has changed
 *******************************************************************
 */
static void
pixfmt_update_bits(PixelFormat * pixfmt) {
  pixfmt->red_bits = SGLib_OnecountU32(pixfmt->red_max);
  pixfmt->green_bits = SGLib_OnecountU32(pixfmt->green_max);
  pixfmt->blue_bits = SGLib_OnecountU32(pixfmt->blue_max);

}

static void
pixfmt_update_translation(RfbConnection * rcon) {
  uint32_t red, green, blue;
  FrameBufferInfo *fbi = rcon->fbi;
  PixelFormat *pixf = &rcon->pixfmt;
  PixelFormat *fbpixf = &fbi->pixfmt;
  for (red = 0; red <= fbpixf->red_max; red++) {
    uint32_t trans;
    trans = red * pixf->red_max / fbpixf->red_max;
    trans <<= pixf->red_shift;
    rcon->trans_red[red] = trans;
  }
  for (green = 0; green <= fbpixf->green_max; green++) {
    uint32_t trans;
    trans = green * pixf->green_max / fbpixf->green_max;
    trans <<= pixf->green_shift;
    rcon->trans_green[green] = trans;
  }
  for (blue = 0; blue <= fbpixf->blue_max; blue++) {
    uint32_t trans;
    trans = blue * pixf->blue_max / fbpixf->blue_max;
    trans <<= pixf->blue_shift;
    rcon->trans_blue[blue] = trans;
  }
}

static void free_rcon(Handle_t *handle, void *clientdata) {
  RfbConnection *cursor, *prev;
  RfbConnection *rcon = clientdata;
  RfbServer *rfbserv = rcon->rfbserv;
  for (prev = NULL, cursor = rfbserv->con_head; cursor; prev = cursor, cursor = cursor->next) {
    if (cursor == rcon) {
      if (prev) {
        prev->next = cursor->next;
      } else {
        rfbserv->con_head = cursor->next;
      }
    }
  }
#ifndef NO_ZLIB
  deflateEnd(&rcon->zs);
#endif
  if (rcon->obuf) {
    free(rcon->obuf);
  }
  free(rcon);
  if (rfbserv->exit_on_close && !rfbserv->con_head) {
    fprintf(stderr, "Exiting after termination of last VNC connection\n");
    exit(0);
  }
}

/*
 * --------------------------------------------------------------------------
 * rfbsrv_disconnect
 * 	Close connection to client
 * --------------------------------------------------------------------------
 */
static void
rfbsrv_disconnect(RfbConnection * rcon) {
  AsyncManager_ReadStop(rcon->handle);
  AsyncManager_Close((Handle_t *)rcon->handle, &free_rcon, rcon);
}

static int
Msg_ProtocolVersion(RfbConnection *rcon) {
  char *msg = "RFB 003.003\n";
  return AsyncManager_WriteSync(rcon->handle, msg, strlen(msg));
}

/*
 *********************************************************************
 * Check the reply if protocol version is compatible
 *********************************************************************
 */
static int
CheckProtocolVersion(RfbConnection * rcon) {
  int major, minor;
  uint32_t protoversion;
  rcon->ibuf[12] = 0;
  sscanf((char *)rcon->ibuf, "RFB %03d.%03d", &major, &minor);
  protoversion = major << 16 | (minor & 0xffff);
  if (protoversion != rcon->protoversion) {
    /* Compatibility check missing here */
    fprintf(stderr, "VNC: disagrement in Protocoll Version: "
      "%08x - %08x", protoversion, rcon->protoversion);
    rcon->protoversion = protoversion;
  }
  dbgprintf("protocol version reply: %s", rcon->ibuf);
  return 0;
}

/*
 * ----------------------------------------------------------
 * Currently no authentication is used.
 * Need DES implementation for adding VNC authentication
 * ----------------------------------------------------------
 */
static int
Msg_Auth(RfbConnection *rcon) {
  char msg[] = { 0, 0, 0, 1 };	/* no authentication required */
  return AsyncManager_WriteSync(rcon->handle, msg, 4);
}

/**
 ****************************************************************
 * Pixel format proposals. Required for silly realvnc which
 * quits if 24 Bits per pixel is proposed by the server.
 ****************************************************************
 */
static PixelFormat pixfmt_proposals[4] = {
  {
   .bits_per_pixel = 8,
   .bypp = 1,
   .depth = 8,
   .big_endian_flag = 0,
   .true_color_flag = 0,
   .red_max = 7,
   .blue_max = 3,
   .green_max = 7,
   .red_shift = 5,
   .green_shift = 2,
   .blue_shift = 0,
   .red_bits = 3,
   .green_bits = 3,
   .blue_bits = 2,
   },
  {
   .bits_per_pixel = 16,
   .bypp = 2,
   .depth = 16,
   .big_endian_flag = 0,
   .true_color_flag = 1,
   .red_max = 15,
   .blue_max = 15,
   .green_max = 15,
   .red_shift = 8,
   .green_shift = 4,
   .blue_shift = 0,
   .red_bits = 4,
   .green_bits = 4,
   .blue_bits = 4,
   },
  {
   .bits_per_pixel = 24,
   .bypp = 3,
   .depth = 24,
   .big_endian_flag = 0,
   .true_color_flag = 1,
   .red_max = 255,
   .blue_max = 255,
   .green_max = 255,
   .red_shift = 16,
   .green_shift = 8,
   .blue_shift = 0,
   .red_bits = 8,
   .green_bits = 8,
   .blue_bits = 8,
   },
  {
   .bits_per_pixel = 32,
   .bypp = 4,
   .depth = 24,
   .big_endian_flag = 0,
   .true_color_flag = 1,
   .red_max = 255,
   .blue_max = 255,
   .green_max = 255,
   .red_shift = 16,
   .green_shift = 8,
   .blue_shift = 0,
   .red_bits = 8,
   .green_bits = 8,
   .blue_bits = 8,
   },
};

static int
Msg_ServerInitialisation(RfbConnection * rcon) {
  RfbServer *rfbserv = rcon->rfbserv;
  char *msg = sg_calloc(sizeof(FrameBufferInfo)); // FIXME: leak?
  char *p = msg;
  int i;
  FrameBufferInfo *fbi = &rfbserv->fbi;
  PixelFormat *pixfmt = &fbi->pixfmt;
  fbi->name_length = strlen(fbi->name_string);
  *(uint16_t *)p = htons(fbi->fb_width);
  p += 2;
  *(uint16_t *)p = htons(fbi->fb_height);
  p += 2;
  if (rfbserv->propose_bpp > 0) {
    for (i = 0; i < array_size(pixfmt_proposals); i++) {
      PixelFormat *pf = pixfmt_proposals + i;
      if (pf->bits_per_pixel == rfbserv->propose_bpp) {
        pixfmt = pf;
        break;
      }
    }
    if (i == array_size(pixfmt_proposals)) {
      fprintf(stderr, "Illegal bits per pixel proposal %d "
        "in configuration file", rfbserv->propose_bpp);
    }
  }
  *(uint8_t *)p = pixfmt->bits_per_pixel;
  p += 1;
  *(uint8_t *)p = pixfmt->depth;
  p += 1;
  *(uint8_t *)p = pixfmt->big_endian_flag;
  p += 1;
  *(uint8_t *)p = pixfmt->true_color_flag;
  p += 1;
  *(uint16_t *)p = htons(pixfmt->red_max);
  p += 2;
  *(uint16_t *)p = htons(pixfmt->green_max);
  p += 2;
  *(uint16_t *)p = htons(pixfmt->blue_max);
  p += 2;
  *(uint8_t *)p = pixfmt->red_shift;
  p += 1;
  *(uint8_t *)p = pixfmt->green_shift;
  p += 1;
  *(uint8_t *)p = pixfmt->blue_shift;
  p += 1;
  /* padding */
  p += 3;

  *(uint32_t *)p = htonl(fbi->name_length);
  p += 4;
  memcpy(p, fbi->name_string, fbi->name_length);
  p += fbi->name_length;
  return AsyncManager_WriteSync(rcon->handle, msg, p - msg);
}

/*
 * -------------------------------------------------------------------
 * The following messages might be received by the rfbserver
 * -------------------------------------------------------------------
 */
#define CLNT_SET_PIXEL_FORMAT	(0)
#define CLNT_SET_ENCODINGS	(2)
#define		ENC_RAW			(0)
#define		ENC_COPYRECT		(1)
#define		ENC_RRE			(2)
#define		ENC_CORRE		(4)
#define		ENC_HEXTILE		(5)
#define		ENC_ZLIB		(6)
#define		ENC_TIGHT		(7)
#define		ENC_ZLIBHEX		(8)
#define		ENC_ZRLE		(16)
#define		ENC_CURSOR_PSEUDO	(-239)
#define		ENC_DESKTOP_SIZE	(-223)
#define CLNT_FB_UPDATE_REQ	(3)
#define	CLNT_KEY_EVENT		(4)
#define CLNT_POINTER_EVENT	(5)
#define CLNT_CLIENT_CUT_TEXT	(6)

 /*
  * ---------------------------------------------------
  * The following messages are sent by the server
  * ---------------------------------------------------
  */
#define SRV_FB_UPDATE			(0)
#define SRV_SET_COLOUR_MAP_ENTRIES	(1)
#define SRV_BELL			(2)
#define	SRV_CUT_TEXT			(3)

#define read16be(addr) (ntohs(*(uint16_t*)(addr)))
#define read32be(addr) (ntohl(*(uint32_t*)(addr)))
#define write16be(addr,value) (*(uint16_t*)(addr) = htons(value))
#define write32be(addr,value) (*(uint32_t*)(addr) = htonl(value))

#define write16(addr,value) (*(uint16_t*)(addr) = (value))
#define write32(addr,value) (*(uint32_t*)(addr) = (value))

  /*
   * ------------------------------------------------------------------------------
   * Convert a single pixel stored in a src buffer from the framebuffer
   * pixel format into a destination pixel format which is connection specific
   * ------------------------------------------------------------------------------
   */

static inline uint32_t
encode_pixval(RfbConnection * rcon, void *src, PixelFormat * pixf, PixelFormat * fbpixf) {
  uint32_t pixval;
  uint8_t red, green, blue;

  switch (fbpixf->bits_per_pixel) {
  case 32:
    pixval = BYTE_LeToH32(*(uint32_t *)src);
    break;
  case 16:
    pixval = BYTE_LeToH16(*(uint16_t *)src);
    break;
  case 8:
    pixval = *(uint8_t *)src;
    break;
  default:
    pixval = 0;
  }
  red = (pixval >> fbpixf->red_shift) & fbpixf->red_max;
  green = (pixval >> fbpixf->green_shift) & fbpixf->green_max;
  blue = (pixval >> fbpixf->blue_shift) & fbpixf->blue_max;
  return rcon->trans_red[red] | rcon->trans_green[green] | rcon->trans_blue[blue];
}

/*
 * ---------------------------------------------------------------
 * write a pixel value into a buffer
 * ---------------------------------------------------------------
 */
static int
write_pixel8(uint8_t * dst, uint32_t pixval) {
  dst[0] = pixval;
  return 1;
}

static int
write_pixel16_swap(uint8_t * dst, uint32_t pixval) {
  write16(dst, BYTE_Swap16(pixval));
  return 2;
}

static int
write_pixel16(uint8_t * dst, uint32_t pixval) {
  write16(dst, pixval);
  return 2;
}

static int
write_pixel24_swap(uint8_t * dst, uint32_t pixval) {
  uint8_t *pix = (uint8_t *)& pixval;
  dst[0] = pix[3];
  dst[1] = pix[2];
  dst[2] = pix[1];
  return 3;
}

static int
write_pixel24(uint8_t * dst, uint32_t pixval) {
  /* Do not use write32 because of alignment traps */
  uint8_t *pix = (uint8_t *)& pixval;
  dst[0] = pix[0];
  dst[1] = pix[1];
  dst[2] = pix[2];
  return 3;
}

static int
write_pixel32_swap(uint8_t * dst, uint32_t pixval) {
  write32(dst, BYTE_Swap32(pixval));
  return 4;
}

static int
write_pixel32(uint8_t * dst, uint32_t pixval) {
  write32(dst, pixval);
  return 4;
}

typedef int WritePixelProc(uint8_t * dst, uint32_t pixval);

static WritePixelProc *
GetWritePixelProc(int con_bypp, int swap) {
  switch (con_bypp) {
  case 1:
    return write_pixel8;
  case 2:
    if (swap) {
      return write_pixel16_swap;
    } else {
      return write_pixel16;
    }
    break;
  case 3:
    if (swap) {
      return write_pixel24_swap;
    } else {
      return write_pixel24;
    }
    break;
  case 4:
    if (swap) {
      return write_pixel32_swap;
    } else {
      return write_pixel32;
    }
    break;
  default:
    fprintf(stderr, "%d bytes per pixel not implemented\n", con_bypp);
    return NULL;
  }
}

static inline int
write_pixel(int con_bypp, int swap, uint8_t * dst, uint32_t pixval) {
  switch (con_bypp) {
  case 1:
    dst[0] = pixval;
    return 1;
  case 2:
    if (swap) {
      write16(dst, BYTE_Swap16(pixval));
    } else {
      write16(dst, pixval);
    }
    return 2;
  case 3:
    if (swap) {
      uint8_t *pix = (uint8_t *)& pixval;
      dst[0] = pix[3];
      dst[1] = pix[2];
      dst[2] = pix[1];
    } else {
      /* Do not use write32 because of alignment traps */
      uint8_t *pix = (uint8_t *)& pixval;
      dst[0] = pix[0];
      dst[1] = pix[1];
      dst[2] = pix[2];
    }
    return 3;
  case 4:
    if (swap) {
      write32(dst, BYTE_Swap32(pixval));
    } else {
      write32(dst, pixval);
    }
    return 4;
  default:
    fprintf(stderr, "%d bytes per pixel not implemented\n", con_bypp);
    return 0;
  }
}

/*
 * -----------------------------------------------------------------------------
 * Writes an update message header into the header of the current message
 * -----------------------------------------------------------------------------
 */
static inline int
add_update_header(uint8_t * hdr, int rects) {
  hdr[0] = SRV_FB_UPDATE;
  hdr[1] = 0;
  write16be(hdr + 2, rects);	/* Number Rectangles */
  return 4;
}

static inline void
rle_init(RLEncoder * rle) {
  rle->run_length = 0;
}

/*
 * --------------------------------------------------------
 * rle_add_pixval
 *	Add a pixel value to a rle encoding
 * --------------------------------------------------------
 */
static inline int
rle_add_pixval(RLEncoder * rle, int con_bypp, int swap, uint8_t * dst, uint32_t pixval) {
  if (rle->run_length == 0) {
    rle->pixval = pixval;
    rle->run_length++;
    return 0;
  } else {
    if (rle->pixval == pixval) {
      rle->run_length++;
      return 0;
    } else {
      int count;
      count = write_pixel(con_bypp, swap, dst, rle->pixval);
      dst += count;
      rle->run_length -= 1;
      while (rle->run_length >= 255) {
        *dst++ = 255;
        count++;
        rle->run_length -= 255;
      }
      *dst = rle->run_length;
      count++;
      rle->pixval = pixval;
      rle->run_length = 1;
      return count;
    }
  }
}

/*
 * --------------------------------------------------------------
 * rle_flush
 *	Flush the last value in the rle buffer to the encoding
 *      destination, Then reset the run length to zero.
 * --------------------------------------------------------------
 */
static inline int
rle_flush(RLEncoder * rle, int con_bypp, int swap, uint8_t * dst) {
  int retval;
  retval = rle_add_pixval(rle, con_bypp, swap, dst, rle->pixval ^ 0xffffffff);
  rle->run_length = 0;
  return retval;
}

/*
 * ------------------------------------------------------------------------
 * srv_encode_update_raw
 *	Make a raw update request from the rectangles in the fifo
 * ------------------------------------------------------------------------
 */
static inline void
srv_fb_encode_update_raw(RfbConnection * rcon) {
  WritePixelProc *wpProc;
  FrameBufferInfo *fbi = rcon->fbi;
  PixelFormat *pixf = &rcon->pixfmt;
  PixelFormat *fbpixf = &fbi->pixfmt;
  int con_bypp = pixf->bits_per_pixel >> 3;
  int fb_bypp = fbi->pixfmt.bits_per_pixel >> 3;
  uint8_t *reply;
  uint8_t *data;
  int x, y;
  int swap;
  reply = rcon->obuf;
  data = reply;
  data += add_update_header(data, rcon->udrect_wp - rcon->udrect_rp);

  swap = (pixf->big_endian_flag != fbpixf->big_endian_flag);
  wpProc = GetWritePixelProc(con_bypp, swap);
  if (!wpProc) {
    return;
  }
  while (rcon->udrect_rp < rcon->udrect_wp) {
    int memsize;
    UpdateRectangle *udrect = &rcon->udrect_fifo[rcon->udrect_rp % UDRECT_FIFOSIZE];

    rcon->udrect_rp++;
    memsize = udrect->width * udrect->height * con_bypp + 16;
    if (rcon->obuf_size < memsize) {
      rcon->obuf_size = memsize;
      reply = realloc(rcon->obuf, rcon->obuf_size);
      if (!reply) {
        fprintf(stderr, "RFB-Server: No memory\n");
        return;
      }
      data = reply + (data - rcon->obuf);
      rcon->obuf = reply;
    }

    write16be(data, udrect->x);
    write16be(data + 2, udrect->y);
    write16be(data + 4, udrect->width);
    write16be(data + 6, udrect->height);
    write32be(data + 8, ENC_RAW);
    data += 12;

    for (y = udrect->y; y < (udrect->y + udrect->height); y++) {
      int ofs = (y * fbi->fb_width + udrect->x) * fb_bypp;
      for (x = udrect->x; x < (udrect->x + udrect->width); x++) {
        uint32_t pixval =
          encode_pixval(rcon, &fbi->framebuffer[ofs], pixf, fbpixf);
        data += wpProc(data, pixval);
        ofs += fb_bypp;
      }
    }
    AsyncManager_WriteSync(rcon->handle, reply, data - reply);
    data = reply;
  }
  return;
}

#ifndef NO_ZLIB
/*
 * ----------------------------------------------------------------------------
 * srv_fb_encode_update_zrle
 * 	create zrle encoded update request.
 * ----------------------------------------------------------------------------
 */
static inline void
srv_fb_encode_update_zrle(RfbConnection * rcon) {
  FrameBufferInfo *fbi = rcon->fbi;
  PixelFormat *pixf = &rcon->pixfmt;
  PixelFormat *fbpixf = &fbi->pixfmt;
  int con_bypp = pixf->bits_per_pixel >> 3;
  int fb_bypp = fbi->pixfmt.bits_per_pixel >> 3;
  uint8_t *tileBuf = alloca(1 + 64 * 64 * 5);
  uint8_t *tileBufEnd;
  int lengthP;
  int ofs;
  int x0, y0, x1, y1, y;
  RLEncoder *rle = &rcon->rle;
  z_stream *zs = &rcon->zs;
  rcon->obuf_wp = add_update_header(rcon->obuf, rcon->udrect_wp - rcon->udrect_rp);
  while (rcon->udrect_rp < rcon->udrect_wp) {
    UpdateRectangle *udrect = &rcon->udrect_fifo[rcon->udrect_rp % UDRECT_FIFOSIZE];
    /* fprintf(stderr,"Udrect height %d, width %d\n",udrect->height,udrect->width); */
    rcon->udrect_rp++;
    write16be(rcon->obuf + rcon->obuf_wp, udrect->x);
    write16be(rcon->obuf + rcon->obuf_wp + 2, udrect->y);
    write16be(rcon->obuf + rcon->obuf_wp + 4, udrect->width);
    write16be(rcon->obuf + rcon->obuf_wp + 6, udrect->height);
    write32be(rcon->obuf + rcon->obuf_wp + 8, ENC_ZRLE);
    rcon->obuf_wp += 12;
    if (con_bypp == 4) {
      con_bypp = 3;
    }
    lengthP = rcon->obuf_wp;
    rcon->obuf_wp += 4;
    zs->next_out = rcon->obuf + rcon->obuf_wp;
    zs->avail_out = rcon->obuf_size - rcon->obuf_wp;
    zs->total_out = 0;
    dbgprintf("con bypp %d, bits per pixel %d\n", con_bypp, pixf->bits_per_pixel);
    int swap = (pixf->big_endian_flag != fbpixf->big_endian_flag);
    for (y0 = 0; y0 < udrect->height; y0 += 64) {
      for (x0 = 0; x0 < udrect->width; x0 += 64) {
        /* One Tile */
        tileBufEnd = tileBuf;
        *tileBufEnd++ = 128;	/* Plain RLE */
        rle_init(rle);
        for (y1 = 0; (y1 < 64) && ((y0 + y1) < udrect->height); y1++) {
          y = y0 + y1 + udrect->y;
          ofs = (y * fbi->fb_width + (x0 + udrect->x)) * fb_bypp;
          for (x1 = 0; (x1 < 64) && ((x1 + x0) < udrect->width); x1++) {
            uint32_t pixval =
              encode_pixval(rcon, &fbi->framebuffer[ofs],
                pixf, fbpixf);
            tileBufEnd +=
              rle_add_pixval(rle, con_bypp, swap, tileBufEnd,
                pixval);
            ofs += fb_bypp;
          }
        }
        //fprintf(stderr,"Tile at %d %d, %d %d\n",x0+udrect->x,y0+udrect->y,x1,y1);
        tileBufEnd += rle_flush(rle, con_bypp, swap, tileBufEnd);
        zs->next_in = tileBuf;
        zs->avail_in = tileBufEnd - tileBuf;
        deflate(zs, Z_SYNC_FLUSH);
        if (zs->avail_out < 32768) {
          uint8_t *obuf;
          rcon->obuf_size *= 2;
          obuf = realloc(rcon->obuf, rcon->obuf_size);
          if (!obuf) {
            fprintf(stderr, "RFB: Not enough memory\n");
            return;
          }
          rcon->obuf = obuf;
          zs->next_out = rcon->obuf + rcon->obuf_wp + zs->total_out;
          zs->avail_out =
            rcon->obuf_size - (rcon->obuf_wp + zs->total_out);
        }
      }
    }
    //fprintf(stderr,"total out %lu av out %lu bpp %d bytes %d\n",zs->total_out,zs->avail_out,fbpixf->bits_per_pixel,con_bypp);
    write32be(rcon->obuf + lengthP, zs->total_out);
    rcon->obuf_wp += zs->total_out;
    AsyncManager_WriteSync(rcon->handle, rcon->obuf, rcon->obuf_wp);
    rcon->obuf_wp = 0;
  }
  return;
}
#endif

/*
 * ----------------------------------------------------------------
 * srv_fb_update
 *	Calls the encoding specific framebuffer update proc
 * ----------------------------------------------------------------
 */

static inline void
srv_fb_update(RfbConnection * rcon) {
  if ((rcon->state != CONSTAT_IDLE)) {
    return;
  }
  dbgprintf("SRV update display enc %d\n",rcon->current_encoding); // jk
  switch (rcon->current_encoding) {
  case ENC_RAW:
    srv_fb_encode_update_raw(rcon);
    break;
#ifndef NO_ZLIB
  case ENC_ZRLE:
    srv_fb_encode_update_zrle(rcon);
    break;
#endif
  case ENC_HEXTILE:
  default:
    fprintf(stderr, "encoding %d not implemented\n", rcon->current_encoding);
  }
}

/*
 * -------------------------------------------------------------------------
 * trigger_fb_update
 *	Start an update if there is something in the rectangle fifo
 * -------------------------------------------------------------------------
 */
static inline void
trigger_fb_update(RfbConnection * rcon) {
  if ((rcon->udrect_wp != rcon->udrect_rp) && rcon->update_outstanding) {
    rcon->update_outstanding = 0;
    srv_fb_update(rcon);
  }
}

/*
 * ----------------------------------------------------------------------
 * Expand a color with less than 16 bit to a color with 16 Bit by
 * Repeating the bit pattern
 * ----------------------------------------------------------------------
 */
static int
expand_color_to_16(int color, int bits) {
  int result = 0;
  int bitcount = 16;
  if (!bits) {
    return 0;
  }
  while (bitcount > 0) {
    if (bitcount - bits > 0) {
      result = result | (color << (bitcount - bits));
    } else {
      result = result | (color >> (bits - bitcount));
    }
    bitcount -= bits;
  }
  return result;
}

/*
 * ---------------------------------------------------------------------
 * srv_set_8Bit_color_map_entries
 * 	For 8 Bit Pseudo color a color map has to be sent to the client.
 *	I'm to lazy to find the colors in
 *	the framebuffer. So a rgb332 pixel encoding is used
 * ---------------------------------------------------------------------
 */
static void
srv_set_8bit_color_map_entries(RfbConnection * rcon, PixelFormat * pixf) {
  uint8_t reply[256 * 6 + 100];
  uint8_t *wp;
  int i;
  pixf->red_max = 7;
  pixf->red_shift = 5;
  pixf->green_max = 7;
  pixf->green_shift = 2;
  pixf->blue_max = 3;
  pixf->blue_shift = 0;
  pixfmt_update_bits(pixf);
  pixfmt_update_translation(rcon);
  wp = reply;
  *wp++ = SRV_SET_COLOUR_MAP_ENTRIES;
  *wp++ = 0;
  *wp++ = 0;
  *wp++ = 0;		// first color
  write16be(wp, 256);
  wp += 2;
  for (i = 0; i < 256; i++) {
    uint16_t red = (i >> pixf->red_shift) & pixf->red_max;
    uint16_t green = (i >> pixf->green_shift) & pixf->green_max;
    uint16_t blue = (i >> pixf->blue_shift) & pixf->blue_max;
    red = expand_color_to_16(red, pixf->red_bits);
    green = expand_color_to_16(green, pixf->green_bits);
    blue = expand_color_to_16(blue, pixf->blue_bits);
    write16be(wp, red);
    wp += 2;
    write16be(wp, green);
    wp += 2;
    write16be(wp, blue);
    wp += 2;
  }
  AsyncManager_WriteSync(rcon->handle, reply, wp - reply);
}

static void
clnt_set_pixel_format(RfbConnection * rcon, uint8_t * data, int len) {

  PixelFormat *pixf = &rcon->pixfmt;

  pixf->bits_per_pixel = data[4];
  pixf->depth = data[5];
  pixf->big_endian_flag = !!data[6];
  pixf->true_color_flag = data[7];
  pixf->red_max = read16be(data + 8);
  pixf->green_max = read16be(data + 10);
  pixf->blue_max = read16be(data + 12);
  pixf->red_shift = data[14];
  pixf->green_shift = data[15];
  pixf->blue_shift = data[16];
  pixfmt_update_bits(pixf);
  if ((!pixf->true_color_flag) && (pixf->bits_per_pixel == 8)) {
    srv_set_8bit_color_map_entries(rcon, pixf);
  }
  pixfmt_update_translation(rcon);
  dbgprintf("got set pixelformat message\n");
  dbgprintf("msgtype %02x\n", data[0]);
  dbgprintf("bpp    %d\n", data[4]);
  dbgprintf("depth  %d\n", data[5]);
  dbgprintf("bigend  %d\n", data[6]);
  dbgprintf("truecol  %d\n", data[7]);
  dbgprintf("redmax  %d\n", read16be(data + 8));
  dbgprintf("greenmax  %d\n", read16be(data + 10));
  dbgprintf("bluemax  %d\n", read16be(data + 12));
  dbgprintf("redshift  %d\n", data[14]);
  dbgprintf("greenshift  %d\n", data[15]);
  dbgprintf("blueshift  %d\n", data[16]);
}

static void
clnt_set_encodings(RfbConnection * rcon, uint8_t * data, int len) {
  int32_t *encodings = (int32_t *)(data + 4);
  int32_t enc;
  int n_encs = read16be(data + 2);
  int i;
  for (i = 0; i < n_encs; i++) {
    enc = ntohl(encodings[i]);
    dbgprintf("enc %d\n", enc);
    switch (enc) {
#ifndef NO_ZLIB
    case ENC_ZRLE:
      rcon->current_encoding = ENC_ZRLE;
      dbgprintf("Switched to encoding %d\n", rcon->current_encoding);
      return;
#endif
    case ENC_RAW:
      rcon->current_encoding = ENC_RAW;
      return;

    case ENC_COPYRECT:
    case ENC_RRE:
    case ENC_CORRE:
    case ENC_HEXTILE:
    case ENC_ZLIB:
    case ENC_TIGHT:
    case ENC_ZLIBHEX:

    case ENC_CURSOR_PSEUDO:
    case ENC_DESKTOP_SIZE:
    default:
      dbgprintf("Encoding %d not implemented\n", enc);
      break;
    }
  }
}

static void
write_udrect_to_fifo(RfbConnection * rcon, UpdateRectangle * udrect) {
  UpdateRectangle *u;
  FrameBufferInfo *fbi = rcon->fbi;
  if (((udrect->y + udrect->height) > fbi->fb_height) ||
    ((udrect->x + udrect->width) > fbi->fb_width)) {
    fprintf(stderr, "RFB-Server received update outside of window\n");
    return;
  }

  if ((rcon->udrect_wp - rcon->udrect_rp + 1) >= UDRECT_FIFOSIZE) {
    /* Fifo full, queue a complete screen update instead */
    rcon->udrect_wp = rcon->udrect_rp;
    u = &rcon->udrect_fifo[rcon->udrect_wp % UDRECT_FIFOSIZE];
    u->x = 0;
    u->y = 0;
    u->width = fbi->fb_width;
    u->height = fbi->fb_height;
    rcon->udrect_wp++;
  } else {
    u = &rcon->udrect_fifo[rcon->udrect_wp % UDRECT_FIFOSIZE];
    *u = *udrect;
    rcon->udrect_wp++;
  }
}

/*
 ***********************************************************
 * Update request from Client
 ***********************************************************
 */
static void
clnt_fb_update_req(RfbConnection * rcon, uint8_t * data, int len) {
  uint8_t incremental;
  UpdateRectangle udrect;
  incremental = data[1];
  udrect.x = read16be(data + 2);
  udrect.y = read16be(data + 4);
  udrect.width = read16be(data + 6);
  udrect.height = read16be(data + 8);
  dbgprintf("Got updaterequest from Client\n");
  if (!incremental) {
    write_udrect_to_fifo(rcon, &udrect);
    rcon->update_outstanding = 1;
    trigger_fb_update(rcon);
  } else {
    dbgprintf("INCREMENTAL UDRQ x %d y %d w %d h %d\n",udrect.x,udrect.y,udrect.width,udrect.height);
    rcon->update_outstanding = 1;
    trigger_fb_update(rcon);
  }
}

#ifndef NO_KEYBOARD
static void
clnt_key_event(RfbConnection * rcon, uint8_t * data, int len) {
  struct KeyEvent kev;
  RfbServer *rfbserv = rcon->rfbserv;
  kev.down = data[1];
  kev.key = read32be(data + 4);
  Keyboard_SendEvent(&rfbserv->keyboard, &kev);
  //fprintf(stderr,"Got key event %08x\n",kev.key);
}
#endif

#ifndef NO_MOUSE
static void
clnt_pointer_event(RfbConnection * rcon, uint8_t * data, int len) {
  RfbServer *rfbserv = rcon->rfbserv;
  struct MouseEvent mev;
  uint8_t buttonMask;
  uint16_t x, y;
  buttonMask = data[1];
  x = read16be(data + 2);
  y = read16be(data + 4);
  /* do something */
  mev.x = x;
  mev.y = y;
  mev.eventMask = buttonMask;
  Mouse_SendEvent(&rfbserv->mouse, &mev);
  //printf("mask %02x, x %u, y %u\n", button_mask, x, y);
}
#endif

static void
clnt_client_cut_text(RfbConnection * rcon, uint8_t * data, int len) {
  uint32_t length;
  length = read32be(data + 4);
  /* do something */
}

/*
 * -------------------------------------------------------------------
 * decode_message
 * 	The message decoder becomes active as soon as Connection
 * 	initialization sequence is done
 * -------------------------------------------------------------------
 */
static int
decode_message(RfbConnection * rcon, uint8_t * data, int len) {
  int opcode;
  int required_len;
  if (len < 4) {
    return 4;
  }
  opcode = data[0];

  dbgprintf("RFB: Decode Msg %d\n", opcode);
  switch (opcode) {
  case CLNT_SET_PIXEL_FORMAT:
    required_len = 20;
    if (len < required_len) {
      return required_len;
    }
    clnt_set_pixel_format(rcon, rcon->ibuf, rcon->ibuf_wp);
    break;

  case CLNT_SET_ENCODINGS:
    required_len = 4 + (ntohs(*(uint16_t *)(data + 2)) << 2);
    if (len < required_len) {
      return required_len;
    }
    clnt_set_encodings(rcon, rcon->ibuf, rcon->ibuf_wp);
    break;

  case CLNT_FB_UPDATE_REQ:
    required_len = 10;
    if (len < required_len) {
      return required_len;
    }
    clnt_fb_update_req(rcon, rcon->ibuf, rcon->ibuf_wp);
    break;

  case CLNT_KEY_EVENT:
    required_len = 8;
    if (len < required_len) {
      return required_len;
    }
#ifndef NO_KEYBOARD
    clnt_key_event(rcon, rcon->ibuf, rcon->ibuf_wp);
#endif
    break;

  case CLNT_POINTER_EVENT:
    required_len = 6;
    if (len < required_len) {
      return required_len;
    }
#ifndef NO_MOUSE
    clnt_pointer_event(rcon, rcon->ibuf, rcon->ibuf_wp);
#endif
    break;

  case CLNT_CLIENT_CUT_TEXT:
    if (len >= 8) {
      required_len = 8 + ntohl(*(uint32_t *)(data + 4));
    } else {
      required_len = 8;
    }
    if (len < required_len) {
      return required_len;
    }
    clnt_client_cut_text(rcon, rcon->ibuf, rcon->ibuf_wp);
    break;
  default:
    fprintf(stderr, "RFBServer got invalid opcode %02x from client\n", opcode);
    break;
  }
  return 0;
}

static int
rfbcon_handle_message(RfbConnection * rcon) {
  int result;
  switch (rcon->state) {
  case CONSTAT_PROTO_WAIT:
    if (CheckProtocolVersion(rcon) < 0) {
      //close
    }
    dbgprintf("RFB: Sending Auth\n");
    Msg_Auth(rcon);
    rcon->state = CONSTAT_SHARED_WAIT;
    rcon->ibuf_expected = 1;
    return 0;
    break;

  case CONSTAT_SHARED_WAIT:
    // handle shared flag is missing here (close other cons)
    dbgprintf("RFB: Got Shared\n");
    Msg_ServerInitialisation(rcon);
    /*
     * Clear Incomming buffer before going to idle to remove
     * outstanding requests with wrong depth ????
     */
    rcon->state = CONSTAT_IDLE;
    dbgprintf("RFB: IDLE now\n");
    rcon->ibuf_expected = 4;
    break;

    /* This is the normal operating state */
  case CONSTAT_IDLE:
    result = decode_message(rcon, rcon->ibuf, rcon->ibuf_wp);
    if (result > rcon->ibuf_wp) {
      rcon->ibuf_expected = result;
      return result;
    } else {
      rcon->ibuf_expected = 4;
    }
    break;
  default:
    fprintf(stderr, "RFBCon Invalid state %d\n", rcon->state);
    break;
  }
  return 0;
}

/*
 * -----------------------------------------------------------------
 * RFB input event handler
 *	read data from socket and assemble into an input buffer.
 *	check if inputbuffer is a complete message
 * -----------------------------------------------------------------
 */
static void
rfbcon_input(StreamHandle_t *handle, const void *buf, signed long len, void *clientdata) {
  RfbConnection *rcon = clientdata;
  const char *p = buf;
  int count;
  if (len < 0) {
    perror("error reading from socket");
    rfbsrv_disconnect(rcon);
    return;
  }
  if (len == 0) {
    dbgprintf("socket: end of file\n");
    rfbsrv_disconnect(rcon);
    return;
  }
  while (len > 0) {
    count = rcon->ibuf_expected - rcon->ibuf_wp;
    if (count > len) {
      count = len;
    }
    if (sizeof(rcon->ibuf) < (rcon->ibuf_wp + count)) {
      fprintf(stderr, "rfbserver: input buffer overflow. \n");
      /* Maybe it would be better to close connection here */
      rcon->ibuf_wp = 0;
    }
    memcpy(&rcon->ibuf[rcon->ibuf_wp], p, count);
    rcon->ibuf_wp += count;
    len -= count;
    p += count;
    /* check if buffer is a complete message */
    if (rcon->ibuf_wp >= rcon->ibuf_expected) {
      int result;
      result = rfbcon_handle_message(rcon);
      if (result == 0) {
        rcon->ibuf_wp = 0;
      }
    }
  }
  return;
}

/*
 * --------------------------------------------------------------------------
 * The socket connect event handler
 * --------------------------------------------------------------------------
 */
static void
rfbsrv_accept(int status, StreamHandle_t *handle, const char *host, int port, void *clientdata) {
  RfbServer *rfbserv = (RfbServer *)clientdata;
  RfbConnection *rcon;
  rcon = sg_new(RfbConnection);
  rcon->current_encoding = -1;	/* Hope this doesnt exist */
  rcon->handle = handle;
  rcon->rfbserv = rfbserv;
  rcon->next = rfbserv->con_head;
  rcon->fbi = &rfbserv->fbi;
  rcon->obuf_size = 65536;
  rcon->obuf = sg_calloc(rcon->obuf_size);

#ifndef NO_ZLIB
  rcon->zs.zalloc = Z_NULL;
  rcon->zs.zfree = Z_NULL;
  rcon->zs.opaque = Z_NULL;
  if (deflateInit(&rcon->zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
    fprintf(stderr, "Decompressor init failed\n");
  }
#endif

  /* Default Pixel format is framebuffers pixelformat */
  memcpy(&rcon->pixfmt, &rfbserv->fbi.pixfmt, sizeof(PixelFormat));
  pixfmt_update_translation(rcon);
  rfbserv->con_head = rcon;
  rcon->current_encoding = ENC_RAW;
  AsyncManager_ReadStart(handle, &rfbcon_input, rcon);
  Msg_ProtocolVersion(rcon);
  rcon->ibuf_expected = 12;	/* Expecting Protocol version reply */
  rcon->state = CONSTAT_PROTO_WAIT;
  rcon->protoversion = 0x00030003;
  return;
}

/*
 * -----------------------------------------------------------------------
 * rfbserv_set_fbformat
 *	Set the framebuffer format. Called by a user of the display
 *	(For example a LCD controller emulator) to tell the rfbserver
 *	about memory layout.
 * -----------------------------------------------------------------------
 */

static void
rfbserv_set_fbformat(struct FbDisplay *fbdisp, FbFormat * fbf) {
  int fb_size;
  RfbServer *rfbserv = fbdisp->owner;
  FrameBufferInfo *fbi = &rfbserv->fbi;
  PixelFormat *pixf = &fbi->pixfmt;
  if ((fbf->red_bits > 8) || (fbf->green_bits > 8) || (fbf->blue_bits > 8)) {
    fprintf(stderr,
      "Framebuffer format with more than 8 Bit per color not supported\n");
    exit(1);
  }
  pixf->red_max = (1 << fbf->red_bits) - 1;
  pixf->red_bits = fbf->red_bits;
  pixf->red_shift = fbf->red_shift;

  pixf->green_max = (1 << fbf->green_bits) - 1;
  pixf->green_bits = fbf->green_bits;
  pixf->green_shift = fbf->green_shift;

  pixf->blue_max = (1 << fbf->blue_bits) - 1;
  pixf->blue_bits = fbf->blue_bits;
  pixf->blue_shift = fbf->blue_shift;

  /* Should come from the LCD controller */
  pixf->bits_per_pixel = fbf->bits_per_pixel;
  pixf->bypp = (pixf->bits_per_pixel + 7) / 8;
  pixf->depth = fbf->depth;
  pixf->true_color_flag = 1;
  fb_size = fbi->fb_width * fbi->fb_height * pixf->bypp;
  fbi->fb_linebytes = fbi->fb_width * pixf->bypp;
  if (fb_size != fbi->fb_size) {
    fbi->fb_size = fb_size;
    fbi->framebuffer = realloc(fbi->framebuffer, fbi->fb_size);
    if (!fbi->framebuffer) {
      fprintf(stderr, "OOM: realloc of framebuffer %d bytes failed\n",
        fbi->fb_size);
      exit(1);
    }
  }
  RfbConnection *rcon;
  for (rcon = rfbserv->con_head; rcon; rcon = rcon->next) {
    pixfmt_update_translation(rcon);
  }

}

/*
 * ----------------------------------------------------------------
 * rfbserv_update_display
 *	Called for example from a LCD controller emulator
 *	when it detects some change in framebuffer memory
 * ----------------------------------------------------------------
 */
static int
rfbserv_update_display(struct FbDisplay *fbdisp, FbUpdateRequest * fbudreq) {
  RfbServer *rfbserv = fbdisp->owner;
  RfbConnection *rcon;
  UpdateRectangle udrect;
  FrameBufferInfo *fbi = &rfbserv->fbi;
  unsigned int start = fbudreq->offset;
  unsigned int count = fbudreq->count;
  if (start > fbi->fb_size) {
    return -1;
  }
  if (start + count > fbi->fb_size) {
    count = fbi->fb_size - start;
  }
  /*
   * Would be better to check the differences before overwriting
   * also comparing differences should be delayed to time of sending the
   * the data in a fileevent handler
   */
  memcpy(fbi->framebuffer + start, fbudreq->fbdata, count);
  udrect.x = 0;
  udrect.y = (start / fbi->fb_linebytes);
  udrect.width = fbi->fb_width;
  udrect.height = (count + fbi->fb_linebytes - 1) / (fbi->fb_linebytes);
  if (udrect.height + udrect.y > fbi->fb_height) {
    udrect.height = fbi->fb_height - udrect.y;
  }
#if 0
  fprintf(stderr, "Got update request from LCD controller x %d ,y %d, w %d h %d\n", udrect.x,
    udrect.y, udrect.width, udrect.height);
  fprintf(stderr, "linebytes %d count %d\n", fbi->fb_linebytes, count);
#endif
  for (rcon = rfbserv->con_head; rcon; rcon = rcon->next) {
    write_udrect_to_fifo(rcon, &udrect);
    trigger_fb_update(rcon);
  }
  return 0;
}

/*
 *****************************************************************************
 * RfbServer_New
 * 	Create a new rfbserver. The rfbserver implements a display, a keyboard and a mouse.
 *****************************************************************************
 */
void
RfbServer_New(const char *name, FbDisplay ** displayPP, Keyboard ** keyboardPP, Mouse **mousePP) {
  int result;
  RfbServer *rfbserv;
  FrameBufferInfo *fbi;
  PixelFormat *pixf;
  uint32_t width, height;
  uint32_t port;
  char *host = Config_ReadVar(name, "host");
#ifndef NO_STARTCMD
  int softgunpid;
  char *startcmd = Config_ReadVar(name, "start");
#endif
  if (displayPP) {
    *displayPP = NULL;
  }
  if (keyboardPP) {
    *keyboardPP = NULL;
  }
  if (mousePP) {
    *mousePP = NULL;
  }
  if (Config_ReadUInt32(&port, name, "port") < 0) {
    return;
  }
  if (Config_ReadUInt32(&width, name, "width") < 0) {
    return;
  }
  if (Config_ReadUInt32(&height, name, "height") < 0) {
    return;
  }
  if (!host) {
    host = "127.0.0.1";
  }
  /* Sanity check for width/height is missing here */

  rfbserv = sg_new(RfbServer);
  rfbserv->exit_on_close = 0;
  rfbserv->propose_bpp = -1;
  Config_ReadInt32(&rfbserv->propose_bpp, name, "propose_bpp");
  Config_ReadUInt32(&rfbserv->exit_on_close, name, "exit_on_close");
  fbi = &rfbserv->fbi;

  fbi->fb_width = width;
  fbi->fb_height = height;
  fbi->fb_linebytes = fbi->fb_width * 2;
  fbi->fb_size = fbi->fb_width * fbi->fb_height * 2;
  fbi->framebuffer = sg_calloc(fbi->fb_size);
  sprintf(fbi->name_string, "%s %s", "softgun", name);

  pixf = &fbi->pixfmt;
  pixf->bits_per_pixel = 16;
  pixf->bypp = 2;
  pixf->depth = 12;

  if (BYTE_ORDER_NATIVE == BYTE_ORDER_BIG) {
    pixf->big_endian_flag = 1;
  } else if (BYTE_ORDER_NATIVE == BYTE_ORDER_LITTLE) {
    pixf->big_endian_flag = 0;
  } else {
    fprintf(stderr, "No byteorder detected\n");
    exit(1);
  }
  pixf->true_color_flag = 1;
  pixf->red_max = 15;
  pixf->blue_max = 15;
  pixf->green_max = 15;
  pixf->red_shift = 8;
  pixf->green_shift = 4;
  pixf->blue_shift = 0;
  pixfmt_update_bits(pixf);

  result = AsyncManager_InitTcpServer(host, port, 5, 1, &rfbsrv_accept, rfbserv);
  if (result < 0) {
    sg_free(fbi->framebuffer);
    sg_free(rfbserv);
    fprintf(stderr, "Can not create RFB server\n");
    return;
  }
  fprintf(stderr, "RFB Server Listening on host \"%s\" port %d\n", host, port);
  rfbserv->display.owner = rfbserv;
  rfbserv->display.fbUpdateRequest = rfbserv_update_display;
  rfbserv->display.setFbFormat = rfbserv_set_fbformat;
  rfbserv->display.name = sg_strdup(name);
  rfbserv->display.width = width;
  rfbserv->display.height = height;
#ifndef NO_KEYBOARD
  if (keyboardPP) {
    *keyboardPP = &rfbserv->keyboard;
  }
#endif
  if (displayPP) {
    *displayPP = &rfbserv->display;
  }
#ifndef NO_MOUSE
  if (mousePP) {
    *mousePP = &rfbserv->mouse;
  }
#endif
#ifndef NO_STARTCMD
  softgunpid = getpid();
  if (startcmd && !(rfbserv->viewerpid = fork())) {
    /* Shit this inherits the atexit commands currently */
    int maxfd = getdtablesize();
    int retcode;
    int fd;
    struct timespec ts_start;
    struct timespec ts_exit;

    for (fd = 3; fd < maxfd; fd++) {
      close(fd);
    }
    result = dup(2);	/* keep stderr as fd 3 */
    close(0);
    close(1);
    close(2);
    if (open("/dev/null", O_RDWR) != 0) {
      /* Error message does not work because stderr is closed */
      exit(1);
    }
    result = dup(0);
    result = dup(0);
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    retcode = system(startcmd);
    clock_gettime(CLOCK_MONOTONIC, &ts_exit);
    /* restore stderr */
    close(2);
    result = dup(3);
    if ((retcode != 0) && (ts_exit.tv_sec <= ts_start.tv_sec + 2)) {
      fprintf(stderr, "*** VNC server exited with code: %d\n", retcode);
      fprintf(stderr, "*** Please check your config file:\n");
      fprintf(stderr, "*** \"%s\"\n", startcmd);
    }
    close(2);
    /* If an error happens during the first 1-2 seconds exit */
    if ((retcode != 0) && (ts_exit.tv_sec <= ts_start.tv_sec + 1)) {
      kill(softgunpid, SIGTERM);
    }
    exit(0);
  }
#endif
}
