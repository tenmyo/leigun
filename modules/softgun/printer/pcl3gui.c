/*
 *************************************************************************************************
 * PCL3GUI Interpreter 
 *
 * state: working with hplip dj9xxvip 
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "pcl3gui.h"
#include "printengine.h"
#include "decompress.h"
#include "sgstring.h"

#define STATE_IDLE      (0)
#define STATE_CMD       (1)
#define STATE_DATA	(2)

#define MAX_RASTERINFOS (10)
typedef struct RasterInfo {
	int xdpi;
	int ydpi;
	int compression_method;
	int bits_per_component;
	int planes_per_component;
	uint8_t *decomp_buf;
	int decomp_bufsize;
	int maxdatalen;
	int seed_reset_val;
} RasterInfo;

struct PCL3GUI_Interp {
	PrintEngine *print_engine;
	int state;
	int cmd_wp;
	char cmdbuf[100];
	uint8_t *databuf;
	/* Buffer for data belonging to a command */
	int databuf_size;
	int data_expected;
	int data_wp;
	void (*dataHandler) (PCL3GUI_Interp * interp);

	/* PCL state variables */
	int current_component;	/* Component counter */
	int current_plane;	/* Plane counter */
	int colorspace;
	int current_line;
	/* Buffer for assembling a raster line */
	int max_xdpi;
	uint8_t *linebuf;
	int linebuf_size;
	int raster_components;
	RasterInfo rasterInfo[MAX_RASTERINFOS];
};

/*
 * -----------------------------------------------------------------------
 * KtoRGB only sets pixels to black, so memory must be set to 0xff
 * if they shall appear white
 * -----------------------------------------------------------------------
 */
static int
KtoRGB(uint8_t * dst, uint8_t * kbuf, int kbytes)
{
	int i;
	int dst_wp = 0;
	for (i = 0; i < kbytes * 8; i++) {
		int bit = (i & 7) ^ 7;
		if (kbuf[i >> 3] & (1 << bit)) {
			dst[dst_wp++] = 0;
			dst[dst_wp++] = 0;
			dst[dst_wp++] = 0;
		} else {
			dst_wp += 3;
		}
	}
	return dst_wp;
}

static int
MergeRGB(uint8_t * dst, uint8_t * src, int rgbbytes)
{
	int i;
	int dst_wp = 0;
	for (i = 0; i < rgbbytes; i++) {
		dst[i] = dst[i] & src[i];
	}
	return dst_wp;
}

/*
 * --------------------------------------------------------------------------------------
 * Assemble a raster into the outgoing linebuffer from single Raster data planes 
 * --------------------------------------------------------------------------------------
 */
static void
assemble_raster(PCL3GUI_Interp * interp, RasterInfo * ri)
{
	int leftborder = ((int)(0.23622 * ri->xdpi)) * 3;
	int datalen;

	if (ri->compression_method == 9) {
		datalen =
		    Mode9_Decompress(ri->decomp_buf, interp->databuf, ri->decomp_bufsize,
				     interp->data_wp);
		if (datalen > ri->maxdatalen) {
			ri->maxdatalen = datalen;
		}
		if (datalen >= 0) {
			KtoRGB(interp->linebuf + leftborder, ri->decomp_buf, ri->maxdatalen);
		} else {
			fprintf(stderr, "Decompress failed with %d\n", datalen);
		}
	} else if (ri->compression_method == 10) {
		datalen =
		    Mode10_Decompress(ri->decomp_buf, interp->databuf, ri->decomp_bufsize,
				      interp->data_wp);
		if (datalen > ri->maxdatalen) {
			ri->maxdatalen = datalen;
		}
		MergeRGB(interp->linebuf + leftborder, ri->decomp_buf, ri->maxdatalen);
	} else {
		fprintf(stderr, "Decompressor for mode %d not implemented\n",
			ri->compression_method);
		return;
	}
}

/*
 * ---------------------------------------------------------
 * start_raster_graphics 
 *	Never checked if the real printer requires this at
 *	all in PCL3GUI mode
 * ---------------------------------------------------------
 */
static void
start_raster_graphics(PCL3GUI_Interp * interp)
{
	interp->linebuf_size = 3 * interp->max_xdpi * 21;
	interp->linebuf = realloc(interp->linebuf, interp->linebuf_size);
	memset(interp->linebuf, 0xff, interp->linebuf_size);
}

/*
 * ---------------------------------------------------------
 * send_empty_line 
 *	Send a raster line to the PrintEngine 
 * ---------------------------------------------------------
 */
