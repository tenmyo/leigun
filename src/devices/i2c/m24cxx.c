/*
 *************************************************************************************************
 *
 * Emulation of M24Cxx I2C EEPROM
 *
 * State: working , destructor missing
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/file.h>
#include "i2c.h"
#include "m24cxx.h"
#include "signode.h"
#include "configfile.h"
#include "cycletimer.h"
#include "diskimage.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define EE_STATE_ADDR0 (0)
#define EE_STATE_ADDR1 (1)
#define EE_STATE_DATA  (2)

typedef struct EEPromType {
	char *name;
	int size;
	int addrlen;
	int pagesize;
	/* 
	 * ----------------------------------------------------------
	 * Some chips do not writeprotect complete chip. 
	 * If addr & mask matches value then writecontrol is used 
	 * ----------------------------------------------------------
	 */
	uint32_t wc_mask;
	uint32_t wc_value;
	/* 
	 * ------------------------------------------------------------------------
	 * Some chips misuse some bits of the I2C-Address as extension to 
	 * the data-address. So they have multiple I2C-addresses
	 * ------------------------------------------------------------------------
	 */
	int nr_addresses;
} EEPromType;

static EEPromType eeprom_types[] = {
	{
	 .name = "M24C01",
	 .size = 128,
	 .addrlen = 1,
	 .pagesize = 16,
	 .nr_addresses = 1,
	 },
	{
	 .name = "AT24C01A-10PA-2.7C",
	 .size = 128,
	 .addrlen = 1,
	 .pagesize = 8,
	 .nr_addresses = 1,
	 },
	{
	 .name = "M24C02",
	 .size = 256,
	 .addrlen = 1,
	 .pagesize = 16,
	 .nr_addresses = 1,
	 },
	{
	 .name = "AT24C02A-10PA-2.7C",
	 .size = 256,
	 .addrlen = 1,
	 .pagesize = 8,
	 .nr_addresses = 1,
	 },
	{
	 .name = "M24C04",
	 .size = 512,
	 .addrlen = 1,
	 .pagesize = 16,
	 .nr_addresses = 2,
	 },
	{
	 .name = "AT24C04A-10PA-2.7C",
	 .size = 512,
	 .addrlen = 1,
	 .pagesize = 16,
	 .nr_addresses = 2,
	 },
	{
	 .name = "M24C08",
	 .size = 1024,
	 .addrlen = 1,
	 .pagesize = 16,
	 .nr_addresses = 4,
	 },
	{
	 .name = "M24C16",
	 .size = 2048,
	 .addrlen = 1,
	 .pagesize = 16,
	 .nr_addresses = 8,
	 },
	{
	 .name = "AT24C16AN-10SU-2.7",
	 .size = 2048,
	 .addrlen = 1,
	 .pagesize = 16,
	 .nr_addresses = 8,
	 },
	{
	 .name = "AT24C16A-10PA-2.7C",
	 .size = 2048,
	 .addrlen = 1,
	 .pagesize = 16,
	 /*  Writecontrol is only for upper half */
	 .wc_mask = 0x400,
	 .wc_value = 0x400,
	 .nr_addresses = 8,
	 },
	{
	 /*  The version from ST-M */
	 .name = "M24C32",
	 .size = 4096,
	 .addrlen = 2,
	 .pagesize = 32,
	 .nr_addresses = 1,
	 },
	{
	 /*  The version from ST-M */
	 .name = "M24C64",
	 .size = 8192,
	 .addrlen = 2,
	 .pagesize = 32,
	 .nr_addresses = 1,
	 },
	{
	 /*  The version from Atmel */
	 .name = "AT24C64A",
	 .size = 8192,
	 .addrlen = 2,
	 .pagesize = 32,
	 .nr_addresses = 1,
	 },
	{
	 NULL,
	 }
};

struct M24Cxx {
	I2C_Slave i2c_slave;
	SigNode *write_control;
	uint32_t mem_address;
	int current_i2c_addr;	/* For chips responding to multiple addresses */
	int nr_addresses;
	int state;
	int size;
	int addrlen;
	int pagesize;
	/* Range for which the Write Control line has a meaning */
	uint32_t wc_mask;
	uint32_t wc_value;

	uint8_t writebuffer_data[32];
	uint32_t writebuffer_dirty;	/* Bitfield */
	uint32_t writebuffer_addr[32];
	CycleCounter_t busy_until;

	uint8_t *data;
	DiskImage *disk_image;
};

/*
 * --------------------------------------------------
 * M24Cxx write
 *	Write 1 or two addressbytes
 *	Following data is going to the writebuffer
 *	which is marked dirty and written on stop
 *	condition 
 * --------------------------------------------------
 */
