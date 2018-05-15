//
//  upstream_server_module.c
//  pWebServer
//
//  Created by pyb on 2018/5/9.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "http.h"
#include "upstream_server_module.h"
#include "commonUtil.h"
#include "connection.h"
#include <arpa/inet.h>

#define is_server_down(s) (s->max_fails\
&& s->fails >= s->max_fails\
&& (int)(now - s->last_failed) <= (int)s->fail_timeout)

#define in_fail_time_duration(s) ((int)(now - s->last_failed) <= (int)s->fail_timeout)

domain_upstream_server_arr_t servers;

/*upstream 加权轮询模块通用context*/
upsream_server_module_t upstream_server_round_module_ctx = {
    get_server_by_round,
    free_server_after_round_select
};

/*upstream 一致性hash模块通用context*/
upsream_server_module_t upstream_server_chash_module_ctx = {
    get_server_by_consistent_hash,
    free_server_after_round_select
};

/*  在本次加权轮询之前挑选之前进行初始化
    创造一个位图bit_map:用来确定本轮挑选是否被访问过
    可能需要分配对应的内存空间
 */
int init_before_round(http_request_t* r, string* target_domain){
    upsream_server_arr_t* servers_of_domain = servers.server_ip_arr;
    int n = servers.nelts;
    // 找到对应的域名
    if(r->cur_server_domain == NULL){
        int i = 0;
        for(i = 0; i < n; ++i){
            if( stringEq(target_domain, servers_of_domain->domain_name)){
                break;
            }
            servers_of_domain++;
        }
        r->cur_server_domain = servers_of_domain;
    }

    //首先分配位图的空间
    int ip_num = servers_of_domain->nelts;
    if(ip_num <= 8 * sizeof(uintptr_t)){
        servers_of_domain->tried = &servers_of_domain->data;
        servers_of_domain->data = 0;
    }else{
        int n = (ip_num + (8 * sizeof(uintptr_t) - 1)) / (8 * sizeof(uintptr_t));//向上取整,分配的基本单位是8 * 4 = 32byte
        servers_of_domain->tried = (uintptr_t*)pMalloc(r->pool, n * sizeof(uintptr_t));
        if (servers_of_domain->tried == NULL) {
            return ERROR;
        }
    }
    
    // 然后初始化这些数组
    msec_t now = current_msec;
    upsream_server_t* server = servers_of_domain->upstream_server;
    for(int j = 0; j < ip_num;++j){
        // 检查有没有状态的变化,从fail状态超时,那么状态发生变化
        if(server->fails >= server->max_fails && !in_fail_time_duration(server)){
            servers_of_domain->init_state = MODIFIED;
            server->fails = 0;//清除失败的次数
        }
        server->state = UPSTREAM_CONN_SUC;
        server++;
    }
    return OK;
}

/*
 在挑选一次之后,需要尝试链接该服务器
 */
int try_connect(http_request_t *r,upsream_server_t* us){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1){
        plog("err on get a socket ,errno: %d",errno);
        return ERROR;
    }
    
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(us->location->port);
    
    int success = inet_pton(AF_INET, us->location->host.c, &addr.sin_addr);
    if (success <= 0) {
        plog("err pm omet_pton,errno:%d",errno);
        return ERROR;
    }
    
    // 尝试connect,需要处理三种情况
    /*  case1.最可能的情况是直接成功
        case2.返回EINPROGRESS错误,那么需要设置定时器检查是否超时
        case3.被信号打断了系统调用,忽略之后继续connect
        case4.其他错误
     */
    while(1){
        int err = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
        if(err == OK){
            // case1 在这里输出对应的port
            struct sockaddr_in local_address;
            int addr_size = sizeof(local_address);
            getsockname(fd, &local_address, &addr_size);
            
            plog("upstream server => backend  => port : %d => %d => %d",r->connection->fd,fd,ntohs(local_address.sin_port));
            r->cur_upstream = us;
            init_upstream_connection(r,r->upstream,fd);
            return OK;
        }
        if(errno == EINPROGRESS || errno == EAGAIN){
            // case 2
            r->upstream->wev->handler = process_connection_result_of_upstream;
            event_add_timer(r->upstream->wev, server_cfg.post_accept_timeout);
            return OK;
        }else if(errno == EINTR){
            // case 3
            
        }else{
            // case 4
            plog("server(%d) connect to upstream(%s:%d) failed,errno:%d",
                 r->connection->fd,
                 us->location->host.c,
                 us->location->port,errno);
            return ERROR;
        }
    }
    return ERROR;
}


/*  按照加权轮训算法来进行挑选一个服务器,一次挑选,如果本次失败,那么会记录在位图里面
 */
