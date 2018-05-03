//
//  http_response.c
//  pWenServer
//
//  Created by pyb on 2018/4/23.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "http.h"
#include "event.h"
#include "server.h"
#include "module.h"
#include "connection.h"
#include "string_t.h"
#include "commonUtil.h"
#include "header.h"

#define CRLF "\r\n"

static char err_page_tail[] =
"<hr><center><span style='font-style: italic;'>"
 "</span></center>" CRLF
"</body>" CRLF
"</html>" CRLF;

static char err_301_page[] =
"<html>" CRLF
"<head><title>301 Moved Permanently</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>301 Moved Permanently</h1></center>" CRLF;

static char err_302_page[] =
"<html>" CRLF
"<head><title>302 Found</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>302 Found</h1></center>" CRLF;

static char err_303_page[] =
"<html>" CRLF
"<head><title>303 See Other</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>303 See Other</h1></center>" CRLF;

static char err_307_page[] =
"<html>" CRLF
"<head><title>307 Temporary Redirect</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>307 Temporary Redirect</h1></center>" CRLF;

static char err_400_page[] =
"<html>" CRLF
"<head><title>400 Bad Request</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF;

static char err_401_page[] =
"<html>" CRLF
"<head><title>401 Authorization Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>401 Authorization Required</h1></center>" CRLF;

static char err_402_page[] =
"<html>" CRLF
"<head><title>402 Payment Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>402 Payment Required</h1></center>" CRLF;

static char err_403_page[] =
"<html>" CRLF
"<head><title>403 Forbidden</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>403 Forbidden</h1></center>" CRLF;

static char err_404_page[] =
"<html>" CRLF
"<head><title>404 Not Found</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>404 Not Found</h1></center>" CRLF;

static char err_405_page[] =
"<html>" CRLF
"<head><title>405 Not Allowed</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>405 Not Allowed</h1></center>" CRLF;

static char err_406_page[] =
"<html>" CRLF
"<head><title>406 Not Acceptable</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>406 Not Acceptable</h1></center>" CRLF;

static char err_407_page[] =
"<html>" CRLF
"<head><title>407 Proxy Authentication Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>407 Proxy Authentication Required</h1></center>" CRLF;

static char err_408_page[] =
"<html>" CRLF
"<head><title>408 Request Time-out</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>408 Request Time-out</h1></center>" CRLF;

static char err_409_page[] =
"<html>" CRLF
"<head><title>409 Conflict</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>409 Conflict</h1></center>" CRLF;

static char err_410_page[] =
"<html>" CRLF
"<head><title>410 Gone</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>410 Gone</h1></center>" CRLF;

static char err_411_page[] =
"<html>" CRLF
"<head><title>411 Length Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>411 Length Required</h1></center>" CRLF;

static char err_412_page[] =
"<html>" CRLF
"<head><title>412 Precondition Failed</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>412 Precondition Failed</h1></center>" CRLF;

static char err_413_page[] =
"<html>" CRLF
"<head><title>413 Request Entity Too Large</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>413 Request Entity Too Large</h1></center>" CRLF;

static char err_414_page[] =
"<html>" CRLF
"<head><title>414 Request-URI Too Large</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>414 Request-URI Too Large</h1></center>" CRLF;

static char err_415_page[] =
"<html>" CRLF
"<head><title>415 Unsupported Media Type</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>415 Unsupported Media Type</h1></center>" CRLF;

static char err_416_page[] =
"<html>" CRLF
"<head><title>416 Requested Range Not Satisfiable</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>416 Requested Range Not Satisfiable</h1></center>" CRLF;

static char err_417_page[] =
"<html>" CRLF
"<head><title>417 Expectation Failed</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>417 Expectation Failed</h1></center>" CRLF;


static char err_500_page[] =
"<html>" CRLF
"<head><title>500 Internal Server Error</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>500 Internal Server Error</h1></center>" CRLF;

static char err_501_page[] =
"<html>" CRLF
"<head><title>501 Not Implemented</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>501 Not Implemented</h1></center>" CRLF;

static char err_502_page[] =
"<html>" CRLF
"<head><title>502 Bad Gateway</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>502 Bad Gateway</h1></center>" CRLF;

static char err_503_page[] =
"<html>" CRLF
"<head><title>503 Service Temporarily Unavailable</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>503 Service Temporarily Unavailable</h1></center>" CRLF;

