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

/* 用来初始化一个http链接(connection) *
 设置对应的事件回调函数
 */
#define RETURN_ERR_ON(cond, err) {   \
if (cond) {                     \
return (err);               \
}                               \
};
void http_init_connection(connection_t* c)
{
    event_t* rev = c->rev;
    rev->timedout = 0;//设置超时状态
    //读事件回调
    rev->handler = http_init_request;//应该是init_request函数
    
    //该写回调没有做任何事件，因为这个阶段还不需要向客户端写入任何数据
    c->wev->handler = NULL;
    
    //将读事件插入到红黑树中，用于管理超时事件，post_accept_timeout超时事件,这是第一个超时事件,实际上后面的函数都可以检验这个超时事件
    event_add_timer(rev, server_cfg.post_accept_timeout);
    
    //将读事件注册到epoll中，此时并没有把写事件注册到epoll中，因为现在还不需要向客户端发送任何数据，所以写事件并不需要注册
    if (add_event(rev, READ_EVENT, 0) == ERROR)
    {
        plog("add event error");
        return;
    }
    
    plog("init the conncection for %d", c->fd);
}

/*分配一个request和它的内存池
  request的所有内存都是由内存池分配的,
  包括它自己,所以释放的时候,应该最后释放pool
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
    plog("close the request %d",r->connection->fd);
    connection_t* c = r->connection;
    if(c != NULL){
        http_close_connection(c);
    }

    if(r->upstream != NULL){
        http_close_connection(r->upstream);
    }
    if(r->resource_fd > 0){
        close(r->resource_fd);
        r->resource_fd = -1;
    }
    
    freePool(r->pool);//连request自己都被释放了
}


/*clear request,但是不释放链接,也不释放request的内存池,等待下一次的链接重用
  注意这里需要释放upstream链接,因为下一个请求不一定还是需要转发的了.
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
}

// 处理response发完以后的请求
void http_response_done(http_request_t* r){
    if(r->keep_alive){
        connection_t* c = r->connection;
        plog("reuse the connection  fd(%d)",c->fd);
        del_event(c->wev,WRITE_EVENT,0);//已经写完了
        add_event(c->rev,READ_EVENT,0);//已经写完了,回复到读取的状态
        http_clear_request(r);// 清理请求的状态,等待复用
        c->rev->handler = ngx_http_process_request_line;//重新恢复到处理请求行的状态
        // 这里需要添加一个定时器,避免大量的空闲链接,同时删除写事件的定时器
        event_del_timer(c->wev);
        event_add_timer(c->rev, server_cfg.keep_alive_timeout);
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
    
    if(wev->timedout){
        plog("server timed out : send response");
        http_close_request(r);
        return;
    }
    
    if(c->fd == -1){
        return;
    }
    
    int err = buffer_send(r->send_buffer, c->fd);
    if(err == OK){
        if(r->resource_fd > 0 && !r->response_done)
        {
            //注意response_done的用途,如果已经出错,那么就不执行发送file的操作
            plog("send file (fd:%d)",c->fd);
            c->wev->handler = handle_response_file;
            handle_response_file(c->wev);
            return;
        }
        http_response_done(r);
    }
    else if(err == AGAIN){
        if(wev->timer_set){
            event_add_timer(wev,server_cfg.post_accept_timeout);
        }
    }
    else if(err == ERROR){
        http_close_request(r);
    }
}



void handle_response_file(event_t* wev)
{
    connection_t* c = wev->data;
    http_request_t* r = c->data;
    if(c->fd < 0 ){
        http_close_request(r);
        return;
    }
    
    if(wev->timedout){
        plog("server timed out : send response file");
        r->keep_alive = false;//超时,关闭链接,不再复用
        construct_err(r, r->connection, 404);
        //http_close_request(r);
        return;
    }
    
    int err = send_file(r);
    if(err == OK){
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
 接收到真正的数据时,才开始初始化http_request
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

    plog("begin init request!!!");
    c->data = create_http_request();
    http_request_t* r = (http_request_t*)c->data;
    r->connection = c;
    
    if(r->recv_buffer == NULL){
        // 这时候可能是复用的tcp链接,所以可能会存在对应的recv_buffer已经分配好了
        r->recv_buffer = createBuffer(r->pool);
    }
    r->send_buffer = createBuffer(r->pool);// 因为upstream是一个短链接,所以每一次请求都会导致send_buffer被释放
    
    /* 设置当前读事件的处理方法为ngx_http_process_request_line */
    rev->handler = ngx_http_process_request_line;
    
    /* 执行该读事件的处理方法ngx_http_process_request_line，接收HTTP请求行 */
    ngx_http_process_request_line(rev);
}



