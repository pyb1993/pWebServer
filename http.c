//
//  http.c
//  pWenServer
//
//  Created by pyb on 2018/3/28.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "http.h"
#include "event.h"
#include "server.h"
#include "module.h"
#include "connection.h"
#include "commonUtil.h"
#include "header.h"
#include "upstream_server_module.h"
/* 用来初始化一个http链接(connection) *
 设置对应的事件回调函数
 */



/* 创建一个request结构体,有以下几件事情要做
   1 分配内存池,用来在后面分配对应的buffer
   2 清空这个request结构体里面的所有数据
   3 设置初始化的状态
 
  request的所有内存都是由内存池分配的,包括它自己,所以释放的时候,应该逆序释放
 */

http_request_t* create_http_request(){
    ABORT_ON(server_cfg.request_pool_size <= 0, "error on config");
    memory_pool* pool = createPool(server_cfg.request_pool_size);
    http_request_t* request = (http_request_t*)pMalloc(pool, sizeof(http_request_t));
    memzero(request, sizeof(http_request_t));
    request->pool = pool;
    request->state = RQ_BEGIN;
    request->uri.state = URI_BEGIN;
    return request;
}

/*关闭request
 关闭链接
    (自动)删除链接上面对应的读事件和写事件
    删除对应事件的定时器
    删除链接,归还到正常的链表里面
    释放自己对应的内存
 todo
    需要考虑将整个请求对应的链接全部释放了
    那么这里存在这样三个情况
        正常释放,全部关闭
        如果是connection出错,那么需要断开和上游的链接,同时对client返回对应的错误信息
        如果是上游出错,那么需要写入对应的错误信息,比如bad gateway
    上面的错误都应该由外部调用来实现,该函数只管释放
 */
void http_close_request(http_request_t* r)
{
    
    plog("close the request %d -> %d",r->connection->fd,r->upstream == NULL ? 0 : r->upstream->fd);
    connection_t* c = r->connection;
    if(c){
        http_close_connection(c);
    }

    if(r->upstream){
        http_close_connection(r->upstream);
    }
    if(r->resource_fd > 0){
        close(r->resource_fd);
        r->resource_fd = -1;
    }
    
    // 如果是upstream 链接,且链接成功,那么要调用这个函数
    if(r->cur_upstream){
        free_upstream(r->cur_upstream);
    }
    
    freePool(r->pool);//连request自己都被释放了
}


/*clear request,但是不释放链接,也不释放request的内存池,等待下一次的链接重用
  注意这里需要释放upstream链接,因为下一个请求的未必需要同一个后端服务器
  注意到这里需要做的一件
 */
void http_clear_request(http_request_t* r)
{
    memory_pool* pool = r->pool;
    connection_t* c = r->connection;
    buffer_t *rb = r->recv_buffer;
    buffer_t *sb = r->send_buffer;
    buffer_clear(rb);
    buffer_clear(sb);
    if(r->upstream){
        http_close_connection(r->upstream);//释放长链接
    }
    
    if(r->resource_fd > 0){
        close(r->resource_fd);
        r->resource_fd = -1;
    }
    
    memzero(r, sizeof(http_request_t));
    r->connection = c;
    r->pool = pool;
    r->recv_buffer = rb;
    r->send_buffer = sb;
    c->is_idle = true; //设置处于idle状态

    // 如果是upstream 链接,且链接成功,那么要调用这个函数
    if(r->cur_upstream){
        free_upstream(r->cur_upstream);
    }
    
    // todo debug
    r->header_name = STRING("fuck you");
}

// 处理response发完以后的请求
void http_response_done(http_request_t* r){
    ABORT_ON(r->response_done != true, "error on response done");
    if(r->keep_alive){
        connection_t* c = r->connection;
        plog("reuse the connection:(%d)",c->fd);
        http_clear_request(r);// 清理请求的状态,等待复用
        del_event(&c->wev,WRITE_EVENT,0);//已经写完了(自动清理定时器)
        add_event(&c->rev,READ_EVENT,0);//已经写完了,回复到读取的状态
        c->rev.handler = ngx_http_process_request_line;//重新恢复到处理请求行的状态
        event_add_timer(&c->rev, server_cfg.keep_alive_timeout);
    }
    else{
        http_close_request(r);
    }
}

