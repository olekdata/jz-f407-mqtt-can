#include "my_time.h"
#include "rtc.h"
#include "my_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os2.h"
#include "lwip/apps/sntp.h"

static StaticSemaphore_t rtc_mutex_storage;
static osMutexId_t rtc_mutex;
static uint32_t pending_seconds;
static bool ntp_pending;
static bool rtc_read_error_reported;
static uint32_t last_sync_tick;
static uint32_t last_wait_report;
static bool ntp_synced;

static bool IsLeapYear(unsigned year)
{
	return year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U);
}

static unsigned DaysInMonth(unsigned year, unsigned month)
{
	static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return days[month - 1U] + (month == 2U && IsLeapYear(year) ? 1U : 0U);
}

static bool ReadUtc(uint32_t *seconds)
{
	if (rtc_mutex == NULL) {
		return false;
	}
	if (osMutexAcquire(rtc_mutex, osWaitForever) != osOK) {
		Error_Handler();
		return false;
	}
	bool valid = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == MY_TIME_RTC_MARKER;
	bool failed = false;
	RTC_TimeTypeDef time;
	RTC_DateTypeDef date;
	if (valid) {
		/* Reading DR after TR releases the RTC shadow-register lock. */
		HAL_StatusTypeDef time_status = HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
		HAL_StatusTypeDef date_status = HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
		failed = time_status != HAL_OK || date_status != HAL_OK;
		if (!failed) {
			failed = date.Year > 99U || date.Month < 1U || date.Month > 12U ||
			         date.Date < 1U || time.Hours > 23U ||
			         time.Minutes > 59U || time.Seconds > 59U;
			if (!failed) {
				failed = date.Date > DaysInMonth(2000U + date.Year, date.Month);
			}
		}
	}
	bool report_error = failed && !rtc_read_error_reported;
	if (failed) {
		rtc_read_error_reported = true;
	}
	if (osMutexRelease(rtc_mutex) != osOK) {
		Error_Handler();
	}
	if (failed) {
		if (report_error) {
			printf("RTC read failed or calendar invalid; using uptime\n");
		}
		return false;
	}
	if (!valid) {
		return false;
	}

	uint32_t days = 0U;
	unsigned year = 2000U + date.Year;
	for (unsigned y = 2000U; y < year; y++) {
		days += IsLeapYear(y) ? 366U : 365U;
	}
	for (unsigned month = 1U; month < date.Month; month++) {
		days += DaysInMonth(year, month);
	}
	days += date.Date - 1U;
	*seconds = 946684800U + days * 86400U + time.Hours * 3600U +
	           time.Minutes * 60U + time.Seconds;
	return true;
}

static bool ToPolishTime(uint32_t seconds, struct tm *local)
{
	time_t epoch = (time_t)seconds;
	struct tm utc;
	if (gmtime_r(&epoch, &utc) == NULL) {
		printf("RTC UTC conversion failed\n");
		return false;
	}
	unsigned last_sunday = 31U - (unsigned)((utc.tm_wday + 31 - utc.tm_mday) % 7);
	bool summer = (utc.tm_mon > 2 && utc.tm_mon < 9) ||
	              (utc.tm_mon == 2 && ((unsigned)utc.tm_mday > last_sunday ||
	               ((unsigned)utc.tm_mday == last_sunday && utc.tm_hour >= 1))) ||
	              (utc.tm_mon == 9 && ((unsigned)utc.tm_mday < last_sunday ||
	               ((unsigned)utc.tm_mday == last_sunday && utc.tm_hour < 1)));
	/* EU transitions occur at 01:00 UTC on the last Sunday of March/October. */
	epoch += summer ? 7200 : 3600;
	if (gmtime_r(&epoch, local) == NULL) {
		printf("RTC local conversion failed\n");
		return false;
	}
	local->tm_isdst = summer ? 1 : 0;
	return true;
}

bool my_time_init(void)
{
	const osMutexAttr_t attributes = {
		.name = "RTC",
		.attr_bits = osMutexPrioInherit,
		.cb_mem = &rtc_mutex_storage,
		.cb_size = sizeof(rtc_mutex_storage),
	};
	rtc_mutex = osMutexNew(&attributes);
	return rtc_mutex != NULL;
}

bool my_time_get_local(struct tm *local)
{
	uint32_t seconds;
	return ReadUtc(&seconds) && ToPolishTime(seconds, local);
}

void my_time_start_ntp(void *argument)
{
	(void)argument;
	if (sntp_enabled()) {
		return;
	}
	ip_addr_t server;
	IP_ADDR4(&server, 162, 159, 200, 1);
	sntp_setserver(0, &server);
	IP_ADDR4(&server, 162, 159, 200, 123);
	sntp_setserver(1, &server);
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_init();
	log_put(sntp_enabled() ? "NTP client started" : "NTP client start failed");
}

void my_time_ntp_received(uint32_t seconds)
{
	/* Leave RTC writes and logging to the default task, not the TCP/IP callback. */
	taskENTER_CRITICAL();
	pending_seconds = seconds;
	ntp_pending = true;
	taskEXIT_CRITICAL();
}

void my_time_poll(void)
{
	uint32_t seconds;
	bool pending;
	taskENTER_CRITICAL();
	seconds = pending_seconds;
	pending = ntp_pending;
	ntp_pending = false;
	taskEXIT_CRITICAL();
	uint32_t now = HAL_GetTick();
	if (!pending) {
		if ((!ntp_synced || (uint32_t)(now - last_sync_tick) >= 3660000U) &&
		    (uint32_t)(now - last_wait_report) >= 60000U) {
			log_put("NTP waiting for valid time; check Internet/UDP 123");
			last_wait_report = now;
		}
		return;
	}

	if (seconds < 1704067200U || seconds >= 4102444800U) {
		log_put("NTP time rejected: outside 2024-2099");
		return;
	}
	time_t epoch = (time_t)seconds;
	struct tm utc;
	if (gmtime_r(&epoch, &utc) == NULL) {
		log_put("NTP UTC conversion failed");
		return;
	}
	RTC_TimeTypeDef time = {
		.Hours = (uint8_t)utc.tm_hour,
		.Minutes = (uint8_t)utc.tm_min,
		.Seconds = (uint8_t)utc.tm_sec,
		.DayLightSaving = RTC_DAYLIGHTSAVING_NONE,
		.StoreOperation = RTC_STOREOPERATION_RESET,
	};
	RTC_DateTypeDef date = {
		.WeekDay = (uint8_t)(utc.tm_wday == 0 ? 7 : utc.tm_wday),
		.Month = (uint8_t)(utc.tm_mon + 1),
		.Date = (uint8_t)utc.tm_mday,
		.Year = (uint8_t)(utc.tm_year - 100),
	};
	if (osMutexAcquire(rtc_mutex, osWaitForever) != osOK) {
		Error_Handler();
		return;
	}
	HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0U);
	HAL_StatusTypeDef status = HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
	if (status == HAL_OK) {
		status = HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
	}
	if (status == HAL_OK) {
		HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, MY_TIME_RTC_MARKER);
	}
	if (osMutexRelease(rtc_mutex) != osOK) {
		Error_Handler();
	}
	if (status != HAL_OK) {
		log_put("RTC update from NTP failed");
		return;
	}
	last_sync_tick = now;
	ntp_synced = true;
	log_put("RTC updated from NTP; local time CET/CEST");
}
