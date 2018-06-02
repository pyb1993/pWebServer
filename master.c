//
//  master.c
//  pWenServer
//
//  Created by pyb on 2018/4/23.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "worker.h"
#include "server.h"
#include "commonUtil.h"
#include <sys/time.h>

int process_status = MASTER;// worker/master/single

/*创建一个新的子进程*/
int spawn_process(vector* workers){
    int pid = fork();
    if(pid > 0){
        // master
        return pid;
    }else{
        // worker
        worker_cycle_process();
        exit(0);
        return -1;// make the compiler happy!!!!
    }
}


/* 监视所有的子进程,如果有挂掉的,根据状态判断是否需要重启
 * 已经获得了是哪些进程 exited的了
 * exited = 1(已经退出了), exiting = 0(不是我们主动去退出), respawn = 1(设置为需要重拉起,且不是因为致命错误退出)
 * 就需要重新拉起,需要处理成功或者失败两种情况
 */
int reap_children(vector* workers)
{
    plog("try to repeat the worker");
    int live = 0;
    for(int i = 0; i < workers->capacity; ++i){
        worker_t* worker = vectorAt(workers, i);
        int pid = worker->pid;
        if(pid <= 0 ){ continue; }
        if(worker->exited){
            plog("worker(%d) exited",pid);
            if(worker->respawn && !worker->exiting && !p_quit && !p_terminate){
                /* 需要重新拉起来挂掉的进程 */
                int new_pid = spawn_process(workers);
                if(new_pid <= 0){
                    plog_debug("ERROR: respawn failed!!!!!!!!!!!!");
                    worker->pid = -1;
                    continue;
                }
                plog_debug("respawn successfully\n");
                // 拉起成功,重置状态
                worker->pid = new_pid;
                worker->exited = 0;
                worker->exiting = 0;
                live = 1;
            }
        }else{
            //没有挂掉
            live = 1;
        }
    }
    return live;
}


/* 用来处理退出子进程的status
 * 这个函数会对process的exited和respawn做处理,这些状态在reap_children里面使用
 */
void process_get_status(){
    int one = 0;
    int i;
    while(1){
        int status;
        int pid = waitpid(-1, &status, WNOHANG);//非阻塞等待任意一个子进程
        if (pid == 0) {
            return ;//没有收集到子进程退出了,那么直接退出
        }
        if (pid == -1){
            if (errno == EINTR) {
                continue;//如果waitpid调用被其他系统调用打断了，那么继续调用
            }
            if (errno == ECHILD && one) {
                // 调用进程没有子进程(这是因为waitpid设置的是-1,否则错误代表的意义是该pid不存在或不是子进程)
                // 但是已经捕捉到一个退出的子进程了
                return;
            }
            
            if(errno == ECHILD){
                // 调用进程没有子进程,同时一个都没有catch到(可能是同时引起了多个process_get_status调用)
                plog("error on waitpid, ECHILD");
                return;
            }
            plog("error on waitpid, ECHILD");
            return;
        }
        
        one = 1;
        for (i = 0; i < server_cfg.workers.used; i++) {
            worker_t* worker = vectorAt(&server_cfg.workers, i);
            if (worker->pid == pid) {
                worker->exited = 1;
                /* 将2作为退出的返回值是一种约定,表示出现了致命bug */
                if (WEXITSTATUS(status) == 2 && worker->respawn){
                    plog_debug("%d exited with fatal code %d "
                                  "and cannot be respawned",
                                  pid, WEXITSTATUS(status));
                    worker->respawn = 0;
                }
                break;
            }
        }
    }
}

/* 创建所有的worker,在开始的地方设置respawn属性,代表需要被重启
 * 这个属性在process_get_status的时候,可能会根据是否是正常退出来决定
 * 是不是需要重启
 */
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
            plog("create worker %d \n",pid);
            worker_t *worker;
            worker = vectorPush(&(server_cfg.workers));
            worker->pid = pid;
            worker->exited = 0;
            worker->exiting = 0;
            worker->respawn = 1;//这个进程需要被重启
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
        if (delay){
            /* 说明现在正在处于一个延迟状态
               如果是被延迟信号打断的话,则代表已经等待了delay时间了将下一次等待了时间乘以2
               否则代表是第一次,直接设置信号就好
             */
            if (p_sigalarm){
                delay *= 2;
                p_sigalarm = 0;
            }
            
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
        
        sigsuspend(&set);// 在这里,由于set为空,任何信号都会被打断
        /* the sigsuspend will block all signal in the set and then wait, until a
         signal are captured(not blocked or ignored). the sigsuspend will
         call the relative handler, and return,resotre the signal set
         */
        
        /*有子进程退出了,需要拉起*/
        if(p_reap){
            p_reap = 0;
            live = reap_children(&server_cfg.workers);
        }
        /* 所有的子进程都真正退出了,同时master也收到quit或者exit两个信号之一
         * 那么master也选择退出
         */
        if(!live && (p_quit || p_terminate)){
            master_process_exit(&server_cfg.workers);
        }
        
        /* 收到p_terminate信号,要求强制退出
         * 设置延迟时间,如果超过1s都没有退出,那么强行退出
         */
        if(p_terminate == 1){
            if(delay == 0){
                delay = 50;
            }
            if(delay >= 1000){
                signal_worker_processes(&server_cfg.workers,SIGKILL);
            }
            signal_worker_processes(&server_cfg.workers,SIGINT);
            continue;
        }
        
        /* 收到p_quit信号,这个是要求优雅关闭 */
        if(p_quit == 1){
            close(listen_fd);// 关闭套接字(master也有一个套接字)
            plog("send sigquit to workers");
            signal_worker_processes(&server_cfg.workers,SIGQUIT);
            continue;
        }
    }
}

/* 给workers发送对应的信号
 * 同时对相应的标志进行设定 exiting = 1
 */
void signal_worker_process(vector* workers, int signo){
    for(int i = 0; i < workers->used; ++i)
    {
        worker_t* worker = vectorAt(workers,i);
        if(worker->pid <= 0){continue;}
        if(worker->exiting == 1 && signo == SIGQUIT){continue;}//正在退出,不用重复发送
        if (kill(worker->pid, signo) == -1) {
            plog("kill worker-%d failed,signo:%d",worker->pid,signo);
        }
        // warnning: when the restart signal, there need a if
        worker->exiting = 1;
    }
}

// 直接关闭所有的链接
void master_process_exit(vector* workers){
    plog("the master %d exit!",getpid());
    exit(0);
}

