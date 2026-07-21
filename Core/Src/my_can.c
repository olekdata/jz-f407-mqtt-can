/*
 * my_can.c
 *
 *  Created on: May 5, 2022
 *      Author: olek
// */

#include "my_mcu.h"
#include "my_can.h"
#include "my_mqtt.h"
#include "my_log.h"
#include "cmsis_os2.h"
#include "my_reku.h"

uint32_t TxMailbox;
CAN_FilterTypeDef Can_FilterConfig;

extern osMessageQueueId_t QueueTxCanHandle;


//MsgQRxCan_t msg_can;


void CanConfig(void) {
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

    HAL_CAN_ConfigFilter(&hcan1, &Can_FilterConfig);
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
}


void Can_RX(const MsgQRxCan_t *msg_can){

 char s[80];

	if (Reku_RX(msg_can))
		return;
/*
	 sprintf(s,"can_rx<%X %X %X %X %02X%02X%02X%02X%02X%02X%02X%02X",
				     msg_can->RxHeader.IDE,
						 msg_can->RxHeader.StdId,
						 msg_can->RxHeader.RTR,
						 msg_can->RxHeader.DLC,
						 msg_can->RxData.to, msg_can->RxData.fun, msg_can->RxData.val, msg_can->RxData.valL,
						 msg_can->RxData.xxx[0], msg_can->RxData.xxx[1], msg_can->RxData.xxx[2], msg_can->RxData.xxx[3]);
		log_put(s);
*/


	if (my_mcu_send_can(msg_can))
		return;

	return; // nie publikujemy reszty

	char t[TOPIC_LEN];
//	char s[LOG_LEN];

	sprintf(t,"reszta/ExtID=%lX", msg_can->RxHeader.ExtId);
	sprintf(s,"%X %X %X %X %02X%02X%02X%02X%02X%02X%02X%02X",
			     msg_can->RxHeader.IDE,
					 msg_can->RxHeader.StdId,
					 msg_can->RxHeader.RTR,
					 msg_can->RxHeader.DLC,
					 msg_can->RxData.to, msg_can->RxData.fun, msg_can->RxData.val, msg_can->RxData.valL,
					 msg_can->RxData.xxx[0], msg_can->RxData.xxx[1], msg_can->RxData.xxx[2], msg_can->RxData.xxx[3]);
	// deb
	my_mqtt_to_Queue(t, s);
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
	MsgQTxCanReku_t *msgR = msg;

	sprintf(s,"can_tx>%X %X %X %X %X %02X%02X%02X%02X%02X%02X%02X%02X",
						 msgR->TxHeader.IDE,
						 msgR->TxHeader.ExtId,
						 msgR->TxHeader.StdId,
						 msgR->TxHeader.RTR,
						 msgR->TxHeader.DLC,
						 msgR->TxData[0], msgR->TxData[1], msgR->TxData[2], msgR->TxData[3],
						 msgR->TxData[4], msgR->TxData[5], msgR->TxData[6], msgR->TxData[7]);
  log_put(s);


	HAL_CAN_AddTxMessage(&hcan1, &msg->TxHeader, &msg->TxData, &TxMailbox);
}


