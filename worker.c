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
#include <sys/time.h>
int p_reap = 0;
int p_exit = 0;
int p_terminate = 0;
int p_exiting = 0;
int p_sigalarm = 0;
int p_quit = 0;
long delay = 0;
int p_event_timer_alarm = 0;//是否收到相应的信号


//向所有的worker发送信号
void signal_worker_processes(vector* workers,int signo){
    for(int i = 0; i < workers->used; ++i){
        worker_t* worker = vectorAt(workers,i);
        if (worker->pid == -1) {
            continue;
        }
        // worker正在退出,那么就不需要再次发送quit信号(gracefully shutdown)
        if (worker->exiting && signo == SIGQUIT)
        {
            continue;
        }
        
        if (kill(worker->pid, signo) == -1) {
            
            plog("kill(%d, %d) failed", worker->pid, signo);
            
            //没有找到这样的process group或者process,那么需要
            if (errno == ESRCH) {
                worker->exited = 1;
                worker->exiting = 0;
                p_reap = 1;//用来关闭对应channel(暂时没有实现)
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
    while(1)
    {
        if(p_exiting)
        {
            // 当前worker正在退出,并且不是那种直接退出的逻辑
            // 如果发现那种idle的链接,就需要清理并且expired
            
            worker_process_exit(&server_cfg.workers);//直接退出
            // 正常情况下需要判断是否还有事件(链接)需要处理
        }
        
        process_events_and_timer();//处理各种链接的处理
        
        if(p_terminate)
        {
            worker_process_exit(&server_cfg.workers);//直接退出
        }
        
        if (p_quit)
        {
            p_quit = 0;
            plog("gracefully shutting down");
            //ngx_setproctitle("worker process is shutting down");
            
            if (!p_exiting)
            {
                p_exiting = 1;
                //ngx_set_shutdown_timer(cycle);
                //ngx_close_listening_sockets(cycle);
                //ngx_close_idle_connections(cycle);
            }
        }
    }
}


// 在worker启动的时候进行初始化
void worker_process_init(){
    event_module.process_init();
    header_map_init();
    plog("worker process init end");
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
    plog("update timer");
}

