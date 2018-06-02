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
#include "upstream_server_module.h"
#include "commonUtil.h"

connection_pool_t connection_pool;
void connectionPoolInit(int max_connections){
    //max_connections = (max_connections / 8);
    poolInit(&connection_pool.cpool, sizeof(connection_t), 8, 0);

    /*给所有的connection初始化*/
    //int idx = 0;
    //vector* vc = &(connection_pool.cpool.chunks);
    
    /*不进行任何初始化*/
    /*
    for(int i = 0;i < vc->capacity; ++i){
        chunk* ch = vectorAt(vc,i);
        connection_t* con = ch->data;
        for(int k = 0; k < 8; ++k){
            // 注意这里要先保存next的值,否则会被覆盖
            connection_t* next_con = ((chunk_slot*)con)->next;
            con->rev = &r_events[idx];
            con->wev = &w_events[idx];
            con->rev->data = con;
            con->wev->data = con;
            con->data = NULL;
            con = next_con;
            idx++;
        }
    }*/
}

// 用来debug的一个函数
void test_connection(){
    /*
    static int tt = 0;
    int used = 0;
    tt++;
    if(tt < 500){
        return;
    }else{
    //给所有的connection初始化
    int idx = 0;
    for(;idx < 10000 ;++idx ){
        connection_t* c = pos[idx];
        if(c->used_now)
        {
            used++;
        }
    
    }*/
}
 






/*分配一个空闲的链接对象
  这里有几点需要注意:
    1 是否需要保留request?
        如果是调用http_close_request,那么request早就被清空
        如果是调用http_clear_request,那么根本不会走到getIdleConnection函数
    2 将rev和wev对应的handler清空,这种情况下,即便被stable event唤醒,也不会调用这个handler
 
    3 因为所有的event都是静态分配的,所以需要保存以前分配好的event
 */
connection_t* getIdleConnection(){
    static int event_alloc_pos = 0;// 记录第几个event被绑定到分配的connection上面了
    if(connection_pool.cpool.used <  server_cfg.max_connections){
        connection_t *c = poolAlloc(&(connection_pool.cpool));
        /*
        if(c->rev == NULL || c->wev == NULL){
            rev = &r_events[event_alloc_pos];
            wev = &w_events[event_alloc_pos];
            wev->data = c;
            rev->data = c;
            event_alloc_pos++;
        }else{
            rev = c->rev;
            wev = c->wev;
        }*/

        memzero(c, sizeof(connection_t));
        c->rev.data = c;
        c->wev.data = c;
        c->fd = -33;
        c->rev.handler = NULL;
        c->wev.handler = NULL;
        return c;
    }
    
    plog("the connection exceed the limit!");
    return NULL;
}

void init_connection(connection_t* c)
{
    event_t* rev = &c->rev;
    
    //读事件回调
    rev->handler = http_init_request;//应该是init_request函数
    
    // post_accept_timeout超时事件,这是第一个超时事件
    event_add_timer(rev, server_cfg.post_accept_timeout);
    
    // 将读事件注册到kqueue中，这个阶段暂时不需要关注写事件
    if (add_event(rev, READ_EVENT, 0) == ERROR){
        return;
    }
    
    plog("init the conncection for %d", c->fd);
}


/*
 * 关闭一个链接
 * 这里要处理多个corner_case:
 * 1 刚刚分配链接的时候,这个时候request == NULL,is_connected = true fd > 0
   2 所有后端服务都失败了,此时request != NULL, is_connected = false, fd > 0
   3 所有后端服务都处于失败状态,没有调用任何一次try_connect,此时 request != NULL, is_connected = false, fd < 0
 所以,我们的判断条件是 request == NULL && is_connected == false,这种情况下就代表一定释放过了,不重复释放
 */
void http_close_connection(connection_t* c)
{

    http_request_t* r = c->data;
    if(r == NULL && c->is_connected == false){
        return;
    }
    
    // 清除request对应对connection的映射
    if(r != NULL){
        if(c == r->connection){
            r->connection = NULL;
        }else{
            r->upstream = NULL;
        }
    }
    
    plog("close connection %d ",c->fd);
    if(c->fd > 0){
        close(c->fd);//读写事件自动被删除
    }
    
    // 删除相关的定时器事件
    if (c->rev.timer_set) {
        event_del_timer(&c->rev);
    }
    
    if (c->wev.timer_set) {
        event_del_timer(&c->wev);
    }
    
    c->fd = -10086;
    c->data = NULL;
    c->is_idle = false;
    c->is_connected = false;
    c->rev.active = false;
    c->wev.active = false;
    // 注意这里有一个bug,因为放回free链表里会导致fd被next占用
    poolFree(&connection_pool.cpool, c);//将链接还到正常的free链表里面,注意这里有一个bug
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
    plog("recv buffer(fd:%d), the buffer adress is %d",fd,(long long)buffer);
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
        buffer->end += len;
        break;
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
            plog("error on send buffer %d,fd :%d",errno,fd);
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
    va_end(args);
    return len;
}

/*
 *在shutdown的时候关闭所有的空闲链接
 这样剩下的时间都是和活跃事件相关的链接了
 所以「所有事件结束」这个逻辑得以成立,否则空闲链接的过期事件监听会导致上述判断无法成立
 
 @logic : 需要注意的是,怎么进行遍历,这里的遍历不能使用next(那是指向free的)
          所以需要通过计算大小来进行遍历
 
 */
void clear_idle_connections(){
    vector* vc = &(connection_pool.cpool.chunks);
    for(int i = 0;i < vc->capacity; ++i){
        chunk* ch = vectorAt(vc,i);
        chunk_slot* cs = ch->data ;
        for(int k = 0; k < 8; ++k){
            /* 代表链接有效且处于idle状态
             * upstream链接不会有idle == true的状态
             */
            
            connection_t* c = get_data_from_chunk(cs);
            if(c->fd > 0 && c->is_idle){
                http_close_connection(c);
            }

            c->data = NULL;
            cs = (uint8_t*)ch->data + (k + 1) * (sizeof(connection_t) + sizeof(void*));// 直接计算第k个chunk_slot
        }
    }
}

