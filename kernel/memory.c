#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "global.h"
#include "debug.h"
#include "string.h"

#define PG_SIZE 4096    //4K
/************************ 位图地址 ****************************/
#define MEM_BITMAP_BASE 0xc009a000
/**************************************************************/

/* 0xc0000000是内核从虚拟地址3G开始
 * 而1MB指的是跨过低端1MB内存
 * */

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)       //这里是获取虚拟地址前十位，这里就是PDE的索引值
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)       //这里是获取虚拟地址中间十位，这里是PTE的索引值
/* 0xc0000000是内核从虚拟地址3G起
 * 0x100000是跨过低端1MB内存， 使虚拟地址在逻辑上连续*/

#define K_HEAP_START 0xc0100000         //设置堆起始地址用来进行动态分配

/* 内存池结构，生成两个实例用于管理内核内存池和用户内存池 */
struct pool{
  struct bitmap pool_bitmap;    //本内存池用到的位图结构，用于管理物理内存
  uint32_t phy_addr_start;      //本内存池的物理起始地址
  uint32_t pool_size;
};

struct pool kernel_pool, user_pool; //生成内核物理内存池和用户物理内存池
struct virtual_addr kernel_vaddr;   //此结构用来给内核分配虚拟地址

/* 在pf表示的虚拟内存池中申请pg_cnt个虚拟页，成功则返回虚拟页的起始地址，失败则返回NULL */
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt){
  int vaddr_start = 0, bit_idx_start = -1;
  uint32_t cnt = 0;
  if(pf == PF_KERNEL){
    bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);  //先查找位图看是否有足够大的内存
    if(bit_idx_start == -1){
      return NULL;
    }
    while(cnt < pg_cnt){
      bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
    }
    vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
  }else{
    //用户内存池，之后再补充
  }
  return (void*)vaddr_start;
}

/* 得到虚拟地址vaddr对应的pte指针 */
uint32_t* pte_ptr(uint32_t vaddr){
  /*先访问到页表自己，然后用页目录项pde作为pte的索引访问到页表，
   * 在用pte的索引作为页内偏移
   * */
  uint32_t* pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr)*4); //这里是指计算能够访问到vaddr对应pte的虚拟地址
  return pte;
}

/* 得到虚拟地址vaddr对应的pde的指针 */
uint32_t* pde_ptr(uint32_t vaddr){
  uint32_t* pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr)*4);
  return pde;
}

/* 在m_pool指向的物理内存池中分配1个物理页，
 * 成功则返回页框的物理地址，失败则返回NULL
 * */
static void* palloc(struct pool* m_pool){
  int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);   //找一个物理页面
  if(bit_idx == -1){
    return NULL;
  }
  bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);         //将对应位置1
  uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
  return (void*)page_phyaddr;
}

/* 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射 */
static void page_table_add(void* _vaddr, void* _page_phyaddr){
  uint32_t vaddr = (uint32_t)_vaddr,page_phyaddr = (uint32_t)_page_phyaddr;
  uint32_t* pde = pde_ptr(vaddr);
  uint32_t* pte = pte_ptr(vaddr);

/******************************** 注意 **********************************
 * 执行*pte会访问到空的pde，所以确保pde创建完成后才能执行*pte,
 * 否则会引发page_fault。因此在*pde为0的时候，*pte只能出现在下面else语句块中的*pde后面
 * **********************************************************************/
  /* 先在页目录内判断目录项的P位，若为1则表示该表已经存在 */
  if(*pde & 0x00000001){
    //页目录项和页表项的第0位为p，这里是判断页目录项是否存在
    ASSERT(!(*pte & 0x00000001));   //这里若是说以前有已经装载的物理页框，则会报错
    if(!(*pte & 0x00000001)){
      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }else{
      PANIC("pte repeat");      //ASSERT的内置函数
      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
  }else{
    //页目录项不存在，所以需要先创建页目录再创建页表项
    /* 页表中的页框一律从内核空间分配 */
    uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
    *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    /* 分配到的物理页地址pde_phyaddr对应的物理内存清0,
     * 避免里面的旧数据变成页表项，从而让页表混乱
     * 访问到pde对应的物理地址，用pte取高20位即可
     * 因为pte基于该pde对应的物理地址内再寻址，
     * 把低12位置0便是该pde对应的物理页的起始 */
    memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
    ASSERT(!(*pte & 0x00000001));
    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
  }
}

/* 分配pg_cnt个页空间，成功则返回起始虚拟地址，失败时则返回NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt){
  ASSERT(pg_cnt > 0 && pg_cnt < 32512);         //这里我们的物理内存是512字节，由于用户内存池和内核内存池各占一般，这里保守起见按照127MB，所以最多分配127MB/4KB = 32512页
  /************************** malloc_page的原理是三个动作的合成 *********
   * 1. 通过vaddr_get在虚拟内存池中申请虚拟地址
   * 2. 通过palloc在物理内存池中申请物理页
   * 3. 通过page_table_add将以上得到的虚拟地址和物理地址在页表中完成映射
   * ********************************************************************/
  void* vaddr_start = vaddr_get(pf,pg_cnt);
  if(vaddr_start == NULL){
    return NULL;
  }

  uint32_t vaddr = (uint32_t)vaddr_start,cnt = pg_cnt;
  struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

  /* 这里是因为虚拟地址是连续的，而物理地址不连续，所以逐个映射 */
  while(cnt-- >0){
    void* page_phyaddr = palloc(mem_pool);
    if(page_phyaddr == NULL){
      return NULL;
    }
    page_table_add((void*)vaddr, page_phyaddr);     //做映射
    vaddr+=PG_SIZE;         //下一个虚拟页
  }
  return vaddr_start;
}

