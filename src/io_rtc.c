#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "io.h"
#include "io_rtc.h"
#include "msemu.h"

//----------------------------------------------------------------------------
//
//  Convert uint8_t to BCD format
//
static unsigned char hex2bcd (unsigned char x)
{
	unsigned char y;
	y = (x / 10) << 4;
	y = y | (x % 10);
	return y;
}

int io_rtc_init(void)
{
	return MS_OK;
}

int io_rtc_deinit(void)
{
	return MS_OK;
}

int io_rtc_write(void)
{
	return MS_OK;
}

uint8_t io_rtc_read(int absolute_addr)
{
	time_t curtime;
	struct tm *rtc_time = NULL;

	time(&curtime);
	rtc_time = localtime(&curtime);

	switch(absolute_addr) {
	case RTC_SEC:         //seconds
		return (hex2bcd(rtc_time->tm_sec) & 0x0F);
	case RTC_10SEC:       //10 seconds
		return ((hex2bcd(rtc_time->tm_sec) & 0xF0) >> 4);
	case RTC_MIN:         // minutes
		return (hex2bcd(rtc_time->tm_min) & 0x0F);
	case RTC_10MIN:       // 10 minutes
		return ((hex2bcd(rtc_time->tm_min) & 0xF0) >> 4);
	case RTC_HR:          // hours
		return (hex2bcd(rtc_time->tm_hour) & 0x0F);
	case RTC_10HR:        // 10 hours
		return ((hex2bcd(rtc_time->tm_hour) & 0xF0) >> 4);
	case RTC_DOW:         // day of week
		return rtc_time->tm_wday;
	case RTC_DOM:         // days
		return (hex2bcd(rtc_time->tm_mday) & 0x0F);
	case RTC_10DOM:       // 10 days
		return ((hex2bcd(rtc_time->tm_mday) & 0xF0) >> 4);
	case RTC_MON:         // months
		return (hex2bcd(rtc_time->tm_mon + 1) & 0x0F);
	case RTC_10MON:       // 10 months
		return ((hex2bcd(rtc_time->tm_mon + 1) & 0xF0) >> 4);
	case RTC_YR:          // years since 1980
		return (hex2bcd(rtc_time->tm_year + 80) & 0x0F);
	case RTC_10YR:        // 10s years since 1980
		return ((hex2bcd(rtc_time->tm_year + 80) & 0xF0) >> 4);
	default:
		return 0;
	}
}
