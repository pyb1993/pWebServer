
### pWebServer:一个异步非阻塞http服务器的实现

#### 项目简介.  
>pWebServer是一个运行在「macOS」平台上的异步非阻塞,以kevent/select(正在开发)事件驱动为核心的http服务器;  
>参考了网上很多开源项目,仅仅作为学习之作  
>写这个项目的目的:   
>是为了更好的理解事件驱动机制; 更好的学习socket编程;更好的理解TCP的各种状态;
>也是为了运用在APUE/UNP这类书籍上学习到的系统知识;
>还是为了塑造一个了解开源项目实现(Nginx)的机会;

----------
#### Feature介绍
>**
**0. 事件驱动机制**  
  「0」参考了Nginx的模块化编程方式,将事件驱动的各种操作「抽象」为 module 和 module_contxt两种类型  
 以这个为基础,在服务器启动的时候,选择合适的事件驱动机制;然后将所有的动作都抽象为函数+宏的方式  
 这样只需改变函数指针的指向,其他所有应用代码中的方式都是保持不变的。  
「 1」在整个过程中,是以event_t这个类型,为核心驱动的.  
 事件驱动函数里面绑定的data类型指向一个event_t,它指向了connection_t,而connection_t又指向了event_t和  request_t,  
 这就把整个http的逻辑串起来了.  
 
**1.收发机制**  
.0 关于收和发:  
		 第一, 必须全部是非阻塞的,所以完全可能存在中间状态。这就导致我们必须保留上一次中断时的状态,方便下一次继续执行。这里存在一种可能,buffer的大小不够导致无法继续接受,这种情况下需要对buffer进行扩容(暂时未实现)  
		 第二,  收发过程可能出现错误; 这里有几种错误,阻塞错误,链接关闭的错误,或者其他的错误;这里需要处理  
		 第三, send的时候可能导致sigpipe信号被触发,所以要做好相应的处理  
     
**2.业务流程**  
 .0 业务流程,从本质上来说是一个状态机。我们通过event的函数指针来决定,现有的http请求处理到了哪一个阶段  
 .1 假如是刚刚接受到请求,但是还没有接受到数据. 那么函数指针就指向一个尝试接受request_line并且解析的函数  
 .2 假如request_line已经成功解析完了,那么函数指针就指向尝试接受头部,并且解析的函数  
 .3 其它类似,除了函数指针代表这状态,实际上还有数据代表着状态.比如buffer里面的指针就代表了当前接受的数据。  
 .4 一个事件的回调完全有可能会影响另外的事件,比如反向代理中,后端链接发生错误,这个时候就需要改变和客户端链接  
    的状态,在这个过程中,需要非常小心  
    
 **3. 负载均衡**  
.0 尝试链接后端的时候使用非阻塞connect,这是很重要的一点。但是也因此增加了复杂度,必须在回调函数里面处理connect的成功或失败  
.1 需要将客户端的数据转发到后端服务器, 将后端的数据转发到客户端。存在两者同时进行的情况,所以需要两个buffer  
.2 由于存在「失败时间」的设定,所以也存在负载均衡模块发现所有的后端不可用的情况,这算一个corner case  
.3 存在链接立刻失败的可能,要限制最大尝试的次数。超过次数则认定链接失败  
.4 也是使用模块的机制,存在两种模块(一致性hash算法 / 加权平滑算法 )  

**4. 定时器** 
.0 定时器是一个用来处理超时事件的东西,Nginx里面采取了「侵入式」的写法,把定时器直接写到了event_t里面,不需要讲如
.1 采用红黑树实现
.2 需要处理事件关闭和定时器之间的关系,还需要处理重复添加事件,重复删除事件等情况

**5.基本结构**  
.0 使用静态的hash表,用来实现负载均衡时候后端服务的查找,以及用来查找头部的实现  
.1 string_t结构,各个地方都使用  
.2 chunk结构,用来描述一块内存分配的结构,一个chunk实际上不会单独被使用,而是配合vector一起使用的。  
   主要目的是,希望分配内存的时候一次分配多个需要的内存结构单元(比如项目中对connection_t就是设置为8个)  
   一个chunk_t实际上就仅仅wrap了一个 void*指针,  这个指针指向一个((sizeof(void*) + 基本单元) * 数量)大小的空间  
