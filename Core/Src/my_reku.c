/*
 * my_reku.c
 *
 *  Created on: 13 sie 2022
 *      Author: olek
 */
#include "my_reku.h"
#include "my_log.h"
#include "my_mqtt.h"
#include "my_can.h"
#include "can.h"
#include <string.h>

//https://github.com/marco-hoyer/zcan/blob/master/src/main/python/zcan/mapping.py
// val_typ: 0-transform_temperature; 1-float; 2- transform_air_volume
struct reku_sens_t {
	uint32_t ExtId;
	char topic[30];
	uint8_t val_typ;
} reku_sens[] = {
{ 0x448041, "wywiew_temp", 0}, 		//0
{ 0x488041, "wywiew_wilg", 1}, 		//1
{ 0x44c041, "wyrzut_temp", 0},
{ 0x48C041, "wyrzut_wilg", 1},
{ 0x370041, "czerpnia_temp", 0},  //4
{ 0x490041, "czerpnia_wilg", 1},
{ 0x458041, "nawiew_temp", 0},
{ 0x498041, "nawiew_wilg", 1},
{ 0x1DC041, "wydajnosc_wyw", 2},
{ 0x1E0041, "wydajnosc_naw", 2},
{ 0x1E4041, "wywiew_wen", 2},
{ 0x1E8041, "nawiew_wen", 2},
{ 0x200041, "energia_curent", 2},
{ 0x204041, "energia_total_year", 2},
{ 0x208041, "energia_total", 2},
{ 0x358041, "energia_odzyskana_year", 2},
{ 0x35C041, "energia_odzyskana_total", 2},
{ 0x104041, "wentylator_poziom", 1},
{ 0x108041, "bypass_status", 2},   //"0=auto,1=open,2=close",
{ 0x38C041, "bypass_open", 1},   //%
{ 0x10C041, "profil_temperaturowy", 1}   // ["Normal", "Cool", "Warm"][int(x[0])]
//					 00000000011111111112222
//           12345678901234567890123

};


uint8_t Reku_RX(const MsgQRxCan_t *msg_can) {
	uint8_t reku_sens_len = sizeof(reku_sens) / sizeof(reku_sens[0]);
	for (uint8_t i = 0; i < reku_sens_len; i++)
		if (reku_sens[i].ExtId == msg_can->RxHeader.ExtId){
			int16_t v;
  		char s[LOG_LEN];
  		char t[80];
			sprintf(s,"??");

			switch (reku_sens[i].val_typ){
			  case 0:
			  	 if ((msg_can->RxData.to >= 0) && (msg_can->RxData.fun > 0)) {
//			  		 char s_[LOG_LEN];
				     v = 250+msg_can->RxData.to;
//				     sprintf(s_,"?%d,%d >> %d/%d",v/10,v%10,msg_can->RxData.to,msg_can->RxData.fun);
//				     log_put(s_);
			  	 } else {
				     v = msg_can->RxData.to-msg_can->RxData.fun;
			  	 }
			     sprintf(s,"%d.%d",v/10,v%10);
			     break;
			  case 1:
				   v = msg_can->RxData.to;
				   sprintf(s,"%d",v);
				   break;
			  case 2:
			  	 v = msg_can->RxData.to + 256*msg_can->RxData.fun;
			 		 sprintf(s,"%d",v);
			 		 break;
			};

			sprintf(t,"rekuperator/%s",reku_sens[i].topic);
			my_mqtt_to_Queue(t, s);

			return 1;
	  };
	return 0;
}


MsgQTxCanReku_t msg_TxCanR;


void recuperator_recive_mqtt_topic(const char *topic){
	memset(&msg_TxCanR, 0, sizeof(msg_TxCanR));

	if (strstr(topic, "/set") == NULL)
		return;

	if (strstr(topic, "wentylator_poziom")) {
		msg_TxCanR.TxHeader.ExtId = 0x1F015051;
//		log_put("Receive set reku w_p");
		return;
  }
}
// DataSoft/rekuperator/set/wentylator_poziom
// 12345678901234567890123456789012345678901234567890
//          1         2         3         4

char cmd1[] = {0x01,0x00,0x20,0x1C,0x00,0x00,0x01,0x00};

