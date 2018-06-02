//
//  upstream_server_module.h
//  pWebServer
//
//  Created by pyb on 2018/5/9.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef upstream_server_module_h
#define upstream_server_module_h

#include <stdio.h>
#include "module.h"
#include "server.h"
#include "http.h"

#define UPSTREAM_CONN_FAIL 0
#define UPSTREAM_CONN_SUC 1

#define try_connect_upstream(r,domain) (((upsream_server_module_t*)upstream_module.ctx)->get(r,domain))
#define free_upstream(us) (((upsream_server_module_t*)upstream_module.ctx)->free(us))

struct domain_upsream_server_arr_t;
extern struct _domain_upstream_server_arr servers;


typedef enum {
    IDLE,
    MODIFIED,
    INIT_DONE
} chash_state_type;

/*ip地址以及相应的参数(effective_weight,current_weight,fails,accessed等数据)
  这个结构可以支持两种类型的负载均衡.平滑加权轮训以及一致性hash
  对于第二种,需要依靠虚拟节点和二分查找进行选择,同时要依靠status_changed函数来判断这个节点是否已经down了
  对于一致性hash失败多次的情况,需要退化到平滑加权轮训来实现(todo)
 */
typedef struct upstream_server{
    int weight;//设置的weight
    int effective_weight;//基础权重,对应当前服务器的运行状况
    int current_weight;//当前实际的权重
    bool status_changed;//用来在一致性hash的时候进行判断是否需要重新初始化
    int fails;//失败的次数
    int max_fails;//最大可以尝试的失败次数
    int state;//链接后端服务器的情况
    msec_t last_failed;//最后一次失败时间
    msec_t fail_timeout;
    location_t* location;
} upsream_server_t;

/*一致性hash所需要的虚拟节点结构体定义*/
typedef struct _consistent_hash_vnode{
    uint32_t hashcode;// 虚拟节点的值
    upsream_server_t* rnode;//虚拟节点对应的真实节点
}consistent_hash_vnode_t;

/*定义一致性hash需要的信息*/
typedef struct consistent_hash_data{
    consistent_hash_vnode_t* cycle;
    int vnodes;//虚拟节点的数量
    
} consistent_hash_t;

/*这个结构是某一个域名负载均衡下的所有server*/
typedef struct upstream_server_arr{
    // 该域名下对应的ip数组
    string* domain_name;
    upsream_server_t* upstream_server;
    int nelts;
    uintptr_t data;// 用来做小数量情况下的位图的存储空间
    uintptr_t* tried;// 指向位图的存储空间
    chash_state_type init_state;//用来标识一致性hash的情况的初始化情况
    consistent_hash_t* cycle;//存放一致性hash的数据
} upsream_server_arr_t;

/*该结构是记录负载均衡的所有域名对应的upstream_server_ip_list*/
typedef struct _domain_upstream_server_arr{
    upsream_server_arr_t* server_ip_arr;
    int nelts;//一共有多少个域名需要做负载均衡
}domain_upstream_server_arr_t;

// upsream_server_module里面的ctx类型,用来指向各种各样的负载均衡类型
typedef struct _upstream_module_server_ctx{
    int (*get) (http_request_t*r,string* domain);//根据不同的负载均衡算法获取一个数据
    void (*free) (upsream_server_t*);// 在链接结束以后,要释放对应的server数据,以及做一些权重调整
} upsream_server_module_t;


/*  在本次加权轮训之前挑选之前进行初始化 */
int init_before_round(http_request_t* r, string* target_domain);

/* 按照加权轮训算法来进行挑选 */
void get_server_by_round(http_request_t*r,string* domain);

void free_server_after_round_select(upsream_server_t* us);

// 按照一致性hash算法来获取
void get_server_by_consistent_hash(http_request_t*r,string* domain);

/*upstream 加权轮询模块通用context*/
extern upsream_server_module_t upstream_server_round_module_ctx;

/*upstream 一致性hash模块通用context*/
extern upsream_server_module_t upstream_server_chash_module_ctx;

/*** 设置upstream对应的回调函数 ***/
void init_upstream_connection(http_request_t* r,connection_t * upstream,int fd);
#endif /* upstream_server_module_h */
