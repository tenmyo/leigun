#include <instructions_avr32.h>
#include <idecode_avr32.h>

static AVR32_Instruction instrlist[] = {
	{0x5c400000,0xfff00000,"abs",avr32_abs},
	{0xd0000000,0xf00f0000,"acall",avr32_acall},
	{0x5c000000,0xfff00000,"acr",avr32_acr},
	{0xe0000040,0xe1f00000,"adc",avr32_adc},
	{0x00000000,0xe1000000,"add_I",avr32_add_I},
	{0xe0000000,0xe1f0ffc0,"add_II",avr32_add_II},
	{0xe1d0e000,0xe1f0f0f0,"add_cond4",avr32_cond4},
};
