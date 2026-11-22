/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f4xx_hal.h"
#include "stdio.h"
#include "my_log.h"
#include "my_time.h"
#include "rtc.h"
#include "my_can.h"
#include "iwdg.h"
#include "mqtt.h"
#include "my_mqtt.h"
//#include "eeprom.h"
#include "httpd.h"
#include "my_httpd.h"
#include "fatfs.h"
#include "fatfs_platform.h"

#include "netif.h"
#include "lwip/tcpip.h"
#include "string.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticQueue_t osStaticMessageQDef_t;
/* USER CODE BEGIN PTD */

// ostatnia zajeta komorka FLASH w Buider Analizer to impure_data 0x0801ed98  96 bajtów (0x60)  = 1 EDF8

//VirtAddVarTab[NB_OF_VAR] = {0x5555, 0x6666, 0x7777};
//uint16_t resets = 0;

uint8_t LWIP_Init_flag = 0;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static volatile uint32_t can_rx_read_errors;
static volatile uint32_t can_rx_queue_drops;
static uint32_t sd_task_stack[1024];
static osStaticThreadDef_t sd_task_control_block;
static const osThreadAttr_t sd_task_attributes = {
  .name = "SDCardTask",
  .cb_mem = &sd_task_control_block,
  .cb_size = sizeof(sd_task_control_block),
  .stack_mem = sd_task_stack,
  .stack_size = sizeof(sd_task_stack),
  .priority = osPriorityLow,
};
_Static_assert(sizeof(MsgQRxCan_t) == 36U, "Update CubeMX CAN RX queue item size");
_Static_assert(sizeof(MsgQTxCan_t) == 32U, "Update CubeMX CAN TX queue item size");
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
uint32_t defaultTaskBuffer[ 1024 ];
osStaticThreadDef_t defaultTaskControlBlock;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .cb_mem = &defaultTaskControlBlock,
  .cb_size = sizeof(defaultTaskControlBlock),
  .stack_mem = &defaultTaskBuffer[0],
  .stack_size = sizeof(defaultTaskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TaskButton */
osThreadId_t TaskButtonHandle;
uint32_t TaskButtonBuffer[ 1024 ];
osStaticThreadDef_t TaskButtonControlBlock;
const osThreadAttr_t TaskButton_attributes = {
  .name = "TaskButton",
  .cb_mem = &TaskButtonControlBlock,
  .cb_size = sizeof(TaskButtonControlBlock),
  .stack_mem = &TaskButtonBuffer[0],
  .stack_size = sizeof(TaskButtonBuffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for TaskCan */
osThreadId_t TaskCanHandle;
const osThreadAttr_t TaskCan_attributes = {
  .name = "TaskCan",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TaskMQTT */
osThreadId_t TaskMQTTHandle;
uint32_t TaskMQTTBuffer[ 1024 ];
osStaticThreadDef_t TaskMQTTControlBlock;
const osThreadAttr_t TaskMQTT_attributes = {
  .name = "TaskMQTT",
  .cb_mem = &TaskMQTTControlBlock,
  .cb_size = sizeof(TaskMQTTControlBlock),
  .stack_mem = &TaskMQTTBuffer[0],
  .stack_size = sizeof(TaskMQTTBuffer),
  .priority = (osPriority_t) osPriorityNormal1,
};
/* Definitions for QueueRxCan */
osMessageQueueId_t QueueRxCanHandle;
uint8_t QueueRxCanBuffer[ 32 * 36 ];
osStaticMessageQDef_t QueueRxCanControlBlock;
const osMessageQueueAttr_t QueueRxCan_attributes = {
  .name = "QueueRxCan",
  .cb_mem = &QueueRxCanControlBlock,
  .cb_size = sizeof(QueueRxCanControlBlock),
  .mq_mem = &QueueRxCanBuffer,
  .mq_size = sizeof(QueueRxCanBuffer)
};
/* Definitions for QueueTxMqtt */
osMessageQueueId_t QueueTxMqttHandle;
uint8_t QueueTxMqttBuffer[ 16 * 100 ];
osStaticMessageQDef_t QueueTxMqttControlBlock;
const osMessageQueueAttr_t QueueTxMqtt_attributes = {
  .name = "QueueTxMqtt",
  .cb_mem = &QueueTxMqttControlBlock,
  .cb_size = sizeof(QueueTxMqttControlBlock),
  .mq_mem = &QueueTxMqttBuffer,
  .mq_size = sizeof(QueueTxMqttBuffer)
};
/* Definitions for QueueTxCan */
osMessageQueueId_t QueueTxCanHandle;
uint8_t QueueTxCanBuffer[ 32 * 32 ];
osStaticMessageQDef_t QueueTxCanControlBlock;
const osMessageQueueAttr_t QueueTxCan_attributes = {
  .name = "QueueTxCan",
  .cb_mem = &QueueTxCanControlBlock,
  .cb_size = sizeof(QueueTxCanControlBlock),
  .mq_mem = &QueueTxCanBuffer,
  .mq_size = sizeof(QueueTxCanBuffer)
};
/* Definitions for logMutex */
osMutexId_t logMutexHandle;
const osMutexAttr_t logMutex_attributes = {
  .name = "logMutex"
};
/* Definitions for putMqttMutex */
osMutexId_t putMqttMutexHandle;
const osMutexAttr_t putMqttMutex_attributes = {
  .name = "putMqttMutex"
};
/* Definitions for BinarySemButton */
osSemaphoreId_t BinarySemButtonHandle;
const osSemaphoreAttr_t BinarySemButton_attributes = {
  .name = "BinarySemButton"
};
/* Definitions for BinarySemLCD */
osSemaphoreId_t BinarySemLCDHandle;
const osSemaphoreAttr_t BinarySemLCD_attributes = {
  .name = "BinarySemLCD"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

static void StartSDTask(void *argument);
static void RunSDCardTest(void);
static FRESULT AppendSDLogLine(FIL *file, const char *line);

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == USER_Btn_Pin) {
		osSemaphoreRelease(BinarySemButtonHandle);
	}
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
	MsgQRxCan_t msg_can;
	osStatus_t status;
	uint8_t data[8] = {0};
	_Static_assert(sizeof(msg_can.RxData) == sizeof(data), "CAN payload must be 8 bytes");
	HAL_StatusTypeDef rx_status = HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &msg_can.RxHeader, data);
	if (rx_status != HAL_OK) {
		can_rx_read_errors++;
		return;
	}
	memcpy(&msg_can.RxData, data, sizeof(data));
	status = osMessageQueuePut(QueueRxCanHandle, &msg_can, 0U, 0U);
	if (status != osOK) {
		can_rx_queue_drops++;
	}
}

static void StartHttpServer(void *argument)
{
	(void)argument;
	httpd_init();
	myCGIinit();
	mySSIinit();
}

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartTaskButton(void *argument);
void StartTaskCan(void *argument);
void StartTaskMQTT(void *argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
return 0;
}
/* USER CODE END 1 */

/* USER CODE BEGIN 4 */
static void LogFatFsError(const char *operation, FRESULT result)
{
	char message[LOG_LEN];
	snprintf(message, sizeof(message), "SD %s failed: %u", operation, (unsigned)result);
	log_put(message);
}

static void RunSDCardTest(void)
{
	char test_path[20];
	int path_length = snprintf(test_path, sizeof(test_path), "%sSDTEST.TXT", SDPath);
	if (path_length < 0 || (size_t)path_length >= sizeof(test_path)) {
		log_put("SD test path too long");
		return;
	}

	FIL file;
	FRESULT result = f_open(&file, test_path, FA_READ);
	if (result == FR_OK) {
		uint8_t buffer[32];
		UINT bytes_read = 0U;
		result = f_read(&file, buffer, (UINT)sizeof(buffer), &bytes_read);
		FRESULT close_result = f_close(&file);
		if (result != FR_OK) {
			LogFatFsError("test file read", result);
		}
		if (close_result != FR_OK) {
			LogFatFsError("test file close", close_result);
		}
		if (result == FR_OK && close_result == FR_OK) {
			log_put("SDTEST.TXT read; existing file preserved");
		}
		return;
	}
	if (result != FR_NO_FILE) {
		LogFatFsError("test file open", result);
		return;
	}

	result = f_open(&file, test_path, (BYTE)(FA_WRITE | FA_CREATE_NEW));
	if (result != FR_OK) {
		LogFatFsError("test file create", result);
		return;
	}

	static const uint8_t test_data[] = "DS-CAN SD test\r\n";
	const UINT test_data_size = (UINT)(sizeof(test_data) - 1U);
	UINT bytes_written = 0U;
	result = f_write(&file, test_data, test_data_size, &bytes_written);
	FRESULT close_result = f_close(&file);
	if (result != FR_OK) {
		LogFatFsError("test file write", result);
	}
	if (bytes_written != test_data_size && result == FR_OK) {
		log_put("SD test file short write");
	}
	if (close_result != FR_OK) {
		LogFatFsError("test file close", close_result);
	}
	if (result == FR_OK && bytes_written == test_data_size && close_result == FR_OK) {
		log_put("SDTEST.TXT created and written");
	}
}

static FRESULT AppendSDLogLine(FIL *file, const char *line)
{
	size_t line_length = strlen(line);
	UINT bytes_written = 0U;
	FRESULT result = f_write(file, line, (UINT)line_length, &bytes_written);
	if (result == FR_OK && bytes_written != (UINT)line_length) {
		return FR_DISK_ERR;
	}
	return result;
}

static void StartSDTask(void *argument)
{
	(void)argument;
	if (retSD != 0U) {
		log_put("SD driver link failed");
		osThreadExit();
		return;
	}

	bool card_missing_reported = false;
	bool mount_error_reported = false;
	bool log_open_error_reported = false;
	bool log_write_error_reported = false;
	uint32_t reported_drops = 0U;
	for (;;) {
		if (BSP_PlatformIsDetected() != SD_PRESENT) {
			if (!card_missing_reported) {
				log_put("SD card not detected; waiting for insertion");
				card_missing_reported = true;
			}
			osDelay(500U);
			continue;
		}
		card_missing_reported = false;

		FRESULT result = f_mount(&SDFatFS, SDPath, 1U);
		if (result != FR_OK) {
			if (!mount_error_reported) {
				LogFatFsError("mount", result);
				mount_error_reported = true;
			}
			osDelay(1000U);
			continue;
		}
		mount_error_reported = false;
		log_put("SD FAT volume mounted");

		DWORD free_clusters = 0U;
		FATFS *filesystem = &SDFatFS;
		result = f_getfree(SDPath, &free_clusters, &filesystem);
		if (result != FR_OK) {
			LogFatFsError("space check", result);
		} else {
			char free_space_message[LOG_LEN];
			snprintf(free_space_message, sizeof(free_space_message),
			         "SD free clusters: %lu", (unsigned long)free_clusters);
			log_put(free_space_message);
		}

		RunSDCardTest();

		FIL log_file;
		bool file_open = false;
		uint32_t file_date = 0U;
		uint32_t last_sync = HAL_GetTick();
		bool file_dirty = false;
		for (;;) {
			if (BSP_PlatformIsDetected() != SD_PRESENT) {
				log_put("SD card removed; logging paused");
				break;
			}

			LogSDEntry entry;
			if (log_sd_queue_get(&entry, 100U)) {
				if (!file_open || file_date != entry.date) {
					if (file_open) {
						result = f_close(&log_file);
						file_open = false;
						if (result != FR_OK) {
							LogFatFsError("daily log close", result);
							break;
						}
					}
					char log_path[32];
					int length;
					if (entry.date == 0U) {
						length = snprintf(log_path, sizeof(log_path),
						                  "%slog_unsynced.txt", SDPath);
					} else {
						length = snprintf(log_path, sizeof(log_path),
						                  "%slog%08lu.txt", SDPath,
						                  (unsigned long)entry.date);
					}
					if (length < 0 || (size_t)length >= sizeof(log_path)) {
						log_put("SD daily log path too long");
						break;
					}
					result = f_open(&log_file, log_path, (BYTE)(FA_WRITE | FA_OPEN_APPEND));
					if (result != FR_OK) {
						if (!log_open_error_reported) {
							LogFatFsError("log open", result);
							log_open_error_reported = true;
						}
						break;
					}
					log_open_error_reported = false;
					file_open = true;
					file_date = entry.date;
					file_dirty = false;
					last_sync = HAL_GetTick();
					printf("SD append logging: %s\n", log_path);
				}
				result = AppendSDLogLine(&log_file, entry.line);
				if (result != FR_OK) {
					if (!log_write_error_reported) {
						LogFatFsError("log write", result);
						log_write_error_reported = true;
					}
					break;
				}
				log_write_error_reported = false;
				file_dirty = true;
			} else {
				uint32_t current_drops = log_sd_queue_dropped();
				uint32_t new_drops = current_drops - reported_drops;
				if (new_drops > 0U && file_open) {
					char warning[LOG_LEN];
					snprintf(warning, sizeof(warning),
					         "SD log queue dropped %lu entries\n",
					         (unsigned long)new_drops);
					result = AppendSDLogLine(&log_file, warning);
					if (result != FR_OK) {
						if (!log_write_error_reported) {
							LogFatFsError("drop report write", result);
							log_write_error_reported = true;
						}
						break;
					}
					printf("%s", warning);
					reported_drops = current_drops;
					file_dirty = true;
				}
			}

			uint32_t now = HAL_GetTick();
			if (file_dirty && (uint32_t)(now - last_sync) >= 1000U) {
				result = f_sync(&log_file);
				if (result != FR_OK) {
					if (!log_write_error_reported) {
						LogFatFsError("log sync", result);
						log_write_error_reported = true;
					}
					break;
				}
				file_dirty = false;
				last_sync = now;
			}
		}

		if (file_open) {
			FRESULT close_result = f_close(&log_file);
			if (close_result != FR_OK) {
				LogFatFsError("log close", close_result);
			}
		}
		result = f_mount(NULL, SDPath, 0U);
		if (result != FR_OK) {
			LogFatFsError("unmount", result);
		}
		osDelay(1000U);
	}
}

void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of logMutex */
  logMutexHandle = osMutexNew(&logMutex_attributes);

  /* creation of putMqttMutex */
  putMqttMutexHandle = osMutexNew(&putMqttMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
	if (!my_time_init()) {
		Error_Handler();
	}
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of BinarySemButton */
  BinarySemButtonHandle = osSemaphoreNew(1, 0, &BinarySemButton_attributes);

  /* creation of BinarySemLCD */
  BinarySemLCDHandle = osSemaphoreNew(1, 0, &BinarySemLCD_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of QueueRxCan */
  QueueRxCanHandle = osMessageQueueNew (32, 36, &QueueRxCan_attributes);

  /* creation of QueueTxMqtt */
  QueueTxMqttHandle = osMessageQueueNew (16, 100, &QueueTxMqtt_attributes);

  /* creation of QueueTxCan */
  QueueTxCanHandle = osMessageQueueNew (32, 32, &QueueTxCan_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
	if (QueueRxCanHandle == NULL || QueueTxCanHandle == NULL ||
	    QueueTxMqttHandle == NULL || !log_sd_queue_init()) {
		Error_Handler();
	}
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of TaskButton */
  TaskButtonHandle = osThreadNew(StartTaskButton, NULL, &TaskButton_attributes);

  /* creation of TaskCan */
  TaskCanHandle = osThreadNew(StartTaskCan, NULL, &TaskCan_attributes);

  /* creation of TaskMQTT */
  TaskMQTTHandle = osThreadNew(StartTaskMQTT, NULL, &TaskMQTT_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
	if (defaultTaskHandle == NULL || TaskButtonHandle == NULL ||
	    TaskCanHandle == NULL || TaskMQTTHandle == NULL) {
		Error_Handler();
	}
	if (osThreadNew(StartSDTask, NULL, &sd_task_attributes) == NULL) {
		Error_Handler();
	}
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
	/* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN StartDefaultTask */
	/* Infinite loop */
	log_put(HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == MY_TIME_RTC_MARKER ?
	        "RTC retained time; awaiting NTP refresh" :
	        "RTC not set; uptime logs until NTP sync");

	err_t http_start_status;
	do {
		http_start_status = tcpip_callback(StartHttpServer, NULL);
		if (http_start_status != ERR_OK) {
			char s[LOG_LEN];
			sprintf(s, "HTTP init queue err %d", http_start_status);
			log_put(s);
			HAL_IWDG_Refresh(&hiwdg);
			osDelay(100);
		}
	} while (http_start_status != ERR_OK);

	err_t ntp_start_status;
	do {
		ntp_start_status = tcpip_callback(my_time_start_ntp, NULL);
		if (ntp_start_status != ERR_OK) {
			log_put("NTP init queue failed; retrying");
			HAL_IWDG_Refresh(&hiwdg);
			osDelay(100U);
		}
	} while (ntp_start_status != ERR_OK);

	osDelay(1000);
  LWIP_Init_flag = 1;


/*deb*/
	{
	extern struct netif gnetif;

	u32_t local_IP = gnetif.ip_addr.addr;
	char s[LOG_LEN];
	sprintf(s, "IP %lu.%lu.%lu.%lu", (local_IP & 0xff), ((local_IP >> 8) & 0xff),
			((local_IP >> 16) & 0xff), (local_IP >> 24));
	log_put(s);
	}

/**/

//	stats_display();

	for (;;) {
		my_time_poll();
		HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
		/* Keep the watchdog serviced while HTTP/LwIP run. */
		HAL_IWDG_Refresh(&hiwdg);
		osDelay(1000);
	}
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTaskButton */
/**
 * @brief Function implementing the myTaskButton thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskButton */
void StartTaskButton(void *argument)
{
  /* USER CODE BEGIN StartTaskButton */
	(void)argument;
	GPIO_PinState candidate = HAL_GPIO_ReadPin(SW3_GPIO_Port, SW3_Pin);
	GPIO_PinState stable = candidate;
	uint32_t candidate_since = HAL_GetTick();

	for (;;) {
		GPIO_PinState raw = HAL_GPIO_ReadPin(SW3_GPIO_Port, SW3_Pin);
		uint32_t now = HAL_GetTick();
		if (raw != candidate) {
			candidate = raw;
			candidate_since = now;
		} else if (candidate != stable && (uint32_t)(now - candidate_since) >= 30U) {
			stable = candidate;
			if (stable == GPIO_PIN_SET) {
				MsgQTxCan_t test_frame = {0};
				HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
				test_frame.TxHeader.StdId = 0x108U;
				test_frame.TxHeader.IDE = CAN_ID_STD;
				test_frame.TxHeader.RTR = CAN_RTR_DATA;
				test_frame.TxHeader.DLC = 3U;
				test_frame.TxData.to = 0U;
				test_frame.TxData.fun = 100U;
				test_frame.TxData.val = 0U;
				my_can_Tx_Queue(&test_frame);
			}
		}
		osDelay(10U);
	}
  /* USER CODE END StartTaskButton */
}

/* USER CODE BEGIN Header_StartTaskCan */
/**
 * @brief Function implementing the myTaskCan thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskCan */
void StartTaskCan(void *argument)
{
  /* USER CODE BEGIN StartTaskCan */
	/* Infinite loop */

	//deb

	while(1) {
		if (LWIP_Init_flag)
		  break;
		osDelay(10);
	}


	log_put("CAN1 init 50000 bit/s PD0=RX PD1=TX");
	if (CanConfig() != HAL_OK) {
		if (HAL_CAN_GetState(&hcan1) == HAL_CAN_STATE_LISTENING &&
		    HAL_CAN_Stop(&hcan1) != HAL_OK) {
			log_put("CAN1 stop after init failure failed");
		}
		log_put("CAN1 task stopped: init failed");
		osThreadExit();
	}
	log_put("CAN1 ready; CAN/MQTT bridge and S3 test TX enabled");

	uint32_t last_read_errors = 0;
	uint32_t last_queue_drops = 0;
	uint32_t last_hal_error = HAL_CAN_ERROR_NONE;
	uint32_t last_esr = 0;
	uint32_t status_tick = osKernelGetTickCount();
	const uint32_t tick_freq = osKernelGetTickFreq();

	for (;;) {
		MsgQRxCan_t msg_RxCan;
		osStatus_t status = osMessageQueueGet(QueueRxCanHandle, &msg_RxCan,
		                                      NULL, tick_freq / 10U);
		if (status == osOK) {
			Can_LogRx(&msg_RxCan);
			Can_RX(&msg_RxCan);
		} else if (status != osErrorTimeout) {
			char s[LOG_LEN - 12];
			snprintf(s, sizeof(s), "CAN RX queue get err %d", (int)status);
			log_put(s);
			osThreadExit();
		}

		MsgQTxCan_t msg_TxCan;
		if (osMessageQueueGet(QueueTxCanHandle, &msg_TxCan, NULL, 0U) == osOK) {
			my_can_Tx(&msg_TxCan);
		}

		uint32_t now = osKernelGetTickCount();
		if ((uint32_t)(now - status_tick) < tick_freq) {
			continue;
		}
		status_tick = now;
		uint32_t read_errors = can_rx_read_errors;
		uint32_t queue_drops = can_rx_queue_drops;
		uint32_t hal_error = HAL_CAN_GetError(&hcan1);
		uint32_t esr = hcan1.Instance->ESR &
		               (CAN_ESR_EWGF | CAN_ESR_EPVF | CAN_ESR_BOFF | CAN_ESR_LEC);
		char s[LOG_LEN - 12];
		if (read_errors != last_read_errors || queue_drops != last_queue_drops) {
			snprintf(s, sizeof(s), "CAN RX read_err=%lu queue_drop=%lu",
			         (unsigned long)read_errors, (unsigned long)queue_drops);
			log_put(s);
			last_read_errors = read_errors;
			last_queue_drops = queue_drops;
		}
		if (hal_error != last_hal_error || esr != last_esr) {
			snprintf(s, sizeof(s), "CAN1 HAL=%08lX ESR=%08lX",
			         (unsigned long)hal_error, (unsigned long)esr);
			log_put(s);
			last_hal_error = hal_error;
			last_esr = esr;
		}
	}
  /* USER CODE END StartTaskCan */
}

/* USER CODE BEGIN Header_StartTaskMQTT */
/**
* @brief Function implementing the TaskMQTT thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskMQTT */
void StartTaskMQTT(void *argument)
{
  /* USER CODE BEGIN StartTaskMQTT */


	while(1) {
		if (LWIP_Init_flag)
		  break;
		osDelay(10);
	}

	log_put("MQTT service: CAN bridge and uptime");
	for (;;) {
		err_t status = tcpip_callback(my_mqtt_service, NULL);
		if (status != ERR_OK) {
			char s[LOG_LEN - 12];
			snprintf(s, sizeof(s), "MQTT service queue err %d", (int)status);
			log_put(s);
		}
		osDelay((osKernelGetTickFreq() + 9U) / 10U);
	}
  /* USER CODE END StartTaskMQTT */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

