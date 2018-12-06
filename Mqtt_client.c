/*
 * Mqtt_client.c
 *
 *  Created on: Dec 3, 2018
 *      Author: yifeifan
 */

#include "unp.h"

int MqttClient_Init(MqttClient *client, MqttNet *net,
		MqttMsgCb msg_cb, byte *tx_buf, int tx_buf_len,
		byte *rx_buf, int rx_buf_len,
		int cmd_timeout_ms)
{
	int rc = MQTT_CODE_SUCCESS;
	if(client == NULL ||
	   tx_buf == NULL || tx_buf_len <= 0 ||
	   rx_buf == NULL || rx_buf_len <= 0)
	{
		return MQTT_CODE_ERROR_BAD_ARG;
	}

	/* init the client struct to zero */
	bzero(client, sizeof(MqttClient));

	/* setup client structure */
	client->msg_cb = msg_cb;
	client->rx_buf = rx_buf;
	client->tx_buf = tx_buf;
	client->rx_buf_len = rx_buf_len;
	client->tx_buf_len = tx_buf_len;
	client->cmd_timeout_ms = cmd_timeout_ms;

	rc = MqttSocket_Init(client, net);
	return rc;
}

static int MqttClient_WaitType(MqttClient *client, int timeout_ms, byte wait_type,
		word16 wait_packet_id, void *p_decode)
{
	int rc,len;
	MqttPacket *header;
	byte msg_type, msg_qos;
	word16 packet_id = 0;

	while(1)
	{
		rc = MqttSocket_Read(client, client->rx_buf, client->rx_buf_len,timeout_ms);
		if(rc <= 0)
			return rc;
		len = rc;
		/* Determine packet type */
		header = (MqttPacket *)client->rx_buf;
		msg_type = MQTT_PACKET_TYPE_GET(header->type_flags);
		msg_qos = MQTT_PACKET_FLAGS_GET_QOS(header->type_flags);

		printf("Read Packet: Len %d, Type %d, Qos %d\n", len, msg_type, msg_qos);
		switch(msg_type){
			case MQTT_PACKET_TYPE_CONNECT_ACK:
			{
				/* decode connect ack */
				MqttConnectAck connect_ack, *p_connect_ack = &connect_ack;
				if(p_decode)
					p_connect_ack = p_decode;
				rc = MqttDecode_ConenctAck(client->rx_buf, len, p_connect_ack);
				if (rc <= 0)
					return rc;
				break;
			}
			case MQTT_PACKET_TYPE_PING_RESP:
			{
				rc = MqttDecode_Ping(client->rx_buf,len);
				if(rc <= 0)
					return rc;
				break;
			}
			case MQTT_PACKET_TYPE_SUBSCRIBE_ACK:
			{
				MqttSubscribeAck subscribe_ack, *p_subscribe_ack = &subscribe_ack;
				if(p_decode)
					p_subscribe_ack = p_decode;
				rc = MqttDecode_SubscribeAck(client->rx_buf, len, p_subscribe_ack);
				if(rc <= 0)
					return rc;
				packet_id = p_subscribe_ack->packet_id;
				break;
			}
			case MQTT_PACKET_TYPE_PUBLISH:
			{
				/* decode publish message */
				MqttMessage message;
				rc = MqttDecode_Publish(client->rx_buf, len, &message);
				if(rc <= 0)
					return rc;
				/* issue callback for new message  */
				if(client->msg_cb)
				{
					rc = client->msg_cb(client, &message);
					if(rc != MQTT_CODE_SUCCESS)
						return rc;
				}
				/* handle qos   */
				if(msg_qos > MQTT_QOS_0)
				{
					MqttPublishResp publish_resp;
					MqttPacketType type;
					packet_id = message.packet_id;

					/* determine packet type to write */
					type = (msg_qos == MQTT_QOS_1) ? MQTT_PACKET_TYPE_PUBLISH_ACK : MQTT_PACKET_TYPE_PUBLISH_REC;
					publish_resp.packet_id = packet_id;

					/* encode publish response */
					rc = MqttEncode_PublishResp(client->tx_buf, client->tx_buf_len, type, &publish_resp);
					if(rc <= 0)
						return rc;
					len = rc;
					rc = MqttPacket_Write(client, client->tx_buf, len);
					if(rc <= 0)
						return rc;
				}
				break;
			}
			case MQTT_PACKET_TYPE_PUBLISH_ACK:
			case MQTT_PACKET_TYPE_PUBLISH_REC:
			case MQTT_PACKET_TYPE_PUBLISH_REL:
			case MQTT_PACKET_TYPE_PUBLISH_COMP:
			{
				MqttPublishResp publish_resp, *p_publish_resp = &publish_resp;
				if(p_decode)
					p_publish_resp = p_decode;
				/* Decode publish response message */
				rc = MqttDecode_PublishResp(client->rx_buf, len, msg_type, p_publish_resp);
				if(rc <= 0)
					return rc;
				packet_id = p_publish_resp->packet_id;

				/* if qos = 2 then send response */
				if(msg_type == MQTT_PACKET_TYPE_PUBLISH_REC || msg_type == MQTT_PACKET_TYPE_PUBLISH_REL)
				{
					publish_resp.packet_id = packet_id;
					rc = MqttEncode_PublishResp(client->tx_buf, client->tx_buf_len, msg_type + 1, &publish_resp);
					if(rc <= 0)
						return rc;
					len = rc;
					/* send packet */
					rc = MqttPacket_Write(client, client->rx_buf, len);
					if(rc != len)
						return rc;
				}
				break;
			}

			default:
				printf("MqttClient_WaitMessage: Invalid client packet type %u!\n", msg_type);
				return MQTT_CODE_ERROR_BAD_ARG;
		}
		if(wait_type < MQTT_PACKET_TYPE_MAX)
		{
			if(wait_type == msg_type)
			{
				 if(wait_packet_id == 0 || wait_packet_id == packet_id) {
					 /* We found the packet type and id */
					 break;
				 }
			}
		}
	}
	return MQTT_CODE_SUCCESS;
}

