/*
 * my_mqtt.c
 *
 *  Created on: May 5, 2022
 *      Author: olek
 */

#include "my_mqtt.h"
#include "mqtt.h"
#include <string.h>
#include "cmsis_os2.h"
#include "my_log.h"
#include "my_mcu.h"
#include <stdbool.h>


extern osMessageQueueId_t QueueTxMqttHandle;



//mqtt_msg__t mqtt_msg;

mqtt_client_t *mqtt_client;
static bool mqtt_connect_pending;
static bool mqtt_connect_attempted;
static bool mqtt_publish_now;
static uint32_t mqtt_connect_tick;
static uint32_t mqtt_publish_tick;

static MsgQTxCan_t incoming_command;
static u8_t incoming_payload[4];
static u16_t incoming_expected;
static u16_t incoming_size;
static bool mqtt_sub_pending;
static bool mqtt_subscribed;
static uint32_t mqtt_sub_tick;

static void mqtt_sub_request_cb(void *arg, err_t result)
{
	(void)arg;
	mqtt_sub_pending = false;
	mqtt_subscribed = result == ERR_OK;
	log_put(mqtt_subscribed ? "MQTT CAN command subscription OK" :
	                          "MQTT CAN command subscription failed; retry");
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
	(void)arg;
	incoming_expected = 0U;
	incoming_size = 0U;
	if (!my_mcu_parse_mqtt_topic(topic, &incoming_command)) {
		log_put("MQTT CAN command: invalid topic");
		return;
	}
	if (tot_len == 0U || tot_len > sizeof(incoming_payload)) {
		log_put("MQTT CAN command: invalid payload length");
		return;
	}
	incoming_expected = (u16_t)tot_len;
}


static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{

	(void)arg;
	if (incoming_expected == 0U) {
		return;
	}
	if (len > incoming_expected - incoming_size) {
		incoming_expected = 0U;
		log_put("MQTT CAN command: payload overflow");
		return;
	}
	if (len != 0U) {
		memcpy(incoming_payload + incoming_size, data, len);
		incoming_size += len;
	}
	if ((flags & MQTT_DATA_FLAG_LAST) != 0U) {
		if (incoming_size != incoming_expected ||
		    !my_mcu_send_mqtt_command(&incoming_command, incoming_payload, incoming_size)) {
			log_put("MQTT CAN command: invalid payload");
		}
		incoming_expected = 0U;
	}
}

static void mqtt_subscribe_commands(mqtt_client_t *client)
{
	mqtt_sub_tick = osKernelGetTickCount();
	err_t err = mqtt_subscribe(client, topic_can "/+/set/+", 1,
	                           mqtt_sub_request_cb, NULL);
	mqtt_sub_pending = err == ERR_OK;
	if (err != ERR_OK) {
		log_put("MQTT CAN command subscription not queued; retry");
	}
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
  (void)arg;
  mqtt_connect_pending = false;
  mqtt_sub_pending = false;
  mqtt_subscribed = false;
  incoming_expected = 0U;
  if(status == MQTT_CONNECT_ACCEPTED) {
    log_put("MQTT connected: dscan-jz-f407");
    mqtt_publish_now = true;

    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, NULL);
    mqtt_subscribe_commands(client);
    } else {
      char s[LOG_LEN - 12];
      snprintf(s, sizeof(s), "MQTT disconnected status=%d; retry in 10s", (int)status);
      log_put(s);
      mqtt_connect_tick = osKernelGetTickCount();
    }
//  } else {
//    /* Its more nice to be connected, so try to reconnect */
//  	//osDelay(100);
//		log_put("Re connect");
//    my_mqtt_do_connect(client);
//  }

}



void my_mqtt_do_connect(mqtt_client_t *client)
{
  struct mqtt_connect_client_info_t ci;
  err_t err;

  /* Setup an empty client info structure */
  memset(&ci, 0, sizeof(ci));

  /* Minimal amount of information required is client identifier, so set it here */
  ci.client_id = "dscan-jz-f407";
  ci.keep_alive = 30;
  //ci.client_user = "mosquitto";
  //ci.client_pass = "chupasangre"; /* Tiempo en mi caso */


  /* Initiate client and connect to server, if this fails immediately an error code is returned
     otherwise mqtt_connection_cb will be called with connection result after attempting
     to establish a connection with the server.
     For now MQTT version 3.1.1 is always used */
  ip_addr_t mqttServerIP;
  IP4_ADDR(&mqttServerIP, 192, 168, 1, 2);
//  err = mqtt_client_connect(client, &mqttServerIP, MQTT_PORT, mqtt_connection_cb, 0, &ci);
  mqtt_connect_attempted = true;
  mqtt_connect_tick = osKernelGetTickCount();
  err = mqtt_client_connect(client, &mqttServerIP, MQTT_PORT, mqtt_connection_cb, NULL, &ci);
  mqtt_connect_pending = err == ERR_OK;

  /* For now just print the result code if something goes wrong */
  if(err != ERR_OK) {
	  char s[LOG_LEN];
      sprintf(s, "MQTT Con. error %d", err);
	  log_put(s);
  } else {
	  char s[LOG_LEN];
	  sprintf(s, "MQTT connecting 192.168.1.2:1883");
	  log_put(s);
	}
}