// 开始执行发送的逻辑,从wev过来的
void handle_response(event_t* wev)
{
    connection_t* c = wev->data;
    http_request_t* r = c->data;

    if(c->fd <= 0){
        return;
    }

    if(wev->timedout){
        plog("server timed out : send response");
        http_close_request(r);
        return;
    }

    int err = buffer_send(r->send_buffer, c->fd);
    if(err == OK){
        // 注意顺序非常重要,首先是reponse_done之后就直接结束了
        if(r->response_done){
            http_response_done(r);
            return;
        }
        
        // 本次内容已经回复完了,等待下一次结果
        if(r->upstream){
            del_event(wev,WRITE_EVENT,0);
            return;
        }
        
        // case: 普通请求
        if(r->resource_fd > 0 && !r->response_done)
        {
            //注意response_done的用途,如果已经出错,那么就不执行发送file的操作
            plog("send file (fd:%d)",c->fd);
            c->wev.handler = handle_response_file;
            handle_response_file(&c->wev);
            return;
        }else{
            // 不需要发送 file,那么直接结束
            r->response_done = true;
            http_response_done(r);
            return;
        }
        
        //本次内容已经发完了,关闭写事件
        del_event(wev,WRITE_EVENT,0);
    }
    else if(err == AGAIN){
        event_add_timer(wev,server_cfg.post_accept_timeout);
    }
    else if(err == ERROR){
        http_close_request(r);
    }
}

void handle_response_file(event_t* wev)
{
    connection_t* c = wev->data;
    http_request_t* r = c->data;
    
    if(wev->timedout){
        plog("connection: %dserver timed out : send response file",c->fd);
        construct_err(r, r->connection, 404);
        return;
    }
    
    int err = send_file(r);
    if(err == OK){
        r->response_done = true;
        http_response_done(r);
    }
    else if(err == ERROR){
        http_close_request(r);
    } else{
        if(wev->timer_set){
            event_add_timer(wev,server_cfg.post_accept_timeout);
        }
    }
}

/*
 注意,走到当前这个函数的,都不可能是keep-alive的链接(设置的回调函数是process_request_line)
 接收到真正的数据时,才开始初始化http_request
 
 todo: 采取缓冲池的办法来优化对request内存的分配
        思路如下,我们构建一个大的内存池,叫做request_pool 4096 * 3000,
        然后每次需要申请请求的时候,就从这个request_pool里面申请内存来createRequest
        request自己也需要一个内存池,这个内存池就从里request_pool里面获得
        当request需要被释放的时候,需要将时间
 
*/
void http_init_request(event_t* rev)
{
    connection_t* c = rev->data;
    
    /* 若当前读事件超时，则记录错误日志，关闭所对应的连接并退出
       由于这里还没有申请对应的buffer,所以暂时不支持返回对应的错误信息
     */
    if (rev->timedout) {
        plog( "connection:%d client timed out(init request)",c->fd);
        http_close_connection(c);
        return;
    }

    plog("begin init request!!! for connection:%d",c->fd);
    
    http_request_t* r = c->data = create_http_request();

    r->connection = c;
    plog("try to init buffer for connection:%d",c->fd);

    // todo 这里可能可以优化,因为可以延迟到需要接受数据或者写数据的时候再分配
    r->recv_buffer = createBuffer(r->pool);
    plog("init buffer1 for connection:%d",c->fd);

    r->send_buffer = createBuffer(r->pool);
    plog("init buffer1 for connection:%d",c->fd);

    /* 设置当前读事件的处理方法为ngx_http_process_request_line */
    rev->handler = ngx_http_process_request_line;
    
    /* 执行该读事件的处理方法ngx_http_process_request_line，接收HTTP请求行 */
    ngx_http_process_request_line(rev);
}