.3 vector结构,这里的vector实际上并不是一个真正的类似C++里面的vector,它主要起一个分配和容器的作用,只能增加,不能释放  
.4 pool结构,这里的pool结构结合了 chunk和vector实现。它可以free限制的内存,但是不会返回给操作系统。并且使用了一个名字为free的指针用来链接上所有空闲的内存。     
.5 memory_pool,内存池结构,这个和上面的作用不一样,这里不针对具体的结构分配固定大小的内存单元,而是将一个固定大小的内存池作为资源分配的容器,释放的时候也一起释放所有的内存。采取最简单的遍历技术实现(待修改)  
.6共享内存,主要作用是在进程间互享一些变量,使用mmap用匿名文件映射内存。实现了一个简单的自旋锁,并且用汇编实现了cmp操作(具体汇编细节不是很理解),自旋锁暂时没有使用,因为是为了防止惊群现象,但是有两个原因  
1 现在的unix系统稍微高一点的版本都可以使用reuse_port参数了  
2 高并发下面有锁反而效率低。  
.7 红黑树,主要是模仿了Nginx重新写了一遍,中间出现了一些bug,至今没有找到,最后直接使用Nginx的代码。  	
	
---
#### 基本结构和实现

#### 1. module

module是Nginx里面抽象出来的一个类型,用来控制和屏蔽各个不同的模块的底层实现细节,并且提供了相应的接口和配置来供外部添加新的功能。这里没能做到那么负载,也没有实现第三方添加的功能,只是对一些通用的功能做了抽象,这样需要替换具体的实现细节的时候,只要在代码里面按照规定的接口添加新的模块实现,并且修改配置文件的参数即可。
    
  Module的大部分代码都在module.h和module.c里面,具体来讲,大概是这么几个层次:
  `module_t`类型,这个类型是最高层次的类型,里面继续封装了`module_context(ctx)`,`process_init`,`module_init`这些成员
  `module_context`则分为很多中,到目前为止,针对负载均衡模块和事件循环模块,实际上是不同的类型,所以module_t里面的`module_context`实际上是一个void*类型的变量。

针对事件循环,module_context是一个`event_module_t`类型的结构体,这个结构体有和配置有关的函数(为了从配置文件中读取和创建对应的结构,但是目前没有实现),以及最重要的结构`event_actions`,该结构里面封装了对事件的各种操作(函数指针),
例如`add_event,delete_event,event_init,process_event`,这些操作是任何一种IO多路复用机制都必须满足的。

于是, 我们只需要针对具体的事件循环机制增加一个模块,写好对应的操作函数,然后在初始化的时候将对应的函数指针指向这些函数.其他所有http业务逻辑的操作都是不变的。

针对负载均衡的module,基本思路是类似的,但是有一些差别. 对外只暴露 `get_server` 和 `free_server`这两个接口,包括重试,异常处理都是在里面完成的。

---
##### 2 hash
这里的hash是静态的hash,因为使用的地方都是在配置文件读取之后都全部确定了,不需要动态的进行修改。大概的设计思路:
最重要的函数是`hash_init`这个函数,该函数接受3个参数`hash_initializer, names, nelts`
第一个参数里面存储了hash函数指针，每个桶最大大小,内存池等结构信息。第二个参数代表每一个key,第三个参数则代表有多少数量的元素。

在具体介绍流程之前,要说说涉及到的数据结构。
`hash_slot`,这个结构实际上是存储一个元素的单位,里面有`len(key的长度),name(指向具体的字符数据),value(指向具体的用户数据)`,

1 首先计算每一个的key和数据指针需要的大小,看看是否超过了`hash_init`里面指定的每一个桶的大小(限定了key不能太长)。怎么计算呢, 利用了一个宏: 
`#define HASH_ELT_SIZE(name) (sizeof(void *) + align((name)->key.len + 2, sizeof(void *)))`
注意这个计算的各个部分:  **void *代表指向用户数据的指针**,
**algin((name)->key.len + 2, sizeof(void*))是什么意思?** 首先不管`algin`,里面的部分是字符串的长度+2,+2代表`len(u_short)的长度`。然后algin实际上是用来进行对齐的,因为cpu读内只能通过特定的对齐地址（比如按照机器字,一般8个字节）进行访问的,如果一个数据占据了两个字,那么就要读两次。所以尽量对齐。
`#define ngx_align(d, a)     (((d) + (a - 1)) & ~(a - 1))`,这个很容易理解加了x - 1再对x取余。

2 用`hash_init`的内存池分配一个测试用的内存区域test,目的是**实验什么样的桶的个数在当前key的分布下能得到比较好的冲突情况，而test就是用来保存每个桶容纳了多少数据的**,

3 首先计算需要的最小的size是多少,这种情况下 `start = nelts / (bucket_size / (2 * sizeof(void *) + sizeof(int)))`,其中`bucket_size`代表的实际上是每一个桶最大能容纳多少的数据(需要减去最后一个NULL指针的大小)。
然后 `for (size = start; size <= hinit->max_size; size++){...}`里面进行计算。
如果我们发现所有的bucket容纳的内存大小没有超过指定的值(配置文件写好,默认256bytes),那么就说明这个size是OK的,否则增大继续搜索。

