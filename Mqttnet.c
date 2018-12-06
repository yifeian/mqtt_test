/*
 * Mqttnet.c
 *
 *  Created on: Nov 30, 2018
 *      Author: yifeifan
 */

#include "unp.h"

#if 1
typedef struct _SocketContext
{
	int sock_fd;
}SocketContext;

static void setup_timeout(struct timeval *tv, int timeout_ms)
{
	tv->tv_sec = timeout_ms/1000;
	tv->tv_usec = (timeout_ms % 1000)*1000;
	if(tv->tv_sec < 0 || (tv->tv_sec == 0 && tv->tv_usec <= 0))
	{
		tv->tv_sec = 0;
		tv->tv_usec = 0;
	}
}

static int  tcp_set_nonblocking(int *sockfd)
{
	int reg_flag;
	int flags = fcntl(*sockfd, F_GETFL, 0);
	reg_flag = flags;
	if(flags < 0)
		printf("fcntl get failed\n");
	flags = fcntl(*sockfd, F_SETFL, flags|O_NONBLOCK);
	if(flags < 0)
		printf("fcntl set failed\n");
	return reg_flag;
}

static int NetConnect(void *context, const char *host, uint16_t port,
		int timeout_ms)
{
	SocketContext *sock = (SocketContext *)context;
	int type = SOCK_STREAM;
	struct sockaddr_in address;
	int rc;
	int so_error = 0;
	int sockflag;
	struct addrinfo *result = NULL;
	struct addrinfo hints;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* get address info for host IPV4 */
	if((rc = getaddrinfo(host, NULL, &hints, &result)) != 0 )
	{
		printf("tcp_connect error for %s: %s\n",host,  gai_strerror(rc));
	}
	if(rc == 0 && result != NULL)
	{
		struct addrinfo *res = result;
		while(res)
		{
			if(res->ai_family == AF_INET)
			{
				result = res;
				break;
			}
			res = res->ai_next;
		}
		if(result->ai_family == AF_INET)
		{
			address.sin_port = htons(port);
			address.sin_family = result->ai_family;
			address.sin_addr = ((struct sockaddr_in *)(result->ai_addr))->sin_addr;
		}
		else
			rc = -1;
		freeaddrinfo(result);
	}
	if(rc == 0)
	{
		rc = -1;
		sock->sock_fd = socket(address.sin_family,type, 0);
		printf("sock fd is %d\n",sock->sock_fd );
		if(sock->sock_fd != -1)
		{
			fd_set rset,wset;
			struct timeval tv;
			setup_timeout(&tv, timeout_ms);
			FD_ZERO(&rset);
			FD_SET(sock->sock_fd, &rset);
			wset = rset;
			/* set sock nonblock */
			sockflag = tcp_set_nonblocking(&sock->sock_fd);
			/* start connect */
			if((rc = connect(sock->sock_fd, (struct sockaddr *)&address, sizeof(address))) < 0)
			{
				if(errno != EINPROGRESS)
					return(-1);
			}
			if(rc == 0)
			{
				printf("connect successful \n");
				return(rc);
			}
			/* wair for connectiong */
			if((rc = select(sock->sock_fd+1,&rset,&wset,NULL,&tv)) == 0)
			{
				close(sock->sock_fd);
				errno = ETIMEDOUT;
				return(-1);
			}
			if(FD_ISSET(sock->sock_fd,&rset) || FD_ISSET(sock->sock_fd, &wset))
			{
				socklen_t len = sizeof(rc);
				if(getsockopt(sock->sock_fd, SOL_SOCKET, SO_ERROR,&rc,&len) < 0)
					return(-1);
			}
			else
				printf("select error : sockfd not set\n");

			if(rc)
			{
				close(sock->sock_fd);
				errno = rc;
				return(-1);
			}
			fcntl(sock->sock_fd, F_SETFL, sockflag);
			return (0);
		}
		else
		{
			printf("create socket failed \n");
			return(-1);
		}
	}
	else
		return(-1);
}

static int NetWrite(void *context, const uint8_t *buf, int buf_len,
		int timeout_ms)
{
	SocketContext *sock = context;
	int rc;
	int so_error = 0;
	struct timeval tv;

	if(context == NULL || buf == NULL || buf_len <= 0)
	{
		return MQTT_CODE_ERROR_BAD_ARG;
	}
	setup_timeout(&tv,timeout_ms);
	setsockopt(sock->sock_fd, SOL_SOCKET, SO_SNDTIMEO,(char *)&tv, sizeof(tv));

	rc = (int)send(sock->sock_fd, buf, (size_t)buf_len, 0);
	if(rc == -1)
	{
		socklen_t len = sizeof(so_error);
		getsockopt(sock->sock_fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
		if(so_error == 0)
			rc = 0; /* handle signal */
		else
			printf("mqttsocket_netwrite : error %d\n", so_error);
	}
	return rc;
}

static int NetRead(void *context, uint8_t *buf, int buf_len,
		int timeout_ms)
{
	SocketContext *sock = context;
	int rc = -1, bytes = 0;
	socklen_t so_error = 0;
	fd_set recvfds, errfds;
	struct timeval tv;

	if(context == NULL || buf == NULL || buf_len <= 0)
	{
		return MQTT_CODE_ERROR_BAD_ARG;
	}
    /* Setup timeout and FD's */
    setup_timeout(&tv, timeout_ms);
    FD_ZERO(&recvfds);
    FD_ZERO(&errfds);
	/* loop until buf_len has been read, error or timeout */
	while(1)
	{
		FD_SET(sock->sock_fd, &recvfds);
		FD_SET(sock->sock_fd, &errfds);

		rc = select(sock->sock_fd + 1, &recvfds,NULL,&errfds, NULL);
		if(rc > 0)
		{
			/* check if rx or error */
			if(FD_ISSET(sock->sock_fd,&recvfds))
			{
				rc = (int)recv(sock->sock_fd, &buf[bytes], (size_t)(buf_len), 0);
				if(rc <= 0)
				{
					rc = -1;
					break;
				}
				else
				{
					break;
				}
			}
			if(FD_ISSET(sock->sock_fd, &errfds))
			{
				rc = -1;
				break;
			}
		}
		else if(rc < 0)
		{
			if(errno == EINTR)
				continue;
			else
			{
				printf("MQqttSocket Netread : error %d\n",errno);
				break;
			}

		}
		else
			break;
	}

	return rc;
}

static int NetDisconnect(void *context)
{
	SocketContext * sock = (SocketContext *)context;
	if(sock->sock_fd)
	{
		if(sock->sock_fd != -1)
		{
			close(sock->sock_fd);
			sock->sock_fd = -1;
		}
	}
	return 0;
}

/* Public Functions */
int MqttClientNet_Init(MqttNet * net)
{
	if(net)
	{
		bzero(net, sizeof(MqttNet));
		net->connect = NetConnect;
		net->disconnect = NetDisconnect;
		net->read = NetRead;
		net->write = NetWrite;
		net->context = malloc(sizeof(MqttNet));
	}
	return 0;
}

int MqttClientNet_DeInit(MqttNet *net)
{
	if(net)
	{
		if(net->context)
			free(net->context);
		memset(net,0,sizeof(MqttNet));
	}
	return 0;
}

#endif
