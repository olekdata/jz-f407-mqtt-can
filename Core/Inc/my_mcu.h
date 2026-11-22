/*
 * roleta.h
 *
 *  Created on: 17 maj 2022
 *      Author: olek
 */

#ifndef INC_MY_MCU_H_
#define INC_MY_MCU_H_

#include "my_can.h"
#include "lwip/arch.h"
#include <stdbool.h>

uint8_t my_mcu_send_can(const MsgQRxCan_t *msg_can);
bool my_mcu_parse_mqtt_topic(const char *topic, MsgQTxCan_t *msg);
bool my_mcu_send_mqtt_command(MsgQTxCan_t *msg, const u8_t *data, u16_t len);

#define topic_root "DataSoft"
#define topic_can  "DataSoft/dscan"
#define topic_roleta  "/roleta"
#define topic_switch1 "/switch1"
#define topic_switch2 "/switch2"
#define topic_roleta_pos "/roleta_pos"
#define topic_sensor1 "/sensor1"
#define topic_sensor2 "/sensor2"
#define topic_prad "/prad"
#define topic_temp "/temp"


#define TOPIC_LEN 80



#endif /* INC_MY_MCU_H_ */
