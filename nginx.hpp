#ifndef NGINX_HPP
#define NGINX_HPP
//移植nginx内存池代码，oop实现


using u_char=unsigned char;
using ngx_uint_t=unsigned int;
using size_t=long unsigned int;
struct ngx_pool_s;//类型前置声明



//清理函数的类型
typedef void (*ngx_pool_cleanup_pt)(void *data);
struct ngx_pool_cleanup_s {
    ngx_pool_cleanup_pt   handler;//定义了一个函数指针保存清理操作的被调函数
    void                 *data;//传递给回调函数的参数
    ngx_pool_cleanup_s   *next;//所有cleanup清理操作都被串在一起
};

//大块内存头部信息
struct ngx_pool_large_s {
    ngx_pool_large_s     *next;//串在一条链表上
    void                 *alloc;//保存分配出去的大块内存起始地址
};

//内存池分配小块内存的头部数据信息
struct ngx_pool_data_t{
    u_char               *last;//小块内存池可用内存的起始地址
    u_char               *end;//末尾地址
    ngx_pool_s           *next;//串起来
    ngx_uint_t            failed;//记录当前小块内存池分配失败的次数
};
//ngx内存池的头部信息和管理成员信息
struct ngx_pool_s {
    ngx_pool_data_t       d;//内存池的数据域，内存的使用情况
    size_t                max;//存储的小块内存和大块内存分界线
    ngx_pool_s           *current;//指向第一个提供小块内存分配的小块内存池
    ngx_pool_large_s     *large; //指向大块内存（链表）的入口地址
    ngx_pool_cleanup_s   *cleanup;//指向所有预置的清理操作的回调函数
};

#define ngx_align(d, a) (((d) + (a - 1)) & ~(a - 1))//调整成a的倍数

const int ngx_pagesize=4096;//默认一个页面大小
const int NGX_MAX_ALLOC_FROM_POOL = (ngx_pagesize - 1);//能分配的最大内存

const int NGX_DEFAULT_POOL_SIZE = 16 * 1024;//默认池的大小 16k

const int NGX_POOL_ALIGNMENT= 16;//字节对齐
const int NGX_MIN_POOL_SIZE  =  
        ngx_align((sizeof(ngx_pool_s) + 2 * sizeof(ngx_pool_large_s)),
                NGX_POOL_ALIGNMENT) ;   //最小的大小 
#define NGX_ALIGNMENT   sizeof(unsigned long )    /* platform word */
#define ngx_align_ptr(p,a)                                     \         
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1)) //调整倍数    
#define ngx_memzero(buf, n)       (void) memset(buf, 0, n)

class nginx_pool
{
private:
    ngx_pool_s *pool;//指向nginx内存池入口指针
    void *ngx_palloc_small( size_t size, ngx_uint_t align);//小块内存分配
    void *ngx_palloc_block( size_t size);//分配新的小块内存池
    void *ngx_palloc_large( size_t size);//大块内存分配
public:
    void* ngx_create_pool(size_t size);//创建内存池成功/失败
    void ngx_destroy_pool();//内存池销毁
    void ngx_reset_pool();//内存重置函数

    void *ngx_palloc( size_t size);//考虑内存对齐 分配内存
    void *ngx_pnalloc(size_t size);//不考虑内存对齐 分配内存
    void *ngx_pcalloc(size_t size);//调用ngx_palloc，会初始化0
    void *ngx_pmemalign(void*pool);
    void ngx_pfree( void *p);//释放大块内存

    ngx_pool_cleanup_s *ngx_pool_cleanup_add(size_t size);//添加 清理操作的回调函数的函数

};


#endif