static err_t my_mqtt_publish(mqtt_client_t *client, const char *t, const char *m);

void my_mqtt_service(void *argument)
{
    static mqtt_msg__t pending;
    static bool has_pending;
    (void)argument;
    /* Called only through tcpip_callback: all raw MQTT API calls stay in LwIP. */
    uint32_t now = osKernelGetTickCount();
    uint32_t interval = 10U * osKernelGetTickFreq();
    if (mqtt_client == NULL || !mqtt_client_is_connected(mqtt_client)) {
        if (mqtt_connect_pending ||
            (mqtt_connect_attempted && (uint32_t)(now - mqtt_connect_tick) < interval)) {
            return;
        }
        if (mqtt_client == NULL) {
            mqtt_client = mqtt_client_new();
            if (mqtt_client == NULL) {
                log_put("MQTT client allocation failed; retry in 10s");
                mqtt_connect_attempted = true;
                mqtt_connect_tick = now;
                return;
            }
        }
        my_mqtt_do_connect(mqtt_client);
        return;
    }
    if (!mqtt_subscribed && !mqtt_sub_pending &&
        (uint32_t)(now - mqtt_sub_tick) >= interval) {
        mqtt_subscribe_commands(mqtt_client);
    }
    if (mqtt_publish_now || (uint32_t)(now - mqtt_publish_tick) >= interval) {
        char payload[11];
        snprintf(payload, sizeof(payload), "%lu",
                 (unsigned long)(now / osKernelGetTickFreq()));
        if (my_mqtt_publish(mqtt_client, "dscan/uptime", payload) != ERR_MEM) {
            mqtt_publish_now = false;
            mqtt_publish_tick = now;
        }
    }
    for (unsigned i = 0; i < 4U; ++i) {
        if (!has_pending) {
            osStatus_t status = osMessageQueueGet(QueueTxMqttHandle, &pending, NULL, 0U);
            if (status != osOK) {
                if (status != osErrorResource) {
                    log_put("MQTT CAN report queue read failed");
                }
                break;
            }
            has_pending = true;
        }
        if (my_mqtt_publish(mqtt_client, pending.topic, pending.value) == ERR_MEM) {
            break;
        }
        has_pending = false;
    }
}


/* Called when publish is complete either with sucess or failure */
static void mqtt_pub_request_cb(void *arg, err_t result)
{
  if(result != ERR_OK) {
	  char s[LOG_LEN];
	  sprintf(s, "Pub. result: %d\n", result);
	  log_put(s);
  } else {
      log_put("MQTT publish sent (QoS0)");
  }
}


static err_t my_mqtt_publish(mqtt_client_t *client, const char *t, const char *m)
{
	if (! mqtt_client_is_connected(client)){
		log_put("MQTT publish failed: not connected");
		return ERR_CONN;
	}
  err_t err;
  u8_t qos = 0; // 0 - bez potwierdzenia odbioru
  u8_t retain = 0; // No don't retain such crappy payload...
  char topic[TOPIC_LEN];
  int len = snprintf(topic, sizeof(topic), "%s/%s", topic_root, t);
  if (len < 0 || (size_t)len >= sizeof(topic)) {
    log_put("MQTT publish: invalid topic length");
    return ERR_ARG;
  }

//  if (mqtt_output_check_space(client->output, 20) == 0) {

 // }
  uint16_t ml = strlen(m);


  err = mqtt_publish(client, topic, m, ml, qos, retain, mqtt_pub_request_cb, 0);
  if (err == ERR_MEM) {
    return err;
  }
  if(err != ERR_OK) {
	  char s[LOG_LEN];
	    sprintf(s, "Pub. err: %d (%s)", err, t); 	log_put(s);
//	  uint8_t l = tl - MIN(tl,10);
//    sprintf(s, "Pub. err: %d (%s)", err, &t+l); 	log_put(s);
//    sprintf(s, "Pub. err: %d (%d+%d)", err, tl, ml); 	log_put(s);
//    sprintf(s, "Title>%s<", t); 			log_put(s);
//    sprintf(s, "Msg>%s<", m);   			log_put(s);
//    Error_Handler();
//    osDelay(1000);
    //HAL_NVIC_SystemReset();
    //while (1) {}; // reset przez wotch dog
  } else {
      char s[LOG_LEN - 12];
      snprintf(s, sizeof(s), "MQTT queued %s=%s", t, m);
      log_put(s);
  }
  return err;
}


void my_mqtt_to_Queue(const char *t, const char *m){

	mqtt_msg__t mqtt_msg = {0};
	size_t topic_len = strlen(t);
	size_t value_len = strlen(m);
	if (topic_len >= sizeof(mqtt_msg.topic) || value_len >= sizeof(mqtt_msg.value)) {
		log_put("MQTT CAN report too long");
		return;
	}
	memcpy(mqtt_msg.topic, t, topic_len + 1U);
	memcpy(mqtt_msg.value, m, value_len + 1U);
	osStatus_t status = osMessageQueuePut(QueueTxMqttHandle, &mqtt_msg, 0U, 0U);
	if (status != osOK) {
		 char s[LOG_LEN];
		 sprintf(s, "QPut mq er %d", status);
		 log_put(s);
	}
}
