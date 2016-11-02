/*
 **********************************************************************************
 * Remote Frame buffer protocol server
 *
 * (C) 2006 Jochen Karrer
 **********************************************************************************
 */

#include <stdint.h>
#include "fbdisplay.h"
#include "keyboard.h"
#include "mouse.h"
#if 0
/*
 * ----------------------------------------------------------------
 * A display has to set the Display info
 * ----------------------------------------------------------------
 */
typedef struct DisplayInfo {
	uint32_t xres;
	uint32_t yres;
	int is_color;
} DisplayInfo;

/*
 * ------------------------------------------------------
 * The PixelDataInfo comes from the displaycontroller
 * which provides the data
 * ------------------------------------------------------
 */
typedef struct PixelDataInfo {
	uint8_t bits_per_pixel;
	uint8_t depth;
	uint8_t big_endian_flag;
	uint8_t true_color_flag;
	uint16_t red_max;
	uint16_t blue_max;
	uint16_t green_max;
	uint8_t red_shift;
	uint8_t green_shift;
	uint8_t blue_shift;
} PixelDataInfo;
#endif

typedef struct RfbServer RfbServer;
void RfbServer_New(const char *name, FbDisplay ** fbdisplay, Keyboard ** kbd, Mouse **mouse);
