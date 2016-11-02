#include <stdint.h>

typedef int64_t RTimeHostOfsUs; /* Diff to host time */

typedef struct RTime {
        uint16_t year;          /* 1970.. */
        uint8_t month;          /* 1..12 */
        uint8_t mday;           /* 1.. 31 */
        uint8_t wday;           /* 0..6 with 0 = Sunday */
        uint8_t hour;           /* 0..23 */
        uint8_t min;            /* 0..59 */
        uint8_t sec;            /* 0..59 */
        uint32_t usec;          /* 0 - 999999 */
} RTime;


RTimeHostOfsUs RTime_DiffToHostTime(const RTime *rtime);
void RTime_ReadWithOffset(RTime *rtime, RTimeHostOfsUs timeOffsetUs);
uint8_t RTime_CalcWDay(const RTime *rtime);
