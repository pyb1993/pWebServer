//
//  header.c
//  pWenServer
//
//  Created by pyb on 2018/4/16.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "header.h"
#include "string.h"
#include "http.h"

#define MAX_HEADER_SIZE 136
#define HEADER_KV(name,header_praser) {STRING(#name),{offsetof(request_header_t, name), header_praser}}

/*静态数组,用来初始化 header hash的*/
static header_kv_t header_array[] = {
    HEADER_KV(accept, header_handle_generic),
    HEADER_KV(cookie, header_handle_generic),
    HEADER_KV(date, header_handle_generic),
    HEADER_KV(connection, header_handle_connection)

};

static hash_key header_ele_array[MAX_HEADER_SIZE] = {};

hash* header_map;

/*利用hash构造 header*/
void header_map_init(){
    int len = sizeof(header_array) / sizeof(header_kv_t);
    
    // 将header的东西转换成hashinit能够接受的参数
    for(int i = 0; i < len; ++i){
        string name = header_array[i].name;
        header_ele_array[i].key = name;
        header_ele_array[i].key_hash = hash_key_function(name.c,name.len);
        header_ele_array[i].value = &header_array[i].val;
    }
    
    memory_pool *pool = createPool(2 * MAX_HEADER_SIZE * HASH_ELT_SIZE(&header_ele_array[0]));
    hash * h = (hash*) pMalloc(pool, sizeof(hash));
    hash_initializer header_hash_init;
    header_hash_init.pool = pool;
    header_hash_init.hash = h;
    header_hash_init.bucket_size = 128;
    header_hash_init.max_size = 1024;
    int ret = hashInit(&header_hash_init, header_ele_array, len);
    ABORT_ON(ret == ERROR, "init header failed!!!");
    header_map = header_hash_init.hash;
}

/***
    用来解析http header部分
 ****/
int parse_header(http_request_t* r,buffer_t* b)
{
#define HEADER_NAME_ALLOW_CASE \
 'A' ... 'Z':\
    *p = ch = ch - 'A' + 'a';\
case 'a' ... 'z':\
case '0' ... '9':\
case '-':\
case '_':\

    char *p;
    for(p = b->begin; p != b->end; ++p){
        char ch = *p;
        switch (r->state) {
            case HD_BEGIN:
                stringInit(&r->header_name);
                stringInit(&r->header_value);
                switch (ch) {
                  case HEADER_NAME_ALLOW_CASE
                        r->header_name.c = p;
                        r->state = HD_NAME;
                        break;
                    case '\r':
                        r->state = HD_ALMOST_DONE;
                        break;
                    case '\n':
                        goto done;
                        break;
                    case ' ':
                        break;
                    default:
                        goto error;
                        break;
                }
                break;
            case HD_NAME:
                switch (ch) {
                    case HEADER_NAME_ALLOW_CASE
                        break;
                    case ':':
                        r->header_name.len = p - r->header_name.c;
                        r->state = HD_COLON;
                        break;
                    case '\r':
                        r->state = HD_ALMOST_DONE;
                        break;
                    case ' ':
                        r->state = ERROR;
                        break;
                    default:
                        goto error;
                        break;
                }
            break;
                
            case HD_BEFORE_VALUE:
            case HD_COLON:
                switch (ch) {
                   case ' ':
                        r->state = HD_BEFORE_VALUE;
                        break;
                    default:
                        r->state = HD_VALUE;
                        r->header_value.c = p;
                        break;
                }
                break;
            case HD_VALUE:
                switch (ch) {
                    case '\r':
                        r->header_value.len = p - r->header_value.c;
                        r->state = HD_ALMOST_DONE;
                        break;
                    case '\n':
                        r->header_value.len = p - r->header_value.c;
                        goto done;
                        break;
                    default:
                        break;
                }
                break;
            break;
            case HD_ALMOST_DONE:
                switch (ch) {
                    case '\n':
                        goto done;
                        break;
                    default:
                        goto error;
                        break;
                }
                break;
            default:
                plog("error on invalid header state %d",r->state);
                goto error;
        }
    }
#undef HEADER_NAME_ALLOW_CASE
    
    b->begin = b->end;
    return AGAIN;
    
    error:
    return ERROR;
    
    done:
    b->begin = p + 1;
    r->state = HD_BEGIN;
    
    return OK;
}


int header_handle_generic(http_request_t* r, int offset) {
    string* member = (string*)((char*)&r->headers + offset);
    *member = r->header_value;
    return OK;
}


int header_handle_connection(http_request_t* r,int offset){
    header_handle_generic(r, offset);
    request_header_t* headers = &r->headers;
    if(strncasecmp("keep-alive", headers->connection.c, 10) == 0) {
        r->keep_alive = true;
    } else if(strncasecmp("close", headers->connection.c, 5) == 0) {
        r->keep_alive = false;
    } else {
        return construct_err(r,r->connection, 400);
    }
    return OK;
}