static void
send_empty_line(PCL3GUI_Interp * interp)
{
	interp->current_plane = 0;
	interp->current_component = 0;
	memset(interp->linebuf, 0xff, interp->linebuf_size);
	/* dpi is totally nonsense */
	PEng_SendRasterData(interp->print_engine, interp->max_xdpi, interp->max_xdpi,
			    interp->linebuf, 8.26 * interp->max_xdpi);
	interp->current_line++;
}

static void
transfer_by_row(PCL3GUI_Interp * interp)
{
	RasterInfo *ri;
	if (interp->current_component >= interp->raster_components) {
		fprintf(stderr, "No raster component found to store the row\n");
		interp->current_plane = 0;
		interp->current_component = 0;
		return;
	}
	ri = &interp->rasterInfo[interp->current_component];
	assemble_raster(interp, ri);
	/* dpi is totally nonsense */
	PEng_SendRasterData(interp->print_engine, ri->xdpi, ri->ydpi, interp->linebuf,
			    8.26 * ri->xdpi);
	interp->current_line++;
	// send raster data
	interp->current_plane = 0;
	interp->current_component = 0;
	memset(interp->linebuf, 0xff, interp->linebuf_size);
}

static void
transfer_by_plane(PCL3GUI_Interp * interp)
{
	RasterInfo *ri;
	if (interp->current_component >= interp->raster_components) {
		fprintf(stderr, "No raster component found to store the plane %d\n",
			interp->current_component);
		return;
	}
	ri = &interp->rasterInfo[interp->current_component];
	assemble_raster(interp, ri);

	interp->current_plane++;
	if (interp->current_plane >= ri->planes_per_component) {
		interp->current_plane = 0;
		interp->current_component++;
	}
}

/*
 * --------------------------------------------------------------------------
 * Reset seed row: Set the seedrows of all Raster infos to the default value
 * --------------------------------------------------------------------------
 */
static void
reset_seed_row(PCL3GUI_Interp * interp)
{
	RasterInfo *ri;
	int i;
	for (i = 0; i < interp->raster_components; i++) {
		ri = &interp->rasterInfo[i];
		ri->maxdatalen = 0;
		memset(ri->decomp_buf, ri->seed_reset_val, ri->decomp_bufsize);
	}
}

/*
 * -------------------------------------------------------------
 * Configure raster data (found in PCL implementorsguide)
 * Unfortunately Format 6 (VIPcrdFormat) is missing there
 * -------------------------------------------------------------
 */
static void
configure_raster_data(PCL3GUI_Interp * interp)
{
	int format, id, components;
	int i;
	uint8_t *data = interp->databuf;
	if (interp->data_wp < 4) {
		fprintf(stderr, "Configure raster data command to small\n");
		return;
	}
	format = *data++;
	id = *data++;
	data++;
	components = *data++;
	if ((format != 6) && (id != 0x1f)) {
		fprintf(stderr, "Configure raster data with unimplemented format/ID\n");
		return;
	}
	if (interp->data_wp < (4 + 8 * components)) {
		fprintf(stderr, "Configure raster data: not enough data for components\n");
		return;
	}
	if (components > MAX_RASTERINFOS) {
		fprintf(stderr, "To many raster components\n");
		return;
	}
	interp->max_xdpi = 0;
	for (i = 0; i < components; i++) {
		RasterInfo *ri = &interp->rasterInfo[i];
		int pixel_major_code;
		ri->xdpi = *data++ << 8;
		ri->xdpi |= *data++;
		ri->ydpi = *data++ << 8;
		ri->ydpi |= *data++;
		ri->compression_method = *data++;
		pixel_major_code = *data++;	/* Pixel major code */
		ri->bits_per_component = *data++;
		ri->planes_per_component = *data++;
		if (ri->xdpi > interp->max_xdpi) {
			interp->max_xdpi = ri->xdpi;
		}
		ri->decomp_bufsize = ri->xdpi * 3 * 20;
		ri->decomp_buf = realloc(ri->decomp_buf, ri->decomp_bufsize);

		/*
		 * No clue where the printer knows the colorspace from
		 */
		if (ri->bits_per_component > 1) {
			ri->seed_reset_val = 0xff;
		} else {
			ri->seed_reset_val = 0;
		}
		ri->maxdatalen = 0;
		memset(ri->decomp_buf, ri->seed_reset_val, ri->decomp_bufsize);
		//fprintf(stderr,"New component with %d:%d dpi\n",ri->xdpi,ri->ydpi);
	}
	interp->raster_components = components;
}

