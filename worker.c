//
//  worker.c
//  pWenServer
//
//  Created by pyb on 2018/4/23.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "worker.h"
#include "header.h"
#include "commonUtil.h"
#include "unistd.h"
#include "event.h"
#include "server.h"
#include <sys/time.h>
int p_reap = 0;
int p_exit = 0;
int p_terminate = 0;
int p_exiting = 0;
int p_sigalarm = 0;
int p_quit = 0;
long delay = 0;
int p_event_timer_alarm = 0;//是否收到相应的信号

// 向所有的worker发送信号
void signal_worker_processes(vector* workers,int signo){
    for(int i = 0; i < workers->used; ++i){
        worker_t* worker = vectorAt(workers,i);
        if (worker->pid == -1) {
            continue;
        }
        
        // worker正在退出,那么就不需要再次发送quit信号(gracefully shutdown)
        if (worker->exiting && signo == SIGQUIT){
            continue;
        }
        
        if (kill(worker->pid, signo) == -1) {
            plog("kill(%d, %d) failed", worker->pid, signo);
            //没有找到这样的process group或者process
            if (errno == ESRCH) {
                worker->exited = 1;
                worker->exiting = 0;
                p_reap = 1;
            }
            
            continue;
        }
        
        worker->exiting = 1;
    }
}

// 直接关闭所有的链接
void worker_process_exit(vector* workers)
{
    //关闭所有的链接
    plog("the worker %d exit!",getpid());
    exit(0);
}



// the worker will run this cycle
void worker_cycle_process()
{
    //ABORT_ON(process_status != WORKER, "this function can only run in worker!!!");
    worker_process_init();
    
    while(1){
        if(p_exiting){
            /* 代表正在退出(shutting down)
               这里需要处理所有空的链接,因为空的链接上面还有防止链接存活时间过长的定时器
               确保剩下的都是正在处理的事情
             */
            clear_idle_connections();
            if(event_timer_rbtree.root == event_timer_rbtree.sentinel){
                // 如果红黑树里面已经没有任何监听的事件了,那么直接结束
                worker_process_exit(&server_cfg.workers);// 直接退出
            }
        }
        
        process_events_and_timer();//处理所有的事件
        /* 收到了SIGINT的信号 */
        if(p_terminate){
            worker_process_exit(&server_cfg.workers);//直接退出
        }
        
        /* 收到了SIGQUIT的信号,不会马上关闭所有的链接,但是需要设置一个timer,到时间了就强制关闭
         * 取消监听的链接,保证不会再接受新的信号
         */
        if (p_quit){
            plog("gracefully shutting down");
            p_quit = 0;
            if (!p_exiting){
                p_exiting = 1;
                clear_idle_connections();
                http_close_connection(listen_connection);// todo: 改成监听多个端口
            }
        }
    }
}


// 在worker启动的时候进行初始化
void worker_process_init(){
    
    struct rlimit nofile_limit = {65535, 65535};
    setrlimit(RLIMIT_NOFILE, &nofile_limit);//控制一个进程里面能够获得的最大打开文件描述符号

    event_module.process_init();
    upstream_module.process_init();
    // todo upstream_module.process_init();
    header_map_init();
    event_timer_init();
    plog("worker process init end");
}

void process_events_and_timer()
{    
    msec_t  timer, delta;
    int flags;
    
    if (server_cfg.timer_resolution) {
        timer = TIMER_INFINITE;
        flags = 0;
        
    } else {
        timer = event_find_timer();
        flags = UPDATE_TIME;
    }

    delta = current_msec;
    event_process(timer,flags);//传0代表设置了时间精度,依靠定时信号来设置
    delta = current_msec - delta;
    
    // 如果时间太短,就没有必要expire
    if (delta) {
        event_expire_timers();
    }
}

// 利用时间数组来更新缓存时间
// 由于这里暂时没有缓存完整的时间,而是仅仅缓存了一个current_msec,所以这个修改操作是原子的,读取也是原子的
// 同时没有考虑多线程,这里信号中断也不会导致中断函数里面改写时间
void time_update()
{
    struct timeval   tv;
    time_t sec;
    uint32_t msec;
    gettimeofday(&tv,NULL); //宏定义，获取时间
    
    sec = tv.tv_sec;
    msec = tv.tv_usec / 1000;   //从微秒usec中计算毫秒msec
    
    current_msec = (msec_t) sec * 1000 + msec;
}

/*
 接受一个connect请求
 */
void accept_connection(int socket){
    while (1) {
        int conn_fd = accept(socket,NULL,NULL);
        
        connection_t* c = getIdleConnection();
        c->fd = conn_fd;
        if(conn_fd == ERROR){
            ERR_ON((errno != EWOULDBLOCK), "accept");
            break;
        }
        else{
            c->is_connected = true;
            plog("new connection conn_fd %d\n", conn_fd);
        }
    }
}