4 接下来开始实际的分配内存,在此之前需要重新计算test区域每个bucket的大小,因为每个bucket还需要一个开头的指针结构。
  然后对于每个bucket需要的大小都进行对齐`align(test[i], cacheline_size`，这是因为针对大的结构体,进行缓存行对齐有两个好处: 第一,避免跨缓存行多次读取,减少占用缓存的空间 第二,避免伪共享 由于缓存一致性协议的影响(比如MESI协议)和缓存锁, 访问另外一个缓存的时候可能还会影响其他缓存,导致被淘汰或者被锁住。
 然后向内存池申请具体的空间: 
 ` hinit->hash = (hash*)pMalloc(hinit->pool, sizeof(int) + size * sizeof(hash_slot *));`
 注意这里申请的空间实际上是按每个桶来申请的,也就是真正的数据实际上没有在这里。

下面这一步才是真正分配主要内存的时候
`elts = pMalloc(hinit->pool, (uint)len + cacheline_size);
    elts = align_ptr(elts, cacheline_size);`
 
5 将bucket数组和elts具体对应起来,注意到bucket数组的每一个元素实际上都是一个指向`hash_slot`的指针，因为前面的test已经计算过每个bucket需要多少大小的内存,所以在这里我们只要按照计算好的数据 `elts += test[i]`来移动`elts`就好。
  我们注意到,由于elts是连续分配的内存,所以`hash_slot*`实际上会指向连续的内存,这样的好处是内存连续分配,对缓存更加友好。同时释放的时候也好管理。

6 开始填充数据。
	`key = names[n].key_hash % size;
     elt = (hash_slot *) ((u_char *) buckets[key] + test[key]);`
   根据每个元素,计算出相应的桶的起始点,注意这里的test[key]不是上面的test了,这里已经重新初始化过,代表当前为止该桶已经装了多少个元素了。

最后在每一个桶的最后部分,添上一个NULL代表结束了。这是为了在查询的时候知道什么时候已经到终点了
` elt = (hash_slot *) ((u_char *) buckets[i] + test[i]);//这里就直接指向该bucket最后的位置了
        elt->value = NULL;`  
#### 3 pool
**1** pool是一种针对特定内存单元大小的分配容器,基本上属于只分配内存,不释放内存。
  它存在的目的是动态扩容和重复利用。类似与C++里面的vector.

**2** 具体实现依赖两种数据结构 `chunk` 和 `vector`,对于这两种类型,实际上并不是很复杂.
2.1 `chunk`本身是一种容器,分配`unit_num * unit_size`大小的内存,每个`chunk_slot`之间通过next指针进行链接.
	`chunk`的结构也很简单,只有一个`void*`,它的其它信息都是由`外层容器`掌握和存储的。
		
2.2 `vector`实际上也是一种容器,只增加,不减少,负责返回指定为止的元素以及添加元素和分配内存,同时每次分配的内存也是固定大小的。在项目中唯一用到`vector`的地方,元素的类型是`chunk`。`vector`的内存是一个`void* data`.
	每次需要扩容的时候,会进行一次`realloc`系统调用。这样保证所有的内存都是连续的。

**3** 由于pool封装了一个保存了`chunk`的`vector chunks`,所以它的操作实际上也是围绕着这两个数据结构来的。
   具体来说,`pool_init`操作首先调用`vector_init`,然后调用`chunk_init`,但是实际上来讲,由于pool本身还保存了一个free链表,如果要进行初始化,还必须把整个free链表指向所有的chunk,以及每一个`chunk`的最后一个`chunk_slot`要能够指向下一个`chunk`的第一个`chunk_slot`,比较麻烦,所以项目里面在init的时候没有真正的初始化这些,而是把分配内存延迟到了`poolAlloc`操作里面(因为这个操作本来就需要处理free链表,所以相当于复用这一块的代码了)