static int
m24cxx_write(void *dev, uint8_t data)
{
	M24Cxx *m24 = dev;
	if (m24->state == EE_STATE_ADDR0) {
		dbgprintf("M24Cxx HiAddr 0x%02x\n", data);
		if (m24->addrlen == 1) {
			m24->mem_address = data & (m24->size - 1);
			m24->state = EE_STATE_DATA;
		} else if (m24->addrlen == 2) {
			m24->mem_address = (data << 8) & (m24->size - 1);
			m24->state = EE_STATE_ADDR1;
		}
	} else if (m24->state == EE_STATE_ADDR1) {
		dbgprintf("M24Cxx LoAddr 0x%02x\n", data);
		m24->mem_address = (m24->mem_address | data) & (m24->size - 1);
		m24->state = EE_STATE_DATA;
	} else if (m24->state == EE_STATE_DATA) {
		int wc;
		uint32_t addr = m24->mem_address +
		    ((m24->current_i2c_addr & (m24->nr_addresses - 1)) << (8 * m24->addrlen));
		addr = addr & (m24->size - 1);
		dbgprintf("M24Cxx Write 0x%02x to %04x\n", data, m24->mem_address);

		wc = SigNode_Val(m24->write_control);
		if (((addr & m24->wc_mask) != m24->wc_value) || (wc == SIG_LOW)) {
			m24->writebuffer_data[addr % m24->pagesize] = data;
			m24->writebuffer_addr[addr % m24->pagesize] = addr;
			m24->writebuffer_dirty |= 1 << (addr & (m24->pagesize - 1));
			m24->mem_address =
			    ((m24->mem_address +
			      1) % m24->pagesize) | (m24->mem_address & ~(m24->pagesize - 1));
		} else {
			dbgprintf("Write control is %d\n", wc);
			return I2C_NACK;
		}
	}
	return I2C_ACK;
};

/*
 * --------------------------------------------------
 * M24Cxx read 
 *	Read a byte from current address
 * --------------------------------------------------
 */
static int
m24cxx_read(void *dev, uint8_t * data)
{
	M24Cxx *m24 = dev;
	uint32_t addr = m24->mem_address +
	    ((m24->current_i2c_addr & (m24->nr_addresses - 1)) << (8 * m24->addrlen));
	addr = addr & (m24->size - 1);
	*data = m24->data[addr];
	m24->mem_address =
	    (m24->mem_address + 1) & (m24->size - 1) & ((1 << (8 * m24->addrlen)) - 1);
	dbgprintf("M24Cxx read 0x%02x from %04x\n", *data, m24->mem_address);
	return I2C_DONE;
};

/*
 * ----------------------------------------------------
 * M24Cxx start
 *	Check if last write is already done,
 *	if yes accept the start
 * ----------------------------------------------------
 */
static int
m24cxx_start(void *dev, int i2c_addr, int operation)
{
	M24Cxx *m24 = dev;
	// may be repeated start ?
	dbgprintf("m24cxx start\n");
	if (CycleCounter_Get() < m24->busy_until) {
		return I2C_NACK;
	}
	m24->state = EE_STATE_ADDR0;
	m24->current_i2c_addr = i2c_addr;
	return I2C_ACK;
}

/*
 * ------------------------------------------------------------
 * Stop condition
 *	Flush writebuffer and disconnect device for 5ms if
 *	writebuffer is dirty 
 * ------------------------------------------------------------
 */
static void
m24cxx_stop(void *dev)
{
	M24Cxx *m24 = dev;
	int i;
	dbgprintf("m24cxx stop\n");
	m24->state = EE_STATE_ADDR0;
	if (m24->writebuffer_dirty) {
		for (i = 0; i < m24->pagesize; i++) {
			if (m24->writebuffer_dirty & (1 << i)) {
				m24->data[m24->writebuffer_addr[i]] = m24->writebuffer_data[i];
				m24->busy_until = CycleCounter_Get() + MillisecondsToCycles(5);
			}
		}
		m24->writebuffer_dirty = 0;
	}
}

static I2C_SlaveOps m24_ops = {
	.start = m24cxx_start,
	.stop = m24cxx_stop,
	.read = m24cxx_read,
	.write = m24cxx_write
};

/*
 * ------------------------------------------------------
 * Create an I2C EEProm with Write control signal line
 * ------------------------------------------------------
 */
I2C_Slave *
M24Cxx_New(const char *typename, const char *name)
{
	M24Cxx *m24 = sg_new(M24Cxx);
	char *dirname;
	EEPromType *type;
	I2C_Slave *i2c_slave;
	i2c_slave = &m24->i2c_slave;
	i2c_slave->devops = &m24_ops;
	i2c_slave->dev = m24;
	i2c_slave->speed = I2C_SPEED_FAST;
	m24->write_control = SigNode_New("%s.wc", name);
	if (!m24->write_control) {
		fprintf(stderr, "Warning, can not create WriteControl Line for Serial eeprom %s\n",
			name);
		exit(1);
	}
	for (type = eeprom_types; type->name; type++) {
		if (!strcmp(typename, type->name)) {
			break;
		}
	}
	if (!type->name) {
		fprintf(stderr, "M24Cxx: EEProm Type \"%s\" not found\n", typename);
		exit(213);
	}
	m24->size = type->size;
	m24->addrlen = type->addrlen;
	m24->nr_addresses = type->nr_addresses;
	m24->pagesize = type->pagesize;
	m24->wc_mask = type->wc_mask;
	m24->wc_value = type->wc_mask;
	dirname = Config_ReadVar("global", "imagedir");
	if (dirname) {
		char *imagename = alloca(strlen(dirname) + strlen(name) + 20);
		sprintf(imagename, "%s/%s.img", dirname, name);
		m24->disk_image = DiskImage_Open(imagename, m24->size, DI_RDWR | DI_CREAT_FF);
		if (!m24->disk_image) {
			fprintf(stderr, "Failed to open diskimage \"%s\"\n", imagename);
			exit(1);
		}
		m24->data = DiskImage_Mmap(m24->disk_image);
	} else {
		m24->data = sg_calloc(type->size);
		memset(m24->data, 0xff, m24->size);
	}
	fprintf(stderr, "%s I2C-EEProm \"%s\" created.\n", typename, name);
	return i2c_slave;
}
