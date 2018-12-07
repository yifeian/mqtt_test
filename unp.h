/*
 * unp.h
 *
 *  Created on: Dec 3, 2018
 *      Author: yifeifan
 */

#ifndef UNP_H_
#define UNP_H_
/*
 * network.h
 *
 *  Created on: Aug 29, 2018
 *      Author: yifeifan
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netdb.h>
#include <stdbool.h>
#include <bits/types.h>
#include <errno.h>
#include <asm-generic/errno-base.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "Mqtt_packet.h"
#include "Mqtt_client.h"
#include "Mqtt_socket.h"

/* Miscellaneous constants */
#define	MAXLINE		4096	/* max text line length */

#define max(a,b)    ((a) > (b) ? (a) : (b))
#define min(a,b)    ((a) > (b) ? (a) : (b))

#define SA struct sockaddr

typedef	void	Sigfunc(int);
Sigfunc *Signal(int, Sigfunc *);



#endif /* UNP_H_ */
