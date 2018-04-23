//
//  server.h
//  pWenServer
//
//  Created by pyb on 2018/3/6.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef server_h
#define server_h

#include <sys/socket.h>
#include "sys/stat.h"
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "pool.h"
#include "config.h"
#include "event.h"
#include "connection.h"

void accept_connection(int socket);
void singal_handler(int signal);

typedef struct {
    int     signo;
    char   *signame;
    void  (*handler)(int signo);
} signal_t;



extern config server_cfg;
extern connection_pool_t connection_pool;
extern int listen_fd;
extern event_t* r_events;
extern event_t* w_events;


#endif /* server_h */


