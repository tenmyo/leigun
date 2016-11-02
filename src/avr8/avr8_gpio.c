#include <stdint.h>
#include "sgstring.h"
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "avr8_gpio.h"

typedef struct AVR8_Gpio {
	uint8_t reg_gpio;
} AVR8_Gpio;

static uint8_t
gpio_read(void *clientData, uint32_t address)
{
	AVR8_Gpio *gpio = (AVR8_Gpio *) clientData;
	return gpio->reg_gpio;
}

static void
gpio_write(void *clientData, uint8_t value, uint32_t address)
{
	AVR8_Gpio *gpio = (AVR8_Gpio *) clientData;
	gpio->reg_gpio = value;
}

void
AVR8_GpioNew(const char *name, uint32_t addr)
{
	AVR8_Gpio *gpio = sg_new(AVR8_Gpio);
	AVR8_RegisterIOHandler(addr, gpio_read, gpio_write, gpio);
}