/*
 * ------------------------------------------------------------------------------
 * ConfigureImageData
 * 	Don't know it this is implemented at all for the PCL3GUI interpreter,
 * 	looks as it has only effect in PCL3 mode
 * ------------------------------------------------------------------------------
 */
static void
configure_image_data(PCL3GUI_Interp * interp)
{
	int colorspace;
	int bits_per_index;
	int pixel_encoding;
	int p1_bits, p2_bits, p3_bits;
	if (interp->data_wp < 6) {
		fprintf(stderr, "Configure image data requires at least 6 data bytes, got %d\n",
			interp->data_wp);
		return;
	}
	interp->colorspace = colorspace = interp->databuf[0];
	if ((colorspace != 2) && (colorspace != 0)) {
		fprintf(stderr, "Non RGB colorspaces are not implemented\n");
	}
	pixel_encoding = interp->databuf[1];
	bits_per_index = interp->databuf[2];
	if (pixel_encoding != 3) {	/* !8bpp */
		fprintf(stderr, "Warning: Only Pixel encoding 8 Bit implemented\n");
	}
	p1_bits = interp->databuf[3];
	p2_bits = interp->databuf[4];
	p3_bits = interp->databuf[5];
	if ((p1_bits != 8) || (p2_bits != 8) || (p3_bits != 8)) {
		fprintf(stderr, "Warning, only 8 Bits per primary supported\n");
	}

}

/*
 * -----------------------------------------------------------------------------
 * return value is <0 if no PCL command was recognized (may be incomplete)
 * else the return value is the number of databytes which will follow
 * -----------------------------------------------------------------------------
 */
static int
eval_pcl3gui_cmd(PCL3GUI_Interp * interp, int *done)
{
	int length;
	int mediasize;
	int mediatype;
	int quality;
	int xresolution;
	int pix_per_row;
	int row;
	char tmp;
	char *cmd = interp->cmdbuf;
	if (sscanf(cmd, "*b%d%[V]", &length, &tmp) == 2) {
		/* Transfer by plane */
		sscanf(cmd, "*b%d", &length);
		interp->dataHandler = transfer_by_plane;
		return length;
	} else if (sscanf(cmd, "*b%d%[W]", &length, &tmp) == 2) {
		/* Transfer by row */
		sscanf(cmd, "*b%d", &length);
		interp->dataHandler = transfer_by_row;
		return length;
	} else if (sscanf(cmd, "*b%d%[Y]", &row, &tmp) == 2) {
		/* Vertical move */
		sscanf(cmd, "*b%d", &row);
		if (row != 0) {
			fprintf(stderr,
				"Non zero argument to vertical movement %d not implemented\n", row);
			return 0;
		}
		reset_seed_row(interp);
		return length;
	} else if (sscanf(cmd, "*p%d%[Y]", &length, &tmp) == 2) {
		/* Goto Line */
		sscanf(cmd, "*p%d", &row);
		while (interp->current_line < row) {
			send_empty_line(interp);
		}
		return 0;
	} else if (cmd[0] == 'E') {
		/* Reset Printer */
		fprintf(stderr, "Exiting from PCL3GUI Interpreter because of Reset command\n");
		PEng_EjectPage(interp->print_engine);
		interp->current_line = 0;
		*done = 1;
		return 0;
	} else if (strcmp("%-12345X", cmd) == 0) {
		/* UEL */
		fprintf(stderr, "Exiting from PCL3GUI Interpreter because of UEL\n");
		PEng_EjectPage(interp->print_engine);
		interp->current_line = 0;
		*done = 1;
		return 0;
	} else if (sscanf(cmd, "*v%d%[W]", &length, &tmp) == 2) {
		/* Configure Image Data */
		sscanf(cmd, "*v%d", &length);
		interp->dataHandler = configure_image_data;
		return length;
	} else if (sscanf(cmd, "*g%d%[W]", &length, &tmp) == 2) {
		/* Configure Raster Data */
		sscanf(cmd, "*g%d", &length);
		interp->dataHandler = configure_raster_data;
		return length;
	} else if (sscanf(cmd, "&l%d%[A]", &mediasize, &tmp) == 2) {
		/* Media size */
		sscanf(cmd, "&l%d", &mediasize);
		fprintf(stderr, "Mediasize %d\n", mediasize);
		if (mediasize == 26) {
			PEng_SetMediaSize(interp->print_engine, PE_MEDIA_SIZE_A4);
		} else if (mediasize == 2) {
			PEng_SetMediaSize(interp->print_engine, PE_MEDIA_SIZE_LETTER);
		}
		return 0;
	} else if (sscanf(cmd, "*r1%[A]", &tmp) == 1) {
		/* Start raster graphics */
		start_raster_graphics(interp);
		return 0;
	} else if (strcmp(cmd, "&l-2H") == 0) {
		/* Media Preload Does not eject the paper ! */
		return 0;
	} else if (sscanf(cmd, "&l%d%[M]", &mediatype, &tmp) == 2) {
		/* Media type */
		sscanf(cmd, "&l%d", &mediatype);
		return 0;
	} else if (sscanf(cmd, "*o%d%[M]", &quality, &tmp) == 2) {
		/* Printint Quality */
		sscanf(cmd, "*o%d", &quality);
		return 0;
	} else if (sscanf(cmd, "&u%d%[D]", &xresolution, &tmp) == 2) {
		/* The real printer does not need this command (tested) */
		sscanf(cmd, "*&u%d", &xresolution);
		return 0;
	} else if (sscanf(cmd, "*r%d%[S]", &pix_per_row, &tmp) == 2) {
		/* Pixels per row: example Esc*r4800S */
		/* The real printer does not need this command (tested) */
		sscanf(cmd, "*r%d", &pix_per_row);
		return 0;
	} else if (strcmp(cmd, "*rbC") == 0) {
		/* End raster Graphics */
		return 0;
	} else {
		return -1;
	}
}

