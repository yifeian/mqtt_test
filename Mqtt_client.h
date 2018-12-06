/*
 * Mqtt_client.h
 *
 *  Created on: Dec 3, 2018
 *      Author: yifeifan
 */

#ifndef MQTT_CLIENT_H_
#define MQTT_CLIENT_H_

#include "unp.h"
#include "Mqtt_socket.h"



/* Allow custom override of data types */
#ifndef WOLFMQTT_CUSTOM_TYPES
    /* Basic Types */
    #ifndef byte
        typedef unsigned char  byte;
    #endif
    typedef unsigned short word16;
    typedef unsigned int   word32;
#endif

/* Response Codes */
enum MqttPacketResponseCodes {
	MQTT_CODE_SUCCESS = 0,
	MQTT_CODE_ERROR_BAD_ARG = -1,
	MQTT_CODE_ERROR_OUT_OF_BUFFER = -2,
	MQTT_CODE_ERROR_MALFORMED_DATA = -3, /* Error (Malformed Remaining Length) */
	MQTT_CODE_ERROR_PACKET_TYPE = -4,
	MQTT_CODE_ERROR_PACKET_ID = -5,
	MQTT_CODE_ERROR_TLS_CONNECT = -6,
	MQTT_CODE_ERROR_TIMEOUT = -7,
	MQTT_CODE_ERROR_NETWORK = -8,
};

/* Client flags */
enum MqttClientFlags {
    MQTT_CLIENT_FLAG_IS_CONNECTED = 0x01,
    MQTT_CLIENT_FLAG_IS_TLS = 0x02,
};

struct _MqttClient;
typedef int (*MqttMsgCb)(struct _MqttClient *client, MqttMessage *message);

/* Client structure */
typedef struct _MqttClient
{
    uint32_t       flags; /* MqttClientFlags */
    int          cmd_timeout_ms;

    uint8_t        *tx_buf;
    int          tx_buf_len;
    uint8_t        *rx_buf;
    int          rx_buf_len;

    MqttNet     *net;   /* Pointer to network callbacks and context */
    //MqttTls      tls;   /* WolfSSL context for TLS */

    MqttMsgCb    msg_cb;
} MqttClient;



#endif /* MQTT_CLIENT_H_ */
