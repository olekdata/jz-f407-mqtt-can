/*
 * roleta.c
 *
 *  Created on: 17 maj 2022
 *      Author: olek
 */

#include "my_mcu.h"
#include "my_can.h"
#include "my_mqtt.h"
#include "my_log.h"
#include "stdio.h"
#include "string.h"


#define TYP_WLACZNIK 1 				// zwykły puszkowy włącznik dwukanałowy
#define TYP_ROLETA_MAN 2			// roleta manualna - jak się trzyma to działą
#define TYP_ROLETA_AUT 3			// roleta automatyczna - raz nacisnąć do końca zamyka/otwiera
#define TYP_ROLETA_POZ 4			// roleta z pozycją zamknięciem i pomiarem prądu


// komunikacja CAN
#define CAN_KIER_PRZY 0
#define CAN_KIER_WYCH 1 // > 0 ; id urządzenie

#define CAN_FUN_ROLETA 1				// roleta dwu stanowa otwarta-zamknieta
#define CAN_FUN_SWITCH1 2
#define CAN_FUN_SWITCH2 3
#define CAN_FUN_SEN_BIN_1 4   			// sensor bin 1
#define CAN_FUN_SEN_BIN_2 5   			// sensor bin 2
#define CAN_FUN_ROLETA_POZ 6			// roleta proporcjonalna od 0(otwarta) - 100(zamklnieta)
#define CAN_FUN_SEN_PRAD 21   			// sensor prądu

#define CAN_FUN_KONF_ID 100   			// konfiguracja wyświetlenie, sprawdzenie id
#define CAN_FUN_KONF_TYP 101   			// konfiguracja wyświetlenie, sprawdzenie typ urządzenia
#define CAN_FUN_KONF_ZAP_CZAS 102 		// konfiguracja kalibracja czasu.
#define CAN_FUN_KONF_KALIBRACJA 103 	// konfiguracja kalibracja czasu.

#define DATA_VAL_OFF 0
#define DATA_VAL_ON 1
#define DATA_VAL_OPEN 0
#define DATA_VAL_CLOSE 1
#define DATA_VAL_STOP 255

//dla typ włącznik
#define I_WYL1 0
#define I_WYL2 1


// dla rolety
#define STAN_ROLETY_ZAMKNIETA 0
#define STAN_ROLETY_ZAMYKANA  1
#define STAN_ROLETY_OTWIERANA 2
#define STAN_ROLETY_OTWARTA 3
#define STAN_ROLETY_NIEUSTALONA 4
#define STAN_ROLETY_KALIBRACJA 6
#define STAN_ROLETY_DO_POZ 7

#define STAN_KALI_DOL 1
#define STAN_KALI_GORA 2


#define I_OTW 0
#define I_ZAM 1
#define I_3   2
#define I_4	  3