int
PCL3GUIInterp_Feed(PCL3GUI_Interp * interp, uint8_t * buf, int len, int *done)
{
	int count;
	int result;
	*done = 0;
	for (count = 0; count < len; count++) {
		switch (interp->state) {
		    case STATE_IDLE:
			    if (buf[count] == 0x1b) {
				    interp->state = STATE_CMD;
				    interp->cmd_wp = 0;
			    }
			    break;
		    case STATE_CMD:
			    if ((interp->cmd_wp + 1) >= sizeof(interp->cmdbuf)) {
				    interp->cmd_wp = 0;
				    interp->state = STATE_IDLE;
			    } else if (buf[count] == 0x1b) {
				    /* premature end or unimplemented command */
				    fprintf(stderr, "PCL3GUI: Unimplemented Esc%s\n",
					    interp->cmdbuf);
				    interp->cmd_wp = 0;
				    interp->state = STATE_CMD;
			    } else {
				    interp->cmdbuf[interp->cmd_wp++] = buf[count];
				    interp->cmdbuf[interp->cmd_wp] = 0;
			    }
			    result = eval_pcl3gui_cmd(interp, done);
			    if (result < 0) {
				    break;
			    } else if (result == 0) {
				    interp->state = STATE_IDLE;
				    interp->cmd_wp = 0;
				    if (*done == 1) {
					    PEng_EjectPage(interp->print_engine);
					    interp->current_line = 0;
					    return count;
				    }
				    if (interp->dataHandler) {
					    interp->data_wp = 0;
					    interp->dataHandler(interp);
					    interp->dataHandler = 0;
				    }
				    break;
			    } else if (result > 0) {
				    interp->data_expected = result;
				    interp->cmd_wp = 0;
				    interp->state = STATE_DATA;
				    interp->data_wp = 0;
				    if (interp->databuf_size < interp->data_expected) {
					    interp->databuf =
						realloc(interp->databuf, interp->data_expected);
					    if (!interp->databuf) {
						    fprintf(stderr, "Out of memory\n");
						    exit(1);
					    }
				    }
			    }
			    break;

		    case STATE_DATA:
			    //fprintf(stderr,"data %02x, exp %d\n",buf[count],interp->data_expected);
			    interp->databuf[interp->data_wp++] = buf[count];
			    if (interp->data_wp >= interp->data_expected) {
				    interp->state = STATE_IDLE;
				    interp->cmd_wp = 0;
				    if (interp->dataHandler) {
					    interp->dataHandler(interp);
					    interp->dataHandler = 0;
				    }
			    }
			    break;
		}
	}
	return count;
}

void
PCL3GUIInterp_Reset(PCL3GUI_Interp * interp)
{
	PEng_EjectPage(interp->print_engine);
	interp->current_line = 0;
}

PCL3GUI_Interp *
PCL3GUIInterp_New(PrintEngine * pe)
{
	PCL3GUI_Interp *interp = sg_new(PCL3GUI_Interp);
	interp->print_engine = pe;
	return interp;
}
