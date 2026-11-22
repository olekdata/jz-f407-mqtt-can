#ifndef INC_MY_TIME_H_
#define INC_MY_TIME_H_

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define MY_TIME_RTC_MARKER 0x52544331U

bool my_time_init(void);
bool my_time_get_local(struct tm *local);
void my_time_start_ntp(void *argument);
void my_time_ntp_received(uint32_t seconds);
void my_time_poll(void);

#endif
