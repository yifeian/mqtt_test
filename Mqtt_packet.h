/*
 * Mqtt_packet.h
 *
 *  Created on: Dec 3, 2018
 *      Author: yifeifan
 */

#ifndef MQTT_PACKET_H_
#define MQTT_PACKET_H_

#include "unp.h"

#define MQTT_DATA_LEN_SIZE  2

typedef enum _MqttQos
{
	MQTT_QOS_0 = 0,
	MQTT_QOS_1 = 1,
	MQTT_QOS_2 = 2,
	MQTT_QOS_3 = 3,
}MqttQos;

/* generic message */

typedef struct _MqttMessage{
	uint16_t    packet_id;
	MqttQos    qos;
	uint8_t     retain;
	uint8_t     dupicate;
	const char * topic_name;
	uint8_t     *message;
	uint16_t    message_len;
}MqttMessage;

/* topic */

typedef struct _MqttTopic
{
	const char *topic_filter;
	/* these are only on subscribe */
	MqttQos qos;
	uint8_t return_code;/* MqttSubscribeAckReturnCodes */
}MqttTopic;

/* packet header */

#define MQTT_PACKET_TYPE_GET(x)    (((x) >> 4)&0xf)
#define MQTT_PACKET_TYPE_SET(x)    (((x)&0xff) << 4)

typedef enum _MqttPacketType {
    MQTT_PACKET_TYPE_RESERVED = 0,
    MQTT_PACKET_TYPE_CONNECT = 1,
    MQTT_PACKET_TYPE_CONNECT_ACK = 2,       /* Acknowledgment */
    MQTT_PACKET_TYPE_PUBLISH = 3,
    MQTT_PACKET_TYPE_PUBLISH_ACK = 4,       /* Acknowledgment */
    MQTT_PACKET_TYPE_PUBLISH_REC = 5,       /* Received */
    MQTT_PACKET_TYPE_PUBLISH_REL= 6,        /* Release */
    MQTT_PACKET_TYPE_PUBLISH_COMP = 7,      /* Complete */
    MQTT_PACKET_TYPE_SUBSCRIBE = 8,
    MQTT_PACKET_TYPE_SUBSCRIBE_ACK = 9,     /* Acknowledgment */
    MQTT_PACKET_TYPE_UNSUBSCRIBE = 10,
    MQTT_PACKET_TYPE_UNSUBSCRIBE_ACK = 11,  /* Acknowledgment */
    MQTT_PACKET_TYPE_PING_REQ = 12,         /* Request */
    MQTT_PACKET_TYPE_PING_RESP = 13,        /* Response */
    MQTT_PACKET_TYPE_DISCONNECT = 14,
    MQTT_PACKET_TYPE_MAX = 15,
} MqttPacketType;

enum MqttPacketFlags {
    MQTT_PACKET_FLAG_RETAIN = 0x1,
    MQTT_PACKET_FLAG_QOS_SHIFT = 0x1,
    MQTT_PACKET_FLAG_QOS_MASK = 0x6,
    MQTT_PACKET_FLAG_DUPLICATE = 0x8,
};
/* packet flag bit-mask, bit 0-3 */
#define  MQTT_PACKET_FLAGS_GET(x)    ((x)&0xf)
#define  MQTT_PACKET_FLAGS_SET(x)    (x)
#define  MQTT_PACKET_FLAGS_GET_QOS(type_flags) \
	((MQTT_PACKET_FLAGS_GET((type_flags))& MQTT_PACKET_FLAG_QOS_MASK) >> MQTT_PACKET_FLAG_QOS_SHIFT)
#define  MQTT_PACKET_FLAGS_SET_QOS(qos) \
	(MQTT_PACKET_FLAGS_SET(((qos) << MQTT_PACKET_FLAG_QOS_SHIFT) & MQTT_PACKET_FLAG_QOS_MASK))

/* packet header size avalibale 2-5 bits*/

#define MQTT_PACKET_MAX_LEN_BYTES   4
#define MQTT_PACKET_LEN_ENCODE_MASK 0x80

typedef struct _MqttPacket
{
	/* type: bit 4-7,flag  0-3 */
	uint8_t   type_flags;
	/* remaining length variable 1-4 bytes ,encoded using */
	uint8_t   len[MQTT_PACKET_MAX_LEN_BYTES];
}MqttPacket;

/* CONNECT PACKET */
/* Connect flag bit-mask: Located in byte 8 of the MqttConnect packet */
#define MQTT_CONNECT_FLAG_GET_QOS(flags) \
    (((flags) MQTT_CONNECT_FLAG_WILL_QOS_MASK) >> MQTT_CONNECT_FLAG_WILL_QOS_SHIFT)
#define MQTT_CONNECT_FLAG_SET_QOS(qos) \
    (((qos) << MQTT_CONNECT_FLAG_WILL_QOS_SHIFT) & MQTT_CONNECT_FLAG_WILL_QOS_MASK)
enum MqttConnectFlags {
    MQTT_CONNECT_FLAG_RESERVED = 0x01,
    MQTT_CONNECT_FLAG_CLEAN_SESSION = 0x02,
    MQTT_CONNECT_FLAG_WILL_FLAG = 0x04,
    MQTT_CONNECT_FLAG_WILL_QOS_SHIFT = 3,
    MQTT_CONNECT_FLAG_WILL_QOS_MASK = 0x18,
    MQTT_CONNECT_FLAG_WILL_RETAIN = 0x20,
    MQTT_CONNECT_FLAG_PASSWORD = 0x40,
    MQTT_CONNECT_FLAG_USERNAME = 0x80,
};

