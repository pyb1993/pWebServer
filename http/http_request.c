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
    
    upstream->data = r;
    return upstream;
}

/*
  辅助函数,用来处理绝对路径,返回一个real_path
  case1 如果uri里面没有对应的路径,那么就需要
  case2 如果有对应的路径,由于要打开对应的静态文件时要传入有\0结尾的字符串
        所以需要加上/0来进行修饰

 */
static char * process_abs_path(http_request_t* r){
    uri_t* uri = &r->uri;
    char* real_path;
    
    // 去掉host的属性
    /*
    if(uri->host.c != NULL){
        char *c = uri->host.c;
        for(int i = 0; i < uri->host.len;++i){
                *c = ' ';
        }
    }
    
    if(uri->port.c != NULL){
        char *c = uri->port.c;
        for(int i = 0; i < uri->port.len;++i){
            *c = ' ';
        }
    }*/
    
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

/*在转发的时候,需要将http到port位置的数据都变成空*/
static void skip_host(http_request_t* r){
    /*
    uri_t* uri = &r->uri;
    char* real_path;

    if(uri->scheme.c != NULL){
        char *c = uri->scheme.c;
        for(int i = 0; i < uri->scheme.len+3;++i){
            *(c+i) = ' ';
        }
    }
    
    if(uri->host.c != NULL){
        char *c = uri->host.c;
        for(int i = 0; i < uri->host.len;++i){
            *(c+i) = ' ';
        }
    }
    // 去掉:port
    if(uri->port.c != NULL){
        char *c = uri->port.c;
        for(int i = -1; i < uri->port.len;++i){
            *(c+i) = ' ';
        }
    }
     */
    return;
}


/*解析请求行 method uri http/version*/
int parse_request_line(http_request_t* r,buffer_t *b){
#define RETURN_ERR_ON(cond, err) do{   \
        if (cond) {                     \
            return (err);               \
        }                               \
    }while(0)
    
    
    char *p;
    int uri_err;
    for(p = b->begin; p < b->end;++p)
    {
        char ch = *p;
        switch (r->state) {
            case RQ_BEGIN:
                switch (ch)
            {
                case 'A' ... 'Z':
                    r->request_line.c = p;
                    r->state = RQ_METHOD;
                    break;
                default:
                    return INVALID_REQUEST;
            }
                break;
                
            case RQ_METHOD:
                switch (ch) {
                    case 'A'...'Z':
                        break;
                        
                    case ' ':
                        r->method = parse_request_method(r->request_line.c,p);
                        if(r->method == M_INVALID_METHOD){
                            return INVALID_REQUEST;
                        }
                        r->state = RQ_BEFORE_URI;
                        break;
                }
                break;
            case RQ_BEFORE_URI:
                switch (ch) {
                    case '\t':
                    case '\r':
                    case '\n':
                        return INVALID_REQUEST;
                    case ' ':
                        // 忽略所有的连续' '
                        break;
                    default:
                        if ((uri_err = parse_uri(&r->uri, p)) != OK) {
                            return uri_err;
                        }
                        r->state = RQ_URI;
                        break;
                }
                break;
            case RQ_URI:
                switch (ch) {
                    case '\t':
                    case '\r':
                    case '\n':
                        return INVALID_REQUEST;
                    case ' ':
                        if(r->uri.state == URI_ABS_PATH){
                            r->uri.abs_path.len = p - r->uri.abs_path.c;
                        }
                        else if (r->uri.state == URI_HOST){
                            r->uri.host.len = p - r->uri.host.c;
                        }
                        else if (r->uri.state == URI_PORT){
                            ++(r->uri.port.c);
                            r->uri.port.len = p - r->uri.port.c;
                        }
                        else if (r->uri.state == URI_EXTENSION){
                            r->uri.extension.extension_str.len = p - r->uri.extension.extension_str.c;
                            
                        }
                        else if (r->uri.state == URI_QUERY){
                            r->uri.query.len = p - r->uri.query.c;
                        }
                        else{
                            ABORT_ON(0, "unexpected state");
                        }
                        r->state = RQ_BEFORE_VERSION;
                        break;
                    default:
                        if ((uri_err = parse_uri(&r->uri, p)) != OK) {
                            return uri_err;
                        }
                        break;
                }
                break;
            case RQ_BEFORE_VERSION:
                switch (ch) {
                    case ' ':
                        break;
                    case 'H':
                        r->state = RQ_VERSION_H;
                        break;
                    default:
                        return INVALID_REQUEST;
                }
                break;
            case RQ_VERSION_H:
                RETURN_ERR_ON(ch != 'T', INVALID_REQUEST);
                r->state = RQ_VERSION_HT;
                break;
            case RQ_VERSION_HT:
                RETURN_ERR_ON(ch != 'T', INVALID_REQUEST);
                r->state = RQ_VERSION_HTT;
                break;
            case RQ_VERSION_HTT:
                RETURN_ERR_ON(ch != 'P', INVALID_REQUEST);
                r->state = RQ_VERSION_HTTP;
                break;
            case RQ_VERSION_HTTP:
                RETURN_ERR_ON(ch != '/', INVALID_REQUEST);
                r->state = RQ_VERSION_SLASH;
                break;
            case RQ_VERSION_SLASH:
                switch (ch) {
                    case '0' ... '9':
                        r->version.major = r->version.major * 10 + ch - '0';
                        RETURN_ERR_ON(r->version.major > 100, INVALID_REQUEST);
                        r->state = RQ_VERSION_DOT;
                        break;
                    default:
                        return INVALID_REQUEST;
                }
                break;
            case RQ_VERSION_MAJOR:
                switch (ch) {
                    case '0' ... '9':
                        r->version.major = r->version.major * 10 + ch - '0';
                        RETURN_ERR_ON(r->version.major > 100, INVALID_REQUEST);
                        break;
                    case '.':
                        r->state = RQ_VERSION_DOT;
                        break;
                    default:
                        return INVALID_REQUEST;
                }
                break;
            case RQ_VERSION_DOT:
                RETURN_ERR_ON('0' <= ch && ch <= '9', INVALID_REQUEST);
                r->state = RQ_VERSION_MINOR;
                break;
            case RQ_VERSION_MINOR:
                switch (ch) {
                    case '0' ... '9':
                        r->version.minor = r->version.minor * 10 + ch - '0';
                        RETURN_ERR_ON(r->version.minor > 100, INVALID_REQUEST);
                        break;
                    case ' ':
                        r->state = RQ_AFTER_VERSION;
                        break;
                    case '\r':
                        r->state = RQ_ALMOST_DONE;
                        break;
                    default:
                        return INVALID_REQUEST;
                }
                break;
            case RQ_AFTER_VERSION:
                switch (ch) {
                    case ' ':
                        break;
                    case '\r':
                        r->state = RQ_ALMOST_DONE;
                        break;
                    default:
                        return INVALID_REQUEST;
                }
                break;
            case RQ_ALMOST_DONE:
                RETURN_ERR_ON(ch != '\n', INVALID_REQUEST);
                goto done;
                break;
            default:
                return INVALID_REQUEST;
        }
    }
    b->begin = b->end;
    return AGAIN;
    
done:
    b->begin = p + 1;
    r->request_line.len = (int)(p - r->request_line.c);
    r->state = RQ_DONE;
    return OK;
    
#undef RETURN_ERR_ON
}