**4** `pllAlloc`操作是像`pool`申请一块固定大小内存的操作
首先需要判断free链表是不是还有空闲的内存,
如果有的话,那么应该直接进入最后步骤4
否则,进入步骤1
步骤1 `vectorPush`操作获取一个新的chunk,`vectorPush`会自动的扩容并且返回最后一个元素(这个名字起的不好,应该叫`vectorAlloc`)
步骤2 对这个`chunk`进行初始化,分配内存并且设置`chunk_slot`之间的next指针.
步骤3 将`free`指向新的chunk的`第一个chunk_slot`
步骤4 保存当前`free`指向的内存单元,然后`free`进行移动到下一个,返回保存的结果
具体实现如下:
```
    void* ret;
    p->used++;
    chunk_slot* new_chunk_slot;
    chunk* new_chunk;
    if(p->free == NULL){
        new_chunk = vectorPush(&p->chunks);// alloc a new chunk
        if(new_chunk == NULL) return NULL;
        if(chunkInit(new_chunk,p->unit_size,p->unit_num) != OK){
            return NULL;
        }
        new_chunk_slot = (chunk_slot*)(new_chunk->data);
        p->free = new_chunk_slot;
    }
    ret = (uint8_t*)p->free;
    p->free = ((chunk_slot*)ret)->next;
    return get_data_from_chunk(ret);
```
 5 `poolFree`  是释放的操作,别误会,它不是说将内存释放给操作系统,而是在pool结构里面将这个内存单元归于`free`.
    这里有一些小细节需要注意,由于`free`指针占据了一个位置,所以我们需要消除从`数据`到`chunk_slot`的这个`offset`
  
```
 void poolFree(pool* p, void* x) {
    if (x == NULL) {
        return;
    }
     --p->used;
     x = (uint8_t*)x - sizeof(void*);//回到真实的位置
    ((chunk_slot*)x)->next = p->free;
    p->free = x;
}
```
6 这个结构在项目中目前只用来分配`connection`

---
####4 memory_pool
memory_pool和上面的pool名字十分相似,但是内在逻辑是完全「不一致」的。
它是传统意义上的内存池,分配的内存是不固定的。
这里的思路相对比较简单,直接分配固定大小的内存作为一个内存池的「资源」
然后定义了一个`HEADER_SIZE`,这个代表每次分配的头部的大小,保存了这些元信息
```
typedef struct header{
    struct header * next;
    unsigned usedsize;
    unsigned freesize;
} Header;
```

接下来说明具体的分配是怎么实现
内存池有一个成团变量叫做memptr, 这个玩意最开始指向了NULL
首先计算出需要分配的总的大小,并且和Header的大小对齐(这样实现的目的是: 任何一块内存可以很轻易转换到Header)
第二步, 判断memptr,如果等于NULL,代表第一次分配,那么进行简单的初始化(分配头部的信息)
然后开始进行搜索`while (p->next != memptr && (p->freesize < nunits)) { p = p->next;}`
这样只要存在一个内存区域,使得p->freesize < nuints, 那么就一定可以找到这样的p
现在的需要将p以及对应的内存大小分配出去:
	首先 `nextp = p + p->usedsize, 然后nextp->freesize = p->freesize - nuints; nextp->usedsize = nuints;`
   然后 `nextp->next = p->next;p->next = nextp;p->freesize = 0;`
   最后 `memptr = nextp;`
   这种内存分配算法,实际上是保留最近一次分配的内存块,然后进行接下来的搜索。这样在free进行的很少的时候,实际上效率是很高的。但是如果free比较频繁,就会造成内存碎片很多。

释放是怎么实现的:
原理很简单,首先找到这一次分配的`header`bp,然后p是最近一次分配的元素的下一个块
我们从p开始寻找,直到找到bp或者memptr为止,目的就是找到bp之前的那个块`prev`
然后,把`prev`和bp合并起来
实际上,这里可以有一个小优化,可以看看bp后一个块是不是也能够合并
一个更好的实现是,将header实现成一个双端的节点,这样在pFree的时候,可以迅速的找到前一个节点,同时还非常方便两边连续合并。
```
void pFree(memory_pool * pool,void *ap)
{
    Header *bp, *p, *prev;
    bp = (Header*)ap - 1;
    prev = pool->memptr, p = pool->memptr->next;
    Header * memptr = pool->memptr;
    while ((p != bp) && (p != memptr)){
        prev = p;
        p = p->next;
    }
    if (p != bp) return;
    
    prev->freesize += p->usedsize + p->freesize;
    prev->next = p->next;
    pool->memptr = prev;
}
```

####4 rb_tree
红黑树是本项目里面最复杂的数据结构,具体算法就不说了,基本抄的Nginx。
使用了侵入式设计的结构,所以有一些旋转操作和普通的树不一样
比如对于普通的树,我们完全可以交换两个节点,这样A节点就变成了B,B就变成了A
比如说要删除key是1的节点,可以把那个节点的key赋值为它的后继,然后删除后继
但是现在是侵入式的结构,往往一个节点是绑定是具体的事件的,如果这样做,就相当于把其他事件给删除了