int MqttClient_Connect(MqttClient *client, MqttConnect *connect)
{
	int rc, len;
	/* validate required argument */
	if(client == NULL || connect == NULL)
	{
		return MQTT_CODE_ERROR_BAD_ARG;
	}

	/* Encode the connect packet */
	rc = MqttEncode_Connect(client->tx_buf, client->tx_buf_len, connect);
	if(rc <= 0)
		return rc;
	len = rc;
	/*send connect packet */
	rc = MqttPacket_Write(client, client->tx_buf, len);
	if(rc != len)
		return rc;
	rc = MqttClient_WaitType(client, client->cmd_timeout_ms,MQTT_PACKET_TYPE_CONNECT_ACK,0,&connect->ack);
	if(rc <= 0)
		return rc;
	return MQTT_CODE_SUCCESS;
}
const char* MqttClient_ReturnCodeToString(int return_code)
{
    switch(return_code) {
        case MQTT_CODE_SUCCESS:
            return "Success";
        case MQTT_CODE_ERROR_BAD_ARG:
            return "Error (Bad argument)";
        case MQTT_CODE_ERROR_OUT_OF_BUFFER:
            return "Error (Out of buffer)";
        case MQTT_CODE_ERROR_MALFORMED_DATA:
            return "Error (Malformed Remaining Length)";
        case MQTT_CODE_ERROR_PACKET_TYPE:
            return "Error (Packet Type Mismatch)";
        case MQTT_CODE_ERROR_PACKET_ID:
            return "Error (Packet Id Mismatch)";
        case MQTT_CODE_ERROR_TLS_CONNECT:
            return "Error (TLS Connect)";
        case MQTT_CODE_ERROR_TIMEOUT:
            return "Error (Timeout)";
        case MQTT_CODE_ERROR_NETWORK:
            return "Error (Network)";
    }
    return "Unknown";
}
int MqttClient_WaitMessage(MqttClient *client, int timeout_ms)
{
    return MqttClient_WaitType(client, timeout_ms, MQTT_PACKET_TYPE_MAX, 0, NULL);
}

int MqttClient_NetConnect(MqttClient *client, const char* host,
    word16 port, int timeout_ms, int use_tls)
{
    return MqttSocket_Connect(client, host, port, timeout_ms);
}
/*
int MqttClient_NetDisconnect(MqttClient *client)
{
    return MqttSocket_Disconnect(client);
}
*/
void err_sys(const char* msg)
{
    printf("wolfMQTT error: %s\n", msg);
    if (msg) {
        exit(EXIT_FAILURE);
    }
}

int MqttClient_Ping(MqttClient *client)
{
	int rc,len;
	if(client == NULL)
		return MQTT_CODE_ERROR_BAD_ARG;
	/* encode the subscribe packet  */
	rc = MqttEncode_Ping(client->tx_buf,client->tx_buf_len);
	if(rc < 0)
		return rc;
	len = rc;

	/* sending ping */
	rc = MqttPacket_Write(client, client->tx_buf, len);
	if(rc != len)
		return rc;
	/* wait for ping resp packet */
	rc = MqttClient_WaitType(client, client->cmd_timeout_ms,\
			MQTT_PACKET_TYPE_PING_RESP,0,NULL);
	if(rc <= 0)
		return rc;
	return MQTT_CODE_SUCCESS;
}
int MqttClient_Subscribe(MqttClient *client, MqttSubscribe *subscribe)
{
	int rc,len,i;
	MqttSubscribeAck subscribe_ack;
	MqttTopic *topic;
	if(client == NULL || client == subscribe)
		return MQTT_CODE_ERROR_BAD_ARG;
	/* encode the subscribe packet */
	rc = MqttEncode_Subscribe(client->tx_buf,client->tx_buf_len,subscribe);
	if(rc <= 0)
		return rc;
	len = rc;
	rc = MqttPacket_Write(client, client->tx_buf, len);
	if(rc != len)
		return rc;
	/* wait for subscribe ack packet */
	rc = MqttClient_WaitType(client, client->cmd_timeout_ms, MQTT_PACKET_TYPE_SUBSCRIBE_ACK,\
			subscribe->packet_id,&subscribe_ack);
	if(rc < 0)
		return rc;
	for(i = 0; i < subscribe->topic_count; i++)
	{
		topic = &subscribe->topics[i];
		topic->return_code = subscribe_ack.return_codes[i];
	}

	return MQTT_CODE_SUCCESS;
}

int MqttClient_Publish(MqttClient *client, MqttPublish *publish)
{
	int rc, len;
	if(client == NULL || publish == NULL)
		return MQTT_CODE_ERROR_BAD_ARG;
	rc = MqttEncode_Publish(client->tx_buf, client->tx_buf_len, publish);
	if(rc <= 0)
		return rc;
	len = rc;
	rc = MqttPacket_Write(client, client->tx_buf, len);
	if(rc != len)
		return rc;
	if(publish->qos > MQTT_QOS_0)
	{
		MqttPacketType type = (publish->qos == MQTT_QOS_1) ?\
				MQTT_PACKET_TYPE_PUBLISH_ACK : MQTT_PACKET_TYPE_PUBLISH_COMP;

		/* wait for publish response packet */
		rc = MqttClient_WaitType(client, client->cmd_timeout_ms, \
				type, publish->packet_id, NULL);
		if(rc <= 0)
			return rc;
	}
	return MQTT_CODE_SUCCESS;


}
