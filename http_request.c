//
//  http_request.c
//  pWenServer
//
//  Created by pyb on 2018/4/23.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "http.h"
#include "server.h"
#include "event.h"
#include "module.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "commonUtil.h"

// static函数,用来转换uri的extension,之所以要做转换是因为extension所在内存(buffer)可能会被修改
static void process_extension(http_request_t* r){
    if(r->uri.extension.extension_str.c == NULL){
        r->uri.extension.extension_type = HTML;
    }else if (stringEq(&r->uri.extension.extension_str ,&STRING("html"))){
        r->uri.extension.extension_type = HTML;
    }else if(stringEq(&r->uri.extension.extension_str ,&STRING("txt"))){
        r->uri.extension.extension_type = TXT;
    }else if(stringEq(&r->uri.extension.extension_str ,&STRING("json"))){
        r->uri.extension.extension_type = JSON;
    }else {
        r->uri.extension.extension_type = UNKNOWN_EXTENSION;
    }
}




static void http_init_upstream_connection(connection_t * upstream){
    event_t *wev = upstream->wev;
    event_t *rev = upstream->rev;
    wev->timedout = 0;
    wev->handler = http_proxy_pass;//将 recv buffer里面的数据 转发 到backend
    upstream->rev->handler = http_recv_upstream;//第一个步骤是分配相关的buffer
    add_event(rev,READ_EVENT,0);//监听读事件,这个事件应该一直开启
}


// 打开一个upstream connection,根据loc来实现
static connection_t* open_upstream_connection(http_request_t* r, location_t* loc){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ERR_ON(fd == -1, "create socket error");
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(loc->port);
    int success = inet_pton(AF_INET, loc->host.c, &addr.sin_addr);
    if (success <= 0) {
        return NULL;
    }
    
    int err = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (err < 0) {
        plog("err on connec backend,fd:%d",r->connection->fd);
        return NULL;
    }
    connection_t* c = getIdleConnection();
    c->side = C_UPSTREAM;
    set_nonblocking(c->fd);
    
    /*
     add_event(rev)
     add_timer
     监听backend -> server的数据,一旦有数据,就需要写入request的send_buffer里面,这样connection就可以send给client
     */
    return r->upstream;
}

static char * process_abs_path(http_request_t* r){
    uri_t* uri = &r->uri;
    char* real_path;
    
    if(uri->abs_path.c == NULL){
        uri->abs_path = STRING("./");
        real_path = "./";
    }
    else{
        real_path = uri->abs_path.c;
        *(real_path + uri->abs_path.len + uri->extension.extension_str.len + 1) = 0;
    }

    return real_path;
}

// 解析请求行之后需要对请求行进行处理
// 首先判断访问的abs_path是否可以匹配到配置里面的转发路径(通过配置来限制访问的权限 todo)
// 然后判断是否存在转发(这里相当于做负载均衡)的情况,如果需要转发,就要打开一个链接,直接返回
// 如果没有转发,那么就直接访问当前目录下的文件,并且需要检查是不是有足够的权限
int request_process_uri(http_request_t* r){
    uri_t* uri = &r->uri;
    char* real_path = process_abs_path(r);
    location_t* loc;

    if((loc = hash_find(server_cfg.locations, uri->abs_path.c, uri->abs_path.len))){
        // need create a new connection
        // need to change the send buffer
        ABORT_ON(r->upstream, "error on not null upstream");
        r->upstream = open_upstream_connection(r, loc);
    }else{
        // 不需要转发,那么直接寻找对应的目录下的文件
        int fd = server_cfg.root_fd;
        fd = openat(server_cfg.root_fd, real_path, O_RDONLY);//打开对应的rel_path
        
        if(fd == -1){
            r->response_done = true;
            construct_err(r, r->connection, 404);
            return ERROR;
        }
        
        struct stat st;
        fstat(fd, &st);
        if(S_ISDIR(st.st_mode)){
            // 如果对应打开的是一个目录,那么就要打开这个目录下面index.html的文件
            int tmp_fd = fd;
            fd = openat(fd, "index.html", O_RDONLY);
            close(tmp_fd);
            if (fd == -1) {
                // Accessing to a directory is forbidden
                r->response_done = true;
                construct_err(r, r->connection, 403);
                return ERROR;
            }
            
            fstat(fd, &st);
            r->resource_fd = fd;
            r->resource_len = st.st_size;
        }
    }

    // process the extension
    process_extension(r);
    if(r->version.minor == 1){
        r->keep_alive = true;
    }

    return OK;
}

/*
 可能的情况为:没有body,直接返回ok
 存在body,而且读到了完整body(或超过),返回OK
 存在body,还没读完,返回AGAIN
 如果内容超过content-length,那么就要截断
 */
int parse_request_body_identity(http_request_t* r) {
    buffer_t* b = r->recv_buffer;
    if (r->content_length <= 0) {
        return OK;
    }
    
    int recieved = max(r->content_length - r->body_received,buffer_size(b));
    r->body_received += recieved;
    b->begin += recieved;
    
    if (r->body_received == r->content_length) {
        return OK;
    }else{
        return AGAIN;
    }
}