upsream_server_t* get_server_by_round_once(upsream_server_arr_t* server_domain){
    int n = 0;
    int i = 0;
    int total = 0;
    msec_t now = current_msec;
    uintptr_t m = 0;
    uintptr_t* tried = server_domain->tried;
    
    upsream_server_t* best = NULL;
    upsream_server_t* server;
    for(i = 0; i < server_domain->nelts; ++i){
        /* 计算当前后端服务器在位图中的位置 n,以及n那个位置的m */
        
        n = i / (8 * sizeof(uintptr_t));
        m = (uintptr_t) (1 << i % (8 * sizeof(uintptr_t)));
        
        /* 当前后端服务器在位图中已经有记录，则不再次被选择，即 continue 检查下一个后端服务器 */
        if (tried[n] & m) {
            continue;
        }
        
        /*下面检查这个server的情况,假如设定了最大失败次数,且失败的次数超过了它
          且距离检查的时间还没有吵过等待时间,那么就直接忽略这个服务器
         */
        server = server_domain->upstream_server + i;
        if (is_server_down(server)){
            continue;
        }
        
        // 接下来就是核心算法,平滑加权轮询问
        server->current_weight += server->effective_weight;
        total += server->effective_weight;
        
        /* 服务器正常，如果effective_weight比较小(说明中间失败过),那么现在慢慢 effective_weight 的值,相当于缓慢恢复 */
        if (server->effective_weight < server->weight) {
            server->effective_weight++;
        }
        
        /* 选择current_weight最大的作为best */
        if (best == NULL || server->current_weight > best->current_weight) {
            best = server;
        }
    }
    
    if (best == NULL) {
        return NULL;
    }
    
    i = (int)(best - server_domain->upstream_server);//计算best是第几个
    
    /* 在位图相应的位置记录被选中后端服务器 */
    tried[n] |= m;
    
    /* 更新被选中后端服务器的权重,理解这里为什么要这样做,这样可以根据effective_weight的大小,决定大概多少次就能再次被选中 */
    best->current_weight -= total;
    
    return best;
};

/*需要在链接完成之后,释放整个服务器*/
void free_server_after_round_select(upsream_server_t* us){
    msec_t now = current_msec;
    /*
     * 若在本轮被选中的后端服务器在连接时失败
     * 则需要进行重新选择后端服务器
     */
    
    if (us->state == UPSTREAM_CONN_FAIL) {
    
        us->fails++;/* 增加当前后端服务器失败的次数 */
        
        /* 设置当前后端服务器访问的时间,其实就是上一次失败的时间 */
        us->last_failed = now;
        
        if (us->max_fails) {
            /* 由于当前后端服务器失败，表示发生异常，此时降低 effective_weight 的值 */
            us->effective_weight -= us->weight / us->max_fails;
        }
        
        /* 保证 effective_weight 的值不能小于 0 */
        if (us->effective_weight < 0) {
            us->effective_weight = 0;
        }
    }else{
        /* 若被选中的后端服务器成功处理请求,并且已经超过fail timeout了，则将其 fails 设置为 0
           这里需要理解的是,在fail_timeout时间之内,是不会清空fails的
         */
        if (!in_fail_time_duration(us)) {
            us->fails = 0;
        }
    }
}

/*
 按照加权轮询算法来挑选一个服务器,并且测试该链接是否可用
 如果测试成功,那么直接把对应的fd返回给前面
 如果测试失败,那么继续挑选
 */
void get_server_by_round(http_request_t*r,string* domain){
    int err = init_before_round(r, domain);//
    if(err != OK){
        plog("err on init load balance");
        return ;
    }
    
    // 尝试找到一个后端服务器,最多不会尝试超过服务器的总次数
    upsream_server_arr_t* server_domain = r->cur_server_domain;
    while(r->upstream_tries < server_domain->nelts){
        upsream_server_t* us = get_server_by_round_once(server_domain);
        r->upstream_tries++;

        if(us == NULL){
            continue;
        }
        
        /* 找到一个目前处于可用状态的后端服务,尝试connect,并且设置相应回调函数
           结果有两种情况
           case 1: OK,说明connect已经发送出去了/直接成功.
           case 2: ERROR,某个系统调用或者connect出现错误,继续尝试其他服务
         */
        int err = try_connect(r,us);
        if(err == OK){
            break;
        }else{
            us->state = UPSTREAM_CONN_FAIL;
            free_server_after_round_select(us);
        }
    }
    
    // worst case,所有服务都挂了
    if(r->upstream_tries >= server_domain->nelts){
        if(r->upstream->wev->timer_set){
            event_del_timer(r->upstream->wev);
        }
        construct_err(r, r->connection, 502);
    }
}