/* connect protocol */
#define  MQTT_CONNECT_PROTOCOL_NAME_LEN  4
#define  MQTT_CONNECT_PROTOCOL_NAME    "MQTT"
#define  MQTT_CONNECT_PROTOCOL_LEVEL    4

/* Initializer */
#define  MQTT_CONNECT_INIT    {{0,MQTT_CONNECT_PROTOCOL_NAME_LEN},{'M','Q','T','T'},MQTT_CONNECT_PROTOCOL_LEVEL,0,0}
/* connect packet structure */
typedef struct _MqttConnectPacket
{
	uint8_t  protocol_len[MQTT_DATA_LEN_SIZE];
	uint8_t  protocol_name[MQTT_CONNECT_PROTOCOL_NAME_LEN];
	uint8_t  protocol_level;
	uint8_t  flags;
	uint16_t keep_alive;
}MqttConnectPacket;

/* CONNECT ACKNOWLEDGE PACKET */
/* Connect Ack flags */
enum MqttConnectAckFlags {
    MQTT_CONNECT_ACK_FLAG_SESSION_PRESENT = 0x01,
};
/* Connect Ack return codes */
enum MqttConnectAckReturnCodes {
    MQTT_CONNECT_ACK_CODE_ACCEPTED = 0,             /* Connection accepted */
    MQTT_CONNECT_ACK_CODE_REFUSED_PROTO = 1,        /* The Server does not support the level of the MQTT protocol requested by the Client */
    MQTT_CONNECT_ACK_CODE_REFUSED_ID = 2,           /* The Client identifier is correct UTF-8 but not allowed by the Server */
    MQTT_CONNECT_ACK_CODE_REFUSED_UNAVAIL = 3,      /* The Network Connection has been made but the MQTT service is unavailable */
    MQTT_CONNECT_ACK_CODE_REFUSED_BAD_USER_PWD = 4, /* The data in the user name or password is malformed */
    MQTT_CONNECT_ACK_CODE_REFUSED_NOT_AUTH = 5,     /* The Client is not authorized to connect */
};

/* Connect Ack packet structure */
typedef struct _MqttConnectAck
{
	uint8_t  flags;
	uint8_t  return_code;
}MqttConnectAck;/*no payload */

/* Connect */
typedef struct _MqttConnect {
    uint16_t    keep_alive_sec;
    uint8_t     clean_session;
    const char *client_id;

    /* Optional Last will and testament */
    uint8_t     enable_lwt;
    MqttMessage *lwt_msg;

    /* Optional login */
    const char *username;
    const char *password;

    /* Ack data */
    MqttConnectAck ack;
} MqttConnect;

/* publist packet */
/* packetid sent only if Qos > 0*/

typedef MqttMessage MqttPublish;  /* Publish message */

/* PUBLISH RESPONSE PACKET */
/* This is the response struct for PUBLISH_ACK, PUBLISH_REC and PUBLISH_COMP */
/* If QoS = 0: No response */
/* If QoS = 1: Expect response packet with type = MQTT_PACKET_TYPE_PUBLISH_ACK */
/* If QoS = 2: Expect response packet with type = MQTT_PACKET_TYPE_PUBLISH_REC */
/* Packet Id required if QoS is 1 or 2 */
/* If Qos = 2: Send MQTT_PACKET_TYPE_PUBLISH_REL with PacketId to complete QoS2 protcol exchange */
/*  Expect response packet with type = MQTT_PACKET_TYPE_PUBLISH_COMP */

typedef struct _MqttPublishresp
{
	uint16_t  packet_id;
}MqttPublishResp;

/* subscribe packet */
/* Packet Id followed by contiguous list of topics w/Qos to subscribe to. */
typedef struct _MqttSubscribe {
    uint16_t      packet_id;
    int         topic_count;
    MqttTopic  *topics;
} MqttSubscribe;

typedef MqttSubscribe MqttUnsubscribe;

/* SUBSCRIBE ACK PACKET */
/* Packet Id followed by list of return codes coorisponding to subscription topic list sent. */
enum MqttSubscribeAckReturnCodes {
    MQTT_SUBSCRIBE_ACK_CODE_SUCCESS_MAX_QOS0 = 0,
    MQTT_SUBSCRIBE_ACK_CODE_SUCCESS_MAX_QOS1 = 1,
    MQTT_SUBSCRIBE_ACK_CODE_SUCCESS_MAX_QOS2 = 2,
    MQTT_SUBSCRIBE_ACK_CODE_FAILURE = 0x80,
};
typedef struct _MqttSubscribeAck {
    uint16_t      packet_id;
    uint8_t       *return_codes; /* MqttSubscribeAckReturnCodes */
} MqttSubscribeAck;

/* unsubscribe response ack*/
/* no response  payload(besides packet id) */
typedef struct _MqttUnsubscribeAck
{
	uint16_t packet_id;
}MqttUnsubscribeAck;

/* mqtt packet application interface */
struct _MqttClient;


#endif /* MQTT_PACKET_H_ */
