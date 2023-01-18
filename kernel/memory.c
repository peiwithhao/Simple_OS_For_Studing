#include "memory.h"
#include "stdint.h"
#include "print.h"

#define PG_SIZE 4096    //4K
/************************ 位图地址 ****************************/
#define MEM_BITMAP_BASE 0xc009a000
/**************************************************************/

/* 0xc0000000是内核从虚拟地址3G开始
 * 而1MB指的是跨过低端1MB内存
 * */

/* 0xc0000000是内核从虚拟地址3G起
 * 0x100000是跨过低端1MB内存， 使虚拟地址在逻辑上连续*/
#define K_HEAP_START 0xc0100000

/* 内存池结构，生成两个实例用于管理内核内存池和用户内存池 */
struct pool{
  struct bitmap pool_bitmap;    //本内存池用到的位图结构，用于管理物理内存
  uint32_t phy_addr_start;
  uint32_t pool_size;
};

struct pool kernel_pool, user_pool; //生成内核内存池和用户内存池
struct virtual_addr kernel_vaddr;   //此结构用来给内核分配虚拟地址

/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem){
  put_str("     mem_poool_init_start \n ");
  uint32_t page_table_size = PG_SIZE * 256;
  //页表大小 = 1页的页目录表 + 第0项和第768个页目录项指向同一个页表 + 第769～1022个页目录项共指向254个页表，共256个页框
  uint32_t used_mem = page_table_size + 0x100000;   //0x100000为低端1MB内存
  uint32_t free_mem = all_mem - used_mem;
  uint16_t all_free_pages = free_mem/PG_SIZE;
}
