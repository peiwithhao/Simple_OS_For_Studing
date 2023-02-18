#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
#include "memory.h"
#include "list.h"

/* 内存池标记，用于判断用哪个内存池，这里采用enum枚举 */
enum pool_flags{
  PF_KERNEL = 1,    //内核内存池
  PF_USER = 2       //用户内存池
};

#define PG_P_1 1    //页表项或页目录项存在属性位
#define PG_P_0 0    //页表项或页目录项存在属性位
#define PG_RW_R 0   //R/W属性位值，读/执行
#define PG_RW_W 2   //R/W属性位值，读/写/执行
#define PG_US_S 0   //U/S属性位值，系统级
#define PG_US_U 4   //U/S属性位值，用户级
/* 虚拟地址池，用于虚拟地址管理 */
struct virtual_addr {
  struct bitmap vaddr_bitmap;
  uint32_t vaddr_start;
};

/* 内存块 */
struct mem_block{
  struct list_elem free_elem;
};

/* 内存块描述符，一个描述符描述对应的arena */
struct mem_block_desc{
  uint32_t block_size;  //内存块大小
  uint32_t blocks_per_arena;    //本arena中可容纳此mem_block的数量
  struct list free_list;        //目前可用的mem_block链表
};

#define DESC_CNT 7              //内存块描述符个数，这里我们实现了16,32,64,128,256,512,1024字节这几种规格



extern struct pool kernel_pool, user_pool;
void mem_init(void);
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt);
void* get_kernel_pages(uint32_t pg_cnt);
void* get_user_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf, uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void block_desc_init(struct mem_block_desc* desc_array);
void* sys_malloc(uint32_t size);
void pfree(uint32_t pg_phy_addr);
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt);
void sys_free(void* ptr);
void malloc_init(void);
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr);
#endif