uint8_t my_mcu_send_can(const MsgQRxCan_t *msg_can){ //oodbiór z mcu (can) i wysyłka MQTT

	char b[10];
	char t[TOPIC_LEN] = "dscan/";
	char s[LOG_LEN];

	if (msg_can->RxHeader.IDE != CAN_ID_STD ||
	    msg_can->RxHeader.RTR != CAN_RTR_DATA ||
	    msg_can->RxHeader.DLC < 3U || msg_can->RxHeader.DLC > 8U ||
	    msg_can->RxData.to == 0U ||
	    msg_can->RxHeader.StdId != 0x100U + msg_can->RxData.to) {
		return 0;
	}
	if ((msg_can->RxData.fun == CAN_FUN_SEN_PRAD || msg_can->RxData.fun == 0U) &&
	    msg_can->RxHeader.DLC < 4U) {
		log_put("CAN RX: missing low byte of measurement");
		return 0;
	}


	uint8_t id = msg_can->RxData.to;

	strcat(t, itoa(id, b, 10));
  if (msg_can->RxData.fun == CAN_FUN_ROLETA){
  	if (msg_can->RxData.val == DATA_VAL_STOP)
  		strcpy(s, "STOP");
  	else if (msg_can->RxData.val == 1)
  		strcpy(s, "ON");
  	else
  		strcpy(s, "OFF");
  	strcat(t, topic_roleta);
  	my_mqtt_to_Queue(t, s);
  	return 1;
  }

  if (msg_can->RxData.fun == CAN_FUN_ROLETA_POZ){
  	strcat(t, topic_roleta_pos);
		strcpy(s, itoa(msg_can->RxData.val, b, 10));
  	my_mqtt_to_Queue(t, s);
  	return 1;
  }

  if (msg_can->RxData.fun == CAN_FUN_SWITCH1){
  	if (msg_can->RxData.val == DATA_VAL_ON)
  		strcpy(s, "ON");
  	else
  		strcpy(s, "OFF");
  	strcat(t, topic_switch1);
  	my_mqtt_to_Queue(t, s);
  	return 1;
  }
  if (msg_can->RxData.fun == CAN_FUN_SWITCH2){
  	if (msg_can->RxData.val == DATA_VAL_ON)
  		strcpy(s, "ON");
  	else
  		strcpy(s, "OFF");
  	strcat(t, topic_switch2);
  	my_mqtt_to_Queue(t, s);
  	return 1;
  }
  if (msg_can->RxData.fun == CAN_FUN_SEN_BIN_1){
  	if (msg_can->RxData.val == DATA_VAL_ON)
  		strcpy(s, "ON");
  	else
  		strcpy(s, "OFF");
  	strcat(t, topic_sensor1);
  	my_mqtt_to_Queue(t, s);
  	return 1;
  }
  if (msg_can->RxData.fun == CAN_FUN_SEN_BIN_2){
  	if (msg_can->RxData.val == DATA_VAL_ON)
  		strcpy(s, "ON");
  	else
  		strcpy(s, "OFF");
  	strcat(t, topic_sensor2);
  	my_mqtt_to_Queue(t, s);
  	return 1;
  }
  if (msg_can->RxData.fun == CAN_FUN_SEN_PRAD){
		strcpy(s, itoa(256*msg_can->RxData.val + msg_can->RxData.valL, b, 10));
  	strcat(t, topic_prad);
  	my_mqtt_to_Queue(t, s);
  	return 1;
  }
  if (msg_can->RxData.fun == 0){
		strcpy(s, itoa(256*msg_can->RxData.val + msg_can->RxData.valL, b, 10));
  	strcat(t, topic_temp);
  	my_mqtt_to_Queue(t, s);
  	return 1;
  }

	snprintf(s, sizeof(s), "to:%d fun:%d val:%d val2:%u",
	         msg_can->RxData.to, msg_can->RxData.fun, msg_can->RxData.val,
	         msg_can->RxHeader.DLC >= 4U ? msg_can->RxData.valL : 0U);
	snprintf(t, sizeof(t), "dscan/reszta/StdId=%lu",
	         (unsigned long)msg_can->RxHeader.StdId);

	my_mqtt_to_Queue(t, s);
	return 1;
}

bool my_mcu_parse_mqtt_topic(const char *topic, MsgQTxCan_t *msg)
{
	static const struct {
		const char *suffix;
		uint8_t function;
	} commands[] = {
		{topic_roleta, CAN_FUN_ROLETA},
		{topic_roleta_pos, CAN_FUN_ROLETA_POZ},
		{topic_switch1, CAN_FUN_SWITCH1},
		{topic_switch2, CAN_FUN_SWITCH2},
		{topic_sensor1, CAN_FUN_SEN_BIN_1},
		{topic_sensor2, CAN_FUN_SEN_BIN_2}
	};
	static const char prefix[] = topic_can "/";
	if (strncmp(topic, prefix, sizeof(prefix) - 1U) != 0) {
		return false;
	}

	const char *p = topic + sizeof(prefix) - 1U;
	unsigned id = 0;
	unsigned digits = 0;
	while (digits < 3U && *p >= '0' && *p <= '9') {
		id = 10U * id + (unsigned)(*p++ - '0');
		digits++;
	}
	if (id == 0U || id > UINT8_MAX || strncmp(p, "/set", 4U) != 0) {
		return false;
	}
	p += 4U;
	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		if (strcmp(p, commands[i].suffix) == 0) {
			*msg = (MsgQTxCan_t){0};
			msg->TxHeader.StdId = 0x100U + id;
			msg->TxHeader.DLC = 3U;
			msg->TxHeader.IDE = CAN_ID_STD;
			msg->TxHeader.RTR = CAN_RTR_DATA;
			msg->TxData.fun = commands[i].function;
			return true;
		}
	}
	return false;
}

bool my_mcu_send_mqtt_command(MsgQTxCan_t *msg, const u8_t *data, u16_t len)
{
	unsigned value = 0U;
	if (len == 2U && memcmp(data, "ON", 2U) == 0) {
		value = DATA_VAL_ON;
	} else if (len == 3U && memcmp(data, "OFF", 3U) == 0) {
		value = DATA_VAL_OFF;
	} else if (len == 4U && memcmp(data, "STOP", 4U) == 0) {
		value = DATA_VAL_STOP;
	} else {
		if (len == 0U || len > 3U) {
			return false;
		}
		for (u16_t i = 0; i < len; i++) {
			if (data[i] < '0' || data[i] > '9') {
				return false;
			}
			value = 10U * value + (unsigned)(data[i] - '0');
		}
		if (value > UINT8_MAX) {
			return false;
		}
	}
	msg->TxData.val = (uint8_t)value;
	my_can_Tx_Queue(msg);
	return true;
}