// 解析请求行之后需要对请求行进行处理
// 首先判断访问的abs_path是否可以匹配到配置里面的转发路径(通过配置来限制访问的权限 todo)
// 然后判断是否存在转发(这里相当于做负载均衡)的情况,如果需要转发,就要打开一个链接,直接返回
// 如果没有转发,那么就直接访问当前目录下的文件,并且需要检查是不是有足够的权限
int request_process_uri(http_request_t* r){
    uri_t* uri = &r->uri;
    
    if(hash_find(server_cfg.locations, uri->abs_path.c, uri->abs_path.len) != NULL){/*需要和后端进行通信*/
        ABORT_ON(r->upstream, "error on not null upstream");
        skip_host(r);
        /* 将在这里try_connect_upstream尝试非阻塞connect,并且设置对应的处理回调(process_connection_result_of_upstream)*/
        r->upstream = open_upstream_connection(r);
        try_connect_upstream(r, &uri->abs_path);//在这里设置回调函数
    }else{
        /*不需要转发,那么直接寻找对应的目录下的文件 */
        char* real_path = process_abs_path(r);
        int fd = server_cfg.root_fd;
        fd = openat(server_cfg.root_fd, real_path, O_RDONLY);//打开对应的rel_path
        
        if(fd <= 0){
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
            if (fd <= 0) {
                // 缺少权限
                plog("permission denied : %s",real_path);
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