/* 接受从客户端传过来的body数据
   无论本次是否将body接受完成,都分为3种情况
   case1: 不需要链接后端服务器,直接进入下一个handle_response的阶段
   case2: 需要和后端服务器通信,并且已经链接上了
   case3: 需要和后端服务器进行通信,但是目前还没有链接上
          如果接受没有完成,那么应该继续等待
          如果接受完成,如果不做特殊处理,就会导致事件丢失。所以在这个地方,必须设置一个状态,方便链接成功的时候的回调添加写事件
 */
void request_handle_body(event_t * rev)
{
    connection_t* c = rev->data;
    http_request_t* req = c->data;
    if(c->fd <= 0){
        return;
    }
    
    if(rev->timedout){
        plog("error:client timed out : handle body");
        construct_err(req, req->connection, 404);
        return;
    }
    
    plog("connection:(%d) handle body",c->fd);
    /* 读取当前请求未解析的数据 */
    int n = buffer_recv(req->recv_buffer, c->fd);
    /* 若链接关闭(OK)，或读取失败(ERROR)，则直接退出 */
    if (n == ERROR || n == OK) {
        plog("error:connection:%d closed by peer when recving the body",c->fd);
        http_close_request(req);
        return;
    }
    
    // 从buffer里面获取body
    int err = OK;
    req->t_encoding = IDENTITY;
    switch (req->t_encoding)
    {
        case IDENTITY:
            err = parse_request_body_identity(req);//r->rb->begin指向了消费过body之后的位置(不一定读完了body)
            break;
        default:
            assert(false);
    }
    
    buffer_t* b = req->recv_buffer;
    switch (err)
    {
        case AGAIN:
            /*  todo: 现在的做法是: 关闭client->server的读事件,等到本次读取到的数据在handle_pass中转发完了之后再继续
                可以提高吞吐量的一种做法是: 同时维护client->server的读取状态,和维护handle_pass中server->banckend的发送状态,这样同时进行
                需要验证的是: 进行压力测试,观察这个部分是否会成为某个小瓶劲(直觉上不会是优先的瓶劲)。思考在什么样的业务模式下会成为瓶劲。
                b->end = b->begin;
                将本次buffer所有数据都转发给backend
             */
            b->begin = b->data;
            if(req->upstream == NULL){
                return;
            }
            if(req->upstream->is_connected){
                // case2
                del_event(rev,READ_EVENT,0);
                add_event(&req->upstream->wev,WRITE_EVENT,0);//因为存在body,所以默认是需要后台转发。所以从第一次接受到数据开始,就需要监听front->backend
            }else{
                // case3 接受完成,但是还没有链接好后端服务器
                req->state = BODY_RECV_BEGIN;
            }
            return;
        case OK:
            del_event(rev,READ_EVENT,0);
            if(req->upstream == NULL){
                //case1
                c->wev.handler = handle_response;
                
                // 如果已经在解析的时候就出错了,那么response_done会被设置为true
                if (req->response_done){
                    return;
                }
                
                add_event(&c->wev,WRITE_EVENT,0);
                req->status = 200;
                construct_response(c->data);
                event_add_timer(&c->wev, server_cfg.post_accept_timeout);// todo: fix me : time out
                return;
            }else if(req->upstream->is_connected){
                // case2
                b->end = b->begin;
                b->begin = b->data;
                add_event(&req->upstream->wev,WRITE_EVENT,0);//当request处理完成了以后,接下来需要监听front->backend(需要转发最后一次的内容)
                event_add_timer(&req->upstream->wev, server_cfg.post_accept_timeout);// todo fix me, time out
            }else{
                // case3 等待后台通信成功/超时/失败
                b->end = b->begin;
                b->begin = b->data;
                req->state = BODY_RECV_DONE;
            }
            break;
        default:
            construct_err(req,req->connection, 400);
            return;
    }
    
}


