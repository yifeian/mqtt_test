/*
 * Mqtt_packet.c
 *
 *  Created on: Dec 3, 2018
 *      Author: yifeifan
 */
#include "unp.h"

static int MqttEncode_FixedHeader(byte *tx_buf, int tx_buf_len, int remain_len,
		byte type, byte retain, byte qos, byte duplicate)
{
	int header_len;
	MqttPacket header;

	/* encode the length remaining into the header */
	header_len = MqttEncode_Remainlen(&header, remain_len);
	if(header_len < 0)
		return header_len;
    header_len += 1; /* For packet type and flags */

    /* Check to make sure length is within max provided buffer */
    if ((header_len + remain_len) > tx_buf_len) {
        return MQTT_CODE_ERROR_OUT_OF_BUFFER;
    }
    /* Encode fixed header */
    header.type_flags = MQTT_PACKET_TYPE_SET(type) | MQTT_PACKET_FLAGS_SET(0);
    if(retain)
    	header.type_flags |= MQTT_PACKET_FLAGS_SET(MQTT_PACKET_FLAG_RETAIN);
    if(qos)
    	header.type_flags |= MQTT_PACKET_FLAGS_SET_QOS(qos);
    if(duplicate)
    	header.type_flags |= MQTT_PACKET_FLAGS_SET(MQTT_PACKET_FLAG_DUPLICATE);
    memcpy(tx_buf, &header, header_len);
    return header_len;
}

static int MqttDecode_FixedHeader(byte *rx_buf, int rx_buf_len, int *remain_len,
		byte type)
{
	int header_len;
	MqttPacket *header = (MqttPacket *)rx_buf;
	header_len = MqttDecode_RemainLen(header, rx_buf_len, remain_len);
	if(header_len < 0)
		return header_len;
	header_len += 1;
	if(rx_buf_len < (header_len + *remain_len))
		return MQTT_CODE_ERROR_OUT_OF_BUFFER;

	if(MQTT_PACKET_TYPE_GET(header->type_flags) != type)
		return MQTT_CODE_ERROR_PACKET_TYPE;
	return header_len;
}

/* Returns number of encoded bytes, errors are negative value */
int MqttEncode_Remainlen(MqttPacket *header, int remain_len)
{
    int encode_bytes = 0;
    byte tmp_len;

    if (header == NULL || remain_len < 0) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    do {
        tmp_len = (remain_len % MQTT_PACKET_LEN_ENCODE_MASK);
        remain_len /= MQTT_PACKET_LEN_ENCODE_MASK;

        /* If more length, set the top bit of this byte */
        if (remain_len > 0) {
            tmp_len |= MQTT_PACKET_LEN_ENCODE_MASK;
        }
        header->len[encode_bytes++] = tmp_len;
    } while (remain_len > 0);

    return encode_bytes;
}
/* Returns number of buffer bytes decoded */
int MqttDecode_Num(byte *buf, word16 *len)
{
	if(len)
	{
		*len = buf[0];
		*len = (*len << 8) | buf[1];
	}
	return MQTT_DATA_LEN_SIZE;
}

/* return numbers of buffer bytes encode */
int MqttEncode_Num(byte *buf, word16 len)
{
	buf[0] = len >> 8;
	buf[1] = len & 0xff;
	return MQTT_DATA_LEN_SIZE;
}

int MqttEncode_String(byte *buf, const char *str)
{
	int str_len = (int)strlen(str);
	int len = MqttEncode_Num(buf, str_len);
	buf += len;
	memcpy(buf,str,str_len);
	return len+str_len;
}
/* Returns number of buffer bytes encoded */
int MqttEncode_Data(byte *buf, const byte *data, word16 data_len)
{
	int len = MqttEncode_Num(buf, data_len);
	buf += len;
	memcpy(buf, data, data_len);
	return len + data_len;
}

