//
//  worker.h
//  pWenServer
//
//  Created by pyb on 2018/4/23.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef worker_h
#define worker_h

#include <stdio.h>
#include "vector.h"
#include "errno.h"
#include "config.h"
#include "module.h"


extern int p_reap;
extern int p_quit;
extern int p_exit;
extern int p_terminate;
extern int p_exiting;
extern int p_sigalarm;
extern int process_status;// worker/master/single
extern int current_msec;//缓存了当前的时间,单位是ms
extern int p_event_timer_alarm;//设定时间缓存的精度
extern long delay;

typedef struct worker_t{
    int pid;
    unsigned            respawn:1; //是否需要被重新拉起
    unsigned            just_spawn:1; //是否是刚被拉起的进程
    unsigned            exiting:1;//父进程向unix socket发送信息给子进程后将这个字段设为1
    unsigned            exited:1;//在ngx_process_get_status发现waitpid触发的pid和这个对象的pid相同时，设为1
} worker_t;


void process_events_and_timer();
void signal_worker_processes(vector* workers,int signo);
void process_get_status();
void create_worker_process();
void master_process_exit(vector* workers);
void worker_process_exit(vector* workers);
void worker_cycle_process();
void worker_process_init();
void update_time();
#endif /* worker_h */