/*解析buffer里面的header部分,如果阻塞,就加入timer*/
void http_parse_headers(event_t* rev) {
    int n;
    connection_t* c = rev->data;
    http_request_t* req = c->data;
    
    if(rev->timedout){
        plog("error:client time out: handle header");
        req->keep_alive = false;//超时,关闭链接,不再复用
        construct_err(req, req->connection, 404);
        return;
    }
    
    if(c->fd <= 0){
        return;
    }
    
    plog("handle headers for connection:%d",c->fd);
    
    /* 读取当前请求未解析的数据 */
    n = buffer_recv(req->recv_buffer, c->fd);
    /* 若链接关闭(OK)，或读取失败(ERROR)，则直接退出 */
    if (n == ERROR || n == OK) {
        plog("error:connection(%d) closed by peer when parsing the header",c->fd);
        http_close_request(req);
        return;
    }
    
    // 当解析没有完成的时候执行循环
    int err;
    while (!req->response_done) {
        err = parse_header(req,req->recv_buffer);
        switch (err) {
            case OK:
            {
                if(req->header_name.c == NULL){
                    goto done;
                }
                
                header_val_t* header_val = hash_find(header_map, req->header_name.c,req->header_name.len);
                *(req->header_name.c + req->header_name.len) = 0;
                if (header_val == NULL){
                    plog("unknown header:%s",req->header_name.c );
                    break;
                }

                if (header_val->offset != -1) {
                    int err = header_val->header_parser(req, header_val->offset);
                    if (err != 0) {
                        // 解析头部错误
                        plog("err on process header:%s",req->header_name.c);
                        http_close_request(req);
                        return;
                    }
                }
            }
                break;
            case ERROR:
            {
                *(req->header_name.c + req->header_name.len) = 0;
                plog("err on parse header:%s",req->header_name.c);
                http_close_request(req);
                return;
            }
                
            case AGAIN:
                if(!rev->timer_set){
                    event_add_timer(rev, server_cfg.post_accept_timeout);
                }
                return;
        }
    }
done:
    rev->handler = request_handle_body;
    request_handle_body(rev);
}

/*
 由于接受到数据而被唤醒,开始解析
 这里有两个可能的到达路径,目前来看没有什么区别:
    1 keep-alive的链接直接被唤起
    2 刚链接上的新链接
 在这里需要处理超时和链接被释放的情况(其他handler函数类似)
 */
void ngx_http_process_request_line(event_t* rev){
    //需要处理读取到的数据,直到把请求行读完
    int                n;
    http_request_t        *req;
    connection_t          *c;
    int rc;

    /* 获取当前请求所对应的连接 */
    c = rev->data;
    req = c->data;
    
    if (rev->timedout) {
        plog("error: connection(%d)client timed out in procees_request_line",c->fd);
        construct_err(req, req->connection, 404);
        return;
    }
    
    if(c->fd <= 0){
        return;
    }
    
    plog("process request line for connection:%d",c->fd);
    
    n = buffer_recv(req->recv_buffer, c->fd);
    
    /* 若链接关闭(OK)，或读取失败(ERROR)，则直接退出 */
    if (n == ERROR || n == OK) {
        plog("the connection(%d) failed in process_request_line(connection reset by peer or other unknown reason)",c->fd);
        http_close_request(req);
        return;
    }

    /* 解析接收缓冲区c->buffer中的请求行 */
    rc = parse_request_line(req, req->recv_buffer);
    
    /* 若请求行解析完毕 */
    if (rc == OK) {
        /* 开始解析header的部分,注意设置状态的转换 */
        plog("connection %d: process uri success",c->fd);
        rev->handler = http_parse_headers;
        req->state = HD_BEGIN;
        ERR_ON(request_process_uri(req) != OK,"err on process uri");
        http_parse_headers(rev);//交给解析头部的函数了
        return;
    }
    
    /* 解析请求行出错 */
    if (rc != AGAIN) {
        plog("err :connection %d: process uri failed",c->fd);
        construct_err(req, req->connection, 400);
        return;
    }
    
    // 阻塞了,需要设置超时事件
    event_add_timer(rev, server_cfg.post_accept_timeout);
    
    /* NGX_AGAIN: a request line parsing is still incomplete */
    /* 请求行仍然未解析完毕，则继续读取请求数据 */
    /* 若当前接收缓冲区内存不够，则分配更大的内存空间
       注意,如果分配更大内存,需要处理所有的uri上面的指针指向新的内存上面
        存在只发了一半的请求行就结束的可能
     */
    ABORT_ON(buffer_full(req->recv_buffer) || (n == OK), "buffer is not enough!!!");
    return;
}



