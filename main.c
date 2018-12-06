/*
 * main.c
 *
 *  Created on: Dec 4, 2018
 *      Author: yifeifan
 */
#include "unp.h"

/* Configuration */
#define DEFAULT_MQTT_HOST       "test.mosquitto.org"
#define DEFAULT_CMD_TIMEOUT_MS  5000
#define DEFAULT_CON_TIMEOUT_MS  5000
#define DEFAULT_MQTT_QOS        MQTT_QOS_0
#define DEFAULT_KEEP_ALIVE_SEC  60
#define DEFAULT_CLIENT_ID       "yifeifanMQTTClient"

#define MAX_BUFFER_SIZE         1024
#define TEST_MESSAGE            "test" /* NULL */
#define TEST_TOPIC_COUNT        2
#define MY_EX_USAGE 2 /* Exit reason code */
/* Globals */
int mStopRead = 0;
const char* mTlsFile = NULL;

typedef struct _func_args
{
	int argc;
	char **argv;
	int return_code;
}func_args;

static void Usage(void)
{
    printf("mqttclient:\n");
    printf("-?          Help, print this usage\n");
    printf("-h <host>   Host to connect to, default %s\n", DEFAULT_MQTT_HOST);
    printf("-p <num>    Port to connect on, default: Normal %d, TLS %d\n", MQTT_DEFAULT_PORT, MQTT_SECURE_PORT);
    printf("-t          Enable TLS\n");
    printf("-c <file>   Use provided certificate file\n");
    printf("-q <num>    Qos Level 0-2, default %d\n", DEFAULT_MQTT_QOS);
    printf("-s          Disable clean session connect flag\n");
    printf("-k <num>    Keep alive seconds, default %d\n", DEFAULT_KEEP_ALIVE_SEC);
    printf("-i <id>     Client Id, default %s\n", DEFAULT_CLIENT_ID);
    printf("-l          Enable LWT (Last Will and Testament)\n");
    printf("-u <str>    Username\n");
    printf("-w <str>    Password\n");
}
static int myoptind = 0;
static char* myoptarg = NULL;
static int mygetopt(int argc, char** argv, const char* optstring)
{
    static char* next = NULL;

    char  c;
    char* cp;

    if (myoptind == 0)
        next = NULL;   /* we're starting new/over */

    if (next == NULL || *next == '\0') {
        if (myoptind == 0)
            myoptind++;

        if (myoptind >= argc || argv[myoptind][0] != '-' ||
                                argv[myoptind][1] == '\0') {
            myoptarg = NULL;
            if (myoptind < argc)
                myoptarg = argv[myoptind];

            return -1;
        }

        if (strcmp(argv[myoptind], "--") == 0) {
            myoptind++;
            myoptarg = NULL;

            if (myoptind < argc)
                myoptarg = argv[myoptind];

            return -1;
        }

        next = argv[myoptind];
        next++;                  /* skip - */
        myoptind++;
    }

    c  = *next++;
    /* The C++ strchr can return a different value */
    cp = (char*)strchr(optstring, c);

    if (cp == NULL || c == ':')
        return '?';

    cp++;

    if (*cp == ':') {
        if (*next != '\0') {
            myoptarg = next;
            next     = NULL;
        }
        else if (myoptind < argc) {
            myoptarg = argv[myoptind];
            myoptind++;
        }
        else
            return '?';
    }

    return c;
}

static int mqttclient_message_cb(MqttClient *client, MqttMessage *msg)
{
    (void)client; /* Supress un-used argument */

    /* Print incoming message */
    printf("MQTT Message: Topic %s, Len %u\n, message: %s", msg->topic_name, msg->message_len,\
    		msg->message);

    return MQTT_CODE_SUCCESS; /* Return negative to termine publish processing */
}


#define MAX_PACKET_ID   ((1 << 16) - 1)
static int mPacketIdLast;
static word16 mqttclient_get_packetid(void)
{
    mPacketIdLast = (mPacketIdLast >= MAX_PACKET_ID) ? 1 : mPacketIdLast + 1;
    return (word16)mPacketIdLast;
}

