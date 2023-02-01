## 0x00 基础知识们
今天我们来讲解一些关于用户进程的知识，首先我们需要了解一下TSS，这个在我们之前讲解特权级的时候曾经提到过，今天我们来对其进行详细的分析。
TSS是为了使得CPU支持多任务来实现的，这里我感觉类似于线程中的PCB，但是这个是基于进程，而PCB是线程自己也会拥有一个，所以这里切换进程也就是需要使用TSS来标记上下文等任务信息，而CPU也是用不同的TSS来区分不同的任务。
为了使用TSS，我们需要知道TSS其实也是一段数据，我们要访问他需要通过咱们的GDT，也就是全局描述符表，这个表写到现在涵盖的东西确实多，有LDT描述符，门，数据段，代码段，视频段，所以这里他才叫全局。咱们为了访问TSS所在的那个段，所以必定存在一个描述符来帮助我们，他同其他描述符结构类似，如下：
![](http://imgsrc.baidu.com/super/pic/item/e61190ef76c6a7efc728a3e0b8faaf51f2de66ed.jpg)
TSS描述符属于系统段，所以S位为0,在S为0的情况下，TYPE字段字段值为10B1,这里的B（Busy）位为1表示任务繁忙，为0表示空闲。
这里的任务繁忙有两种情况，一种是任务确实正在CPU上运行，另一种是该任务调用了新任务，而新任务正在CPU上运行，实现了嵌套。
而任务空闲也就是他不再CPU上运行，且他调用的新进程也是，此时TYPE字段为1001.且该B位是由硬件来设置的，跟我们没关系
B位的含义是保证任务不会自己调用自己，因为若正在执行的任务所调用的函数段的B位为1,则说明他在调用自己，或者在调用自己的调用者们。下面便是TSS的结构，这里的图在之前也放出来过
![](http://imgsrc.baidu.com/super/pic/item/f703738da9773912367d8ea7bd198618377ae299.jpg)
这里可以看出TSS本身自己就完全上下文的信息，其中包含了许多寄存器的备份，并且这里也包含了该任务所需要的栈地址，我提醒大家一点之前讲解的知识，那就是除了从中断和调用门返回之外，CPU不允许从高特权级转向为低特权级。所以这三组栈仅仅是CPU用来从低特权级跳到高特权级使用的，注意这三组栈地址在TSS中是不会改变的，也就是说不论你在哪个特权级进行了怎么样的压栈，当你从别的特权级返回到这个特权级的时候，他还是会从TSS中获取原始栈基址，而不管你曾经是否压过许多值。
Linux中只使用到了0级和3级特权级，我们的操作系统是仿linux的，所以这里我们也采用同样的作法，咱么也只设置SS0和esp0就行.
CPU本身是支持TSS的，这说明访问TSS以及识别他的结构过程并不是咱们需要做的工作。当任务被换下CPU的时候，CPU会自动将一些寄存器的值存入TSS相应位置，当任务上CPU运行的时候同样如此。
而我们本身是需要访问TSS的，所以说这里存在一个专门帮助我们寻找到TSS地址的寄存器TR，注意这里是帮助咱们寻找到TSS，而不是像GDTR那样专门有一部分位用来存放GDT首地址，前面咱们说过TSS是存在描述符的且存放在GDT中，所以我们访问他是跟访问其他普通段描述符一样，都是使用选择子，通过这个选择子我们就能够找到在GDT中的TSS段描述符，然后通过该描述符来找到咱们的TSS结构，下面给出TR结构和描述符缓冲器：
![](http://imgsrc.baidu.com/super/pic/item/77c6a7efce1b9d16f58feea7b6deb48f8d5464af.jpg)
这里说明一点LDT跟TSS也是同样的存储以及访问标志。
说完了访问规则，这里我再补充一点那就是最开始我们需要对TR进行初始化，这里当然跟其他基址寄存器类似不能使用mov等指令，这里使用的是
```
ltr "16位通用寄存器"或者"16位内存单元"
```

这里注意我们的地一个任务是咱们手动存储TSS描述符选择子，但是之后的任务切换时就不关咱们的事了，在切换任务后，CPU会将其对应的TSS中的寄存器值加载到对应的寄存器当中，然后将该任务的TSS描述符选择子自动加载到TR当中。下面再给出一个GDT,LDT,TSS的大郅关系图：
![](http://imgsrc.baidu.com/super/pic/item/503d269759ee3d6dfb7df30d06166d224e4adeb9.jpg)
CPU原生态所支持的本来是希望每个任务拥有一个TSS，然后任务切换通过中断门或者调用门进行切换，然后设置eflags寄存器的标志位来确保不会调用自身，这样有个十分严重的问题那就是效率太慢，因为每次你要进行从任务加载选择子到TR寄存器，这些切换工作就十分繁杂，如果大伙有兴趣了解一下CPU原生态支持的任务切换可以自行搜索。
事实上许多x86操作系统也并不采用原生态的方案，因此我们采用Linux的做法，也就是一个CPU就单独使用一个TSS，我们进行任务切换就是直接更新其中的内容，而不是说更新TR了。接下来我们直接开始实现。

## 0x01 初始化TSS
首先我们就往global.h中加入对应的TSS描述符字段，如下
```
#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H
#include "stdint.h"
typedef int bool;
#define true 1
#define false 0
#define NULL 0

/* ---------------- GDT描述符属性 ---------------- */
#define DESC_G_4K   1
#define DESC_D_32   1
#define DESC_L      0       //64位代码标记
#define DESC_AVL    0       //cpu不用此位
#define DESC_P      1
#define DESC_DPL_0  0
#define DESC_DPL_1  1
#define DESC_DPL_2  2
#define DESC_DPL_3  3
/***************************************************
 * 代码段和数据段属于存储段，tss和各种门属于系统段，
 * 所以这里的S位需要区分开来
 * *************************************************/
#define DESC_S_CODE     1
#define DESC_S_DATA     DESC_S_CODE
#define DESC_S_SYS      0
#define DESC_TYPE_CODE  8   //x=1,c=0,r=0,a=0代码段可执行、非依从、不可读，已访问位a清0
#define DESC_TYPE_DATA  2   //x=0,e=0,w=1,a=0数据段不可执行、向上扩展、可写，已访问位a清0
#define DESC_TYPE_TSS   9   //B位为0,不忙

/* ---------------- 选择子属性 ------------------- */
#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

#define TI_GDT 0
#define TI_LDT 1

#define SELECTOR_K_CODE ((1<<3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA ((2<<3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_STACK SELECTOR_K_DATA
#define SELECTOR_K_GS ((3<<3) + (TI_GDT << 2) + RPL0)
/* 第3个段描述符是显存，第4个是TSS */
#define SELECTOR_U_CODE ((5<<3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_DATA ((6<<3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_STACK SELECTOR_U_DATA

#define GDT_ATTR_HIGH ((DESC_G_4K << 7) + (DESC_D_32 << 6) + (DESC_L << 5) + (DESC_AVL << 4))
#define GDT_CODE_ATTR_LOW_DPL3 ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_CODE << 4) + DESC_TYPE_CODE)
#define GDT_DATA_ATTR_LOW_DPL3 ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_DATA << 4) + DESC_TYPE_DATA)


/* ----------------- TSS描述符属性-------------------- */
#define TSS_DESC_D  0
#define TSS_ATTR_HIGH ((DESC_G_4K << 7) + (TSS_DESC_D << 6) + (DESC_L << 5) + (DESC_AVL << 4) + 0x0)
#define TSS_ATTR_LOW ((DESC_P << 7) + (DESC_DPL_0 << 5) + (DESC_S_SYS << 4) + DESC_TYPE_TSS)
#define SELECTOR_TSS ((4<<3) + (TI_GDT << 2) + RPL0)

/* ------------ IDT描述符属性 -------------- */
#define IDT_DESC_P  1
#define IDT_DESC_DPL0 0
#define IDT_DESC_DPL3 3
#define IDT_DESC_32_TYPE    0xE     //32位的门
#define IDT_DESC_16_TYPE    0x6     //16位的门，不会用到
#define IDT_DESC_ATTR_DPL0 ((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)
#define IDT_DESC_ATTR_DPL3 ((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)

/* 定义GDT中的描述符的结构 */
struct gdt_desc{
  uint16_t limit_low_word;
  uint16_t base_low_word;
  uint8_t base_mid_byte;
  uint8_t attr_low_byte;
  uint8_t limit_high_attr_high;
  uint8_t base_high_byte;
};


#endif
```

以上就是我们最新的global.h，然后我们创建文件userprog/tss.c,其实质内容也仅仅是构造TSS描述符然后加到GDT中，然后重载gdtr和tr
```
#include "tss.h"
#include "thread.h"
#include "global.h"
#include "string.h"
#include "print.h"
/* 任务状态段tss结构 */
struct tss{
  uint32_t backlink;
  uint32_t* esp0;
  uint32_t ss0;
  uint32_t* esp1;
  uint32_t ss1;
  uint32_t* esp2;
  uint32_t ss2;
  uint32_t cr3;
  uint32_t (*eip) (void);
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint32_t es;
  uint32_t cs;
  uint32_t ss;
  uint32_t ds;
  uint32_t fs;
  uint32_t gs;
  uint32_t ldt;
  uint32_t trace;
  uint32_t io_base;
};
static struct tss tss;

/* 更新tss中esp0字段的值为pthread的0级栈 */
void update_tss_esp(struct task_struct* pthread){
  tss.esp0 = (uint32_t*)((uint32_t)pthread + PG_SIZE);
}

/* 创建gdt描述符 */
static struct gdt_desc make_gdt_desc(uint32_t* desc_addr, uint32_t limit, uint8_t attr_low, uint8_t attr_high){
  uint32_t desc_base = (uint32_t)desc_addr;
  struct gdt_desc desc;
  desc.limit_low_word = limit & 0x0000ffff;
  desc.base_low_word = desc_base & 0x0000ffff;
  desc.base_mid_byte = ((desc_base & 0x00ff0000) >> 16);
  desc.attr_low_byte = (uint8_t)(attr_low);
  desc.limit_high_attr_high = ((limit & 0x000f0000)>>16) + (uint8_t)(attr_high);
  desc.base_high_byte = desc_base >> 24;
  return desc;
}

/* 在gdt中创建tss并重新加载gdt */
void tss_init(){
  put_str("tss_init start\n");
  uint32_t tss_size = sizeof(tss);
  memset(&tss, 0, tss_size);
  tss.ss0 = SELECTOR_K_STACK;
  tss.io_base = tss_size;       //io位图的偏移地址大于或等于TSS大小，这样设置表示没有IO位图
  /* gdt段基址是0x900,我们将tss放到第4个位置，也就是0x900 + 0x20 */
  /* 在gdt当中添加dpl为0的TSS描述符 */
  *((struct gdt_desc*)0xc0000920) = make_gdt_desc((uint32_t*)&tss, tss_size - 1, TSS_ATTR_LOW, TSS_ATTR_HIGH);
  /* 在gdt当中添加dpl为3的代码段和数据段描述符 */
  *((struct gdt_desc*)0xc0000928) = make_gdt_desc((uint32_t*)0, 0xfffff, GDT_CODE_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
  *((struct gdt_desc*)0xc0000930) = make_gdt_desc((uint32_t*)0, 0xfffff, GDT_DATA_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
  /* gdt中16位的limit 32位的段基址 */
  uint64_t gdt_operand = ((8*7-1) | ((uint64_t)(uint32_t)0xc0000900 << 16));    //7个描述符大小
  asm volatile ("lgdt %0" : : "m"(gdt_operand));
  asm volatile ("ltr %w0" : : "r"(SELECTOR_TSS));
  put_str("tss_init and ltr done\n");
}

```

最后我们再来看看实现成果，我们在bochs的调试界面使用info gdt来查看GDT内容，发现确实增加了三个描述符，他们分别是TSS描述符、用户代码段描述符和用户数据段描述符
![](http://imgsrc.baidu.com/super/pic/item/4034970a304e251fbca6983ee286c9177e3e53a6.jpg)

## 0x02 实现用户进程
上回我们构建了初始的TSS，现在我们来了解一下用户进程的知识。
我们的进程是基于线程来实现的,所以我们的进程同样需要PCB，这里我们需要到thread.h中修改一下我们PCB的内容，其中在task_struct结构体中添加一个咱们用户进程的虚拟地址池。
```
  struct virtual_addr userprog_vaddr;   //用户进程的虚拟地址
```

由于咱们创建的用户进程大多是时间是在特权级3工作，所以我们还必须为他构建出一个3级特权栈，并且不同的进程有着不同的页表，所以我们需要为他分配一些内存，这里我们到memory.c中新增一些代码：
```
/* 在用户空间申请4K内存，并返回其虚拟地址 */
void* get_user_pages(uint32_t pg_cnt){
  lock_acquire(&user_pool.lock);
  void* vaddr = malloc_page(PF_USER, pg_cnt);
  memset(vaddr, 0, pg_cnt * PG_SIZE);
  lock_release(&user_pool.lock);
  return vaddr;
}

/* 将地址vaddr与pf池中的物理地址关联，仅支持一页空间分配,这里是咱们自己选择一块虚拟地址进行分配 */
void* get_a_page(enum pool_flags pf, uint32_t vaddr){
  struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
  lock_acquire(&mem_pool->lock);

  /* 先将虚拟地址对应的位图置1 */
  struct task_struct* cur = running_thread();
  int32_t bit_idx = -1;
  /* 若当前是用户进程申请用户内存，就修改用户进程自己的虚拟地址位图 */
  if(cur->pgdir != NULL && pf == PF_USER){
    bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start)/PG_SIZE;
    ASSERT(bit_idx > 0);
    bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
  }else if(cur->pgdir == NULL && pf == PF_KERNEL){
    /* 如果当前是内核线程申请内核内存，则修改kernel_vaddr */
    bit_idx = (vaddr - kernel_vaddr.vaddr_start)/PG_SIZE;
    ASSERT(bit_idx > 0);
    bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
  }else{
    PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
  }

  void* page_phyaddr = palloc(mem_pool);
  if(page_phyaddr == NULL){
    return NULL;
  }
  page_table_add((void*)vaddr, page_phyaddr);
  lock_release(&mem_pool->lock);
  return (void*)vaddr;
}

/* 得到虚拟地址映射到的物理地址 */
uint32_t addr_v2p(uint32_t vaddr){
  uint32_t* pte = pte_ptr(vaddr);
  /* (*pte)的值是页表所在的物理页框的地址，
   * 去掉其低12位的页表项属性 + 虚拟地址vaddr的低12位*/
  return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

```

这里我贴上了几个本次实现的关键函数，大家之前一起写过memory.c的会感觉到并不是很难，完整源码还是放github上了，放这里太长。
现在我们已经添加了一些实现用户内存池的管理函数，现在我们需要实现的就是让咱们的处理器从特权级0降级到特权级3,这里我们之前强调过很多次从高特权级到低特权级只有一种方法那就是iretd指令。
大家知道从中断返回的时候会使用iretd指令，而他会将栈中的数据作为返回地址,所以咱们就需要伪装成中断返回的过程，然后就可以实现高到低了。
当我们中断返回之后CPU是如何知道要进入哪个特权级呢，那就是看弹出的CS中选择子的RPL，所以说我们在栈上面先伪造一系列寄存器的值，这里注意要保证其中对应的CS选择子的RPL需要为3,然后使用iretd指令弹出相应的寄存器的值，这样CPU就会以为你是中断返回然后查看CS的选择子RPL发现你是低特权级，然后就将该RPL设置为CPL。而任务之所以能进入中断，那是因为eflags寄存器中的IF位为1,退出“中断”后，还需要保持他为1继续响应中断。还有一点就是既然我们已经到达了特权级3,此时我们只能访问特权级3的代码段和数据段，所以我们的段寄存器的选择子必须指向DPL为3的内存段，这些段我们刚刚在上面已经实现了。
最后一点，那就是用户是最低特权级的存在，所以不允许用户直接访问硬件，因此eflags中的IOPL位必须为0。

## 0x03 进程切换处理
废话不必多说，我们立刻开始实现，先提一嘴，那就是咱们的进程是基于线程的，所以我们需要先创建线程，然后通过该线程创建进程,首先我们先来添加一点eflags寄存器的标志，他是添加在global.h中
```
efine EFLAGS_MBS  (1<<1)  //此项必须设置
#define EFLAGS_IF_1     (1<<9)  //if为1,开中断
#define EFLAGS_IF_0     0   //if为0,关中断
#define EFLAGS_IOPL_3   (3<<12)     //IOPL3,用于测试用户在非系统调用下进行IO
#define EFLAGS_IOPL_0   (0<<12)     //IOPL0
#define DIV_ROUND_UP(X, STEP) ((X + STEP - 1) / (STEP))
```

然后就是咱们的用户进程的创建等函数
```
#include "process.h"
#include "debug.h"
#include "global.h"
#include "thread.h"
#include "memory.h"
#include "console.h"
#include "bitmap.h"

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
  pagedir_phy_addr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1; //这里是补充页目录项的标识
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


```

其中的start_process(filename_)就是我们想要最后运行的程序，这里我们首先将栈抬高到intr_stack,这里大伙是否还记得我们在线程初始化的时候首先构造了两个栈，一个是intr_stack，用来存放从本线程到中断例程的环境，还有一个是thread_stack，他是用来存放中断处理程序过程中任务切换的上下文信息.
上面的代码注释已经很清楚了，但这里我来给大家梳理一下进程创建的过程，首先咱们运行一个线程，这里大家都清楚。
1, 线程调用process_execute，首先重新获取一个PCB，然后对其初始化，再调用create_user_vaddr_bitmap来创建用户位图，之后通过thread_create来处理一些进程的栈，然后将kernel_thread最终指向start_process(filename)函数，再获取一个内核页目录，这个页目录会作为用户单独的页目录，这里注意每个用户页目录项都是不同的，但是每个用户进程都共享同一个内核页目录，如下：
![](http://imgsrc.baidu.com/super/pic/item/d000baa1cd11728b68b1ebe48dfcc3cec2fd2c53.jpg)
所以我们在内核空间创建一个单独的用户页目录表，然后在其中通过复制内核页目录表对应的内核页目录项来实现各进程共享，最后再设置咱们进程的pgdir以此证明他是个进程而不是线程（线程的该值为NULL）。
在我们初始化完成之后再将其加入就绪队列和全部队列，然后处理器就可以调用进程执行了。

---
当我们已经准备好了进程，现在是时候对他进行调度了，所以我们需要修改一下thread.c中的调度器schedule()函数：
```
  /* 激活任务页表等 */
  process_activate(next);

```
只需要在咱们的switch_to函数前面加上这段激活代码即可，当我们查看该代码的时候我们会发现他的功能就是将相应的页目录表载入cr3,然后转到对应的特权级0的栈，下面我们就来测试一下用户进程,下面咱们修改main函数测试
```
#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0, test_var_b = 0;

int main(void){
  put_str("I am Kernel\n");
  init_all();
  thread_start("k_thread_a", 31, k_thread_a, " A_");
  thread_start("k_thread_b", 31, k_thread_b, " B_");
  process_execute(u_prog_a, "user_prog_a");
  process_execute(u_prog_b, "user_prog_b");
  intr_enable();
  while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  while(1){
    console_put_str("v_a:0x");
    console_put_int(test_var_a);
    console_put_str("   ");
  }
}
void k_thread_b(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  while(1){
    console_put_str("v_b:0x");
    console_put_int(test_var_b);
    console_put_str("   ");
  }
}

void u_prog_a(void){
  while(1){
    test_var_a++ ;
  }
}

void u_prog_b(void){
  while(1){
    test_var_b++ ;
  }
}

```

这里我们分别创建了两个线程函数和两个进程函数，这里创建线程是因为我们的用户进程现在无法使用显存，因为权限不够，所以这里我们的用户进程暂时只是将全局变量增加，然后使用内核线程进行输出，我们来看看效果
![](http://imgsrc.baidu.com/super/pic/item/f703738da9773912dad6eaa7bd198618377ae200.jpg)
这里我们可以看到确实是ab两个线程分别输出，且对应数字都在增加则表示咱们的两个进程也都正常切换了，这里进程的切换其实同线程十分类似，都是经过时钟中断然后执行调度函数schedule(),再通过激活对应线程或者进程的页表，如果是进程，他的PCB中pgdir不会为NULL,然后进行切换，否则就说明是线程，则没必要切换页表，然后就会跳转到对应的intr_exit函数进行还原中断前的状态。当我们第一次调用用户进程的时候，我们会执行其中的start_process(function)功能，此时我们在这里伪造了一系列值，其中最重要的莫过于cs了，其中选择子的RPL必须为特权级3,这样我们再返回intr_exit函数就可以实现高特权级到地特权及的转化，而在之后的切换我们本身的RPL就是3,所以不需要多余改变。

## 0x04 总结
大致来看进程线程区别不大，但是我们的进程是拥有了自己的页表，并且每个进程拥有独立的用户空间，共享同一个内核页表。这里tss中字段的使用倒没有很多，使用tss.c中的函数用来创建tss描述符和创建一些用户段。进程切换的时候调用其中的函数来获得其0级特权栈。代码易懂但是得细心琢磨，通过本次的学习对线程和进程之间的关系可以认识的更加透彻，包括以往的应试答案比如说“进程是资源分配的单位，线程是处理器分配的单位”等，综上，本次知识十分有用。

