/*
 **********************************************************************************
 *
 * Pin Level Emulation of the M93c46 Microwire EEProm
 *
 **********************************************************************************
 */

#include <stdint.h>
#include <signode.h>
#include <diskimage.h>

typedef struct M93C46 {
	char *name;
	SigNode *sclk;
	SigTrace *sclkTrace;
	SigNode *sdi;
	SigTrace *sdiTrace;
	SigNode *csel;
	SigTrace *cselTrace;
	SigNode *sdo;
	int pinstate;
	int oldpinstate;
	int state;
	int expected_bits;
	uint16_t inbuf;
	uint8_t address;
	uint8_t *data;
	int size;
	int write_enabled;
	DiskImage *disk_image;
} M93C46;

/*
 * -----------------------------------------------------
 * Feed Pinstates returns new Pinstates  
 * -----------------------------------------------------
 */

#define MW_CS   (1)
#define MW_SDI  (2)
#define MW_SDO  (4)
#define MW_SCLK (8)

/*
 * -------------------------------------------------------
 * Direct interface to be used by the chips without
 * emulation of Microwire master
 * -------------------------------------------------------
 */
static inline uint16_t
m93c46_readw(M93C46 * eprom, uint32_t address)
{
	uint16_t *data = (uint16_t *) eprom->data;
	if (address > 63) {
		return 0;
	} else {
		return data[address];
	}
}

static inline uint8_t
m93c46_readb(M93C46 * eprom, uint32_t address)
{
	uint8_t *data = (uint8_t *) eprom->data;
	if (address > 127) {
		return 0;
	} else {
		return data[address];
	}
}

static inline void
m93c46_writew(M93C46 * eprom, uint16_t value, uint32_t address)
{
	uint16_t *data = (uint16_t *) eprom->data;
	if (address > 63) {
		return;
	} else {
		data[address] = value;
	}
}

static inline void
m93c46_writeb(M93C46 * eprom, uint16_t value, uint32_t address)
{
	uint8_t *data = (uint8_t *) eprom->data;
	if (address > 127) {
		return;
	} else {
		data[address] = value;
	}
}

/*
 * ------------------------------------------------------
 * Constructor
 * ------------------------------------------------------
 */
M93C46 *m93c46_New(const char *name);
