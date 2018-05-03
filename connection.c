//
//  connection.c
//  pWenServer
//
//  Created by pyb on 2018/3/23.
//  Copyright © 2018年 pyb. All rights reserved.
//

/*
 connection的过期事件应该被融合到红黑树或者最小堆里面
 */

#include "server.h"
#include "pool.h"
#include "http.h"
#include "commonUtil.h"

connection_pool_t connection_pool;
void connectionPoolInit(int max_connections){
    max_connections = (max_connections / 8);
    poolInit(&connection_pool.cpool, sizeof(connection_t), 8, max_connections);

    //给所有的connection初始化
    int idx = 0;
    vector* vc = &(connection_pool.cpool.chunks);
    
    for(int i = 0;i < vc->capacity;++i){
        chunk* ch = vectorAt(vc,i);
        connection_t* con = ch->data;

        for(int k = 0; k < 8; ++k){
            // 注意这里要先保存next的值,否则会被覆盖
            connection_t* next_con = ((chunk_slot*)con)->next;
            //con->fd = -1;
            con->rev = &r_events[idx];
            con->wev = &w_events[idx];
            con->rev->data = con;
            con->wev->data = con;
            con->data = NULL;
            con = next_con;
            idx++;
        }
    }
}

/*分配一个空闲的链接对象*/
connection_t* getIdleConnection(){
    vector* vc = &(connection_pool.cpool.chunks);
    if(connection_pool.cpool.used < vc->capacity * 8){
        connection_t *c = (connection_t*)poolAlloc(&(connection_pool.cpool));
        event_t* rev = c->rev;
        event_t* wev = c->wev;
        http_request_t* req = c->data;
        memzero(c, sizeof(connection_t));
        c->rev = rev;
        c->wev = wev;
        c->data = req;// 保存可能存在的请求
        rev->handler = NULL;
        wev->handler = NULL;
        c->fd = -1;
        c->side = C_IDLE;
        return c;
    }
    
    plog("the connection exceed the limit!");
    return NULL;
}


/*
 关闭一个链接
 */
void http_close_connection(connection_t* c)
{
    close(c->fd);//读写事件自动被删除

    // 删除相关的定时器事件
    if (c->rev->timer_set) {
        event_del_timer(c->rev);
    }
    
    if (c->wev->timer_set) {
        event_del_timer(c->wev);
    }

    c->data = NULL;
    c->fd = -1;
    c->rev->active = false;
    c->wev->active = false;
    
    // 在accept之后,init request之前可能timeout,这时候是没有对connection分配内存的
    if(c->pool != NULL){
        freePool(c->pool);//将整个pool释放,那么导致buffer将被释放
    }
    poolFree(&connection_pool.cpool, c);//将链接还到正常的free链表里面
}



/****关于connection的buffer******/
/****关于connection的buffer******/
/****关于connection的buffer******/
/****关于connection的buffer******/
bool buffer_full(buffer_t* buffer) {
    return buffer->end >= buffer->limit;
}


buffer_t* createBuffer(memory_pool* pool){
    buffer_t* p = (buffer_t*)pMalloc(pool,sizeof(buffer_t));
    p->begin = p->data;
    p->end = p->data;
    return p;
}

int buffer_recv(buffer_t* buffer, int fd)
{
    while (!buffer_full(buffer))
    {
        int margin = (int)(buffer->limit - buffer->end);
        int len = (int)recv(fd, buffer->end, margin, 0);//最多读margin个字符
        if (len == 0)
        {   // EOF
            return OK;
        }
        if (len == -1) {
            if (errno == EAGAIN)
            {
                return AGAIN;
            }
            
            if(errno == EINTR){
                continue;
            }

            plog("error on buffer recv %d fd:%d",errno,fd);
            return ERROR;
        }
        //read_n += len;
        buffer->end += len;
    };  // We may have not read all data
    return AGAIN;
}

void buffer_clear(buffer_t* buffer)
{
    buffer->begin = buffer->data;
    buffer->end = buffer->data;
}

int buffer_size(buffer_t*buffer){
    return (int)(buffer->end - buffer->begin);
}

int buffer_send(buffer_t* buffer, int fd)
{
    plog("send buffer(fd:%d)",fd);

    while (buffer_size(buffer) > 0) {
        int len = (int)send(fd, buffer->begin, buffer_size(buffer), 0);
        if (len == -1) {
            if (errno == EAGAIN) {
                return AGAIN;
            }
            else if(errno == EINTR){
                continue;
            }
            plog("error on send buffer %d,fd :#d",errno,fd);
            return ERROR;
        }
        //sent += len;
        buffer->begin += len;
    };
    buffer_clear(buffer);
    return OK;
}


int append_string_to_buffer(buffer_t* buffer, const string* str)
{
    int margin = (int)(buffer->limit - buffer->end);
    assert(margin > 0);
    // todo: if the buffer is not enough,we should malloc a big buffer
    
    
    int appended = min(margin, str->len);
    memcpy(buffer->end, str->c, appended);
    buffer->end += appended;
    return appended;
}

int buffer_sprintf(buffer_t* buffer, const char* format,...)
{
    va_list args;
    va_start (args, format);
    int margin = (int)(buffer->limit - buffer->end);
    assert(margin > 0);

    int len = vsnprintf(buffer->end, margin, format, args);
    buffer->end += len;
    assert(len <= margin);// todo:the buffer is not enough
    va_end (args);
    return len;
}