/*----------------- 一致性hash的分界线 -------------------------------*/

/*利用一致性hash来实现
  1 首先根据权重初始化虚拟节点,计算总权重 w = w1 + w2 + w3
  2 然后计算真实节点的数量是 num
    虚拟节点的总数定位 vn = num * 8
  3 接下来 计算每一个真实节点对应的虚拟节点的数量
     假设权重是 wi,那么 vni = ceil(vn * wi / w)
     这样最后的vni的和可能大于n,但是没有关系,以sum(vni)为准
  4 在 3 的过程中,计算每一个虚拟节点的hashcode值,
    计算的方式是: 用当前节点的ip进行hash,然后乘以一个数i
    然后分别构造出虚拟节点的数值,为 0,1,2...vni
    这里的一个问题是可能造成数据分布不均匀(类似的ip得到的hashcode接近),所以还要再进行一次hash
    得到的字符串是ip/random/0,ip/random/1这样进行hash
    将结果排入数组,然后将数组排序
 
  --------以上是初始化过程---------------
  接下来是二分查找过程
  如果节点的情况没有发生变化,那么每次走只需进行二分查找,就可以得到对应的虚拟节点
  如果节点的情况已经发生了变化,比如某个节点挂掉的次数已经超过了max_fail + 2次,那么就认为
  该节点已经down了,这种情况下,需要我们重新对节点进行初始化
 */

    /*用于上述算法步骤4里面的排序比较函数*/
int vnode_cmp ( const void *a , const void *b ){
    consistent_hash_vnode_t* va = (consistent_hash_vnode_t*)a;
    consistent_hash_vnode_t* vb = (consistent_hash_vnode_t*)b;
    return va->hashcode > vb->hashcode ? 1 : -1;
}

int init_before_consistent_hash_get(http_request_t* r, string* target_domain){
    int ret = init_before_round(r, target_domain);
    msec_t now = current_msec;// 注意这里要缓存,否则会因为中间被update_time的信号打断导致cuurent_msec不一致
    ERR_ON(ret == ERROR,"???" );
    
    /*接下来需要进行一致性hash相关的初始化,这里不需要每次都初始化,只有节点状态发生变化的时候才执行*/
    upsream_server_arr_t* server_of_domain = r->cur_server_domain;
  
    /*这里需要检查,是否节点已超时*/

    if(server_of_domain->init_state == IDLE || server_of_domain->init_state == MODIFIED){
        plog("consistent hash:the init state changed,reinit");
        /*开始按照上面的算法进行初始化*/
        int total_w = 0;
        int total_vnum = 0;//虚拟节点的个数
        int temp_vnum = 0;//估算的虚拟节点的个数
        
        /****** 计算整体权重 ****/
        for(int i = 0; i < server_of_domain->nelts; ++i){
            upsream_server_t * server = server_of_domain->upstream_server + i;
            if(!is_server_down(server)){
                total_w += server->weight;//这里只使用最基本的weight
                temp_vnum += 8;
            }
        }
        
        /***** 计算真实的虚拟节点的数量 *****/
        for(int i = 0; i < server_of_domain->nelts; ++i){
            upsream_server_t * server = server_of_domain->upstream_server + i;
            if(!is_server_down(server)){
                total_vnum += p_ceil((temp_vnum * server->weight + 0.0) / total_w);
            }
        }

        /**** 分配内存 ***/
        if(server_of_domain->init_state == IDLE){
            server_of_domain->cycle = (consistent_hash_t*)pMalloc(r->pool, sizeof(consistent_hash_t));
            server_of_domain->cycle->cycle = (consistent_hash_vnode_t*)pMalloc(r->pool, total_vnum * sizeof(consistent_hash_vnode_t));
            ABORT_ON(server_of_domain->cycle->cycle == NULL, "memory not enough");
        }
        server_of_domain->cycle->vnodes = total_vnum;

        /****** 计算hashcode并且放入cycle *****/
        consistent_hash_vnode_t* cycle = server_of_domain->cycle->cycle;
        for(int i = 0; i < server_of_domain->nelts; ++i){
            upsream_server_t * server = server_of_domain->upstream_server + i;
            if(is_server_down(server)){
                continue;
            }
            
            // 该server的权重下面有多少的虚拟节点
            int vnodes_of_server = p_ceil((temp_vnum * server->weight + 0.0) / total_w);
            for(int j = 0; j < vnodes_of_server; ++j){
                string* host = &server->location->host;
                uint32_t key = hash_key_function(host->c, host->len) + i;
                // todo fix me: 检查key冲突
                cycle->rnode = server;
                cycle->hashcode = hash_code_of_ip_integer(key);// 再hash
                cycle++;
            }
        }
        
        //接下来进行排序
        qsort(server_of_domain->cycle->cycle, total_vnum, sizeof(consistent_hash_vnode_t),vnode_cmp);
        server_of_domain->init_state = INIT_DONE;//标识初始化已经完成了
    }
    return OK;
}



