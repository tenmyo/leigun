#include <stdint.h>
#include <mmcard.h>

void MMC_CRC16Init(uint16_t * crc, uint16_t initval);
void MMC_CRC7Init(uint8_t * crc, uint16_t initval);
void MMC_CRC16(uint16_t * crc, const uint8_t * vals, int len);
void MMC_CRC7(uint8_t * crc, const uint8_t * vals, int len);

/* 
 * Calculate the CRC byte of the Response (7Bits CRC + 1) 
 */
static inline uint8_t
MMC_RespCRCByte(MMCResponse * resp)
{
	uint8_t crc;
	MMC_CRC7Init(&crc, 0);
	MMC_CRC7(&crc, resp->data, resp->len - 1);
	return (crc << 1) | 1;
}