int MqttEncode_Connect(byte *tx_buf, int tx_buf_len, MqttConnect *connect)
{
	int header_len, remain_len;
	MqttConnectPacket packet = MQTT_CONNECT_INIT;
	byte *tx_payload;

	if(tx_buf == NULL || connect == NULL || tx_buf_len <= 0)
		return MQTT_CODE_ERROR_BAD_ARG;

	 /* Determine packet length */
	remain_len = sizeof(MqttConnectPacket);
	remain_len += (int)strlen(connect->client_id) + MQTT_DATA_LEN_SIZE;

	if(connect->enable_lwt)
	{
        if (connect->lwt_msg == NULL || connect->lwt_msg->topic_name == NULL ||
            connect->lwt_msg->message == NULL || connect->lwt_msg->message_len <= 0)
        {
            return MQTT_CODE_ERROR_BAD_ARG;
        }
        remain_len += (int)strlen(connect->lwt_msg->topic_name) + MQTT_DATA_LEN_SIZE;
        remain_len += connect->lwt_msg->message_len + MQTT_DATA_LEN_SIZE;
	}
    if (connect->username) {
        remain_len += (int)strlen(connect->username) + MQTT_DATA_LEN_SIZE;
    }
    if (connect->password) {
        remain_len += (int)strlen(connect->password) + MQTT_DATA_LEN_SIZE;
    }
    /* Encode fixed header */
    header_len = MqttEncode_FixedHeader(tx_buf, tx_buf_len, remain_len,MQTT_PACKET_TYPE_CONNECT,0,0,0);

    if(header_len < 0)
    	return header_len;
    tx_payload = &tx_buf[header_len];

    /* encode variable header */
    /* set connection flags */
    if(connect->clean_session)
    	packet.flags |= MQTT_CONNECT_FLAG_CLEAN_SESSION;
    if(connect->enable_lwt)
    {
    	packet.flags |= MQTT_CONNECT_FLAG_WILL_FLAG;
    	if(connect->lwt_msg->qos)
    		packet.flags |= MQTT_CONNECT_FLAG_SET_QOS(connect->lwt_msg->qos);
    	 if (connect->lwt_msg->retain)
    	    packet.flags |= MQTT_CONNECT_FLAG_WILL_RETAIN;
    }
    if (connect->username) {
        packet.flags |= MQTT_CONNECT_FLAG_USERNAME;
    }
    if (connect->password) {
        packet.flags |= MQTT_CONNECT_FLAG_PASSWORD;
    }
    MqttEncode_Num((byte *)&packet.keep_alive, connect->keep_alive_sec);
    memcpy(tx_payload, &packet, sizeof(MqttConnectPacket));
    tx_payload += sizeof(MqttConnectPacket);
    /* encode payload */
    tx_payload += MqttEncode_String(tx_payload, connect->client_id);
    if (connect->enable_lwt) {
        tx_payload += MqttEncode_String(tx_payload, connect->lwt_msg->topic_name);
        tx_payload += MqttEncode_Data(tx_payload, connect->lwt_msg->message, connect->lwt_msg->message_len);
    }
    if (connect->username) {
        tx_payload += MqttEncode_String(tx_payload, connect->username);
    }
    if (connect->password) {
        tx_payload += MqttEncode_String(tx_payload, connect->password);
    }

    return header_len+remain_len;
}

int MqttPacket_Write(MqttClient *client, byte* tx_buf, int tx_buf_len)
{
    int rc;
    rc = MqttSocket_Write(client, tx_buf, tx_buf_len, client->cmd_timeout_ms);
    return rc;
}

/* Packet Element Encoders/Decoders */
/* Returns number of decoded bytes, errors are negative value */
int MqttDecode_RemainLen(MqttPacket *header, int buf_len, int *remain_len)
{
	int decode_bytes = 0;
	int multiplier = 1;
	byte temp_len;

	if(header == NULL || buf_len <= 0 || remain_len == NULL)
		return MQTT_CODE_ERROR_BAD_ARG;
	*remain_len = 0;
	do {
		if(decode_bytes >= buf_len)
			return 0;/* incidates we need another byte */
		if(decode_bytes >= MQTT_PACKET_MAX_LEN_BYTES)
			return MQTT_CODE_ERROR_MALFORMED_DATA;
		temp_len = header->len[decode_bytes++];
		*remain_len += (temp_len & ~MQTT_PACKET_LEN_ENCODE_MASK) * multiplier;
		multiplier *= MQTT_PACKET_LEN_ENCODE_MASK;
	}while (temp_len & MQTT_PACKET_LEN_ENCODE_MASK);
	return decode_bytes;
}

