#ifndef __IO_RTC_H__
#define __IO_RTC_H__

#include <stdint.h>

int io_rtc_init(void);
int io_rtc_deinit(void);
uint8_t io_rtc_read(int absolute_addr);
int io_rtc_write(void);

#endif // __IO_RTC_H__
