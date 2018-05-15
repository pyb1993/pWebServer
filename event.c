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
    
    while(1)
    {
        // 接收客户端连接
        struct sockaddr_in client;
        socklen_t c_len = sizeof(client);

        int conn_fd = accept(listenning->fd,(struct sockaddr*)&client,&c_len);//这里是可能阻塞的
        if(conn_fd == -1){
            ERR_ON((errno != EWOULDBLOCK), "accept error");
            int err = errno;
            break;
        }
        else{
            plog("accept connection %d", conn_fd);
        }
        
        // 因为上一个事件里面的timedout可能是1,所以这里要进行处理,但是将处理延迟到分配到该事件之后进行
        if (ev->timedout) {
            ev->timedout = 0;
        }
        
        //获取一个空闲连接对象
        //延迟分配connection的内存池
        connection_t* c = getIdleConnection();
        if(c == NULL){
            close(conn_fd);
            return;
        }
        //给新连接对象赋值
        c->fd = conn_fd;
        c->side = C_DIRECTSTREAM;
        
        // 获取ip地址,但是需要对ip地址进行hash
        c->ip = client.sin_addr.s_addr;
        http_init_connection(c);      //ngx_http_init_connection
    }
}