/*一次二分查找的过程
  找到在环上第一个大于hashcode的元素
  target用来保存查询过程中比hashcode大的元素
  容易证明最后的target一定是第一个大于hashcode的
 */
upsream_server_t* get_server_by_consistent_hash_once(upsream_server_arr_t* server_domain,uint32_t hashcode){
    consistent_hash_t* cycle = server_domain->cycle;
    consistent_hash_vnode_t* vec =  (consistent_hash_vnode_t*)cycle->cycle;
    msec_t now = current_msec;
    int left = 0;
    int right = cycle->vnodes;
    upsream_server_t* target = NULL;
    while(right > left){
        int mid = (left + right) >> 1;
        consistent_hash_vnode_t* ch = vec + mid;
        if(ch->hashcode < hashcode){
            left = mid + 1;
        }else if(ch->hashcode > hashcode){
            right = mid;
            target = ch->rnode;
        }else{
            target = ch->rnode;
            break;
        }
    }
    
    // 没有找到比它大的,那么直接赋值第一个
    if(target == NULL){
        target = ((consistent_hash_vnode_t*)cycle->cycle)->rnode;
    }
    
    // 如果该节点曾经失败过max_fails + 2(因为这里只要选择round就会违背一致性hash的规则,所以多尝试2次)
    if (is_server_down(target))
    {
        return NULL;
    }
    
    return target;
}

/*
 初始化之后,按照二分查找获取对应的server
 */
void get_server_by_consistent_hash(http_request_t* r,string* domain){
    /*考虑到当前函数可能会在回调函数 process_connection_result_of_upstream里面再次被调用
      所以可以避免不必要的初始化(再次调用说明consistent_hash对应的节点一定失败了),直接跳转到failed部分
     */
    if(r->upstream_tries > 0){
        goto failed;
    }
    r->upstream_tries++;
  
    int err = init_before_consistent_hash_get(r, domain);
    uint32_t hashcode = hash_code_of_ip_integer(r->connection->ip);
    upsream_server_arr_t* server_domain = r->cur_server_domain;
    if(err != OK){
        plog("err on init load balance");
        construct_err(r, r->connection, 500);
        return;
    }

    /* 由于这里是一致性hash,所以每次获得的节点都是一样的,如果链接不上,那么立刻切换成轮询的算法*/
    upsream_server_t* us = get_server_by_consistent_hash_once(server_domain,hashcode);
    
    /* 这种情况是因为对应的node已经暂时挂了,所以没有能找到对应的节点.
       但是不需要改变init_state,因为节点本来就是fail的
     */
    if(us == NULL){
        plog("consistent hash: failed to find a available server");
        goto failed;
    }
    
    err = try_connect(r,us);
    /* 两种情况,
        case1 如果获取us成功,那么可能已经设置了回调,或者直接连接上了.
        在回调的函数里面,判断成功或者失败的情况
     
        case2 如果失败,那么需要换到round的算法,同时再次尝试
        同时注意到,由于有节点从available到fail的状态,需要改变init_state
     */
    if(err == OK){
        return;
    }else{
        server_domain->init_state = MODIFIED;
    }

   
    // 对us的链接失败,更新对应的状态
    plog("consistent hash: failed to connect backend server %s:%d,errno:%d",us->location->host.c,us->location->port,errno);
    us->state = UPSTREAM_CONN_FAIL;
    free_server_after_round_select(us);

    /*  测试失败,那么需要换成轮询算法
        同时需要先设置初始化状态,
    */
failed:;
    r->cur_upstream = NULL;
    get_server_by_round(r, domain);
    return;
}

/*初始化upstream connection的各个属性*/
void init_upstream_connection(http_request_t* r,connection_t * upstream,int fd){
    event_t *wev = upstream->wev;
    event_t *rev = upstream->rev;
    //wev->timedout = 0;
    upstream->side = C_UPSTREAM;
    wev->handler = http_proxy_pass;//将 recv buffer里面的数据 转发 到backend
    upstream->rev->handler = http_recv_upstream;//第一个步骤是分配相关的buffer
    upstream->fd = fd;
    upstream->data = r;
    set_nonblocking(upstream->fd);
    add_event(rev,READ_EVENT,0);//监听读事件,这个事件应该一直开启
}
