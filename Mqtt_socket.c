/*
 * Mqtt_socket.c
 *
 *  Created on: Dec 3, 2018
 *      Author: yifeifan
 */

#include "unp.h"

int MqttSocket_Init(MqttClient *client, MqttNet *net)
{
	int rc = MQTT_CODE_ERROR_BAD_ARG;
	if(client)
	{
		client->net = net;
		client->flags &= ~(MQTT_CLIENT_FLAG_IS_CONNECTED | MQTT_CLIENT_FLAG_IS_TLS);

		if(net && net->connect && net->read && net->write && net->disconnect)
		{
			rc = MQTT_CODE_SUCCESS;
		}
	}
	return rc;

}

int MqttSocket_Write(MqttClient *client, const byte *buf, int buf_len, int timeout_ms)
{
	int rc;
	if(client == NULL || buf == NULL || buf_len <= 0)
		return MQTT_CODE_ERROR_BAD_ARG;
	if(client->flags & MQTT_CLIENT_FLAG_IS_CONNECTED)
		rc = client->net->write(client->net->context,buf,buf_len,timeout_ms);
	printf("MqttSocket_Write: Len=%d, Rc=%d\n", buf_len, rc);
	if(rc < 0)
		rc = MQTT_CODE_ERROR_NETWORK;
	return rc;
}

int MqttSocket_Connect(MqttClient *client, const char *host, word16 port, int timeout_ms)
{
	int rc;
    /* Validate arguments */
    if (client == NULL || client->net == NULL || client->net->connect == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    if(port == 0)
    	port = MQTT_DEFAULT_PORT;
    /* connect to host */
    rc = client->net->connect(client->net->context, host, port, timeout_ms);
    if(rc != 0)
    	return rc;
    client->flags |= MQTT_CLIENT_FLAG_IS_CONNECTED;
    printf("MqttSocket_Connect: Rc=%d\n", rc);
    if(rc < 0)
    	rc = MQTT_CODE_ERROR_NETWORK;
    return rc;
}

int MqttSocket_Read(MqttClient *client, byte *buf, int buf_len, int timeout_ms)
{
    int rc;

    /* Validate arguments */
    if (client == NULL || client->net == NULL || client->net->read == NULL ||
        buf == NULL || buf_len <= 0) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    rc = client->net->read(client->net->context, buf, buf_len, timeout_ms);
    printf("MqttSocket_Read: Len=%d, Rc=%d\n", buf_len, rc);
    /* Check for timeout */
    if (rc == 0) {
        rc = MQTT_CODE_ERROR_TIMEOUT;
    }
    else if (rc < 0) {
        rc = MQTT_CODE_ERROR_NETWORK;
    }
    return rc;
}
