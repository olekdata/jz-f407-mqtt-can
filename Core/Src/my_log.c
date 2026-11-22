/*
 * my_log.c
 *
 *  Created on: 8 maj 2022
 *      Author: olek
 */

#include "my_log.h"
#include "cmsis_os2.h"
#include "string.h"
#include "stats.h"
#include "stdint.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "my_time.h"

#define LOG_SD_QUEUE_DEPTH 32U

char log_items[LOG_MAX][LOG_LEN];
//uint8_t log_item = 0;
int log_item = 0;

static uint8_t log_sd_queue_storage[LOG_SD_QUEUE_DEPTH * sizeof(LogSDEntry)];
static StaticQueue_t log_sd_queue_control;
static QueueHandle_t log_sd_queue_handle;
static volatile uint32_t log_sd_queue_drop_count;

//uint32_t itime = 0;
int itime = 0;
char slicz[20];
char stime[20];


extern osSemaphoreId_t BinarySemLCDHandle;
extern osMutexId_t  logMutexHandle;

bool log_sd_queue_init(void)
{
	log_sd_queue_handle = xQueueCreateStatic(LOG_SD_QUEUE_DEPTH, sizeof(LogSDEntry),
	                                         log_sd_queue_storage,
	                                         &log_sd_queue_control);
	return log_sd_queue_handle != NULL;
}

bool log_sd_queue_get(LogSDEntry *entry, uint32_t timeout_ms)
{
	return xQueueReceive(log_sd_queue_handle, entry, pdMS_TO_TICKS(timeout_ms)) == pdPASS;
}

uint32_t log_sd_queue_dropped(void)
{
	return log_sd_queue_drop_count;
}

int ILinia(int lp) { //uint8_t lp) {
	return (log_item + lp) % LOG_MAX;
}

void log_get(int lp, char *destination, size_t size) {
	osMutexAcquire(logMutexHandle, osWaitForever);
	snprintf(destination, size, "%s", log_items[ILinia(lp)]);
	osMutexRelease(logMutexHandle);
}

void log_put(const char *s) {

	osMutexAcquire(logMutexHandle, osWaitForever);
	set_stime();
	LogSDEntry entry = {0};
	struct tm local;
	if (my_time_get_local(&local)) {
		entry.date = (uint32_t)(local.tm_year + 1900) * 10000U +
		             (uint32_t)(local.tm_mon + 1) * 100U + (uint32_t)local.tm_mday;
		strftime(stime, sizeof(stime), "%Y-%m-%d %H:%M:%S", &local);
	}
	char *sl = entry.line;
	if (s[0] == 0) {
		snprintf(sl, sizeof(entry.line), "+%s\n", stime);
		snprintf(log_items[log_item], LOG_LEN, "%s", sl);
		if (itime % 60 == 0){
			printf("%s", sl);
		}
	} else {
		snprintf(sl, sizeof(entry.line), "%s-%.*s\n", stime,
		         (int)(sizeof(entry.line) - strlen(stime) - 3U), s);
		snprintf(log_items[log_item], LOG_LEN, "%s", sl);
		log_item = (log_item + 1) % LOG_MAX;
		printf("%s", sl);
	}
	if (log_sd_queue_handle != NULL &&
	    xQueueSend(log_sd_queue_handle, &entry, 0U) != pdPASS) {
		log_sd_queue_drop_count++;
	}
	osMutexRelease(logMutexHandle);

//	osSemaphoreRelease(myBinarySemLCDHandle);
//  	printf(sl);
//	uint8_t len = strlen(sl);
//  CDC_Transmit_FS(sl,  len);
//  CDC_Transmit_FS("\n",  1);
}

void set_stime(){

	itime = osKernelGetTickCount() / osKernelGetTickFreq();
	uint8_t h = itime / (60 * 60);
	uint8_t m = (itime / 60) % 60;
	uint8_t s = itime % 60;
//		sprintf(stime, "%d/%d:%02d:%02d", resets, h, m, s);
	sprintf(stime, "%d:%02d:%02d", h, m, s);
  sprintf(slicz, "%lu", (unsigned long)itime);

//	sprintf(stime, "%lu", itime);

}


