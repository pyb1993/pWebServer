//
//  hash.c
//  pWenServer
//
//  Created by pyb on 2018/3/1.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "hash.h"

uint hash_key_function(char *data, size_t len)
{
    uint  i, key;
    
    key = 0;
    
    for (i = 0; i < len; i++) {
        /* 调用宏定义的hash 函数 */
        key = char_hash(key, data[i]);
    }

    return key;
}


int hashInit(hash_initializer *hinit, hash_key *names, uint nelts)
{
    u_char          *elts;
    size_t           len;
    u_short         *test;
    uint       i, n, key, size, start, bucket_size;
    hash_slot  *elt, **buckets;
    
    for (n = 0; n < nelts; n++) {
        /* 若每个桶bucket的内存空间不足以存储一个关键字元素，则出错返回
         * 这里考虑到了每个bucket桶最后的null指针所需的空间，即该语句中的sizeof(void *)，
         * 该指针可作为查找过程中的结束标记
         */
        if (hinit->bucket_size < HASH_ELT_SIZE(&names[n]) + sizeof(void *))
        {
            assert(!"could not build the %s, you should " "increase %s_bucket_size");
            return ERROR;
        }
    }
    
    test = pMalloc(hinit->pool, hinit->max_size * sizeof(u_short));
    if(test == NULL){
        return ERROR;
    }
    
    /* 每个bucket桶实际容纳的数据大小，
     * 由于每个bucket的末尾结束标志是null，
     * 所以bucket实际容纳的数据大小必须减去一个指针所占的内存大小
     */
    
    bucket_size = hinit->bucket_size - sizeof(void *);
    
    /* 估计hash表最少bucket数量；
     * 每个关键字元素需要的内存空间是 NGX_HASH_ELT_SIZE(&name[n])，至少需要占用两个指针的大小即2*sizeof(void *)
     * 这样来估计hash表所需的最小bucket数量
     * 因为关键字元素内存越小，则每个bucket所容纳的关键字元素就越多
     * 那么hash表的bucket所需的数量就越少，但至少需要一个bucket
     */
    start = nelts / (bucket_size / (2 * sizeof(void *) + sizeof(int)));
    start = start ? start : 1;
    
    /* 以前面估算的最小bucket数量start，通过测试数组test估算hash表容纳 nelts个关键字元素所需的bucket数量
     * 根据需求适当扩充bucket的数量
     */
    for (size = start; size <= hinit->max_size; size++) {
        
        memzero(test, size * sizeof(u_short));
        
        for (n = 0; n < nelts; n++) {
            if (names[n].key.c == NULL) {//如果key是NULL,那么就需要跳过
                continue;
            }
            
            /* 根据关键字元素的hash值计算存在到测试数组test对应的位置中，即计算bucket在hash表中的编号key,key取值为0～size-1 */
            key = names[n].key_hash % size;
            test[key] = (u_short) (test[key] + HASH_ELT_SIZE(&names[n]));// 累积增加一次大小
            
            /* test数组中对应的内存大于每个桶bucket最大内存(256byte)，则需扩充bucket的数量
             * 即在start的基础上继续增加size的值
             */
            if (test[key] > (u_short) bucket_size) {
                goto next;
            }
        }
        /* 若size个bucket桶可以容纳name数组的所有关键字元素，则表示找到合适的bucket数量大小即为size */
        goto found;
    next:
        continue;
    }
    
    assert(!"could not build optimal %s, you should increase "
            "either %s_max_size: %i or %s_bucket_size: %i; "
            "ignoring %s_bucket_size");
    
found:
    /* 到此已经找到合适的bucket数量，即为size
     * 重新初始化test数组元素，初始值为一个指针大小
     */
    for (i = 0; i < size; i++) {
        test[i] = sizeof(void *);
    }
    
    /* 计算每个bucket中关键字所占的空间，即每个bucket实际所容纳数据的大小，
     * 必须注意的是：test[i]中还有一个指针大小
     */
    for (n = 0; n < nelts; n++) {
        if (names[n].key.c == NULL) {
            continue;
        }
        
        /* 根据hash值计算出关键字放在对应的test[key]中，即test[key]的大小增加一个关键字元素的大小 */
        key = names[n].key_hash % size;
        test[key] = (u_short) (test[key] + HASH_ELT_SIZE(&names[n]));
    }
    
    len = 0;// len是累积的总长度
    
    /* 调整成对齐到cacheline的大小，并记录所有元素的总长度 */
    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {
            continue;
        }
        
        test[i] = (u_short) (align(test[i], cacheline_size));
        len += test[i];
    }
    
    /*
     * 向内存池申请bucket元素所占的内存空间，
     * 注意：若前面没有申请hash表头结构，则在这里将和ngx_hash_wildcard_t一起申请
     */
    if (hinit->hash == NULL) {
        hinit->hash = (hash*)pMalloc(hinit->pool, sizeof(int) + size * sizeof(hash_slot *));
        if (hinit->hash == NULL) {
            pFree(hinit->pool,(Header*)test);
            return ERROR;
        }
        
        /* 计算buckets的起始位置 */
        buckets = (hash_slot **)((u_char *) hinit->hash);
        
    } else {
        buckets = (hash_slot**)pMalloc(hinit->pool, size * sizeof(hash_slot *));
        if (buckets == NULL) {
            pFree(hinit->pool,(Header*)test);
            return ERROR;
        }
    }
    
    /* 分配elts，整个连续的内存分配了出去 elts -> [[slot1][slot2][slot3].....] */
    elts = pMalloc(hinit->pool, (uint)len + cacheline_size);// 这个 cacheline_size是为了对齐,因为对齐时候舍去的部分肯定小于cacheline_size,所以最后总大小还是大于len
    if (elts == NULL) {
        pFree(hinit->pool,(Header*)test);
        return ERROR;
    }
    
    
    elts = align_ptr(elts, cacheline_size);
    /* 将buckets数组与相应的elts对应起来，即设置每个bucket对应实际数据的地址 */
    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {
            continue;//这里代表test[i]什么数据都没有存,只存了一个指针,那么buckets[i]就不需要指向对应的elts(也就是不需要分配任何内存给这个bucket)
        }
        
        buckets[i] = (hash_slot *) elts;
        elts += test[i];// test[i]代表第i个bucket一共需要容纳多少数据
    }
    
    /* 清空test数组，以便用来累计实际数据的长度，这里不计算结尾指针的长度 */
    for (i = 0; i < size; i++) {
        test[i] = 0;
    }
    
    /* 依次向各个bucket中填充实际数据 */
    /* test[i]代表第i个bucket已经实际容纳了多少数据了  */
    for (n = 0; n < nelts; n++) {
        if (names[n].key.c == NULL) {
            continue;
        }
        
        key = names[n].key_hash % size;
        elt = (hash_slot *) ((u_char *) buckets[key] + test[key]);
        
        elt->value = names[n].value;
        elt->len = (u_short) names[n].key.len;
        strlow(elt->name, (u_char *)names[n].key.c, names[n].key.len);//小写转换
        
        /* test[key]记录当前bucket内容的填充位置，即下一次填充的起始位置 */
        test[key] = (u_short) (test[key] + HASH_ELT_SIZE(&names[n]));// 
    }
    
    /* 设置bucket结束位置的null指针 */
    for (i = 0; i < size; i++) {
        if (buckets[i] == NULL) {
            continue;
        }
        
        elt = (hash_slot *) ((u_char *) buckets[i] + test[i]);//这里就直接指向该bucket最后的位置了
        
        elt->value = NULL;
    }
    
    //将分配的内存释放掉
    pFree(hinit->pool,test);
    
    hinit->hash->buckets = buckets;
    hinit->hash->size = size;
    return OK;
}