int MqttPacket_Read(MqttClient *client, byte *rx_buf, int rx_buf_len, int timeout_ms)
{
	int rc, header_len = 1, remain_len = 0;
	MqttPacket *header = (MqttPacket *)rx_buf;

	/* read fix header portion */
	rc = MqttSocket_Read(client, &rx_buf[0],header_len + 1, timeout_ms);
	if(rc != header_len + 1)
		return rc;
	do{
		rc = MqttDecode_RemainLen(header, header_len, &remain_len);
		if(rc < 0)
			return rc;/* indicates error */
		if(rc > 0)
			header_len = rc;
		header_len++;
		rc = MqttSocket_Read(client, &rx_buf[header_len+1], 1, timeout_ms);
	}while (header_len < (int)sizeof(MqttPacket));

    header_len += 1; /* For packet type and flags */

    /* Validate we have enough buffer space to read remaining */
    if (rx_buf_len < (header_len + remain_len)) {
        return MQTT_CODE_ERROR_OUT_OF_BUFFER;
    }

    /* Read remaining */
    if (remain_len > 0) {
        rc = MqttSocket_Read(client, &rx_buf[header_len], remain_len, timeout_ms);
        if (rc != remain_len) {
            return rc;
        }
    }

    /* Return entire packet length */
    return header_len + remain_len;
}

int MqttDecode_ConenctAck(byte *rx_buf, int rx_buf_len, MqttConnectAck *connect_ack)
{
	int header_len, remain_len;
	byte *rx_payload;

	/* validate required arguments */
	if(rx_buf == NULL || rx_buf_len <= 0)
		return MQTT_CODE_ERROR_BAD_ARG;
	header_len = MqttDecode_FixedHeader(rx_buf, rx_buf_len, &remain_len, MQTT_PACKET_TYPE_CONNECT_ACK);
	if(header_len < 0)
		return header_len;
	rx_payload = &rx_buf[header_len];
    /* Decode variable header */
    if (connect_ack) {
        connect_ack->flags = rx_payload[0];
        connect_ack->return_code = rx_payload[1];
    }

    /* Return total length of packet */
    return header_len + remain_len;
}

int MqttEncode_Ping(byte *tx_buf, int tx_buf_len)
{
	int header_len ,remain_len = 0;
	if(tx_buf == NULL)
		return MQTT_CODE_ERROR_BAD_ARG;
	header_len = MqttEncode_FixedHeader(tx_buf, tx_buf_len, remain_len,\
			MQTT_PACKET_TYPE_PING_REQ,0,0,0);
    if (header_len < 0) {
        return header_len;
    }

    /* Return total length of packet */
    return header_len + remain_len;
}

int MqttDecode_Ping(byte *rx_buf, int rx_buf_len)
{
    int header_len, remain_len;

    /* Validate required arguments */
    if (rx_buf == NULL || rx_buf_len <= 0) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* Decode fixed header */
    header_len = MqttDecode_FixedHeader(rx_buf, rx_buf_len, &remain_len,
        MQTT_PACKET_TYPE_PING_RESP);
    if (header_len < 0) {
        return header_len;
    }

    /* Return total length of packet */
    return header_len + remain_len;
}

int MqttEncode_Unsubscribe(byte *tx_buf, int tx_buf_len, MqttSubscribe *unsubscribe)
{
	int header_len, remain_len, i;
	byte *tx_payload;
	MqttTopic *topic;
	if(tx_buf == NULL || unsubscribe == NULL )
		return MQTT_CODE_ERROR_BAD_ARG;
	remain_len = 2;  /* packet id */
	for(i = 0; i < unsubscribe->topic_count; i++)
	{
		topic = &unsubscribe->topics[i];
		remain_len += (int)strlen(topic->topic_filter) + MQTT_DATA_LEN_SIZE;/*for datalen */
	}
	/*encode header */
	header_len = MqttEncode_FixedHeader(tx_buf, tx_buf_len, remain_len, MQTT_PACKET_TYPE_UNSUBSCRIBE,\
			0, MQTT_QOS_1, 0);
    if (header_len < 0) {
        return header_len;
    }
	tx_payload = &tx_buf[header_len];
	/*encode packet id */
	tx_payload += MqttEncode_Num(&tx_buf[header_len], unsubscribe->packet_id);
	/*encode payload */
	for(i = 0; i < unsubscribe->topic_count; i++)
	{
		topic = &unsubscribe->topics[i];
		tx_payload += MqttEncode_String(tx_payload, topic->topic_filter);
	}
	return header_len + remain_len;
}

