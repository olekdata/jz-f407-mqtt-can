/*
 * my_reku.h
 *
 *  Created on: 13 sie 2022
 *      Author: olek
 */

#ifndef INC_MY_REKU_H__
#define INC_MY_REKU_H__
#include "can.h"
#include "my_can.h"


typedef struct  {
	CAN_TxHeaderTypeDef TxHeader; // 5 x uint32_t + 1 = 21
	uint8_t TxData[8];							// 4 x uint8_t = 4
} MsgQTxCanReku_t;									// size = 25 bajty

uint8_t Reku_RX(const MsgQRxCan_t *msg_can);
void  recuperator_recive_mqtt_topic(topic);
void recuperator_recive_mqtt_data(data, len);


#endif /* INC_MY_REKU_H__ */
