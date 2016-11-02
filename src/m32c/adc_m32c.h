#ifndef _M32C_ADC_H
#define  _M32C_ADC_H
#include "bus.h"

#define M32C_AD_P10_0	(0)
#define M32C_AD_P10_1	(1)
#define M32C_AD_P10_2	(2)
#define M32C_AD_P10_3	(3)
#define M32C_AD_P10_4	(4)
#define M32C_AD_P10_5	(5)
#define M32C_AD_P10_6	(6)
#define M32C_AD_P10_7	(7)

#define M32C_AD_P15_0	(8)
#define M32C_AD_P15_1	(9)
#define M32C_AD_P15_2	(10)
#define M32C_AD_P15_3	(11)
#define M32C_AD_P15_4	(12)
#define M32C_AD_P15_5	(13)
#define M32C_AD_P15_6	(14)
#define M32C_AD_P15_7	(15)

#define M32C_AD_P0_0	(16)
#define M32C_AD_P0_1	(17)
#define M32C_AD_P0_2	(18)
#define M32C_AD_P0_3	(19)
#define M32C_AD_P0_4	(20)
#define M32C_AD_P0_5	(21)
#define M32C_AD_P0_6	(22)
#define M32C_AD_P0_7	(23)

#define M32C_AD_P2_0	(24)
#define M32C_AD_P2_1	(25)
#define M32C_AD_P2_2	(26)
#define M32C_AD_P2_3	(27)
#define M32C_AD_P2_4	(28)
#define M32C_AD_P2_5	(29)
#define M32C_AD_P2_6	(30)
#define M32C_AD_P2_7	(31)

typedef struct M32C_Adc M32C_Adc;

M32C_Adc *M32C_AdcNew(const char *name);
void M32C_AdChSet(M32C_Adc * adc, unsigned int channel, uint16_t mvolt);
#endif