/*
  Request-URI = [scheme ":" "//" host[":" port]][abs_path["?" query]]
 */
int parse_uri(uri_t* uri,char* p)
{
    char ch = *p;
    switch (uri->state) {
        case URI_BEGIN:
            switch (ch) {
                case '/':
                    uri->state = URI_SLASH_BEFORE_ABS_PATH;
                    break;
                case 'A' ... 'Z':
                case 'a' ... 'z':
                    uri->scheme.c = p;
                    uri->state = URI_SCHEME;
                    break;
                default:
                    return ERROR;
            }
            break;
        case URI_SCHEME:
            switch (ch) {
                case 'A' ... 'Z':
                case 'a' ... 'z':
                case '0' ... '9':
                case '+':
                case '-':
                case '.':
                    break;
                case ':':
                    uri->scheme.len = p - uri->scheme.c;
                    uri->state = URI_SCHEME_COLON;
                    break;
                default:
                    return ERROR;
            }
            break;
        case URI_SCHEME_COLON:
            switch (ch) {
                case '/':
                    uri->state = URI_SLASH_AFTER_SCHECME;
                    break;
                default:
                    return ERROR;
            }
            break;
        case URI_SLASH_AFTER_SCHECME:
            // http:
            switch (ch) {
                case '/':
                    uri->state = URI_SLASH_SLASH_AFTER_SCHECME;
                    break;
                default:
                    return ERROR;
                    break;
                }
            break;
        case URI_SLASH_SLASH_AFTER_SCHECME:
            switch (ch) {
                case 'A' ... 'Z':
                case 'a' ... 'z':
                case '0' ... '9':
                    uri->host.c = p;
                    uri->state = URI_HOST;
                    break;

                default:
                    return ERROR;
                    break;
            }
            break;
        case URI_HOST:
            switch (ch) {
                case 'A' ... 'Z':
                case 'a' ... 'z':
                case '0' ... '9':
                case '.':
                case '-':
                    break;
                case ':':
                    uri->host.len = p - uri->host.c;
                    uri->state = URI_PORT;
                    uri->port.c = p;
                    break;
                case '/':
                    uri->host.len = p - uri->host.c;
                    uri->state = URI_SLASH_BEFORE_ABS_PATH;
                    break;
                default:
                    return ERROR;
            }
            break;
        case URI_PORT:
            // ok.com:300//
            switch (ch) {
                case '0' ... '9':
                    break;
                case '/':
                    ++(uri->port.c);//skip the :
                    uri->port.len = p - uri->port.c;
                    uri->state = URI_SLASH_BEFORE_ABS_PATH;
                    break;
                default:
                    return ERROR;
            }
            break;
#   define ALLOWED_IN_ABS_PATH      \
'A' ... 'Z':           \
case 'a' ... 'z':           \
case '0' ... '9':           \
/* Mark */                  \
case '-':                   \
case '_':                   \
case '!':                   \
case '~':                   \
case '*':                   \
case '\'':                  \
case '(':                   \
case ')':                   \
/* Escaped */               \
case '%':                   \
case ':':                   \
case '@':                   \
case '&':                   \
case '=':                   \
case '+':                   \
case '$':                   \
case ',':                   \
case ';'
            
            
#   define ALLOWED_IN_QUERY         \
'.':                   \
case '/':                   \
case '?':                   \
case ALLOWED_IN_ABS_PATH
            
        case URI_SLASH_BEFORE_ABS_PATH:
            switch (ch) {
                case ALLOWED_IN_ABS_PATH:
                    uri->state = URI_ABS_PATH;
                    uri->abs_path.c = p;
                    break;
                default:
                    return ERROR;
                    break;
            }
            break;
            
        case URI_ABS_PATH:
            switch (ch) {
                case ALLOWED_IN_ABS_PATH:
                    break;
                case '.':
                    uri->abs_path.len = p - uri->abs_path.c;
                    uri->state = URI_BEFORE_EXTENSION;
                    break;
                case '?':
                    uri->state = URI_BEFORE_QUERY;
                    break;
            }
            break;
        case URI_BEFORE_EXTENSION:
            // /index.html
            switch (ch) {
                case 'a' ... 'z':
                case 'A' ... 'Z':
                    uri->extension.extension_str.c = p;
                    uri->state = URI_EXTENSION;
                    break;
                default:
                    return ERROR;
                    break;
            }
            break;
        case URI_EXTENSION:
            switch (ch) {
                case 'a' ... 'z':
                case 'A' ... 'Z':
                    break;
               case '?':
                    uri->extension.extension_str.len = p - uri->extension.extension_str.c;
                    uri->state = URI_BEFORE_QUERY;
                    break;
                default:
                    return ERROR;
                    break;
            }
            break;
        case URI_BEFORE_QUERY:
            switch(ch){
                case ALLOWED_IN_QUERY:
                    uri->query.c = p;
                    uri->state = URI_QUERY;
                    break;
                default:
                    return ERROR;
            }
            break;
        case URI_QUERY:
            switch (ch) {
                case ALLOWED_IN_QUERY:
                    break;
                default:
                    return ERROR;
            }
            break;
#   undef ALLOWED_IN_ABS_PATH

        default:
            return ERROR;
    }

    return OK;
}

