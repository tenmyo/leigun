#include <stdlib.h>
#include <SDL/SDL.h>
#include "sgstring.h"
#include "fbdisplay.h"
#include "keyboard.h"
#include "configfile.h"
#include "byteorder.h"
#include "cycletimer.h"

#define write16(addr,value) (*(uint16_t*)(addr) = (value))
#define write32(addr,value) (*(uint32_t*)(addr) = (value))

typedef struct PixelFormat {
	uint16_t red_max;
	uint8_t red_bits;	/* redundant, calculated from red_max */
	uint16_t blue_max;
	uint8_t blue_bits;	/* redundant, calculated from blue_max */
	uint16_t green_max;
	uint8_t green_bits;	/* redundant, calculated from green_max */
	uint8_t red_shift;
	uint8_t green_shift;
	uint8_t blue_shift;
	uint32_t bypp;
	uint32_t bits_per_pixel;
	uint32_t depth;
} PixelFormat;

typedef struct UpdateRectangle {
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
} UpdateRectangle;

typedef struct SDLDisplay {
	CycleTimer update_timer;
	FbDisplay display;
	Keyboard keyboard;
	SDL_Surface *screen;
	uint32_t width;
	uint32_t height;
	uint32_t pixels;
	PixelFormat inpixf;

	/* The output pixel format with translation table */
	uint32_t bypp;
	uint32_t trans_red[256];
	uint32_t trans_green[256];
	uint32_t trans_blue[256];
} SDLDisplay;

/*
 *******************************************************
 * This assumes that SDL pixel can be
 * calculated by splitting into colors
 *******************************************************
 */
static void
pixfmt_update_translation(SDLDisplay * sd)
{
	uint32_t red, green, blue;
	SDL_Surface *screen = sd->screen;
	PixelFormat *inpixf = &sd->inpixf;
	for (red = 0; red <= inpixf->red_max; red++) {
		uint32_t trans;
		trans = red * 255 / inpixf->red_max;
		trans = SDL_MapRGB(screen->format, trans, 0, 0);
		sd->trans_red[red] = trans;
	}
	for (green = 0; green <= inpixf->green_max; green++) {
		uint32_t trans;
		trans = green * 255 / inpixf->green_max;
		trans = SDL_MapRGB(screen->format, 0, trans, 0);
		sd->trans_green[green] = trans;
	}
	for (blue = 0; blue <= inpixf->blue_max; blue++) {
		uint32_t trans;
		trans = blue * 255 / inpixf->blue_max;
		trans = SDL_MapRGB(screen->format, 0, 0, trans);
		sd->trans_blue[blue] = trans;
	}
}

static void
SD_SetFbFormat(struct FbDisplay *fd, FbFormat * fbf)
{
	SDLDisplay *sd = fd->owner;
	PixelFormat *inpixf = &sd->inpixf;
	if ((fbf->red_bits > 8) || (fbf->green_bits > 8) || (fbf->blue_bits > 8)) {
		fprintf(stderr,
			"Framebuffer format with more than 8 Bit per color not supported\n");
		exit(1);
	}
	inpixf->red_max = (1 << fbf->red_bits) - 1;
	inpixf->red_bits = fbf->red_bits;
	inpixf->red_shift = fbf->red_shift;

	inpixf->green_max = (1 << fbf->green_bits) - 1;
	inpixf->green_bits = fbf->green_bits;
	inpixf->green_shift = fbf->green_shift;

	inpixf->blue_max = (1 << fbf->blue_bits) - 1;
	inpixf->blue_bits = fbf->blue_bits;
	inpixf->blue_shift = fbf->blue_shift;
	inpixf->bits_per_pixel = fbf->bits_per_pixel;
	inpixf->depth = fbf->depth;
	inpixf->bypp = (fbf->bits_per_pixel + 7) / 8;
	pixfmt_update_translation(sd);

}

static inline uint32_t
encode_pixval(SDLDisplay * sd, void *src, PixelFormat * inpixf)
{
	uint32_t pixval;
	uint8_t red, green, blue;

	switch (fbpixf->bits_per_pixel) {
	    case 32:
		    pixval = BYTE_LeToH32(*(uint32_t *) src);
		    break;
	    case 16:
		    pixval = BYTE_LeToH16(*(uint16_t *) src);
		    break;
	    case 8:
		    pixval = *(uint8_t *) src;
		    break;
	    default:
		    pixval = 0;
	}

	red = (pixval >> inpixf->red_shift) & inpixf->red_max;
	green = (pixval >> inpixf->green_shift) & inpixf->green_max;
	blue = (pixval >> inpixf->blue_shift) & inpixf->blue_max;
	return sd->trans_red[red] | sd->trans_green[green] | sd->trans_blue[blue];
}