除此之外,还有一个小技巧就是使用了**sentinel**节点来作为空节点,这样做的好处是: 不用去判断是不是空指针,避免内存越界这样的错误了。



####5 event_t
这是事件的基本类型,所以这是核心数据结构之一, 我们看看怎么定义的
```
struct event_s{
    void *data;// 指向connection
    event_handler_pt  handler;
    bool active; // 标志是否是活跃的事件(用来避免重复添加事件)
    bool timer_set;//标志是否在红黑树里面
    bool timedout;
    rbtree_node_t   timer;
    // timer的结构
};
```
可以看到 `handler`就是简单的指向事件回调函数的函数指针,而`active`是用来标志事件是够活跃,其实就是代表是不是在我们的事件循环机制里面(kevent/select),可以避免重复加入而报错。
`timed_out`用来标志是不是已经超时了,这个标志是在统一处理超时事件的时候设置的,如果发现超时,就会设置这个标志,然后调用回调函数,这样回调函数就可以判断是正常的回调还是超时回调了。
`data`部分则是绑定到一个connection上面的,所有和http有关的网络状态都是通过这个data获取的。

####6 connection_t
```
typedef struct connection {
    void* _NONE; // 用来占位的(由于pool数据结构的设计(一开始没有考虑到),导致next成员会破坏connection_t的成员)
    int fd;
    uint32_t ip;
    void * data;//用来保存request
    event_t rev;
    event_t wev;
    bool is_connected;
    bool is_idle;
} connection_t;
```
这个同样也是核心数据结构之一,第一个`_NONE`是用来占位的,是个历史遗留包袱,暂时不用管它。
`fd`代表对应连接的文件描述符(socket在unix里面是和虚拟文件系统联系在一起的)
`ip`是用来获取client的ip的
`data`指向request,这个类型我们后面再说
`rev`, `wev`,这两个类型是用来存储读写事件的,也就是说一个连接同时关联两个`event`,理论上来说如果支持`pipe`,
完全有可能同时进行读写,也就是在发送请求的过程中,不断的接受请求。双方互相不干扰。
这里稍微猜测一下pipline配合读写事件大概的实现(没有看过Nginx的实现,纯粹脑洞),由于http本身没有办法区分不同的请求,所以要求服务器的response一定和客户端的顺序是对应的,这样假设我们接受到一个通过pipline执行的请求,那么就开始不断的接受请求(假设有10个),现在我们在接受第一个请求的时候就开启第一个读事件,并且将这个事件加入到事件循环,目的是读请求的数据,当第一个请求读完之后,我们将写事件加入事件循环,这里有几种可能的顺序（r1代表请求1的读取,w1代表请求1的写请求）:
1. 在r2读完之前,w1写完了,  这个时候检查,发现第二个请求没有结束,所以无法开启w2
2. 在r2读完的时候, w1没有写完, 这个时候没有办法开启w2

所以需要正确处理这两种情况

####7 http_request_t
这个类型主要是用来存储http相关的业务状态,先看看数据结构
```
typedef struct http_request_s{
    memory_pool* pool;
    buffer_t *recv_buffer;
    buffer_t *send_buffer;
    connection_t* connection;
    connection_t* upstream;
    string request_line;
    uri_t uri;
    version_t version;
    method_t method;
    string header_name;
    string header_value;
    request_header_t headers;
    transfer_encoding_t t_encoding;
    req_state_t state;
    int status;
    int port;
    int content_length;
    int body_received;
    int resource_off;
    int resource_fd;
    int upstream_tries;//尝试链接后端服务器的次数
    long resource_len;
    uint8_t response_done:1;
    uint8_t keep_alive:1;
    void* cur_upstream;
    void* cur_server_domain;//当前请求对应的domain
} http_request_t;
```
第一个结构是 pool,是一个内存池,这里有几个地方需要进行内存分配
1:  是recv_buffer和send_buffer,这两个结构是在初始化request的时候分配的
     不采取静态分配的方式是因为有可能需要动态对buffer进行扩容
2: 是http_request自己也是通过pool来分配的

后面两个结构是 recv_buffer和 send_buffer,主要是用来接受和传输数据
connection 指向连接,维护socket的信息
upstream 是指向后端服务的连接(不一定存在)
request_line则是指向buffer里面request_line的部分
uri_t则指向用来解析uri各种状态的变量的保存(状态机的实现)

还有一些结构没有用到

http_request在长链接的情况下可以长期保持,只需要清空状态，
但是在短连接的情况下就需要直接关闭了,需要把内存也释放了

todo: 
         对于短链接实际上也可以缓存.同时对于http_request的分配实际上可以使用一个大的内存池的方式进行分配 

 


