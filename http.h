//
//  http.h
//  pWenServer
//
//  Created by pyb on 2018/3/28.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef http_h
#define http_h

#include <stdio.h>
#include "string_t.h"
#include "connection.h"
#include "memory_pool.h"
#include "header.h"


typedef enum{
    RQ_BEGIN,
    RQ_METHOD,
    RQ_URI,
    RQ_BEFORE_URI,
    RQ_BEFORE_VERSION,
    RQ_VERSION_H,
    RQ_VERSION_HT,
    RQ_VERSION_HTT,
    RQ_VERSION_HTTP,
    RQ_VERSION_SLASH,
    RQ_VERSION_MAJOR,
    RQ_VERSION_DOT,
    RQ_VERSION_MINOR,
    RQ_AFTER_VERSION,
    RQ_ALMOST_DONE,
    RQ_DONE,
    // header状态
    HD_BEGIN,
    HD_NAME,
    HD_IGNORE,
    HD_IGNORE_TO_BEGIN,
    HD_COLON,
    HD_BEFORE_VALUE,
    HD_VALUE,
    HD_ALMOST_DONE    
    
} req_state_t;

typedef enum{
    URI_BEGIN,
    URI_SCHEME,
    URI_SCHEME_COLON,
    URI_PORT,
    URI_HOST,
    URI_BEFORE_QUERY,
    URI_EXTENSION,
    URI_BEFORE_EXTENSION,
    URI_QUERY,
    URI_ABS_PATH,
    URI_SLASH_BEFORE_ABS_PATH,
    URI_SLASH_AFTER_HOST,
    URI_SLASH_SLASH_AFTER_HOST,
    URI_SLASH_AFTER_SCHECME,
    URI_SLASH_SLASH_AFTER_SCHECME
} uri_state_t;

typedef enum{
    M_GET,
    M_POST,
    M_OPTIONS,
    M_INVALID_METHOD
}method_t;

typedef enum {
    HTML,
    JSON,
    TXT,
    UNKNOWN_EXTENSION
}extenstion_t;

typedef struct uri_t{
    req_state_t state;
    string scheme;
    string abs_path;
    string host;
    string port;
    union{
        string extension_str;
        extenstion_t extension_type;
    } extension;
    string query;
}uri_t;

// 1.1
typedef struct {
    uint16_t major;
    uint16_t minor;
} version_t;

typedef struct http_request_s{
    memory_pool* pool;
    connection_t* connection;
    connection_t* upsream;
    string request_line;
    uri_t uri;
    version_t version;
    method_t method;
    string header_name;
    string header_value;
    request_header_t headers;
    
    int status;
    int state;
    int port;
    int resource_off;
    int resource_fd;
    long resource_len;
    uint8_t response_done:1;
    uint8_t keep_alive: 1;
} http_request_t;


void http_init_connection(connection_t* c);
void ngx_http_process_request_line(event_t* rev);
int http_parse_request_line(http_request_t* r,buffer_t* b);
int parse_request_method(char * begin,char * end);
int parse_uri(uri_t*,char *begin);
void http_init_request(event_t* rev);
void http_close_request(http_request_t*);
void http_clear_request(http_request_t*);
int send_file(http_request_t* r);
void handle_response_file(event_t*);
void http_response_done();
http_request_t* create_http_request();
void request_handle_headers(event_t* rev);
void request_handle_body(event_t * rev);
int request_process_uri(http_request_t* r);
int send_respose(event_t* wev);

void handle_response(event_t* wev);
int construct_err(http_request_t* r,connection_t* c, int err);
void construct_response(http_request_t* req);
#endif /* http_h */
