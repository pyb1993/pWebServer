//
//  main.c
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "unitTest.h"
#include "globals.h"
#include "server.h"
#include "commonUtil.h"
#include "module.h"
#include "header.h"
#include "worker.h"
#include "event.h"


int listen_fd;
connection_t* listen_connection;
event_t* r_events;
event_t* w_events;

signal_t  signals[] = {
    { SIGINT,"SIGINT",singal_handler},
    { SIGQUIT,"SIGQUIT",singal_handler},
    { SIGCHLD,"SIGCHLD",singal_handler},
    { SIGALRM,"SIGALRM",singal_handler},
    { SIGSEGV,"SIGSEGV",singal_handler},
    { SIGPIPE,"SIGPIPE",SIG_IGN},
    {0,"",NULL}
};


int set_nonblocking(int sockfd){
    int flags = fcntl(sockfd, F_GETFL, 0);         //获取文件的flags值。
    if(flags == -1) assert(!"ERROR ON FCTNL");
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);   //设置成非阻塞模式；
    return OK;
}

// 获取一个listen socekt
int startUp(int port)
{
    listen_fd = 0;
    struct sockaddr_in server_addr = {0};
    int addr_len = sizeof(server_addr);
    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd == ERROR) {
        return ERROR;
    }
    
    // 
    int on = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));// if not, restart the server may cause error
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

    memset((void*)&server_addr, 0, addr_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd, (struct sockaddr*)&server_addr, addr_len) < 0) {
        return ERROR;
    }
    
    // todo 主意listen的参数
    if (listen(listen_fd, 65536) < 0) {
        return ERROR;
    }
    
    return listen_fd;
}



// 这个函数处理master进程里面的信号对象
void singal_handler(int signal)
{
    if(process_status == MASTER){
        switch (signal) {
            case SIGINT:
                plog("master: exit!");
                p_terminate = 1;
                break;
            case SIGQUIT:
                plog("master: shutting down!");
                p_quit = 1;
                break;
            case SIGCHLD:
                plog("master: child exited!");
                p_reap = 1;
                break;
            case SIGALRM:
                p_sigalarm = 1;
                break;
            case SIGSEGV:
                plog("master: segment fault :pid[%d]",getpid());
                p_terminate = 1;
                break;
            default:
                break;
        }
    }
    else{
        switch (signal)
        {
            case SIGINT:
                plog("worker: exit!");
                p_terminate = 1;
                break;
            case SIGQUIT:
                plog("worker: shutting dow!");
                p_quit = 1;
                break;
            case SIGSEGV:
                plog("worker: segment fault :pid[%d]",getpid());
                exit(SIGSEGV);
                break;
            default:
                break;
        }
    }
    
    //需要获取子进程的
    if (signal == SIGCHLD) {
        process_get_status();
    }
}

void signal_init()
{
    signal_t      *sig;
    struct sigaction   sa;
    
    for (sig = signals; sig->signo != 0; sig++) {
        memzero(&sa, sizeof(struct sigaction));
        sa.sa_handler = sig->handler;
        sigemptyset(&sa.sa_mask);//除了信号本身,在信号处理函数中不屏蔽任何其他信号
        if(sigaction(sig->signo, &sa, NULL) == -1){
            plog("set signal handler failed!");
        }
    }
}

/*各个模块实现自己的module_init函数*/
void module_init(){
    event_module.module_init();
    upstream_module.module_init();
}

int main(int argc, const char * argv[])
{
    config_load();
    listen_fd = startUp(server_cfg.port);
    set_nonblocking(listen_fd);

    if(server_cfg.daemon){
       daemon(1,0);
    }
    
    signal_init();
    module_init();
    plog("master %d run\n",getpid());
    create_worker_process();
    
    if(process_status == MASTER){
        master_cycle_process();
    }else{
        worker_cycle_process();
        plog("worker %d listen to %d run",getpid(),listen_fd);
    }
    return 0;
}



