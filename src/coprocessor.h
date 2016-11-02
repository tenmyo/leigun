#ifndef COPROCESSOR_H
#define COPROCESSOR_H
#include <stdint.h>

typedef struct ArmCoprocessor {
	void *owner;
	 uint32_t(*mrc) (struct ArmCoprocessor *, uint32_t icode);
	void (*mcr) (struct ArmCoprocessor *, uint32_t icode, uint32_t value);
	void (*cdp) (struct ArmCoprocessor *, uint32_t icode);
	void (*ldc) (struct ArmCoprocessor *, uint32_t icode);
	void (*stc) (struct ArmCoprocessor *, uint32_t icode);
} ArmCoprocessor;

#endif