int parse_request_method(char * begin,char * end){
    long len = end -  begin;
    if(len == 3 && strncmp(begin, "GET",len) == 0)
        return M_GET;
    if(len == 4 && strncmp(begin, "POST",len) == 0)
        return M_POST;
    if(len == 7 && strncmp(begin, "OPTIONS",len) == 0)
        return M_OPTIONS;
    
    return M_INVALID_METHOD;
}

/* 针对proxy模式进行转发,将收到的数据发送到后台
 * 注意这里针对的buffer是recv_buffer
 *
 *
 */
void http_proxy_pass(event_t* wev){
    connection_t* upstream = wev->data;
    if(upstream->fd <= 0){
        return;
    }
    
    http_request_t* r = upstream->data;
    connection_t* c = r->connection;
    buffer_t* rb = r->recv_buffer;
    
    if(wev->timedout){
        // 客户端超时,关闭链接,并且
        plog("timed out when pass %d",upstream);
        http_close_connection(upstream);
        construct_err(r, r->connection, 408);
        return;
    }
    
    int err = buffer_send(rb, upstream->fd);
    if (err == OK) {
        buffer_clear(rb);// 本次所有接受到的data已经转发完成了,移除对write事件的监听
        add_event(&c->rev,READ_EVENT,0);//监视client->server,这里有可能出现client断开链接
        del_event(&upstream->wev,WRITE_EVENT,0);
    } else if (err == ERROR) {
        // 对方已经关闭了链接,所以我们设置对客户端的回应信息
        plog("err on upstream %d",errno);
        construct_err(r, r->connection, 503);
    }
    
    if(!upstream->rev.timer_set){
        // 防止bakcend服务器超时
        event_add_timer(&upstream->rev, server_cfg.upstream_timeout);
    }
}
/* 从上游接受到的数据
 * 需要将数据转发给客户端
 * 这里针对的buffer是 send_buffer
 *
 */
