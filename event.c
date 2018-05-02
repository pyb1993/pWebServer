//
//  event.c
//  pWenServer
//
//  Created by pyb on 2018/3/27.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "event.h"
#include "server.h"
#include "commonUtil.h"
#include "http.h"

void event_accept(event_t *ev)
{
    connection_t* listenning = ev->data;
    
    while(1)
    {
        //接收客户端连接
        int conn_fd = accept(listenning->fd,NULL,NULL);//这里是可能阻塞的
        if(conn_fd == -1){
            ERR_ON((errno != EWOULDBLOCK), "accept");
            break;
        }
        else{
            plog("accept connection %d", conn_fd);
        }
        
        //获取一个空闲连接对象
        //存在的问题在于,如果客户端不发送实际的数据,那么就会平白分配内存
        connection_t* c = getIdleConnection();
        
        //给新连接对象赋值
        c->fd = conn_fd;
        c->side = C_DIRECTSTREAM;
        
        http_init_connection(c);      //ngx_http_init_connection
    }
}
