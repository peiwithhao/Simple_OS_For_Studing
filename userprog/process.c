#include "process.h"
#include "debug.h"
#include "global.h"
#include "thread.h"
#include "memory.h"
#include "console.h"
#include "bitmap.h"
#include "interrupt.h"
#include "tss.h"
#include "string.h"
#include "list.h"

extern void intr_exit(void);    //kernel.S中的中断返回函数

/* 构建用户进程初始上下文信息,伪造中断返回的假象 */
void start_process(void* filename_){
  void* function = filename_;
  struct task_struct* cur = running_thread();
  cur->self_kstack += sizeof(struct thread_stack);      //使得其指向中断栈
  struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;
  proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
  proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
  proc_stack->gs = 0;                                   //不允许用户进程直接访问显存段，所以置0
  proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;
  proc_stack->eip = function;
  proc_stack->cs = SELECTOR_U_CODE;
  proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
  proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE); //分配的是用户栈的最高地址处，也就是0xc0000000
  proc_stack->ss = SELECTOR_U_DATA;
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g"(proc_stack) : "memory");
}

/* 激活页表 */
void page_dir_activate(struct task_struct* p_thread){
  /******************************************************
   * 执行此函数时当前任务可能是线程 
   * 之所以对线程也要重新安装页表，原因是上一次被调度的可能是进程，
   * 否则不恢复页表的话，线程就会使用进程的页表了 
   * ****************************************************/
  /* 若为内核线程，需要重新填充页表为0x100000 */
  uint32_t pagedir_phy_addr = 0x100000;      //默认为内核的页目录物理地址，也就是内核线程所用的页目录表
  if(p_thread->pgdir != NULL){              //用户态进程有自己的页目录表
    pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
  }
  /* 更新页目录寄存器cr3，使页表生效 */
  asm volatile ("movl %0, %%cr3" : : "r"(pagedir_phy_addr) : "memory");
}

/* 激活线程或进程的页表，更新tss中的esp0为进程的特权级0的栈 */
void process_activate(struct task_struct* p_thread){
  ASSERT(p_thread != NULL);
  /* 激活该进程或线程的页表 */
  page_dir_activate(p_thread);
  /* 内核线程特权级本身为0,处理器进入中断时并不会从tss中获取0特权级栈地址，因此不需要更新esp0 */
  if(p_thread->pgdir){
    /* 更新该进程的esp0, 用于此进程被中断时保护上下文 */
    update_tss_esp(p_thread);
  }
}

/* 创建页目录表，将当前页表的表示内核空间的pde复制，
 * 若成功则返回页目录的虚拟地址，否则返回-1 */
uint32_t* create_page_dir(void){
  /* 用户进程的页表不能让用户直接访问到，所以在内核空间来申请 */
  uint32_t* page_dir_vaddr = get_kernel_pages(1);
  if(page_dir_vaddr == NULL){
    console_put_str("create_page_dir : get_kernel_page failed!");
    return NULL;
  }
  /**************************** 1 先复制页表 ******************************/
  /* page_dir_vaddr + 0x300*4是内核页目录的第768项 */
  memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300*4),(uint32_t*)(0xfffff000 + 0x300*4), 1024);  //所有用户进程共享这1GB的内核空间，所以咱们直接复制内核页表
  /************************************************************************/
  /*************************** 2 更新页目录地址 *******************************/
  uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);      //咱们创建的页表仍在内核当中，所以这里我们不需要关心映射问题
  /* 页目录地址是存入在页目录的最后一项，更新页目录地址为新页目录的物理地址 */
  page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1; //这里是补充页目录项的标识
  /****************************************************************************/
  return page_dir_vaddr;
}

/* 创建用户进程虚拟地址位图 */
void create_user_vaddr_bitmap(struct task_struct* user_prog){
  user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;     //咱定义为0x804800
  uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START)/PG_SIZE/8, PG_SIZE);    //这里是计算得到位图所需要的最小页面数
  user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);         //用户位图同样存放在内核空间
  user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START)/PG_SIZE/8;
  bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);         //初始化用户位图
}

/* 创建用户进程 */
void process_execute(void* filename, char* name){
  /* pcb内核的数据结构，由内核来维护进程信息，因此要在内核内存池中申请 */
  struct task_struct* thread = get_kernel_pages(1);             //获取PCB空间
  init_thread(thread, name, default_prio);                      //初始化我们创造的PCB空间
  create_user_vaddr_bitmap(thread);                             //构建位图，并且写入咱们的PCB
  thread_create(thread, start_process, filename);               //这里预留出中断栈和线程栈，然后将还原后的eip指针指向start_process(filename);
  thread->pgdir = create_page_dir();                            //新建用户页目录并且返回页目录首地址

  enum intr_status old_status = intr_disable();
  ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
  list_append(&thread_ready_list, &thread->general_tag);
  ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
  list_append(&thread_all_list, &thread->all_list_tag);
  intr_set_status(old_status);
}
