
#include"nginx.hpp"
#include<iostream>
#include<cstdlib>
#include<memory.h>

void* nginx_pool::ngx_create_pool(size_t size)//创建内存池成功/失败
{
    ngx_pool_s  *p;

    p = (ngx_pool_s*)malloc(size);//根据用户指定size大小内存对齐，不同平台
    if (p == nullptr) {
        return NULL;
    }
	//初始化
    p->d.last = (u_char *) p + sizeof(ngx_pool_s);//指向除ngx_pool_t结构的头信息之后的位置
    p->d.end = (u_char *) p + size;//指向内存池的末尾
    p->d.next = nullptr;//下一个内存块
    p->d.failed = 0;

    size = size - sizeof(ngx_pool_s);//内存池能使用的大小，size-头部信息
    p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;
	//能分配最大字节数		//4095	开辟比一个页面小就用size，比一个页面大就用一个页面				
    p->current = p;//指向内存起始地址
    p->large = nullptr;
    p->cleanup = nullptr;

    pool=p;
    return p;
}

void *nginx_pool::ngx_palloc( size_t size)//考虑内存对齐 分配内存
{
 if (size <= pool->max) {//小块内存<=4095
        return ngx_palloc_small(size,1);
    }
    return ngx_palloc_large(size);//大块
}

void *nginx_pool::ngx_palloc_small( size_t size, ngx_uint_t align)//小块内存分配
{
    u_char      *m;
    ngx_pool_s  *p;

    p = pool->current;

    do {
        m = p->d.last;//可分配内存的地址

        if (align) {
            m = ngx_align_ptr(m, NGX_ALIGNMENT);//根据平台 调整倍数
        }

        if ((size_t) (p->d.end - m) >= size) {//剩余的大于要申请的size
            p->d.last = m + size;//把m指针偏移个size字节

            return m;
        }

        p = p->d.next;

    } while (p);

    return ngx_palloc_block(size);//剩的不够分配了
}
void *nginx_pool::ngx_palloc_block( size_t size)//分配新的小块内存池
{
    u_char      *m;
    size_t       psize;
    ngx_pool_s  *p,*new_;

    psize = (size_t) (pool->d.end - (u_char *) pool);//计算要再分配的内存块大小

	//又开辟一块空间
    m = (u_char*)malloc( psize);
    if (m == NULL) {
        return NULL;
    }
	//指向新内存块
    new_ = (ngx_pool_s *) m;

    new_->d.end = m + psize;
    new_->d.next = NULL;
    new_->d.failed = 0;
	
    m += sizeof(ngx_pool_data_t);//头只存ngx_pool_data_t的成员
    m = ngx_align_ptr(m, NGX_ALIGNMENT);
    new_->d.last = m + size;//分配出去，指向剩的内存起始地址

    for (p = pool->current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {//当前块内存一直不够分配
            pool->current = p->d.next; //去下一个内存块
        }
    }
    p->d.next = new_;//新生成的内存块接到原来的内存块后面
    return m;
}
void *nginx_pool::ngx_palloc_large( size_t size)//大块内存分配
{
    void              *p;
    ngx_uint_t         n;
    ngx_pool_large_s  *large;

    p = malloc(size);//调malloc
    if (p == nullptr) {
        return nullptr;
    }

    n = 0;
	//遍历链表
    for (large = pool->large; large; large = large->next) {
        if (large->alloc == nullptr) {
            large->alloc = p;
            return p;
        }

        if (n++ > 3) {
            break;
        }
    }

    large = (ngx_pool_large_s*)ngx_palloc_small(sizeof(ngx_pool_large_s),1);//通过小块内存把large头分配进去
    if (large == nullptr) {
        free(p);
        return nullptr;
    }

    large->alloc = p;//大块内存起始地址
    large->next = pool->large;//头插 连接起来
    pool->large = large;

    return p;
}
void nginx_pool::ngx_pfree(void *p)//释放大块内存
{
    ngx_pool_large_s  *l;

    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
           
            free(l->alloc);
            l->alloc = NULL;

            return ;
        }
    }
}
void *nginx_pool::ngx_pnalloc(size_t size)//不考虑内存对齐 分配内存
{
    if (size <= pool->max) {
        return ngx_palloc_small(size, 0);
    }
    return ngx_palloc_large(size);
}
void *nginx_pool::ngx_pcalloc(size_t size)//调用ngx_palloc，会初始化
{
    void *p;

    p = ngx_palloc(size);
    if (p) {
        ngx_memzero(p, size);
    }

    return p;
}

void nginx_pool::ngx_destroy_pool()//内存池销毁
{
     ngx_pool_s          *p, *n;
    ngx_pool_large_s    *l;
    ngx_pool_cleanup_s  *c;

    for (c = pool->cleanup; c; c = c->next) {
        if (c->handler) {
            c->handler(c->data);//回调函数
        }
    }

    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            ngx_pfree(l->alloc);
        }
    }

    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
        ngx_pfree(p);

        if (n == nullptr) {
            break;
        }
    }
}
void nginx_pool::ngx_reset_pool()//内存重置函数
{
     ngx_pool_s        *p;
    ngx_pool_large_s  *l;

    for (l = pool->large; l; l = l->next) {//大块
        if (l->alloc) {
            ngx_pfree(l->alloc);
        }
    }
    //下面我改的处理小块内存
	//处理第一个内存块
	p=pool;
	p->d.last = (u_char *) p + sizeof(ngx_pool_s);
    p->d.failed = 0;
	//处理第二个开始到剩下的
	for (p = p->d.next; p; p = p->d.next) {//小块
        p->d.last = (u_char *) p + sizeof(ngx_pool_data_t);
        p->d.failed = 0;
    }
	
    pool->current = pool;
    pool->large = nullptr;
}

ngx_pool_cleanup_s *nginx_pool::ngx_pool_cleanup_add(size_t size)//添加 清理操作的回调函数的函数
{
  ngx_pool_cleanup_s  *c;

    c = (ngx_pool_cleanup_s  *)malloc(sizeof(ngx_pool_cleanup_s));//头信息，小块内存开辟
    if (c == nullptr) {
        return nullptr;
    }

    if (size) {
        c->data = malloc(size);
        if (c->data == nullptr) {
            return nullptr;
        }

    } else {
        c->data = nullptr;
    }

    c->handler = nullptr;
    c->next = pool->cleanup;

    pool->cleanup = c;
    return c;
}