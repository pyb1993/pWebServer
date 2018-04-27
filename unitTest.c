//
//  unitTest.c
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "unitTest.h"
#include "testBase.h"
#include "vector.h"
#include "pool.h"
#include "commonUtil.h"
#include "memory_pool.h"
#include "hash.h"
#include "string_t.h"
#include "http.h"
#include "header.h"
#include "rb_tree.h"
#include "timer_event.h"
void testVector()
{
    vector v;
    vector* vec = &v;
    vectorInit(vec,3,sizeof(int));
    pushIntToVec(vec, 10);
    pushIntToVec(vec, -10);
    pushIntToVec(vec, 100);
    pushIntToVec(vec, 101);
    pushIntToVec(vec, 102);

    test_int(10, IntegerFromVec(vec,0));
    test_int(-10, IntegerFromVec(vec,1));
    test_int(100, IntegerFromVec(vec,2));
    test_int(101, IntegerFromVec(vec,3));
    test_int(102, IntegerFromVec(vec,4));
    
    vectorPop(vec);
    test_int(101, *(int*)vectorBack(vec));
    vectorPop(vec);
    test_int(100, *(int*)vectorBack(vec));
    vectorPop(vec);
    test_int(-10, *(int*)vectorBack(vec));
    vectorPop(vec);
    test_int(10, *(int*)vectorBack(vec));
}


void testChunk(){
    chunk c;
    chunk* cp = &c;
    chunkInit(cp,8,20);
    chunk_slot* slot =(chunk_slot*)(cp->data);
    chunk_slot* slot1 = slot->next;
    chunk_slot* slot2 = slot1->next;
    chunk_slot* slot3 = slot2->next;

    setChunkUnitInt(cp,8,0,1);
    setChunkUnitInt(cp,8,1,2);
    setChunkUnitInt(cp,8,2,3);
    setChunkUnitInt(cp,8,3,4);
    setChunkUnitFlt(cp,8,4,0.2);
    setChunkUnitFlt(cp,8,5,1.3);
    
    test_int(1,getChunkUnitInt(cp,8,0));
    test_int(2,getChunkUnitInt(cp,8,1));
    test_int(3,getChunkUnitInt(cp,8,2));
    test_int(4,getChunkUnitInt(cp,8,3));
    test_flt(0.2,getChunkUnitFlt(cp,8,4));
    test_flt(1.3,getChunkUnitFlt(cp,8,5));
    
    test_int(1,*(int *)(slot));
    test_int(2,*(int *)(slot1));
    test_int(3,*(int *)(slot2));
    test_int(4,*(int *)(slot3));
}

void testPool(){
    pool  pool_instance;
    pool* p = &pool_instance;
    
    int width = sizeof(int);
    poolInit(p,width,8,0);
    
    void * prev = poolAlloc(p);
    for(int i = 0;i < 7;++i){
        void * c = poolAlloc(p);// get a chunkUnit
        test_int(p->unit_size,(int)(c - prev));
        prev = c;
    }
    
    prev = poolAlloc(p);
    for(int i = 0;i < 7;++i){
        void * c = poolAlloc(p);// get a chunkUnit
        test_int(p->unit_size,(int)(c - prev));
        prev = c;
    }
}

void testMemoryPool(){
#define CONV(x,d) ((((x) + (d) - 1) / (d)) * (d))
    
    uint hsize = sizeof(Header);
    memory_pool p;
    pMemoryPoolInit(&p,2000);
    void * init = p.data;

    void * a = pMalloc(&p,100);
    void * b = pMalloc(&p,90);
    void * c = pMalloc(&p,120);

    test_int(hsize * 2, (int)((char*)a - (char*)init));
    test_int(CONV(100,hsize)+hsize, (int)((char *)b - (char *)a));
    test_int(CONV(90,hsize)+hsize, (int)((char *)c - (char *)b));
    test_int((int)NULL,(int)pMalloc(&p,1800));
    pFree(&p, c);
    pFree(&p, b);
    pFree(&p, a);
    test_int((int)(p.data+2),(int)pMalloc(&p,1800));

    a = pMalloc(&p,400);
    pFree(&p, a);
    test_int((int)(a),(int)pMalloc(&p,400));
}

