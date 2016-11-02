/**
 *********************************************************************************
 * Interface to the JTAG Tap state machine
 *********************************************************************************
 */
#include <stdint.h>
typedef void JTAG_CaptureDR(void *owner,uint8_t **data,int *len); 
typedef void JTAG_CaptureIR(void *owner,uint8_t **data,int *len); 
typedef void JTAG_UpdateIR(void *owner); 
typedef void JTAG_UpdateDR(void *owner); 

/**
 ****************************************************************************************************
 * JTAG TAP Bit order. Default is LSBFIRST. This means that the
 * first bit shifted in will be the lowest bit of the first byte of the data/instruction
 * register. This is the order found in jedec files.
 *
 * MSBFIRST means that the first bit shifted in will be on the left side in the data/instruction 
 * register. This is the upper bit of the last byte. Using this bitorder makes the code more
 * readable. The data/ir register will be padded on the right side.
 ****************************************************************************************************
 */
#define JTAG_TAP_ORDER_LSBFIRST	(0)
#define JTAG_TAP_ORDER_MSBFIRST	(1)

typedef struct JTAG_Operations {
	JTAG_CaptureDR	 *captureDR;
	JTAG_CaptureIR	 *captureIR;
	JTAG_UpdateDR	 *updateDR;
	JTAG_UpdateIR	 *updateIR;
	uint8_t bitorder;
} JTAG_Operations;

void JTagTap_New(const char *name,JTAG_Operations *jtagOps,void *owner);