void mqttclient_test(void *args)
{
	int rc;
	char ch;
	word16 port = MQTT_DEFAULT_PORT;
	const char *host = DEFAULT_MQTT_HOST;
	MqttClient client;
	int use_tls = 0;
	byte qos = DEFAULT_MQTT_QOS;
	byte clean_session = 1;
	word16 keep_alive_sec = DEFAULT_KEEP_ALIVE_SEC;
	const char *client_id = DEFAULT_CLIENT_ID;
	int enable_lwt = 0;
	const char *username = NULL;
	const char *password = NULL;
	MqttNet net;
	byte *tx_buf = NULL, *rx_buf = NULL;
/*
	int argc = ((func_args *)args)->argc;
	char **argv = ((func_args *)args)->argv;
	((func_args*)args)->return_code = -1; /* error state */
	/*
	 while ((rc = mygetopt(argc, argv, "?h:p:tc:q:sk:i:lu:w:")) != -1) {
	        ch = (char)rc;
	        switch (ch) {
	            case '?' :
	                Usage();
	                exit(EXIT_SUCCESS);

	            case 'h' :
	                host   = myoptarg;
	                break;

	            case 'p' :
	                port = (word16)atoi(myoptarg);
	                if (port == 0) {
	                    err_sys("Invalid Port Number!");
	                }
	                break;

	            case 't':
	                use_tls = 1;
	                break;

	            case 'c':
	                mTlsFile = myoptarg;
	                break;

	            case 'q' :
	                qos = (byte)atoi(myoptarg);
	                if (qos > MQTT_QOS_2) {
	                    err_sys("Invalid QoS value!");
	                }
	                break;

	            case 's':
	                clean_session = 0;
	                break;

	            case 'k':
	                keep_alive_sec = atoi(myoptarg);
	                break;

	            case 'i':
	                client_id = myoptarg;
	                break;

	            case 'l':
	                enable_lwt = 1;
	                break;

	            case 'u':
	                username = myoptarg;
	                break;

	            case 'w':
	                password = myoptarg;
	                break;

	            default:
	                Usage();
	                exit(MY_EX_USAGE);
	        }
	    }

	myoptind = 0; /* reset for test cases */
	Signal(SIGALRM, sig_alrm);
    printf("mqtt client test\n");
    rc = MqttClientNet_Init(&net);
    printf("MQTT Net Init: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
    /* Initialize MqttClient structure */
    tx_buf = malloc(MAX_BUFFER_SIZE);
    rx_buf = malloc(MAX_BUFFER_SIZE);
    rc = MqttClient_Init(&client, &net, mqttclient_message_cb,
    		tx_buf, MAX_BUFFER_SIZE, rx_buf, MAX_BUFFER_SIZE, DEFAULT_CMD_TIMEOUT_MS);
    printf("MQTT Init: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
    /* connect to broker*/
    rc = MqttClient_NetConnect(&client, host, port, DEFAULT_CON_TIMEOUT_MS);
    printf("MQTT Socket Connect: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
    if(rc == 0)
    {
    	 /* Define connect parameters */
		MqttConnect connect;
		MqttMessage lwt_msg;
		connect.keep_alive_sec = keep_alive_sec;
		connect.clean_session = clean_session;
		connect.client_id = client_id;
		/* Last will and testament sent by broker to subscribers of topic when broker connection is lost */
		memset(&lwt_msg, 0, sizeof(lwt_msg));
		connect.lwt_msg = &lwt_msg;
		connect.enable_lwt = enable_lwt;
		if (enable_lwt) {
			lwt_msg.qos = qos;
			lwt_msg.retain = 0;
			lwt_msg.topic_name = "lwttopic";
			lwt_msg.message = (byte*)DEFAULT_CLIENT_ID;
			lwt_msg.message_len = (word16)strlen(DEFAULT_CLIENT_ID);
		}
		/* Optional authentication */
		connect.username = username;
		connect.password = password;

		/* Send Connect and wait for Connect Ack */
		rc = MqttClient_Connect(&client, &connect);
		printf("MQTT Connect: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
		if(rc == MQTT_CODE_SUCCESS)
		{
			MqttSubscribe subscribe;
			MqttUnsubscribe  unsubscribe;
			MqttTopic topics[TEST_TOPIC_COUNT], *topic;
			MqttPublish publish;
			int i;
			/* build list of topics */
			topics[0].topic_filter = "subtopic_yifei1";
			topics[0].qos = qos;
			topics[1].topic_filter = "subtopic_yifei2";
			topics[1].qos = qos;
			/* validate connect return code */
			rc = connect.ack.return_code;
			printf("MQTT Connect Ack: Return Code %u, Session Present %d\n",\
				connect.ack.return_code,\
				connect.ack.flags & MQTT_CONNECT_ACK_FLAG_SESSION_PRESENT ? 1 : 0);
			/* send ping */
			rc = MqttClient_Ping(&client);
			printf("MQTT Ping: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
			/* subcribe topic */
			subscribe.packet_id = mqttclient_get_packetid();
			subscribe.topic_count = 2;
			subscribe.topics = topics;
			rc = MqttClient_Subscribe(&client, &subscribe);
			printf("MQTT Subscribe %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
			for(i = 0; i < subscribe.topic_count; i++)
			{
				topic = &subscribe.topics[i];
				printf("  Topic %s, Qos %u, Return Code %u\n",\
				       topic->topic_filter, topic->qos, topic->return_code);
			}

			/* Publish Topic*/
			publish.retain = 0;
			publish.qos = 1;
			publish.dupicate = 0;
			publish.topic_name = "subtopic_yifei1";
			publish.packet_id = mqttclient_get_packetid();
			publish.message = (byte *)"this is a test yifei";
			publish.message_len = (word16)strlen(publish.message);
			rc = MqttClient_Publish(&client, &publish);
			printf("MQTT Publish: Topic %s, %s (%d)\n", publish.topic_name,\
					MqttClient_ReturnCodeToString(rc), rc);
			alarm(20);
			/* readloop */
			while(mStopRead == 0)
			{
				rc = MqttClient_WaitMessage(&client, DEFAULT_CMD_TIMEOUT_MS);
				if(rc != MQTT_CODE_SUCCESS && rc != MQTT_CODE_ERROR_TIMEOUT)
				{
					 printf("MQTT Message Wait: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
					 break;
				}
			}

		}
    }

}

int main(int argc, char **argv)
{
	func_args args;
	args.argc = argc;
	args.argv = argv;

	mqttclient_test(&args);
	return args.return_code;
}

