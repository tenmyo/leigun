/*
 * ----------------------------------------------
 * I2C Bus Deserialized device interface
 * ----------------------------------------------
 */
#ifndef I2C_H
#define I2C_H
#include <stdint.h>

/* returnvalues for the start and write I2C Operations */
#define I2C_ACK		(0)
#define I2C_NACK 	(1)
#define I2C_STRETCH_SCL	(2)
/* only for read operation if not strecht scl */
#define I2C_DONE  	(3)

#define I2C_READ	(5)
#define I2C_WRITE	(6)
#define I2C_SPEED_STD	(0)
#define I2C_SPEED_FAST	(1)
#define I2C_SPEED_HIGH	(2)

typedef struct I2C_SlaveOps {
	int (*start) (void *dev, int i2c_addr, int operation);
	void (*stop) (void *dev);
	void (*repstart) (void *dev);	/* ???? */
	/*
	 * -------------------------------------------------
	 * Read and write calls are allowed to return
	 * I2C_STRETCH_SCL if they are not ready to 
	 * eat or vomit data. In this case a later
	 * call of unstretch_scl is required and read/write
	 * will be called again. 
	 * -------------------------------------------------
	 */
	int (*write) (void *dev, uint8_t data);
	int (*read) (void *dev, uint8_t * data);
	/* 
	 * -----------------------------------------------------
	 * Tell device if read was acked or not 
	 * The device can then decide to prepare next
	 * or trigger some finishing action
	 * -----------------------------------------------------
	 */
	void (*read_ack) (void *dev, int ack);
} I2C_SlaveOps;

typedef struct I2C_Slave {
	int speed;
	int tolerated_speed;	/* Speed when other device on same bus is accessed */
	I2C_SlaveOps *devops;
	void *dev;
	struct I2C_Slave *next;
	uint8_t address;
	uint8_t addr_mask;
} I2C_Slave;
#endif
