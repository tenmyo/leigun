#include <stdint.h>
/** 
 **********************************************************
 * CRC for the polynomial 0x1021 
 **********************************************************
 */
uint16_t CRC16_0x1021(uint16_t crc,const uint8_t *data,int len);

/**
 ******************************************************************
 * CRC for polynomial 0x8005. 
 ******************************************************************
 */
uint16_t CRC16_0x8005(uint16_t crc,const uint8_t *data,int len);
uint16_t CRC16_0x8005Rev(uint16_t crc,const uint8_t *data,int len);

/** 
 **********************************************************
 * bitreverse CRC for the polynomial 0x1021 
 **********************************************************
 */
uint16_t CRC16_0x1021Rev(uint16_t crc,const uint8_t *data,int len);

uint16_t CRC16_0x1021_Start(uint16_t initval);
void CRC16_Init(void);