void* hash_find(hash* h, char * name, size_t len){
    uint key = hash_key_function(name, len);
    int idx = key % h->size;
    hash_slot* bucket = h->buckets[idx];
    while(bucket->value != NULL){
        if (len == (size_t) bucket->len &&
            strncmp(name,&bucket->name[0], len) == 0) {
            return bucket->value;
        }

        bucket = (hash_slot *)align_ptr(&bucket->name[0] + bucket->len,sizeof(void*));
    }
    return NULL;
}

/* 输入是ip的整数形式,需要将这个整数变成更随机的hashcode
   否则如果ip都来自同一个局域网,那么可能很接近,导致被很集中的分配到一个区域
   这里就简单的进行一个随机算法,没有什么严格证明
 */
uint32_t hash_code_of_ip_integer(uint32_t x)
{
    static uint32_t random_salt[17] = {1451352145,453454322,3454325,3542345,1214225,75243532,45324515,3445665,1477235,4532543,7855743,4114345,132145,14533432,958930665,34455,25484754};
    uint32_t hashcode = x;
    while(x > 0) {
        hashcode = (uint32_t)(hashcode * 113 + random_salt[hashcode % 17]);/* hash 函数 */
        x = (x >> 1);
    }
    return hashcode;
}

//转换并且拷贝
void strlow(u_char *dst, u_char *src, size_t n)
{
    uint  key;
    key = 0;
    
    while (n--) {/* 把src字符串的前n个字符转换为小写字母 */
        *dst = tolower(*src);
        dst++;
        src++;
    }
}
