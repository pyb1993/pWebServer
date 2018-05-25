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
#include <sys/socket.h>
#include <arpa/inet.h>

// listenning socket的回调函数
/*
 需要取出所有的fd
 
 这里存在的一个问题是,在事件A里面释放事件C的链接,但是事件B又申请了一个新链接,这就导致事件
 C将会把链接搞混(单纯的把fd变成-1解决不了这个问题),需要通过instance来进行取反判定。但是
 如果同时出现事件B,B'也会造成两次取反,所以还有一个做法是将accept的事件先执行,其他事件后面执行
 就不会导致链接出问题了
 */

void event_accept(event_t *ev)
{
    connection_t* listenning = ev->data;
    static bool first = false;
    while(1)
    {
        // 接收客户端连接
        struct sockaddr_in client;
        socklen_t c_len = sizeof(client);

        int conn_fd = accept(listenning->fd,(struct sockaddr*)&client,&c_len);//这里是可能阻塞的
        plog("try to accept one connection");
        
        if(conn_fd <= 0){
            ERR_ON((errno != EWOULDBLOCK), "accept error");
            
            break;
        }
        else{
            #ifdef DEBUG
                plog("accept connection %d,port: %d", conn_fd,ntohs(client.sin_port));
            #endif
        }
        
        
        //获取一个空闲连接对象
        connection_t* c = getIdleConnection();
        if(c == NULL){
            close(conn_fd);
            ERR_ON((c == NULL), "connection pool is empty");
            return;
        }
        
        //给新连接对象赋值
        c->fd = conn_fd;
        c->is_connected = true;
        
        // 获取ip地址,但是需要对ip地址进行hash
        c->ip = client.sin_addr.s_addr;

        init_connection(c);      //ngx_http_init_connection
    }
}