void testHash()
{
    
    static string names[] = {STRING("www.baidu.com"),
        STRING("www.google.com.hk"),
        STRING("www.github.com")};
    static char* descs[] = {"baidu: 1","google: 2", "github: 3"};


  
    memory_pool pool;
    pMemoryPoolInit(&pool,1024 * 20);
    hash_key* elements;
   
    hash * h = (hash*) pMalloc(&pool, sizeof(hash));
    hash_initializer hash_init;
    
    hash_init.pool = &pool;
    hash_init.hash = h;
    hash_init.bucket_size = 64;
    hash_init.max_size = 1024;

    elements = (hash_key*)malloc(sizeof(hash_key) * 3);
    for(int i = 0; i < 3; i++) {
        hash_key * arr_node = elements + i;
        arr_node->key       = (names[i]);
        arr_node->key_hash  = hash_key_function((u_char*)arr_node->key.c, arr_node->key.len);
        arr_node->value     = (void*) descs[i];
        //printf("key: %s , key_hash: %u,value : %s\n", arr_node->key.c, arr_node->key_hash, arr_node->value);
    }
    
    /* hash初始化函数 */
    if (hashInit(&hash_init, elements, 3) != OK){
        assert(0);
    }
    
    /* 查找hash 元素 */
    for(int i = 0; i< 3;++i){
    int k  = hash_key_function(names[i].c, names[i].len);
    char* find = (char*) hash_find(h, names[i].c, names[i].len);
    test_int(0, strcmp(descs[i], find));
    }
}

void testParseOneCase(buffer_t *buf,char *s,int path_len,int host_len,
                          int scheme_len,int port_len,int query_len,int extension_len)
{
    int len = (int)strlen(s);
    strncpy(buf->data, s, len);
    http_request_t *r = create_http_request();

    buf->begin = buf->data;
    buf->end = buf->data + len;
    int ret = http_parse_request_line(r, buf);
    ABORT_ON(ret != OK, "test request line failed!!!");
    ABORT_ON(r->uri.abs_path.len != path_len, "parse abs path failed!!!");
    ABORT_ON(r->uri.host.len != host_len, "parse host failed!!!");
    ABORT_ON(r->uri.scheme.len != scheme_len, "parse scheme failed!!!");
    ABORT_ON(r->uri.port.len != port_len, "parse port failed!!!");
    ABORT_ON(r->uri.extension.len != extension_len, "parse extension failed!!!");
    ABORT_ON(r->uri.query.len != query_len, "parse query failed!!!");
    ABORT_ON(r->version.major != 1, "parse version major failed!!!");
    ABORT_ON(r->version.minor != 1, "parse version minor failed!!!");
    freePool(r->pool);//连request自己都被释放了

}

void testParseRequestLine(){
    memory_pool p;
    pMemoryPoolInit(&p,2000 * 10);
    buffer_t *buf = createBuffer(&p);
   
    // params path host scheme prot query extension
    testParseOneCase(buf,"GET http://host.fuck.com/path HTTP/1.1\r\n",4,13,4,0,0,0);
    testParseOneCase(buf,"GET /path/subpath/index HTTP/1.1 \r\n",18,0,0,0,0,0);
    testParseOneCase(buf,"POST https://fuck.com:3000 HTTP/1.1 \r\n",0,8,5,4,0,0);
    testParseOneCase(buf,"GET https://fuck.com:3000/index.html?absolute=1 HTTP/1.1  \r\n",5,8,5,4,10,4);
}

static void test_header_find(char* hname,int offset){
    header_val_t* find = (header_val_t*) hash_find(header_map, hname, strlen(hname));
    test_int(1, find->offset == offset);
}

/*
    测试header是否能够正确解析
 */
static void test_header_parse(char * s,char * name,char * value,buffer_t* buf)
{
    #define HEADER_AT(r,off) ((string*)((char*)(&r->headers) + off))
    http_request_t *r = create_http_request();
    
    int len = strlen(s);
    strncpy(buf->data, s, len);
    buf->begin = buf->data;
    buf->end = buf->data + len;
    r->state = HD_BEGIN;
    
    int err = parse_header(r,buf);
    test_int(OK, err);

    header_val_t* find = (header_val_t*) hash_find(header_map,name,strlen(name));
    find->header_parser(r,find->offset);
    string* header = HEADER_AT(r,find->offset);
    test_int(0, strncmp(value,header->c,header->len));

    freePool(r->pool);
}

void testHeaderInit()
{
    header_map_init();
    test_header_find("cookie", offsetof(request_header_t, cookie));
    test_header_find("accept", offsetof(request_header_t, accept));
    test_header_find("date", offsetof(request_header_t, date));
}

static void testHeaderParse(){
    memory_pool* p = createPool(2000 * 10);
    pMemoryPoolInit(p,2000 * 10);
    buffer_t* b = createBuffer(p);
    test_header_parse("cookie:123456hhhh\n","cookie","123456hhhh",b);
    test_header_parse("accept:123456hhhh\r\n","accept","123456hhhh",b);
    test_header_parse("cookie:aaa\r\ndate:123456hhhh\r\n","cookie","aaa",b);
}