int MqttEncode_Subscribe(byte *tx_buf, int tx_buf_len, MqttSubscribe *subscribe)
{
	int header_len, remain_len, i;
	byte *tx_payload;
	MqttTopic *topic;

	if(tx_buf == NULL || subscribe == NULL )
		return MQTT_CODE_ERROR_BAD_ARG;
	remain_len = 2; /* for packet id */
	for(i = 0; i < subscribe->topic_count; i++)
	{
		topic = &subscribe->topics[i];
		remain_len += (int)strlen(topic->topic_filter) +  2;
		remain_len++;   /* for qos */
	}
	header_len = MqttEncode_FixedHeader(tx_buf, tx_buf_len, remain_len,\
			MQTT_PACKET_TYPE_SUBSCRIBE, 0, MQTT_QOS_1, 0);
	if(header_len < 0)
		return header_len;
	tx_payload = &tx_buf[header_len];
	tx_payload += MqttEncode_Num(&tx_buf[header_len], subscribe->packet_id);
	for(i = 0; i < subscribe->topic_count; i++)
	{
		topic = &subscribe->topics[i];
		tx_payload += MqttEncode_String(tx_payload, topic->topic_filter);
		*tx_payload = topic->qos;
		tx_payload++;
	}
	return header_len+remain_len;

}
int MqttDeocde_UnsubscribeAck(byte *rx_buf, int rx_buf_len, MqttUnsubscribeAck *unsubscribe_ack)
{
	int header_len, remain_len;
	byte *rx_payload;
	if(rx_buf == NULL || rx_buf_len <= 0 || unsubscribe_ack == NULL)
	{
		return MQTT_CODE_ERROR_BAD_ARG;
	}
	header_len = MqttDecode_FixedHeader(rx_buf, rx_buf_len, &remain_len,\
			MQTT_PACKET_TYPE_UNSUBSCRIBE_ACK);
	rx_payload = &rx_buf[header_len];
	if(unsubscribe_ack)
	{
		rx_payload += MqttDecode_Num(rx_payload, &unsubscribe_ack->packet_id);
	}
	return header_len + remain_len;
}

int MqttDecode_SubscribeAck(byte *rx_buf, int rx_buf_len, MqttSubscribeAck *subscribe_ack)
{
	int header_len ,remain_len;
	byte *rx_payload;
	if(rx_buf == NULL || rx_buf_len <= 0 || subscribe_ack == NULL)
		return MQTT_CODE_ERROR_BAD_ARG;
	header_len = MqttDecode_FixedHeader(rx_buf, rx_buf_len, &remain_len,\
			MQTT_PACKET_TYPE_SUBSCRIBE_ACK);
	rx_payload = &rx_buf[header_len];
	if(subscribe_ack)
	{
		rx_payload += MqttDecode_Num(rx_payload, &subscribe_ack->packet_id);
		subscribe_ack->return_codes = rx_payload;
	}
	return header_len + remain_len;
}

int MqttEncode_Publish(byte *tx_buf, int tx_buf_len, MqttPublish *publish)
{
	int header_len, remain_len;
	byte *tx_payload;
	if(tx_buf == NULL || publish == NULL || tx_buf_len <= 0)
		return MQTT_CODE_ERROR_BAD_ARG;
	remain_len = (int)strlen(publish->topic_name) + 2;
	if(publish->qos > MQTT_QOS_0)
	{
		if(publish->packet_id == 0)
			return MQTT_CODE_ERROR_PACKET_ID;
		remain_len += 2;  /* packet id */
	}
	if(publish->message && publish->message_len > 0)
	{
		remain_len += publish->message_len;
	}

	/*encode packet header */
	header_len = MqttEncode_FixedHeader(tx_buf, tx_buf_len, remain_len, MQTT_PACKET_TYPE_PUBLISH,\
			publish->retain, publish->qos, publish->dupicate);
	if(header_len < 0)
		return header_len;
	tx_payload = &tx_buf[header_len];

	tx_payload += MqttEncode_String(tx_payload, publish->topic_name);
	if(publish->qos > MQTT_QOS_0)
		tx_payload += MqttEncode_Num(tx_payload, publish->packet_id);

	/*encode payload */
	if(publish->message && publish->message_len > 0)
	{
		memcpy(tx_payload, publish->message, publish->message_len);
	}
	return header_len + remain_len;

}