/* 从内核物理内存池中申请1页内存 
 * 成功则返回其虚拟地址，失败则返回NULL*/
void* get_kernel_pages(uint32_t pg_cnt){
  void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
  if(vaddr != NULL){    //如果分配的地址不为空，则将页框清0后返回
    memset(vaddr, 0, pg_cnt * PG_SIZE);
  }
  return vaddr;
}

/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem){    //这里的all_mem传递的参数是总共的物理内存
  put_str("     mem_poool_init_start \n ");
  uint32_t page_table_size = PG_SIZE * 256;     //这里只计算769～1022是因为这一部分是属于内核进程
  //页表大小 = 1页的页目录表 + 第0项和第768个页目录项指向同一个页表 + 第769～1022个页目录项共指向254个页表，共256个页框
  uint32_t used_mem = page_table_size + 0x100000;   //0x100000为低端1MB内存
  uint32_t free_mem = all_mem - used_mem;
  uint16_t all_free_pages = free_mem/PG_SIZE;
  
  uint16_t kernel_free_pages = all_free_pages /2;
  uint16_t user_free_pages = all_free_pages - kernel_free_pages;
  /* 上面为了简化处理没有考虑余数，所以可能会丢失内存，但是这也不打紧，因为位图表示内存会小于物理内存 */
  uint32_t kbm_length = kernel_free_pages / 8;      //kernel_bitmap的长度，以字节为单位
  uint32_t ubm_length = user_free_pages / 8;        //user_bitmap的长度，以字节为单位

  uint32_t kp_start = used_mem;     //kernel_pool_start 内核物理内存池起始地址
  uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;   //user_pool_start 用户物理内存池起始地址

  kernel_pool.phy_addr_start = kp_start;
  user_pool.phy_addr_start = up_start;

  kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
  user_pool.pool_size = user_free_pages * PG_SIZE;

  kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
  user_pool.pool_bitmap.btmp_bytes_len = ubm_length;
  
/******************* 内核内存池和用户内存池位图 *******************
 * 位图是全局的数据，长度不固定
 * 全局或静态的数组需要在编译时知道其长度
 * 而我们需要根据总内存大小算出需要多少字节，
 * 所以改为指定一块来生成位图
 * ****************************************************************/
//内核使用的最高地址是0xc009f000,这里是主线程的栈地址，这里咱们内核占了物理地址的低1MB，但是大概率用不了这么多
//咱们有512MB的内存，所以位图就需要4页
//所以内核内存池的位图定在MEM_BITMAP_BASE(0xc009a000)这里
  kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
  //用户内存池的位图就当跟屁虫辣
  user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

/*********************** 输出内存池信息 ***************************/
  put_str("         kernel_pool_bitmap_start:");
  put_int((int)kernel_pool.pool_bitmap.bits);
  put_str(" kernel_pool_phy_addr_start:");
  put_int((int)kernel_pool.phy_addr_start);
  put_str("\n");
  put_str("         user_pool_bitmap_start:");
  put_int((int)user_pool.pool_bitmap.bits);
  put_str(" user_pool_phy_addr_start:");
  put_int((int)user_pool.phy_addr_start);
  put_str("\n");

  /* 将位图置为0 */
  bitmap_init(&kernel_pool.pool_bitmap);
  bitmap_init(&user_pool.pool_bitmap);
  
  /* 下面初始化内核虚拟地址的位图，按照实际物理内存大小生成数组 */
  kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;         //用于维护内核堆的虚拟地址，所以要和内核内存池大小一致

  /* 位图的数组指向一块未使用的内存
   * 目前定位在内核内存池和用户内存池之外*/
  kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
  kernel_vaddr.vaddr_start = K_HEAP_START;
  bitmap_init(&kernel_vaddr.vaddr_bitmap);
  put_str("     mem_pool_init done \n");
}

/* 内存管理部分初始化入口 */
void mem_init(){
  put_str("mem_init start\n");
  uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));     //这里是咱们之前loader.S中存放物理总内存的地址
  mem_pool_init(mem_bytes_total);
  put_str("mem_init done\n");
}