static rbtree_node_t* create_node(rbtree_key_t key){
    rbtree_node_t* node = (rbtree_node_t*)(malloc(sizeof(rbtree_node_t)));
    node->key = key;
    return node;
}

static void BinTreeInsert(rbtree_key_t arr[],int len){
#define insert_node(node) rbtree_insert_timer_value(event_timer_rbtree.root,node, &event_timer_sentinel)
    for(int i = 0; i < len;i++){
        rbtree_node_t* node = create_node(arr[i]);
        insert_node(node);
    }
#undef insert_node
}


static void testBinTreeInsert()
{
    rbtree_init(&event_timer_rbtree, &event_timer_sentinel,
                    rbtree_insert_timer_value);
    
    rbtree_key_t arr[] = {100,200,300,250,10,1,199,99};
    BinTreeInsert(arr,sizeof(arr) / sizeof(rbtree_key_t));
    rbtree_node_t* root = event_timer_rbtree.root->right;
    test_int(100,root->key);
    test_int(200,root->right->key);
    test_int(300,root->right->right->key);
    test_int(250,root->right->right->left->key);
    test_int(10,root->left->key);
    test_int(1,root->left->left->key);
    test_int(199,root->right->left->key);
    test_int(99,root->left->right->key);
    event_timer_rbtree.sentinel->parent = event_timer_rbtree.sentinel;
    event_timer_rbtree.sentinel->left = event_timer_rbtree.sentinel;
    event_timer_rbtree.sentinel->right = event_timer_rbtree.sentinel;
}


static void RbTreeInsert(rbtree_key_t arr[],int len){
#define insert_node(node) rbtree_insert(&event_timer_rbtree,node)
    for(int i = 0; i < len;i++){
        rbtree_node_t* node = create_node(arr[i]);
        insert_node(node);
    }
#undef insert_node
}

static void testRbTreeInsert()
{
    #define test_red(node) test_int(1,node->color)
    #define test_black(node) test_int(0,node->color)
    
    rbtree_init(&event_timer_rbtree, &event_timer_sentinel,
                rbtree_insert_timer_value);
    rbtree_key_t arr[] = {12,1,9,2,0,11,7,19};
    RbTreeInsert(arr,sizeof(arr) / sizeof(rbtree_key_t));
    rbtree_node_t* root = event_timer_rbtree.root;
    test_int(9,root->key);
    test_black(root);
    test_int(1,root->left->key);
    test_red(root->left);
    test_int(12,root->right->key);
    test_black(root->right);

    test_black(root->right);
    test_int(0,root->left->left->key);
    
    test_black(root->left->right);
    test_int(2,root->left->right->key);
    
    test_red(root->right->left);
    test_int(11,root->right->left->key);
    
    test_red(root->left->right->right);
    test_int(7,root->left->right->right->key);
    
    test_red(root->right->right);
    test_int(19,root->right->right->key);
}

/*测试二叉树删除的算法*/
static void testBinTreeDelete(){
    rbtree_init(&event_timer_rbtree, &event_timer_sentinel,rbtree_insert_timer_value);
    rbtree_key_t arr[] = {5,1,7,4,8,6,19,9};
    rbtree_node_t* nodes[10];
    int len = sizeof(arr) / sizeof(rbtree_key_t);
    
#define insert_node(node) rbtree_insert_timer_value(event_timer_rbtree.root,node, &event_timer_sentinel)
    for(int i = 0; i < len;i++){
        rbtree_node_t* node = create_node(arr[i]);
        nodes[i] = node;
        insert_node(node);
    }
#undef insert_node
    rbtree_node_t* node = nodes[2];
    rbtree_delete_value(&event_timer_rbtree,node);
    rbtree_node_t* root = event_timer_rbtree.root->right;
    test_int(5,root->key);
    test_int(8,root->right->key);
    test_int(6,root->right->left->key);
    test_int(4,root->left->right->key);
    test_int(root->right->right->left->key == 9,1);
    test_int(root->right->right->right == event_timer_rbtree.sentinel,1);
    rbtree_delete_value(&event_timer_rbtree,nodes[2]);
    test_int(9,root->right->key);
    test_int(root->right->right->left == event_timer_rbtree.sentinel,1);

}

void test()
{
    config_load();
    testVector();
    testChunk();
    testPool();
    testMemoryPool();
    testHash();
    testParseRequestLine();
    testHeaderInit();
    testHeaderParse();
    testBinTreeInsert();
    testRbTreeInsert();//
    testBinTreeDelete();
}