void http_recv_upstream(event_t* rev){
    connection_t* upstream = rev->data;
    http_request_t* r = upstream->data;
    if(upstream->fd <= 0){
        return;
    }
    
    if(rev->timedout){
    // upstream超时,关闭链接
        plog("timed out when recv upstream %d",upstream->fd);
        http_close_connection(upstream);
        construct_err(r, r->connection, 504);
        return;
    }
    
    plog("connection: %d: recv from upstream(%d)",r->connection->fd,r->upstream->fd);
    
    int err = buffer_recv(r->send_buffer, upstream->fd);
    if(!r->connection->wev.handler){
        r->connection->wev.handler = handle_response;
    }
    
    // 获取了数据,所以需要转发
    add_event(&r->connection->wev,WRITE_EVENT,0);
    if(err == OK){
        // 上游关闭了链接
        // 关闭upstream的链接,但是不关闭请求。因为connection还需要回复client
        
        http_close_connection(upstream);
        r->response_done = true;// 告知 handle_response 可以结束了
        return;
    }else if(err == ERROR){
        // 上游出现了错误,那么我们直接构造返回的数据即可。
        // 注意这个时候connection可能处于两个状态 1 还在接受客户的数据(上游出错) 2 已经接受完了客户的数据,只等着转发回复到客户端
        // 无论哪个状态,都会直接关闭链接,返回错误信息
        
        plog("err on recv from upstream err:%d",errno);
        construct_err(r, r->connection, 503);
        return;
    }
    
    event_add_timer(rev, server_cfg.upstream_timeout);// 防止bakcend服务器超时
}

/*upstream的第一个回调函数
 在这个函数里面需要处理
 0  该链接已经被释放的情况,比如客户端已经关闭,那么这里就会被提前释放
    该链接对应的request居然已经被clear过了,这是非常严重的bug
    正常逻辑,clear_request之后,upstream链接一定被直接close的
    upstream被清空,那么对应的wev和rev也都不会有任何handler
 
 
 1 connect 失败/超时
 1.1 如果失败,需要设置对应的server的失败次数
 1.2 需要调用free函数进行释放
 
 2 connect 成功
 2.1 如果成功,需要设置request对应的current_upstream
 2.2 需要设置接下来对应的回调函数
 */
void process_connection_result_of_upstream(event_t* wev){
    connection_t* upstream = wev->data;
    
    /* 处理可能的corner case */
    if(upstream == NULL || upstream->fd <= 0){
        return;
    }
    
    http_request_t* r = upstream->data;
    upsream_server_t* us = r->cur_upstream;
    int fd = upstream->fd;
    
    /* 处理connect超时情况 */
    if(wev->timedout){
        plog("connect to upstream server(%s:%d) timedout,fd:%d",us->location->host.c,us->location->port,fd);
        construct_err(r, r->connection, 504);
        return;
    }
        
    /* 调用getsockopt来判断该socket的出错情况*/
    int err;
    socklen_t len = sizeof(err);
    if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&len) < 0)
    {
        plog("getsockopt failed, errno:%d",err);
        construct_err(r, r->connection, 502);
        return;
    }
    
    if(err == 0){
        // 已经链接成功了,那么继续
        plog("connect to upstream succ:info: server => backend  => port : %d => %d => %d",r->connection->fd,fd,us->location->port);
        r->cur_upstream = us;
        init_upstream_connection(r, upstream, fd);
    }else{
        //其他类型的错误,不能忽略
        
        /* connect失败,释放对应的upstream_server对应的数据 */
        r->cur_upstream = NULL;
        upsream_server_arr_t* server_domain = r->cur_server_domain;
        us->state = UPSTREAM_CONN_FAIL;
        free_upstream(us);
        
        /*  case1: 如果尝试次数小于服务器总次数,
                   接下来需要重新选择后端服务器(第二个参数为NULL是因为r的cur_server_domain一定不为空)
            case2: 如果已经到达最大尝试次数,那么继续调用try_connect_upstream,会进入错误页面的流程
            以上两种情况目前统一处理
         */
        
        // case1 || case2
        // 这里对fd/事件/timer的处理思路和try_connect中一致
        del_event(&r->upstream->wev,WRITE_EVENT,0);
        //close(fd);
        plog("connect to upstream(%s:%d) failed,errno:%d",us->location->host.c,us->location->port,err);
        try_connect_upstream(r,NULL);
    }
}



