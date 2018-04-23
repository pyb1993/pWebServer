//
//  master.c
//  pWenServer
//
//  Created by pyb on 2018/4/23.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "worker.h"
#include <sys/time.h>

int process_status = MASTER;// worker/master/single


// 监视所有的子进程,如果有挂掉的,根据状态判断是否需要重启
// exited是0, live = 1
// exited = 1,exiting = 0,respawn = 1,就需要重新拉起,但是如果拉起失败,live还是不会变为1
int reap_children(vector* workers)
{
    int live = 0;
    for(int i = 0; i < workers->used; ++i)
    {
        worker_t* worker = vectorAt(workers, i);
        int pid = worker->pid;
        if(pid <= 0 ){continue;}
        
        if(worker->exited)
        {
            if(worker->respawn && !worker->exiting && !p_quit && !p_terminate)
            {
                //需要重新拉起来挂掉的进程
                //拉起失败
                continue;
                //拉起成功
                worker->pid = 1000;
                live = 1;
            }
        }
        else
        {
            //没有挂掉
            live = 1;
        }
    }
    return live;
}


// 用来处理退出子进程的status
// 这个函数会对process的exited和respawn做处理
void process_get_status(){
    int one = 0;
    while(1)
    {
        int status;
        int pid = waitpid(-1, &status, WNOHANG);//非阻塞等待任意一个子进程
        if (pid == 0) {
            return ;//没有收集到子进程退出了,那么直接退出
        }
        if (pid == -1)
        {
            if (errno == EINTR) {
                continue;//如果waitpid调用被其他系统调用打断了，那么继续调用
            }
            if (errno == ECHILD && one) {
                //调用进程没有子进程(这是因为waitpid设置的是-1,否则错误代表的意义是该pid不存在或不是子进程)
                return;
            }
            
            if(errno == ECHILD){
                // 调用进程没有子进程,同时一个都没有catch到(可能是同时引起了多个process_get_status调用)
                printf("error on waitpid, ECHILD");
                return;
            }
            printf("error on waitpid, ECHILD");
            return;
        }
        
        
        one = 1;
        for (int i = 0; i < server_cfg.workers.used; i++) {
            worker_t* worker = vectorAt(&server_cfg.workers, i);
            if (worker->pid == pid) {
                worker->exited = 1;
                break;
            }
        }
    }
}

void create_worker_process(){
    while (server_cfg.workers.used < server_cfg.worker_num)
    {
        int pid = fork();
        if(pid == 0){
            //进入worker的处理逻辑
            process_status = WORKER;
            return;
        }
        else{
            process_status = MASTER;
            printf("create worker %d \n",pid);
            worker_t *worker;
            worker = vectorPush(&(server_cfg.workers));
            worker->pid = pid;
            worker->exited = 0;
            worker->exiting = 0;
            worker->respawn = 0;
        }
    }
}


// the master will run the cycle
void master_cycle_process()
{
    ABORT_ON(process_status != MASTER, "this function can only run in master!!!");
    int live = 1;
    sigset_t set;
    sigemptyset(&set);
    struct itimerval itv;
    
    while(1)
    {
        sigsuspend(&set);
        if (delay)
        {
            if (p_sigalarm)
            {
                delay *= 2;
                p_sigalarm = 0;
            }
            
            /*
             *  struct itimerval {
             *      struct timeval it_interval;
             *      struct timeval it_value;
             *  };
             *  struct timeval {
             *      long tv_sec;
             *      long tv_usec;
             *  };
             *
             */
            itv.it_interval.tv_sec = 0;
            itv.it_interval.tv_usec = 0;
            itv.it_value.tv_sec = delay / 1000;
            itv.it_value.tv_usec = (delay % 1000 ) * 1000;
            
            /* 设置定时器
             * ITIMER_REAL: 以系统真实的时间来计算，它送出SIGALRM信号。
             * */
            if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
                printf("setitimer() failed");
            }
        }
        
        /* the sigsuspend will block all signal in the set and then wait, until a
         signal are captured(not blocked or ignored). the sigsuspend will
         call the relative handler, and return,resotre the signal set
         */
        if(p_reap){
            p_reap = 0;
            live = reap_children(&server_cfg.workers);
        }
        // 所有的子进程都exited了,同时master也收到了需要退出的信号
        if(!live && (p_quit || p_terminate))
        {
            master_process_exit(&server_cfg.workers);
        }
        
        
        if(p_terminate == 1){
            if(delay == 0){
                delay = 50;
            }
            if(delay >= 1000)
            {
                signal_worker_processes(&server_cfg.workers,SIGKILL);
            }
            signal_worker_processes(&server_cfg.workers,SIGINT);
            continue;
        }
        
        if(p_quit == 1)
        {
            signal_worker_processes(&server_cfg.workers,SIGQUIT);
            continue;
        }
    }
}

//给workers发送对应的信号
void signal_worker_process(vector* workers, int signo){
    for(int i = 0; i < workers->used; ++i)
    {
        worker_t* worker = vectorAt(workers,i);
        if(worker->pid <= 0){continue;}
        if(worker->exiting == 1 && signo == SIGQUIT){continue;}//优雅的退出,所以不要重复发送
        if (kill(worker->pid, signo) == -1) {
            plog("kill worker-%d failed,signo:%d",worker->pid,signo);
        }
        // warnning: when the restart signal, there need a if
        worker->exiting = 1;
    }
}

// 直接关闭所有的链接
void master_process_exit(vector* workers){
    //关闭所有的链接,但是现在没有链接
    plog("the master %d exit!",getpid());
    exit(0);
}

