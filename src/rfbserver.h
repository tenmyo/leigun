/*
 **********************************************************************************
 * Remote Frame buffer protocol server
 *
 * (C) 2006 Jochen Karrer
 **********************************************************************************
 */

#include "fbdisplay.h"
#include "keyboard.h"
#include "mouse.h"

void RfbServer_New(const char *name, FbDisplay ** fbdisplay, Keyboard ** kbd, Mouse **mouse);