void request_handle_body(event_t * rev)
{
    connection_t* c = rev->data;
    http_request_t* req = c->data;
    
    if(c->fd == -1){
        http_close_request(req);
        return;
    }
    
    if(rev->timedout){
        plog("client timed out : handle body");
        req->keep_alive = false;//超时,关闭链接,不再复用
        construct_err(req, req->connection, 404);
        //http_close_request(req);
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
            // TODO(wgtdkp): cannot understanding
            // May discard the body
            assert(false);
    }
    
    /*
     AGINA意味着: body是存在的,那么就应该存在对应的uc
     OK意味这body部分已经结束:(无论不存在还是已经读完了)
     在这里发生的EVENTS的状态改变
     connection_disable_in: 清除EVENTS_IN的状态
     connection_enable_in: 设置EVENT_IN的状态
     */
    
    buffer_t* b = req->recv_buffer;
    switch (err)
    {
        case AGAIN:
            // todo: 现在的做法是: 关闭client->server的读事件,等到本次读取到的数据在handle_pass中转发完了之后再继续
            // 可以提高吞吐量的一种做法是: 同时维护client->server的读取状态,和维护handle_pass中server->banckend的发送状态,这样同时进行
            // 需要验证的是: 进行压力测试,观察这个部分是否会成为某个小瓶劲(直觉上不会是优先的瓶劲)。思考在什么样的业务模式下会成为瓶劲。
            // 不考虑删除定时器,因为切换时间非常短。event_del_timer(rev);
            //connection_disable_in(r->c);
            b->end = b->begin;//将本次buffer所有数据都转发给backend
            b->begin = b->data;
            if(req->upstream){
                del_event(rev,READ_EVENT,0);
                add_event(req->upstream->wev,WRITE_EVENT,0);//因为存在body,所以默认是需要后台转发。所以从第一次接受到数据开始,就需要监听front->backend
            }
            return;
        case OK:
            event_del_timer(rev);
            del_event(rev,READ_EVENT,0);
            if (!req->upstream) {
                //没有和后台通信的情况
                c->wev->handler = handle_response;
                // 如果已经在解析的时候就出错了,那么response_done会被设置为true
                if (req->response_done){
                    return;
                }
                add_event(c->wev,WRITE_EVENT,0);
                req->status = 200;
                construct_response(c->data);
                event_add_timer(c->wev, server_cfg.post_accept_timeout);// todo: fix me : time out
            }else {
                // todo add timer
                b->end = b->begin;
                b->begin = b->data;
                add_event(req->upstream->wev,WRITE_EVENT,0);//当request处理完成了以后,接下来需要监听front->backend(需要转发最后一次的内容)
            }
            break;
        default:
            construct_err(req,req->connection, 400);
            return;
    }
    
}


/*解析buffer里面的header部分,如果阻塞,就加入timer*/
void request_handle_headers(event_t* rev) {
   
    connection_t* c = rev->data;
    http_request_t* req = c->data;
    
    if(rev->timedout){
        plog("client time out: handle header");
        req->keep_alive = false;//超时,关闭链接,不再复用
        construct_err(req, req->connection, 404);
        //http_close_request(req);
        return;
    }
    
    if(c->fd == -1){
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
        plog("client timed out");
        req->keep_alive = false;//超时,关闭链接,不再复用
        construct_err(req, req->connection, 404);
        //http_close_request(req);
        return;
    }
    
    if(c->fd == -1){
        http_close_request(req);
        return;
    }
    
    /* 读取当前请求未解析的数据 */
    n = buffer_recv(req->recv_buffer, c->fd);

    /* 若链接关闭(OK)，或读取失败(ERROR)，则直接退出 */
    if (n == ERROR || n == OK) {
        plog("the request done");
        http_close_request(req);
        return;
    }

    /* 解析接收缓冲区c->buffer中的请求行 */
    rc = http_parse_request_line(req, req->recv_buffer);
    
    /* 若请求行解析完毕 */
    if (rc == OK) {
        /* 开始解析header的部分,注意设置状态的转换 */
        rev->handler = request_handle_headers;
        req->state = HD_BEGIN;
        request_process_uri(req);
        plog("process uri succ");
        request_handle_headers(rev);//交给解析头部的函数了
        return;
    }
    
    /* 解析请求行出错 */
    if (rc != AGAIN) {
        /* there was error while a request line parsing */
        construct_err(req, req->connection, 400);
        return;
    }
    
    // 阻塞了,需要设置超时事件
    if (!rev->timer_set) {
        event_add_timer(rev, server_cfg.post_accept_timeout);
    }
    
    /* NGX_AGAIN: a request line parsing is still incomplete */
    /* 请求行仍然未解析完毕，则继续读取请求数据 */
    /* 若当前接收缓冲区内存不够，则分配更大的内存空间
       注意,如果分配更大内存,需要处理所有的uri上面的指针指向新的内存上面
        存在只发了一半的请求行就结束的可能
     */
    ABORT_ON(buffer_full(req->recv_buffer) || (n == OK), "buffer is not enough!!!");
    return;
}

/*解析请求行 method uri http/version*/
int http_parse_request_line(http_request_t* r,buffer_t *b){
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



