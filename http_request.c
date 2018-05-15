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
#include "upstream_server_module.h"
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





// 打开一个upstream connection,根据loc来实现
static connection_t* open_upstream_connection(http_request_t* r){
    plog("get upstream connection %d",r->connection->fd);
    connection_t* upstream = getIdleConnection();
    ERR_ON(upstream == NULL, "get upstream failed");
    if(upstream == NULL){
        return NULL;
    }
    return upstream;
}

static char * process_abs_path(http_request_t* r){
    uri_t* uri = &r->uri;
    char* real_path;
    
    if(uri->abs_path.c == NULL){
        uri->abs_path = STRING("./");
        real_path = "./";
    }
    else{
        // 现在还没有开始转发请求,所以修改这个没有什么问题
        int skip_len = 0;
        real_path = uri->abs_path.c;
        skip_len = (int)uri->abs_path.len;
        if(uri->extension.extension_str.c != NULL){
            skip_len += uri->extension.extension_str.len + 1;
        }
        real_path[skip_len] = 0;
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

    if(hash_find(server_cfg.locations, uri->abs_path.c, uri->abs_path.len) != NULL){
        ABORT_ON(r->upstream, "error on not null upstream");
        /* 将在这里try_connect_upstream里面设置回调,在回调里面处理链接的成功与否 */
        r->upstream = open_upstream_connection(r);
        if(r->upstream == NULL){
            int x = 20;
        }
        try_connect_upstream(r, &uri->abs_path);//在这里设置回调函数
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
        }
        fstat(fd, &st);
        r->resource_fd = fd;
        r->resource_len = st.st_size;
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
    
    int recieved = min(r->content_length - r->body_received,buffer_size(b));
    r->body_received += recieved;
    b->begin += recieved;
    
    if (r->body_received == r->content_length) {
        return OK;
    }else{
        return AGAIN;
    }
}