int MqttDecode_String(byte *buf, const char **pstr, word16 *pstr_len)
{
	int len;
	word16 str_len;
	len  = MqttDecode_Num(buf, &str_len);
	//buf += len;
	if(pstr_len)
		*pstr_len = str_len;
	if(pstr)
		*pstr = (char *)(buf+len);
	return len + str_len;
}

int MqttDecode_Publish(byte *rx_buf, int rx_buf_len, MqttPublish *publish)
{
	int header_len, remain_len ,vheader_len;
	word16 topic_name_len;
	byte *rx_payload;

	MqttPacket *header = (MqttPacket *)rx_buf;

	if(rx_buf == NULL || publish == NULL || rx_buf_len <= 0)
	{
		return MQTT_CODE_ERROR_BAD_ARG;
	}

	/* decode fixed header */
	header_len = MqttDecode_FixedHeader(rx_buf, rx_buf_len, &remain_len,\
			MQTT_PACKET_TYPE_PUBLISH);
	if(header_len < 0)
		return header_len;
	rx_payload = &rx_buf[header_len];
	/* deocde variable header */
	vheader_len = MqttDecode_String(rx_payload, &publish->topic_name, &topic_name_len);
	rx_payload += vheader_len;
	if(MQTT_PACKET_FLAGS_GET_QOS(header->type_flags) > MQTT_QOS_0)
	{
		vheader_len += MqttDecode_Num(rx_payload, &publish->packet_id);
		rx_payload += 2;
	}
	/* decode payload */
	publish->message_len = remain_len - vheader_len;
	//publish->message = rx_payload;
	uint8_t *message = malloc(publish->message_len+1);
	memcpy(message, rx_payload, publish->message_len);
	publish->message = &message[0];
	/* null terminate decode values */
	*(char *)(&publish->topic_name[topic_name_len]) = '\0';
	*(char *)(&publish->message[publish->message_len]) = '\0';
	return header_len + remain_len;
}

int MqttDecode_PublishResp(byte *rx_buf, int rx_buf_len, byte type,\
		MqttPublishResp *publish_resp)
{
	int header_len ,remain_len;
	byte *rx_payload;
	if(rx_buf == NULL || rx_buf_len <= 0)
		return MQTT_CODE_ERROR_BAD_ARG;

	header_len = MqttDecode_FixedHeader(rx_buf, rx_buf_len, &remain_len, type);
	if(header_len < 0)
		return header_len;
	rx_payload = &rx_buf[header_len];
	if(publish_resp)
		rx_payload += MqttDecode_Num(rx_payload, &publish_resp->packet_id);

	return header_len + remain_len;

}
int MqttEncode_PublishResp(byte *tx_buf, int tx_buf_len, byte type,\
		MqttPublishResp *publish_resp)
{
	int header_len, remain_len;
	byte *tx_payload;
	MqttQos qos;

	if(tx_buf == NULL || publish_resp == NULL)
	{
		return MQTT_CODE_ERROR_BAD_ARG;
	}

	remain_len = MQTT_DATA_LEN_SIZE; /* packet id */
	qos = (type == MQTT_PACKET_TYPE_PUBLISH_REL) ? MQTT_QOS_1 : MQTT_QOS_0;
	/* encode header */
	header_len = MqttEncode_FixedHeader(tx_buf, tx_buf_len, remain_len, type, 0, qos, 0);
	tx_payload = &tx_buf[header_len];

	tx_payload += MqttEncode_Num(&tx_buf[header_len], publish_resp->packet_id);

	return header_len + remain_len;
}

int MqttEncode_Disconnect(byte *tx_buf, int tx_buf_len)
{
	int header_len;
	byte *tx_payload;

	if(tx_buf == NULL || tx_buf_len <= 0)
	{
		return MQTT_CODE_ERROR_BAD_ARG;
	}
	header_len = MqttEncode_FixedHeader(tx_buf, tx_buf_len, 0, MQTT_PACKET_TYPE_DISCONNECT, 0, 0, 0);
	if(header_len <= 0)
		return header_len;
	return header_len;
}
