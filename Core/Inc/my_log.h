/*
 * my_log.h
 *
 *  Created on: 8 maj 2022
 *      Author: olek
 */

#ifndef INC_MY_LOG_H_
#define INC_MY_LOG_H_

#include <stdbool.h>
#include <stdint.h>

#define LOG_MAX 50
#define LOG_LEN 70

typedef struct {
	uint32_t date;
	char line[LOG_LEN];
} LogSDEntry;

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#include "stdio.h"


extern char log_items[LOG_MAX][LOG_LEN];
//extern uint8_t log_item;
extern int log_item;

//extern uint32_t itime;
extern int itime;
extern char stime[20];
extern char slicz[20];

//int ILinia(uint8_t lp);
int ILinia(int lp);
void log_put(const char *s);
void log_get(int lp, char *destination, size_t size);
void set_stime();
bool log_sd_queue_init(void);
bool log_sd_queue_get(LogSDEntry *entry, uint32_t timeout_ms);
uint32_t log_sd_queue_dropped(void);

#endif /* INC_MY_LOG_H_ */