/*
 * ---------------------------------------------------------------
 * write a pixel value into a buffer
 * ---------------------------------------------------------------
 */
static inline int
write_pixel(int con_bypp, uint8_t * dst, uint32_t pixval)
{
	switch (con_bypp) {
	    case 1:
		    dst[0] = pixval;
		    return 1;
	    case 2:
		    write16(dst, pixval);
		    return 2;
	    case 3:
		    /* Do not use write32 because of alignment traps */
		    {
			    uint8_t *pix = (uint8_t *) & pixval;
			    dst[0] = pix[0];
			    dst[1] = pix[1];
			    dst[2] = pix[2];
		    }
		    return 3;
	    case 4:
		    write32(dst, pixval);
		    return 4;
	    default:
		    fprintf(stderr, "%d bytes per pixel not implemented\n", con_bypp);
		    return 0;
	}
}

static void
sdl_update_screen(void *clientData)
{
	SDLDisplay *sd = (SDLDisplay *) clientData;
	SDL_Flip(sd->screen);
}

int
SD_UpdateRequest(FbDisplay * fd, FbUpdateRequest * fbudreq)
{

	SDLDisplay *sd = (SDLDisplay *) fd->owner;
	PixelFormat *inpixf = &sd->inpixf;
	unsigned int startpixel = fbudreq->offset / inpixf->bypp;
	unsigned int pixelcount = fbudreq->count / inpixf->bypp;
	unsigned int i;
	unsigned int dst_bypp = sd->bypp;
	uint8_t *src, *dst;
	if (pixelcount + startpixel > sd->width * sd->height * sd->bypp) {
		fprintf(stderr, "Update request outside of screen\n");
		return -1;
	}
	src = fbudreq->fbdata;
	dst = sd->screen->pixels + sd->bypp * startpixel;
	for (i = 0; i < pixelcount; i++) {
		uint32_t enc;
		enc = encode_pixval(sd, src, inpixf);
		dst += write_pixel(dst_bypp, dst, enc);
		src += inpixf->bypp;
	}
	/* Maybe I should update always, but only the rectangles which changed */
	if (!CycleTimer_IsActive(&sd->update_timer)) {
		CycleTimer_Mod(&sd->update_timer, MillisecondsToCycles(30));
	}
	return 0;
}

int
SD_ControlMsg(FbDisplay * fd, FbCtrlMsg * msg)
{
	return 0;
}

void
SDLDisplay_New(const char *name, FbDisplay ** display, Keyboard ** keyboard, void **mouse)
{
	SDLDisplay *sd = sg_new(SDLDisplay);
	const SDL_VideoInfo *vi;
	FbDisplay *fd = &sd->display;
	int bpp;
	fprintf(stderr, "SDL init started\n");
	/* if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) < 0 ) { */
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);
	if (Config_ReadUInt32(&sd->width, name, "width") < 0) {
		fprintf(stderr, "No width given for display \"%s\"\n", name);
		exit(1);
	}
	if (Config_ReadUInt32(&sd->height, name, "height") < 0) {
		fprintf(stderr, "No height given for display \"%s\"\n", name);
		exit(1);
	}
	vi = SDL_GetVideoInfo();
	bpp = vi->vfmt->BitsPerPixel;
	if ((bpp != 16) && (bpp != 24) && (bpp != 32)) {
		bpp = 16;
	}
	sd->bypp = bpp >> 3;
	sd->screen =
	    SDL_SetVideoMode(sd->width, sd->height, bpp, /* SDL_DOUBLEBUF | */ SDL_SWSURFACE);
	sd->pixels = sd->width * sd->height;
	if (sd->screen == NULL) {
		fprintf(stderr, "can not set SDL videomode %dx%d, bpp %d\n", sd->width, sd->height,
			bpp);
		exit(1);
	}
	fd->width = sd->width;
	fd->height = sd->height;
	fd->setFbFormat = SD_SetFbFormat;
	fd->fbUpdateRequest = SD_UpdateRequest;
	fd->fbCtrlMsg = SD_ControlMsg;
	fd->owner = sd;
	/* Init a default Pixel format ******************* */// jk 
	sd->inpixf.depth = 8;
	sd->inpixf.bypp = 1;

	*display = fd;
	*keyboard = &sd->keyboard;
	CycleTimer_Init(&sd->update_timer, sdl_update_screen, sd);
	fprintf(stderr, "Created SDL display with %d Bits\n", bpp);
}