static char err_504_page[] =
"<html>" CRLF
"<head><title>504 Gateway Time-out</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>504 Gateway Time-out</h1></center>" CRLF;

static char err_505_page[] =
"<html>" CRLF
"<head><title>505 HTTP Version Not Supported</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>505 HTTP Version Not Supported</h1></center>" CRLF;

static char err_507_page[] =
"<html>" CRLF
"<head><title>507 Insufficient Storage</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>507 Insufficient Storage</h1></center>" CRLF;


void buffer_append_status_line(version_t version,int status,buffer_t* b)
{
    
  if(version.minor == 1){
        buffer_append_cstring(b, "HTTP/1.1 ");
    }
    else{
        buffer_append_cstring(b, "HTTP/1.2 ");
    }

    switch (status) {
        case 100: buffer_append_cstring(b,"100 continue");
        case 200: buffer_append_cstring(b,"200 OK");
        default:  buffer_append_cstring(b,"500 Internal Server Error");
    }
    buffer_append_cstring(b,"\r\n");
}

void append_err_page(buffer_t* b,int err){
    int len = sizeof(err_page_tail) - 1;;
#define ERR_PAGE(status) case status:\
    len += sizeof(err_##status##_page) - 1;\
    buffer_sprintf(b, "Content-Length: %d\r\n",len);\
    buffer_append_cstring(b, "\r\n");\
    append_string_to_buffer(b,&(string){err_##status##_page,len});\
    break;
    
    switch(err){
    ERR_PAGE(400);
    ERR_PAGE(403);
    ERR_PAGE(404);
    ERR_PAGE(500);
    }
    
    append_string_to_buffer(b,&(string){err_page_tail,sizeof(err_page_tail)});

#undef ERR_PAGE
}

/*response done来指示,什么时候整个请求已经结束了
 */
int construct_err(http_request_t* r,connection_t* c, int err) {
    buffer_t* b = c->buffer;
    buffer_clear(b);
    buffer_append_status_line(r->version, err, b);
    buffer_append_cstring(b, "Server: " "pwebServer" "\r\n");
    buffer_append_cstring(b, "Connection: close" "\r\n");
    buffer_append_cstring(b, "Content-Type: text/html" "\r\n");
    append_err_page(b,err);
    buffer_append_cstring(b, "\r\n");
    add_event(c->wev,WRITE_EVENT,0);
    del_event(c->rev,READ_EVENT,0);
    r->status = err;
    r->response_done = true;
    return OK;
}

// 写入回复应该有的header
void construct_response(http_request_t* r){
    connection_t* c = r->connection;
    buffer_t* b = c->buffer;
    buffer_clear(c->buffer);
    buffer_append_status_line(r->version, r->status, b);
    
    // append content type to
    switch(r->uri.extension.extension_type){
        case HTML:
            buffer_append_cstring(b, "Content-Type: text/html; charset=utf-8\r\n");
            break;
        case TXT:
            buffer_append_cstring(b, "Content-Type: text/plain; charset=utf-8\r\n");
            break;
        case JSON:
            buffer_append_cstring(b, "Content-Type: application/json; charset=utf-8\r\n");
            break;
        default:
            plog("unknown extension");
            buffer_append_cstring(b, "Content-Type: text/html; charset=utf-8\r\n");
            break;
    }

    buffer_sprintf(b,"Content-Length: %d\r\n",r->resource_len);
    buffer_append_cstring(b, "\r\n");
}

int send_file(http_request_t* r){
    connection_t* c = r->connection;
    
    long long should_send_len = r->resource_len;
    while (1) {
        int ret = sendfile(r->resource_fd,c->fd, r->resource_off, &should_send_len,NULL,0);
        if(ret == -1){
            if(errno == EAGAIN){
                r->resource_len -=  should_send_len;
                r->resource_off += should_send_len;
                return AGAIN;
            }
            else if(errno == EINTR){
                continue;
            }
            else {
                plog("error on send file(%d) err:%d,fd:%d",r->resource_fd,errno,c->fd);
                return ERROR;
            }
        }
        break;// OK
    }
    
    r->resource_off += should_send_len;
    r->resource_len -= should_send_len;
    ABORT_ON(r->resource_len != 0, "???");
    return OK;
}
