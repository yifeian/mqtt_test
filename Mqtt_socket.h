/*
 * Mqtt_socket.h
 *
 *  Created on: Dec 3, 2018
 *      Author: yifeifan
 */

#ifndef MQTT_SOCKET_H_
#define MQTT_SOCKET_H_

#include "unp.h"

#define MQTT_DEFAULT_PORT 1883
#define MQTT_SECURE_PORT  8883

typedef int(*MqttNetConnectCB)(void *context, const char *host, uint16_t port,
		int timeout_ms);
typedef int(*MqttNetReadCB)(void *context, uint8_t *buf, int buf_len,
		int timeout_ms);
typedef int(*MqttNetWriteCB)(void *context, const uint8_t *buf, int buf_len,
		int timeout_ms);
typedef int(*MqttNetDisconnectCb)(void *context);

typedef struct _MqttNet
{
	void                 *context;
	MqttNetConnectCB     connect;
	MqttNetReadCB        read;
	MqttNetWriteCB       write;
	MqttNetDisconnectCb  disconnect;
}MqttNet;

#endif /* MQTT_SOCKET_H_ */
