/*
 * my_can.c
 *
 *  Created on: May 5, 2022
 *      Author: olek
// */

#include "my_mcu.h"
#include "my_can.h"
#include "my_log.h"
#include "cmsis_os2.h"
#include <string.h>

uint32_t TxMailbox;
CAN_FilterTypeDef Can_FilterConfig;

extern osMessageQueueId_t QueueTxCanHandle;


//MsgQRxCan_t msg_can;


HAL_StatusTypeDef CanConfig(void) {
    // Inicjacja magistrali CAN
      /*Na podstawie kodu z materiału https://www.youtube.com/watch?v=ymD3F0h-ilE&t=924s */


    Can_FilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    Can_FilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    Can_FilterConfig.FilterScale = CAN_FILTERSCALE_16BIT;
    Can_FilterConfig.FilterActivation=CAN_FILTER_ENABLE;
    Can_FilterConfig.FilterIdHigh = 0;
    Can_FilterConfig.FilterIdLow = 0;
    Can_FilterConfig.FilterMaskIdHigh = 0; 					//0xFFFF;
    Can_FilterConfig.FilterMaskIdLow = 0; 					//0xFFFF;
    Can_FilterConfig.SlaveStartFilterBank = 14;
    Can_FilterConfig.FilterBank = 0;

    char s[LOG_LEN - 12];
    HAL_StatusTypeDef status = HAL_CAN_ConfigFilter(&hcan1, &Can_FilterConfig);
    if (status != HAL_OK) {
        snprintf(s, sizeof(s), "CAN1 filter err %u HAL=%08lX",
                 (unsigned int)status, (unsigned long)HAL_CAN_GetError(&hcan1));
        log_put(s);
        return status;
    }
    log_put("CAN1 filter FIFO0 accept all OK");

    status = HAL_CAN_Start(&hcan1);
    if (status != HAL_OK) {
        snprintf(s, sizeof(s), "CAN1 start err %u HAL=%08lX",
                 (unsigned int)status, (unsigned long)HAL_CAN_GetError(&hcan1));
        log_put(s);
        return status;
    }
    log_put("CAN1 start OK");

    status = HAL_CAN_ActivateNotification(&hcan1,
             CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO0_OVERRUN);
    if (status != HAL_OK) {
        snprintf(s, sizeof(s), "CAN1 RX IRQ err %u HAL=%08lX",
                 (unsigned int)status, (unsigned long)HAL_CAN_GetError(&hcan1));
        log_put(s);
        return status;
    }
    log_put("CAN1 RX IRQ OK");
    return HAL_OK;
}

void Can_LogRx(const MsgQRxCan_t *msg_can) {
    char s[LOG_LEN - 12];
    if (msg_can->RxHeader.DLC > 8U) {
        snprintf(s, sizeof(s), "CAN RX invalid DLC=%lu",
                 (unsigned long)msg_can->RxHeader.DLC);
        log_put(s);
        return;
    }

    uint32_t id = msg_can->RxHeader.IDE == CAN_ID_EXT ?
                  msg_can->RxHeader.ExtId : msg_can->RxHeader.StdId;
    int len = snprintf(s, sizeof(s), "CAN RX %c:%08lX %s L=%lu",
                       msg_can->RxHeader.IDE == CAN_ID_EXT ? 'E' : 'S',
                       (unsigned long)id,
                       msg_can->RxHeader.RTR == CAN_RTR_REMOTE ? "RTR" : "DATA",
                       (unsigned long)msg_can->RxHeader.DLC);
    if (msg_can->RxHeader.RTR == CAN_RTR_DATA) {
        uint8_t data[8];
        memcpy(data, &msg_can->RxData, sizeof(data));
        for (uint32_t i = 0; i < msg_can->RxHeader.DLC; i++) {
            len += snprintf(s + len, sizeof(s) - (size_t)len,
                            i == 0U ? " %02X" : "%02X", data[i]);
        }
    }
    log_put(s);
}


void Can_RX(const MsgQRxCan_t *msg_can){
	(void)my_mcu_send_can(msg_can);
}


void my_can_Tx_Queue(const MsgQTxCan_t *msg){

	osStatus_t status;

  status = osMessageQueuePut(QueueTxCanHandle, msg, 0U, 0U);
	if (status != osOK) {
		 char s[LOG_LEN];
		 sprintf(s, "QPut can er %d\n", status);
		 log_put(s);
	}
}

void my_can_Tx(const MsgQTxCan_t *msg){
	char s[80];
	const MsgQTxCan_t *msgR = msg;

	sprintf(s,"can_tx>%X %X %X %X %X %02X%02X%02X%02X",
						 msgR->TxHeader.IDE,
						 msgR->TxHeader.ExtId,
						 msgR->TxHeader.StdId,
						 msgR->TxHeader.RTR,
						 msgR->TxHeader.DLC,

						 msgR->TxData.to,
						 msgR->TxData.fun,
						 msgR->TxData.val,
						 msgR->TxData.valL);
  log_put(s);


	CAN_TxHeaderTypeDef header = msg->TxHeader;
	uint8_t data[8];
	_Static_assert(sizeof(msg->TxData) == sizeof(data), "CAN payload must be 8 bytes");
	memcpy(data, &msg->TxData, sizeof(data));
	HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(&hcan1, &header, data, &TxMailbox);
	if (status != HAL_OK) {
		sprintf(s, "CAN TX err %d\n", status);
		log_put(s);
	}
}
