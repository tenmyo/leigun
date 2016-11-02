#ifndef __FBDISPLAY
#define	__FBDISPLAY
#include "keyboard.h"
#include "sound.h"
#include "mouse.h"

typedef struct FbUpdateRequest {
	unsigned int offset;	/* Start address relative to framebuffer start */
	unsigned int count;	/* bytes submitted in fbdata */
	uint8_t *fbdata;
} FbUpdateRequest;

typedef struct FbFormat {
	int red_bits;
	int green_bits;
	int blue_bits;
	int red_shift;
	int green_shift;
	int blue_shift;
	int bits_per_pixel;	/* This is the layout in memory           */
	int depth;		/* This is the number of really used bits */
} FbFormat;

typedef struct FbCtrlMsg {
	int msg;
} FbCtrlMsg;

/* 
 * -----------------------------------------------------------------------------------
 * The framebuffer display class. Every Framebuffer display needs to implement this 
 * An example for an FbDisplay is the RFBServer
 * -----------------------------------------------------------------------------------
 */
typedef struct FbDisplay {
	void *owner;
	void (*setFbFormat) (struct FbDisplay *, FbFormat *);
	int (*fbUpdateRequest) (struct FbDisplay *, FbUpdateRequest *);
	int (*fbCtrlMsg) (struct FbDisplay *, FbCtrlMsg *);
	char *name;
	uint32_t width;
	uint32_t height;
} FbDisplay;

/*
 ***********************************************************
 * Get the width of the display
 ***********************************************************
 */
static inline uint32_t
FbDisplay_Width(FbDisplay * disp)
{
	return disp->width;
}

/*
 ***********************************************************
 * Get the height of the display
 ***********************************************************
 */
static inline uint32_t
FbDisplay_Height(FbDisplay * disp)
{
	return disp->height;
}

/*
 ***********************************************************
 * Get the name of a display
 * Eventually needed by the user to parse the config file
 ***********************************************************
 */
static inline char *
FbDisplay_Name(FbDisplay * disp)
{
	return disp->name;
}

/*
 * --------------------------------------------------------------------
 * FB_Display constructor needs to be called with information about 
 * the display resulution, the depth of each color.
 * The LCD controller emulator has to set the information about
 * the storage format in memory 
 * --------------------------------------------------------------------
 */

static inline int
FbDisplay_UpdateRequest(FbDisplay * disp, FbUpdateRequest * req)
{
	if (disp && disp->fbUpdateRequest) {
		return disp->fbUpdateRequest(disp, req);
	} else {
		fprintf(stderr, "Display does not handle updates\n");
		return -1;
	}
}

static inline void
FbDisplay_SetFbFormat(FbDisplay * disp, FbFormat * fbf)
{
	if (disp && disp->setFbFormat) {
		return disp->setFbFormat(disp, fbf);
	} else {
		fprintf(stderr, "Display does not take framebuffer info\n");
		return;
	}
}

static inline int
FbDisplay_ControlMessage(FbDisplay * disp, FbCtrlMsg * msg)
{
	if (disp && disp->fbCtrlMsg) {
		return disp->fbCtrlMsg(disp, msg);
	} else {
		return -1;
	}
}

void FbDisplay_New(const char *name, FbDisplay ** display, Keyboard ** keyboard, Mouse **mouse,
		   SoundDevice ** sdev);

#endif				/* __FBSIDPLAY */