void recuperator_recive_mqtt_data(const uint8_t *data, uint16_t len){
	if (msg_TxCanR.TxHeader.ExtId == 0)
		return;

	char d[] = {0,0,0,0,0,0,0,0,0,0};
	strncpy(d, (const char *)data, len);

//	char s[80];

	if (msg_TxCanR.TxHeader.ExtId == 0x1F015051) {
		memcpy(msg_TxCanR.TxData, cmd1, 8);
//strncpy(cmd1, msg_TxCanR.TxData, 8);
		uint8_t l = d[0]- (uint8_t)'0';
//		sprintf(s, "Receive set reku w_p=%d",l);
//		log_put(s);
		if ((l >= 0) && (l<=3)) {
			msg_TxCanR.TxData[6] = l;
		} else {
			return;
		}
	}

	msg_TxCanR.TxHeader.DLC = 8; // długość danych TxData/RxData
	msg_TxCanR.TxHeader.IDE = CAN_ID_EXT;
	msg_TxCanR.TxHeader.RTR = CAN_RTR_DATA;
/*
	log_put("Send do reku");
	sprintf(s,"To Q>%X %X %X %X %X %02X%02X%02X%02X%02X%02X%02X%02X %X",
						 msg_TxCanR.TxHeader.IDE,
						 msg_TxCanR.TxHeader.ExtId,
						 msg_TxCanR.TxHeader.StdId,
						 msg_TxCanR.TxHeader.RTR,
						 msg_TxCanR.TxHeader.DLC,
						 msg_TxCanR.TxData[0], msg_TxCanR.TxData[1], msg_TxCanR.TxData[2], msg_TxCanR.TxData[3],
						 msg_TxCanR.TxData[4], msg_TxCanR.TxData[5], msg_TxCanR.TxData[6], msg_TxCanR.TxData[7],
						 sizeof(msg_TxCanR));
  log_put(s);
*/

	MsgQTxCan_t msg_TxCan = { .TxHeader = msg_TxCanR.TxHeader };
	memcpy(&msg_TxCan.TxData, msg_TxCanR.TxData, sizeof(msg_TxCan.TxData));
	my_can_Tx_Queue(&msg_TxCan);
	return;

/*
 *
 * T1F0{}5051 8 0084150101000000".format(iterator), encoding="ASCII"),
   T1F0{}5051 8 0100201C00000300".format(iterator), encoding="ASCII"),
   T1F0{}1441 0".format(int(iterator) - 1), encoding="ASCII")

 *
 * Tiiiiiiiildd..[CR]	Transmit extended (29 bit) frame.
    iiiiiiii: Identifier in hexadecimal format (0000000-1FFFFFFF)
            l: Data length (0-8)
             dd: Data byte value in hexadecimal format (00-FF)
 *
 * command_mapping = {
    "set_ventilation_level_0": b'T 1F075051 8 01 00 20 1C 00 00 00 00\r',
    "set_ventilation_level_1": b'T 1F075051 8 01 00 20 1C 00 00 01 00\r',
    "set_ventilation_level_2": b'T 1F075051 8 01 00 20 1C 00 00 02 00\r',
    "set_ventilation_level_3": b'T 1F075051 8 01 00 20 1C 00 00 03 00\r',
    "auto_mode": b'T1F0750514 8 5150801\r', # verified (also: T1F051051485150801\r)
    "manual_mode": b'T1F075051 8 0084150101000000\r', # verified (also: T1F051051485150801\r)
    "temperature_profile_cool": b'T0010C041101\r',
    "temperature_profile_normal": b'T0010C041100\r',
    "temperature_profile_warm": b'T0010C041102\r',
    "close_bypass": b'T00108041102\r',
    "open_bypass": b'T00108041101\r',
    "auto_bypass": b'T00108041100\r',
    "basis_menu": b"T00400041100\r",
    "extended_menu": b"T00400041101\r"
}
 *
 *
	if (strcmp(d,"ON") == 0) {
		msg_TxCan.TxData.val = 1;
		goto next;
	}
	if (strcmp(d,"OFF") == 0) {
		msg_TxCan.TxData.val = 0;
		goto next;
	}
	if (strcmp(d,"STOP") == 0) {
		msg_TxCan.TxData.val = 255;
		goto next;
	}

	msg_TxCan.TxData.val = atoi(d);

	next:
*/
}

