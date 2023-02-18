## 0x00 fork基础
### 1.fork基本概念
fork直译为叉子，这里咱们可以看作分支，当我们在C语言中进行编程的时候有时会使用fork函数来创建子进程，这里有一点那就是创建的过程是整个进程全部复制，当然程序执行到的指令数也会复制过来，也就是说当我们执行了fork函数创建子进程之后，父进程和子进程均会按照接下来的指令运行，而咱们平时的通过pid来判断父子进程是因为如果你是子进程，上面的fork函数会返回0,若是父进程则会返回自身的pid，如果fork失败，则会返回-1.
总结：用于创建一个进程，所创建的进程复制父进程的代码段/数据段/BSS段/堆/栈等所有用户空间信息；在内核中操作系统重新为其申请了一个PCB，并使用父进程的PCB进行初始化。

### 2.fork的实现
由于fork是复制了父进程的信息创建新进程，所以我们需要先复制进程资源，然后转过去执行，下面是咱们需要分配的基本资源：
1. PCB
2. 用户栈
3. 内核栈
4. 虚拟地址池
5. 页表

而子进程想要执行，我们只需要将其pcb放入就绪队列就行。我们还需要到`task_struct`中分配一些基础信息，如下：
```
int16_t parent_pid;
```
然后我们到进程初始化函数中添加他的默认值为-1,表示没有父进程。我们再到thread/thread.c中添加一个供外部调用的分配pid的函数
```
pid_t fork_pid(){
  return allocate_pid();
}
```

然后我们再到memory.c中增加个函数：
```
/* 安装1页大小的vaddr， 专门针对fork时虚拟地址位图无需操作的情况 */
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr){
  struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
  lock_acquire(&mem_pool->lock);
  void* page_phyaddr = palloc(mem_pool);
  if(page_phyaddr == NULL){
    lock_release(&mem_pool->lock);
    return NULL;
  }
  page_table_add((void*)vaddr, page_phyaddr);
  lock_release(&mem_pool->lock);
  return (void*)vaddr;
}

```

这个函数功能同`get_a_page`类似，区别是他不会修改虚拟位图
紧接着我们就开始正式编写fork功能，我们创建文件userprog/fork.c
```
#include "fork.h"
#include "thread.h"
#include "global.h"
#include "memory.h"
#include "interrupt.h"
#include "file.h"
#include "string.h"
#include "debug.h"
#include "process.h"

extern void intr_exit(void);

/* 将父进程的pcb拷贝给子进程 */
static int32_t copy_pcb_vaddrbitmap_stack0(struct task_struct* child_thread, struct task_struct* parent_thread){
  /* a 复制pcb所在的整个页，里面包含进程信息和0级特权栈，里面包含返回地址 */
  memcpy(child_thread, parent_thread, PG_SIZE);
  /* 这里再单独修改子线程 */
  child_thread->pid = fork_pid();
  child_thread->elapsed_ticks = 0;  //运行时间
  child_thread->status = TASK_READY;
  child_thread->ticks = child_thread->priority;     //时间片加满
  child_thread->parent_pid = parent_thread->pid;
  child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
  child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;
  block_desc_init(child_thread->u_block_desc);  //内存块描述符，也就是malloc的地方
  /* b 复制父进程的虚拟地址池位图 */
  uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START)/PG_SIZE / 8, PG_SIZE);  //获取用户虚拟地址池所需要的页数
  void* vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
  /* 此时子进程的虚拟用户位图仍指向父进程的，所以这里我们复制过来先 */
  memcpy(vaddr_btmp, child_thread->userprog_vaddr.vaddr_bitmap.bits, bitmap_pg_cnt * PG_SIZE);
  /* 然后将子进程的虚拟用户位图的地址指向新分配的位图 */
  child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_btmp;
  /* 调试用 */
  ASSERT(strlen(child_thread->name) < 11);  //pcb.name 的长度是16,这里是为了避免下面strcat越界
  strcat(child_thread->name, "_fork");
  return 0;
}

/* 复制子进程的进程体（代码和数据）以及用户栈 */
static void copy_body_stack3(struct task_struct* child_thread, struct task_struct* parent_thread, void* buf_page){
  uint8_t* vaddr_btmp = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
  uint32_t btmp_bytes_len = parent_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len;  //获取位图字节数
  uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
  uint32_t idx_byte = 0;
  uint32_t idx_bit = 0;
  uint32_t prog_vaddr = 0;
  /* 在父进程的用户空间中查找已有数据的页 */
  while(idx_byte < btmp_bytes_len){
    if(vaddr_btmp[idx_byte]){
      while(idx_bit < 8){
        if((BITMAP_MASK << idx_bit) & vaddr_btmp[idx_byte]){
          prog_vaddr = (idx_byte * 8 + idx_bit) * PG_SIZE + vaddr_start;
          /* 将父进程用户空间中的数据通过内核空间做中转，然后复制到子进程的用户空间 */
          /* a 将父进程在用户空间中的数据复制到内核缓冲区buf_page, 目的是下面切换到子进程的页表后，还能访问父进程的数据 */
          memcpy(buf_page, (void*)prog_vaddr, PG_SIZE);
          /* b 将页表切换到子进程，目的是避免下面申请内存的函数将pte和pde */
          page_dir_activate(child_thread);
          /* c 申请虚拟地址prog_vaddr */
          get_a_page_without_opvaddrbitmap(PF_USER, prog_vaddr);
          /* d 从内核缓冲区将父进程数据复制到子进程的用户空间 */
          memcpy((void*)prog_vaddr, buf_page, PG_SIZE);
          /* e 恢复父进程页表 */
          page_dir_activate(parent_thread);
        }
        idx_bit++;
      }
    }
    idx_byte++;
  }
}

/* 为子进程构建thread_stack和修改返回值 */
static int32_t build_child_stack(struct task_struct* child_thread){
  /* a 使子进程pid返回值为0 */
  /* 获取子进程0级栈栈顶 */
  struct intr_stack* intr_0_stack = (struct intr_stack*)((uint32_t)child_thread + PG_SIZE - sizeof(struct intr_stack));
  /* 修改子进程的返回值为0 */
  intr_0_stack->eax = 0;
  /* b 为switch_to构建struct thread_stack */
  uint32_t* ret_addr_in_thread_stack = (uint32_t*)intr_0_stack - 1;
  /**************************************************************/
  uint32_t* esi_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 2;
  uint32_t* edi_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 3;
  uint32_t* ebx_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 4;
  /**************************************************************/

  /* ebp在thread_stack中的地址便是当时的esp(0级栈地址)
   * 也就是esp为"(uint32_t*)intr_0_stack - 5;" */
  uint32_t* ebp_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 5;
  /* switch_to的返回地址更新为intr_exit，直接从中断返回 */
  *ret_addr_in_thread_stack = (uint32_t)intr_exit;
  /* 下面两行是使得构建的thread_stack更加清晰 */
  *ebp_ptr_in_thread_stack = *ebx_ptr_in_thread_stack = *edi_ptr_in_thread_stack = *esi_ptr_in_thread_stack = 0;
  /***************************************************************/
  /* 把构建的thread_stack的栈顶作为switch_to恢复数据时的栈顶 */
  child_thread->self_kstack = ebp_ptr_in_thread_stack;
  return 0;
}

/* 更新inode打开数 */
static void update_inode_open_cnts(struct task_struct* thread){
  int32_t local_fd = 3, global_fd = 0;
  while(local_fd < MAX_FILES_OPEN_PER_PROC){
    global_fd = thread->fd_table[local_fd];
    ASSERT(global_fd < MAX_FILE_OPEN);
    if(global_fd != -1){
      file_table[global_fd].fd_inode->i_open_cnts++;
    }
    local_fd++;
  }
}

/* 拷贝父进程本身资源给子进程 */
static int32_t copy_process(struct task_struct* child_thread, struct task_struct* parent_thread){
  /* 同样内核缓冲区作为中转站 */
  void* buf_page = get_kernel_pages(1);
  if(buf_page == NULL){
    return -1;
  }

  /* a 复制父进程的pcb、虚拟地址位图、内核栈到子进程 */
  if(copy_pcb_vaddrbitmap_stack0(child_thread, parent_thread) == -1){
    return -1;
  }

  /* b 为子进程创建页表，此页表仅仅包括内核空间 */
  child_thread->pgdir = create_page_dir();
  if(child_thread->pgdir == NULL){
    return -1;
  }

  /* c 复制父进程的进程体及用户栈给子进程 */
  copy_body_stack3(child_thread, parent_thread, buf_page);

  /* d 构建子进程thread_stack,这里也就是完善子进程0级特权栈，使得其可以返回0 */
  build_child_stack(child_thread);

  /* e 更新文件inode的打开数 */
  update_inode_open_cnts(child_thread);

  mfree_page(PF_KERNEL, buf_page, 1);
  return 0;
}

/* fork子进程，内核线程不可直接调用 */
pid_t sys_fork(void){
  struct task_struct* parent_thread = running_thread();
  struct task_struct* child_thread = get_kernel_pages(1);   //为子进程创建pcb
  if(child_thread == NULL){
    return -1;
  }
  ASSERT(INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL);
  if(copy_process(child_thread, parent_thread) == -1){
    return -1;
  }

  /* 添加爱到就绪线程队列和所有线程队列，子进程调度由调度器安排运行 */
  ASSERT(!elem_find(&thread_ready_list, &child_thread->general_tag));
  list_append(&thread_ready_list, &child_thread->general_tag);
  ASSERT(!elem_find(&thread_all_list, &child_thread->all_list_tag));
  list_append(&thread_all_list, &child_thread->all_list_tag);
  return child_thread->pid;     //父进程就返回子进程的pid
}

```

上面我们实现了将父进程整个拷贝到子进程，其中也添加了一点子进程独有的东西，如果上述代码看懂，大伙就会知道为什么子进程调用fork函数后会返回0,而父进程是返回子进程pid了，这里我提示一下那就是构造中断返回栈。


### 3.添加fork系统调用与实现init进程
在Linux当中，init是用户级进程，它是第一个启动的程序，因此他的pid为1,而后续所有的进程都是他的子进程，换句话来讲init是所有进程的父进程，所以他还负责所有子进程的资源回收。所以这里我们需要实现系统调用fork来使得他能够构造子进程。
下面我们回忆一下咱们以前添加系统调用的步骤：
1. 在syscall.h当中的枚举结构中添加对应值，比如我们要实现的fork，就是添加`SYS_FORK`
2. 在syscall.c当中添加fork()函数
3. 在`syscall-init.c`中的函数`syscall_init`当中添加系统调用表的函数地址值，也就是咱们的`sys_fork`函数

下面我们就来写init函数，也是init进程，注意需要写到kernel/main.c当中
```
/* init进程 */
void init(void){
  uint32_t ret_pid = fork();
  if(ret_pid){
    printf(" i am father, my pid is %d, child pid is %d\n", getpid(), ret_pid);
  }else{
    printf(" i am child, my pid is %d, ret pid is %d\n", getpid(), ret_pid);
  }
  while(1);
}

```

我们的pid是从1开始分配的，而init的pid需要为1,但是目前来说我么主线程的pid是1,idle线程的pid是2,所以我们需要在`make_main_thread`函数执行之前创建init，同样是在thread_init函数之中添加
这里可以看到我们已经成功分配了init进程了
![]()

## 0x01 添加read系统调用
我们现在的目标是同系统进行交互，那么我们肯定需要知道用户键入了什么命令，所以我们先从键盘中获取输入，所以我们迫切的需要添加read的系统调用，之前我们已经实现了sys_read函数，但是他只能从文件上面读取信息，还不能从标准输入设备获取数据，因此我们来修改一下sys_read函数
```
/* 从文件描述符fd指向的文件中读取count个字节到buf，若成功则返回读出的字节数，失败则返回-1 */
int32_t sys_read(int32_t fd, void* buf, uint32_t count){
  ASSERT(buf != NULL);
  int32_t ret = -1;
  if(fd < 0 || fd == stdout_no || fd == stderr_no){
    printk("sys_read: fd error\n");
    return -1;
  }else if(fd == stdin_no){
    char* buffer = buf;
    uint32_t bytes_read = 0;
    while(bytes_read < count){
      *buffer = ioq_getchar(&kbd_buf);          //这里的kbd_buf是之前我们实现键盘输入的共用缓冲区
      bytes_read++;
      buffer++;
    }
    ret = (bytes_read == 0 ? -1 : (int32_t)bytes_read);
  }else{
  uint32_t _fd = fd_local2global(fd);
  return file_read(&file_table[_fd], buf, count);
  }
}

```

完善好`sys_read`之后我们再按照正常步骤添加系统调用即可,至于测试等我们接下来完成一些别的系统调用后再一同测试

## 0x02 添加putchar、clear系统调用
我们本节的任务依然很简单，那就是实现单字符输出系统调用和清屏系统调用，我们输出字符早已有了内核实现，也就是`console_put_char`,所以系统调用实现起来十分简单，而至于清屏我们并没有对其有内核实现，所以这里我们仍旧采用汇编的方式。
我们将其定义在print.S当中，如下：
```
global cls_screen
cls_screen:
  pushad
  ;;;;;;;;;;;;;;;;
  ;由于用户程序的cpl为3,显存段的dpl为0,所以这里用于显存段的选择子gs在低于自己特权的环境中为0
  ;导致用户程序再次进入中断后，gs为0,因此直接在put_str中每次都为gs赋值
  mov ax, SELECTOR_VIDEO    ;不能直接把立即数送入gs，须要由ax中转
  mov gs, ax

  mov ebx, 0
  mov ecx, 80*25
  .cls:
    mov word [gs:ebx], 0x0720   ;0x0720是黑底白字的空格建
    add ebx, 2
    loop .cls
    mov ebx, 0

.set_cursor:
;将光标设为bx值，这里和之前获取类似
;;;;;;;; 先设置高8位 ;;;;;;;;
  mov dx, 0x03d4
  mov al, 0x0e
  out dx, al
  mov dx, 0x03d5
  mov al, bh
  out dx, al

;;;;;;;; 再设置低8位 ;;;;;;;;
  mov dx, 0x03d4
  mov al, 0x0f
  out dx, al
  mov dx, 0x03d5
  mov al, bl
  out dx, al
  popad
  ret

```

好久不见汇编代码，还是挺怀念的，这里比较简单也就是设置显存然后更新光标位置，然后我们再到外面套个壳就行
```
/* 系统调用putchar,输出一个字符 */
void putchar(char char_asci){
  _syscall1(SYS_PUTCHAR, char_asci);
}

/* 清空屏幕 */
void clear(void){
  _syscall0(SYS_CLEAR);
}
```

然后到syscall-init.c中增加映射即可

## 0x03 实现shell
### 1.显示提示符以及输入
简单来讲，咱们的shell的功能就是获取用户命令然后执行，真正的shell是十分复杂的，我们这里就单单实现一个比较简单的版本，我们创建文件shell/shell.c
```
#include "shell.h"
#include "stdint.h"
#include "assert.h"
#include "fs.h"
#include "file.h"
#include "stdio.h"
#include "global.h"
#include "syscall.h"
#include "string.h"

#define cmd_len 128     //最大支持键入128个字符的命令行输入
#define MAX_ARG_NR 16   //加上命令外，最多支持15个参数

/* 存储输入的命令 */
static char cmd_line[cmd_len] = {0};

/* 用来记录当前目录，是当前目录的缓存，每次执行cd命令会更新他 */
char cwd_cache[64] = {0};

/* 输出提示符 */
void print_prompt(void){
  printf("[peiwithhao@localhost %s]$ ", cwd_cache);
}

/* 从键盘缓冲区最多读入count个字节到buf */
static void readline(char* buf, int32_t count){
  assert(buf != NULL && count > 0);
  char* pos = buf;
  while(read(stdin_no, pos, 1) != -1 && (pos - buf) < count){
    switch(*pos){
      case '\n':
      case '\r':
        *pos = 0;   //添加cmd_line的终止字符0
        putchar('\n');
        return ;
      case '\b':
        if(buf[0] != '\b'){     //阻止删除非本次输入的信息
          --pos;                //退回到缓冲区cmd_line的山一个字节
          putchar('\b');
        }
        break;
      /* 非控制键则输出字符 */
      default:
        putchar(*pos);
        pos++;
    }
  }
  printf("reawdline: can't find enter_key in the cmd_line, max num of char is 128\n");
}

/* 简单的shell */
void my_shell(void){
  cwd_cache[0] = '/';
  while(1){
    print_prompt();
    memset(cmd_line, 0, cmd_len);
    readline(cmd_line, cmd_len);
    if(cmd_line[0] == 0){ //如果只输入了一个回车
      continue;
    }
  }
  panic("my_shell: should not be here");
}

```

大致意思就是从键盘缓冲区读取字符然后输出到屏幕且存入buf当中，然后我们通过init来调用就行了
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  cls_screen();
  console_put_str("[peiwithhao@localhost /]$ ");
  while(1);
  return 0;
}

/* init进程 */
void init(void){
  uint32_t ret_pid = fork();
  if(ret_pid){  //父进程
    while(1);
  }else{    //子进程
    my_shell();
  }
  panic("init: should not be here");
}

```

紧接着我们立刻进行测验
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  cls_screen();
  console_put_str("[peiwithhao@localhost /]$ ");
  while(1);
  return 0;
}

/* init进程 */
void init(void){
  uint32_t ret_pid = fork();
  if(ret_pid){  //父进程
    while(1);
  }else{    //子进程
    my_shell();
  }
  panic("init: should not be here");
}

```

这里我们的init进程会创建子进程来运行shell函数，结果如下
![]()


### 2.添加ctrl+u 和 ctrl+l 快捷键
我们在平时使用linux的时候，经常会使用快捷键来避免咱们频繁输入命令，接下来我们需要使用的命令就是Ctrl+u 和 Ctrl+l的组合快捷键，他们的功能分别是清除输入和清屏，也就是等同于Clear指令
我们首先的操作需要先在keyboard.c当中添加一些对于这两个快捷键的特殊处理，如果你要添加也是类似
```
    if(cur_char){
      /********************* 快捷键ctrl+l和ctrl+u的处理 **********************
       * cur_char的asc码-字符a的asc码，此差值比较小，
       * 属于asc码表当中不可见字符的部分，故不会产生可见字符
       * 我们在shell当中将ascii值为l-a和u-a的分别处理为清屏和删除输入的快捷键 */
      if((ctrl_down_last && cur_char == 'l') || (ctrl_down_last && cur_char == 'u')){
        cur_char -= 'a';
      }
      /***********************************************************************/

      /* 若kbd_buf中未满且待加入的cur_char不为0,
       * 则将其加入到缓冲区kbd_buf当中 */
      if(!ioq_full(&kbd_buf)){
        ioq_putchar(&kbd_buf, cur_char);
      }
      return;
    }

```

我们就到处理cur_char的地方添加这一个判断即可，然后剩余的工作交给咱们上层函数来办，总不能什么都交给底层驱动吧。接下来我们到shell.c当中实现判断
```
/* 从键盘缓冲区最多读入count个字节到buf */
static void readline(char* buf, int32_t count){
  assert(buf != NULL && count > 0);
  char* pos = buf;
  while(read(stdin_no, pos, 1) != -1 && (pos - buf) < count){
    switch(*pos){
     case '\n':
      case '\r':
        *pos = 0;   //添加cmd_line的终止字符0
        putchar('\n');
        return ;
      case '\b':
        if(buf[0] != '\b'){     //阻止删除非本次输入的信息
          --pos;                //退回到缓冲区cmd_line的山一个字节
          putchar('\b');
        }
        break;
       /* ctrl + l 实现清屏 */
      case 'l' - 'a':
        /* 1.先将当前的字符'l' - 'a' 置为0 */
        *pos = 0;
        /* 2.再将屏幕清空 */
        clear();
        /* 3.打印提示符 */
        print_prompt();
        /* 4.将之前键入的内容再次打印 */
        printf("%s", buf);
        break;
      /* ctrl + u 清除输入 */
      case 'u' - 'a':
        while(buf != pos){
          putchar('\b');
          *(pos--) = 0;
        }
        break;
      /* 非控制键则输出字符 */
      default:
        putchar(*pos);
        pos++;
    }
  }
  printf("reawdline: can't find enter_key in the cmd_line, max num of char is 128\n");
}

```

这里的演示我就不放出来了，也就是一些组合键清屏操作

### 3.解析键入字符
目前为止咱们的shell还是什么功能都没有，他只能输出咱们的输入的字符，所以咱们在shell中添加一个`cmd_str`函数来解析咱们的输入，如下
```
char final_path[MAX_PATH_LEN] = {0};    //用于洗路径时的缓冲

/* 用来记录当前目录，是当前目录的缓存，每次执行cd命令会更新他 */
char cwd_cache[64] = {0};

char* argv[MAX_ARG_NR];     //argv作为全局变量为了以后exec程序可访问参数
int32_t argc = -1;

/* 分析字符串cmd_str中以token为分割符的单词，将各单词存入argv数组 */
static int32_t cmd_parse(char* cmd_str, char** argv, char token){
  assert(cmd_str != NULL);
  int32_t arg_idx = 0;
  while(arg_idx < MAX_ARG_NR){
    argv[arg_idx] = NULL;
    arg_idx++;
  }
  char* next = cmd_str;
  int32_t argc = 0;
  /* 外层循环处理整个命令行 */
  while(*next){
    /* 去除命令字或参数之间的空格 */
    while(*next == token){
      next++;
    }
    /* 处理最后一个参数后接空格的情况 */
    if(*next == 0){
      break;
    }
    argv[argc] = next;
    /* 内层循环处理命令行中的每个命令字及参数 */
    while(*next && *next != token){     //在字符串结束前找单词分割符
      next++;
    }
    /* 如果未结束，使token成为0 */
    if(*next){  //如果上次结束末尾的字符是token
      *next++ = 0;  //将token字符换为0
    }
    /* 避免argv数组访问越界，参数过多则返回0 */
    if(argc > MAX_ARG_NR){
      return -1;
    }
    argc++;
  }
  return argc;
}

/* 简单的shell */
void my_shell(void){
  cwd_cache[0] = '/';
  cwd_cache[1] = 0;
  while(1){
    print_prompt();
    memset(cmd_line, 0, cmd_len);
    memset(final_path, 0, MAX_PATH_LEN);
    readline(cmd_line, MAX_PATH_LEN);
    if(cmd_line[0] == 0){ //如果只输入了一个回车
      continue;
    }
    argc = -1;
    argc = cmd_parse(cmd_line, argv, ' ');
    if(argc == -1){
      printf("num of arguments exceed %d\n", MAX_ARG_NR);
      continue;
    }
    char buf[MAX_PATH_LEN] = {0};
    int32_t arg_idx = 0;
    while(arg_idx < argc){
      printf("%s ", argv[arg_idx]);
      arg_idx++;
    }
    printf("\n");
  }
  panic("my_shell: should not be here");
}

```

上述代码并不难，其中涉及的操作只是字符串的切割，现在我们来运行一下虚拟机查看情况
![]()

### 4.添加系统调用
这里我们统一将以前的系统调用都加进来，开始充实咱们的用户层能干的事
```
enum SYSCALL_NR{
  SYS_GETPID,
  SYS_WRITE,
  SYS_MALLOC,
  SYS_FREE,
  SYS_FORK,
  SYS_READ,
  SYS_PUTCHAR,
  SYS_CLEAR,
  SYS_GETCWD,
  SYS_OPEN,
  SYS_CLOSE,
  SYS_LSEEK,
  SYS_UNLINK,
  SYS_MKDIR,
  SYS_OPENDIR,
  SYS_CLOSEDIR,
  SYS_CHDIR,
  SYS_RMDIR,
  SYS_READDIR,
  SYS_REWINDDIR,
  SYS_STAT,
  SYS_PS
};

```

然后我们同样的到syscall.c当中添加对应函数
```
/* 获取当前工作目录 */
char* getcwd(char* buf, uint32_t size){
  return (char*)_syscall2(SYS_GETCWD, buf, size);
}

/* 以flag方式打开文件pathname */
int32_t open(char* pathname, uint8_t flag) {
   return _syscall2(SYS_OPEN, pathname, flag);
}

/* 关闭文件fd */
int32_t close(int32_t fd) {
   return _syscall1(SYS_CLOSE, fd);
}

/* 设置文件偏移量 */
int32_t lseek(int32_t fd, int32_t offset, uint8_t whence) {
   return _syscall3(SYS_LSEEK, fd, offset, whence);
}

/* 删除文件pathname */
int32_t unlink(const char* pathname) {
   return _syscall1(SYS_UNLINK, pathname);
}

/* 创建目录pathname */
int32_t mkdir(const char* pathname) {
   return _syscall1(SYS_MKDIR, pathname);
}

/* 打开目录name */
struct dir* opendir(const char* name) {
   return (struct dir*)_syscall1(SYS_OPENDIR, name);
}

/* 关闭目录dir */
int32_t closedir(struct dir* dir) {
   return _syscall1(SYS_CLOSEDIR, dir);
}

/* 删除目录pathname */
int32_t rmdir(const char* pathname) {
   return _syscall1(SYS_RMDIR, pathname);
}

/* 读取目录dir */
struct dir_entry* readdir(struct dir* dir) {
   return (struct dir_entry*)_syscall1(SYS_READDIR, dir);
}

/* 回归目录指针 */
void rewinddir(struct dir* dir) {
   _syscall1(SYS_REWINDDIR, dir);
}

/* 获取path属性到buf中 */
int32_t stat(const char* path, struct stat* buf) {
   return _syscall2(SYS_STAT, path, buf);
}

/* 改变工作目录为path */
int32_t chdir(const char* path) {
   return _syscall1(SYS_CHDIR, path);
}

/* 显示任务列表 */
void ps(void) {
   _syscall0(SYS_PS);
}

```

这里咱们的ps系统调用之前还没有实现，所以咱们需要到thread.c当中先实现
```
/* 以填充空格的方式输出buf */
static void pad_print(char* buf, int32_t buf_len, void* ptr, char format){
  memset(buf, 0, buf_len);
  uint8_t out_pad_0idx = 0;
  switch(format){
    case 's':
      out_pad_0idx = sprintf(buf, "%s", ptr);
      break;
    case 'd':
      out_pad_0idx = sprintf(buf, "%d", *((int16_t*)ptr));
    case 'x':
      out_pad_0idx = sprintf(buf, "%x", *((uint32_t*)ptr));
  }
  while(out_pad_0idx < buf_len){    //这里用空格补充剩余的块数，这里的输出是为了对齐
    buf[out_pad_0idx] = ' ';
    out_pad_0idx++;
  }
  sys_write(stdout_no, buf, buf_len - 1);
}

/* 用于在lsit_traversal函数当中的回调函数，用于针对线程队列的处理 */
static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED){
  struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
  char out_pad[16] = {0};
  pad_print(out_pad, 16, &pthread->pid, 'd');
  if(pthread->parent_pid == -1){
    pad_print(out_pad, 16, "NULL", 's');
  }else{
    pad_print(out_pad, 16, &pthread->parent_pid, 'd');
  }
  switch(pthread->status){
    case 0:
      pad_print(out_pad, 16, "RUNNING", 's');
    case 1:
      pad_print(out_pad, 16, "READY", 's');
    case 2:
      pad_print(out_pad, 16, "BLOCKED", 's');
    case 3:
      pad_print(out_pad, 16, "WAITING", 's');
    case 4:
      pad_print(out_pad, 16, "HANGING", 's');
    case 5:
      pad_print(out_pad, 16, "DIED", 's');
  }
  pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');
  memset(out_pad, 0, 16);
  ASSERT(strlen(pthread->name) < 17);
  memcpy(out_pad, pthread->name, strlen(pthread->name));
  strcat(out_pad, '\n');
  sys_write(stdout_no, out_pad, strlen(out_pad));
  return false;     //为了使用list_traversal继续遍历，所以返回false
}

/* 返回任务列表 */
void sys_ps(void){
  char* ps_title = "PID         PPID        STAT        TICKS       COMMAND\n";
  sys_write(stdout_no, ps_title, strlen(ps_title));
  list_traversal(&thread_all_list, elem2thread_info, 0);
}

```

在这里我们添加一个从全局进程列表中获取线程信息的函数然后格式化输出即可

### 5.路径解析转换
这个路径解析是什么呢，其实就是咱们相对路径转换为绝对路径，你想在Linux当中我们并不是什么文件都是以绝对路径进行引用的，我们时不时会采用基于当前目录的相对路径从而获取文件信息，而操作系统底层肯定都是采用绝对路径进行资源定位的，所以这里我们需要实现将用户输入的相对路径转换为绝对路径的函数，这里我们创建函数shell/buildin_cmd.c
```
#include "buildin_cmd.h"
#include "assert.h"
#include "file.h"
#include "dir.h"
#include "syscall.h"
#include "stdio.h"
#include "string.h"
#include "global.h"
#include "fs.h"
#include "shell.h"

/* 将路径old_abs_path中的..和.转换为实际路径后存入new_abs_path */
static void wash_path(char* old_abs_path, char* new_abs_path){
  assert(old_abs_path[0] == '/');
  char name[MAX_FILE_NAME_LEN] = {0};
  char* sub_path = old_abs_path;
  sub_path = path_parse(sub_path, name);
  if(name[0] == 0){     //如果解析后发现没字符，说明路径只有'/'
    new_abs_path[0] = '/';
    new_abs_path[1] = 0;
    return;
  }
  new_abs_path[0] = 0;      //避免传给new_abs_path的缓冲区不干净
  strcat(new_abs_path, "/");
  while(name[0]){
    /* 如果是上一级目录 */
    if(!strcmp("..", name)){
      char* slash_ptr = strrchr(new_abs_path, '/');
      /* 如果未到new_abs_path中的顶层目录，就将最右边的'/'替换为0,
       * 这样便取出了new_abs_path中最后一层路径，相当与到了上一级目录*/
      if(slash_ptr != new_abs_path){    //就比如/a/b ， 这之后就变为/a
        *slash_ptr = 0;
      }else{
        /* 如果new_abs_path中只有1个'/'，即表示已经到了顶层目录，就将下一个字符置为结束符0 */
        *(slash_ptr + 1) = 0;
      }
    }else if(strcmp(".", name)){    //如果路径不是'.'，就将name拼接到new_abs_path
      if(strcmp(new_abs_path, "/")){    //这里判断是为了避免形成"//"的情况
        strcat(new_abs_path, "/")
      }
      strcat(new_abs_path, name);
    }   //如果name为当前目录"."，则无需处理
    memset(name, 0, MAX_FILE_NAME_LEN);
    if(sub_path){
      sub_path = path_parse(sub_path, name);
    }
  }
}

void make_clear_abs_path(char* path, char* final_path){
  char abs_path[MAX_PATH_LEN] = {0};
  /* 先判断是否输入的是绝对路径 */
  if(path[0] != '/'){   //如果不是绝对路径那就拼接成绝对路径
    memset(abs_path, 0, MAX_PATH_LEN);
    if(getcwd(abs_path, MAX_PATH_LEN) != NULL){ //获取当前绝对目录
      if(!((abs_path[0] == '/') && (abs_path[1] == 0))){ //若abs_path表示的当前目录不是根目录
        strcat(abs_path, "/");
      }
    }
  }
  strcat(abs_path, path);
  wash_path(abs_path, final_path);
}

```

这里我们首先将输入的相对路径，通过获取当前路径来进行拼接的到绝对路径，然后调用wash_path来清除掉绝对路径中的"."和".."以此来获取真正的绝对路径
紧接着我们到shell.c当中添加应用
```
/* 简单的shell */
void my_shell(void){
  cwd_cache[0] = '/';
  cwd_cache[1] = 0;
  while(1){
    print_prompt();
    memset(cmd_line, 0, cmd_len);
    memset(final_path, 0, MAX_PATH_LEN);
    readline(cmd_line, MAX_PATH_LEN);
    if(cmd_line[0] == 0){ //如果只输入了一个回车
      continue;
    }
    argc = -1;
    argc = cmd_parse(cmd_line, argv, ' ');
    if(argc == -1){
      printf("num of arguments exceed %d\n", MAX_ARG_NR);
      continue;
    }
    char buf[MAX_PATH_LEN] = {0};
    int32_t arg_idx = 0;
    while(arg_idx < argc){
      make_clear_abs_path(argv[arg_idx], buf);
      printf("%s -> %s\n", argv[arg_idx], buf);
      arg_idx++;
    }
    printf("\n");
  }
  panic("my_shell: should not be here");
}

```

这里我们进行一个简单的测试
![]()
可以看到这里我们是确实随便输入相对路径他都会返回一个正确的绝对路径，这为咱们接下来的工作打好了基础

### 6.实现ls、cd、mkdir、ps、rm等命令
这里我们实现这些命令的内建函数
```
#include "buildin_cmd.h"
#include "assert.h"
#include "file.h"
#include "dir.h"
#include "syscall.h"
#include "stdio.h"
#include "string.h"
#include "global.h"
#include "fs.h"
#include "shell.h"
#include "dir.h"

/* 将路径old_abs_path中的..和.转换为实际路径后存入new_abs_path */
static void wash_path(char* old_abs_path, char* new_abs_path){
  assert(old_abs_path[0] == '/');
  char name[MAX_FILE_NAME_LEN] = {0};
  char* sub_path = old_abs_path;
  sub_path = path_parse(sub_path, name);
  if(name[0] == 0){     //如果解析后发现没字符，说明路径只有'/'
    new_abs_path[0] = '/';
    new_abs_path[1] = 0;
    return;
  }
  new_abs_path[0] = 0;      //避免传给new_abs_path的缓冲区不干净
  strcat(new_abs_path, "/");
  while(name[0]){
    /* 如果是上一级目录 */
    if(!strcmp("..", name)){
      char* slash_ptr = strrchr(new_abs_path, '/');
      /* 如果未到new_abs_path中的顶层目录，就将最右边的'/'替换为0,
       * 这样便取出了new_abs_path中最后一层路径，相当与到了上一级目录*/
      if(slash_ptr != new_abs_path){    //就比如/a/b ， 这之后就变为/a
        *slash_ptr = 0;
      }else{
        /* 如果new_abs_path中只有1个'/'，即表示已经到了顶层目录，就将下一个字符置为结束符0 */
        *(slash_ptr + 1) = 0;
      }
    }else if(strcmp(".", name)){    //如果路径不是'.'，就将name拼接到new_abs_path
      if(strcmp(new_abs_path, "/")){    //这里判断是为了避免形成"//"的情况
        strcat(new_abs_path, "/");
      }
      strcat(new_abs_path, name);
    }   //如果name为当前目录"."，则无需处理
    memset(name, 0, MAX_FILE_NAME_LEN);
    if(sub_path){
      sub_path = path_parse(sub_path, name);
    }
  }
}

void make_clear_abs_path(char* path, char* final_path){
  char abs_path[MAX_PATH_LEN] = {0};
  /* 先判断是否输入的是绝对路径 */
  if(path[0] != '/'){   //如果不是绝对路径那就拼接成绝对路径
    memset(abs_path, 0, MAX_PATH_LEN);
    if(getcwd(abs_path, MAX_PATH_LEN) != NULL){ //获取当前绝对目录
      if(!((abs_path[0] == '/') && (abs_path[1] == 0))){ //若abs_path表示的当前目录不是根目录
        strcat(abs_path, "/");
      }
    }
  }
  strcat(abs_path, path);
  wash_path(abs_path, final_path);
}

/* pwd命令的内建函数 */
void buildin_pwd(uint32_t argc, char** argv UNUSED){
  if(argc != 1){
    printf("pwd: no argument supprot!\n");
    return;
  }else{
    if(NULL != getcwd(final_path, MAX_PATH_LEN)){
      printf("%s\n", final_path);
    }else{
      printf("pwd: get current work directory failed\n");
    }
  }

}
/* cd命令内建函数 */
char* buildin_cd(uint32_t argc, char** argv){
  if(argc > 2){
    printf("cd: only support 1 argument!\n");
    return NULL;
  }
  /* 如果只有cd无参数，则直接返回到根目录 */
  if(argc == 1){
    final_path[0] = '/';
    final_path[1] = 0;
  }else{
    make_clear_abs_path(argv[1], final_path);
  }

  if(chdir(final_path) == -1){
    printf("cd: no such directory %s\n", final_path);
    return NULL;
  }
  return final_path;
}

/* ls命令内建函数 */
void buildin_ls(uint32_t argc, char** argv){
  char* pathname = NULL;
  struct stat file_stat;
  memset(&file_stat, 0, sizeof(struct stat));
  bool long_info = false;
  uint32_t arg_path_nr = 0;
  uint32_t arg_idx = 1;     //跨过argv[0], argv[0]是字符串"ls"
  while(arg_idx < argc){
    if(argv[arg_idx][0] == '-'){    //如果是选项，则首字符应该是'-'
      if(!strcmp("-l", argv[arg_idx])){     //
        long_info = true;
      }else if(!strcmp("-h", argv[arg_idx])){
        printf("usage: -l list all information about the file.\n-h for help \nlist all files in the current dirctory if no option\n");
        return;
      }else{
        printf("ls: invalid option %s\nTry `ls -h` for more information.\n", argv[arg_idx]);
      }
    }else{      //ls的路径参数
      if(arg_path_nr == 0){
        pathname = argv[arg_idx];
        arg_path_nr = 1;
      }else{
        printf("ls: only support one path\n");
        return;
      }
    }
    arg_idx++;
  }
  if(pathname == NULL){     //如果只输入了ls或ls -l, 没有输入路径则默认以当前路径作为输入绝对路径
    if(NULL != getcwd(final_path, MAX_PATH_LEN)){
      pathname = final_path;
    }else{
      printf("ls: getcwd for default path failed\n");
      return;
    }
  }else{                    //用参数路径
    make_clear_abs_path(pathname, final_path);
    pathname = final_path;
  }
  if(stat(pathname, &file_stat) == -1){
    printf("ls: cannot accsess %s: No such file or directory\n", pathname);
    return;
  }
  if(file_stat.st_filetype == FT_DIRECTORY){
    struct dir* dir = opendir(pathname);
    struct dir_entry* dir_e = NULL;
    char sub_pathname[MAX_PATH_LEN] = {0};
    uint32_t pathname_len = strlen(pathname);
    uint32_t last_char_idx = pathname_len - 1;
    mempcpy(sub_pathname, pathname, pathname_len);
    if(sub_pathname[last_char_idx] != '/'){
      sub_pathname[pathname_len] = '/';
      pathname_len++;
    }
    rewinddir(dir);
    if(long_info){  //如果使用了-l 参数
      char ftype;
      printf("total: %d\n", file_stat.st_size);
      while((dir_e = readdir(dir))){        //遍历目录项
        ftype = 'd';
        if(dir_e->f_type == FT_REGULAR){
          ftype = '-';
        }
        sub_pathname[pathname_len] = 0;
        strcat(sub_pathname, dir_e->filename);
        memset(&file_stat, 0, sizeof(struct stat));
        if(stat(sub_pathname, &file_stat) == -1){
          printf("ls: cannot access %s: No such file or directory\n", dir_e->filename);
          return;
        }
        printf("%c  %d  %d  %s\n", ftype, dir_e->i_no, file_stat.st_size, dir_e->filename);
      }
    }else{      //如果没使用-l参数
      while((dir_e == readdir(dir))){
        printf("%s ", dir_e->filename);
      }
      printf("\n");
    }
    closedir(dir);
  }else{
    if(long_info){
      printf("- %d  %d  %s\n", file_stat.st_ino, file_stat.st_size, pathname);
    }else{
      printf("%s\n", pathname);
    }
  }
}

/* ps命令内建函数 */
void buildin_ps(uint32_t argc, char** argv UNUSED){
  if(argc != 1){
    printf("pc: no argument support!\n");
    return;
  }
  ps();
}

/* clear命令内建函数 */
void buildin_clear(uint32_t argc, char** argv UNUSED){
  if(argc != 1){
    printf("clear: no argument support!\n");
    return;
  }
  clear();
}

/* mkdir命令内建函数 */
int32_t buildin_mkdir(uint32_t argc, char** argv){
  int32_t ret = -1;
  if(argc != 2){
    printf("mkdir: only support 1 argument!\n");
  }else{
    make_clear_abs_path(argv[1], final_path);
    /* 若创建的不是根目录 */
    if(strcmp("/", final_path)){
      if(mkdir(final_path) == 0){
        ret = 0;
      }else{
        printf("mkdir: create directory %s failed.\n", argv[1]);
      }
    }
  }
  return ret;
}

/* rmdir命令内建函数 */
int32_t buildin_rmdir(uint32_t argc, char** argv){
  int32_t ret = -1;
  if(argc != 2){
    printf("rmdir: only support 1 argument!\n");
  }else{
    make_clear_abs_path(argv[1], final_path);
    /* 若删除的不是根目录 */
    if(strcmp("/", final_path)){
      if(rmdir(final_path) == 0){
        ret = 0;
      }else{
        printf("rmdir: remove %s failed.\n", argv[1]);
      }
    }
  }
  return ret;
}

/* rm命令内建函数 */
int32_t buildin_rm(uint32_t argc, char** argv){
  int32_t ret = -1;
  if(argc != 2){
    printf("rm: only support 1 argument!\n");
  }else{
    make_clear_abs_path(argv[1], final_path);
    /* 若删除的不是根目录 */
    if(strcmp("/", final_path)){
      if(unlink(final_path) == 0){
        ret = 0;
      }else{
        printf("rm: delete %s failed.\n", argv[1]);
      }
    }
  }
  return ret;
}

```

而这些内建函数的使用是通过shell.c来进行调用，如下
```
/* 简单的shell */
void my_shell(void){
  cwd_cache[0] = '/';
  cwd_cache[1] = 0;
  while(1){
    print_prompt();
    memset(cmd_line, 0, cmd_len);
    memset(final_path, 0, MAX_PATH_LEN);
    readline(cmd_line, MAX_PATH_LEN);
    if(cmd_line[0] == 0){ //如果只输入了一个回车
      continue;
    }
    argc = -1;
    argc = cmd_parse(cmd_line, argv, ' ');
    if(argc == -1){
      printf("num of arguments exceed %d\n", MAX_ARG_NR);
      continue;
    }
    if(!strcmp("ls", argv[0])){
      buildin_ls(argc, argv);
    }else if(!strcmp("cd", argv[0])){
      if(buildin_cd(argc, argv) != NULL){
        memset(cwd_cache, 0, MAX_PATH_LEN);
        strcpy(cwd_cache, final_path);
      }
    }else if(!strcmp("pwd", argv[0])){
      buildin_pwd(argc, argv);
    }else if(!strcmp("ps", argv[0])){
      buildin_ps(argc, argv);
    }else if(!strcmp("clear", argv[0])){
      buildin_clear(argc, argv);
    }else if(!strcmp("mkdir", argv[0])){
      buildin_mkdir(argc, argv);
    }else if(!strcmp("rmdir", argv[0])){
      buildin_rmdir(argc, argv);
    }else if(!strcmp("rm", argv[0])){
      buildin_rm(argc, argv);
    }else{
      printf("external command\n");
    }
  }
  panic("my_shell: should not be here");
}

```

然后我们到虚拟机当中测试一些基本命令，可以看到如下确实成功解析了咱们的输入
![]()

## 0x04 总结
目前用户交互的部分已经完结，剩下最后一部分那就是用户进程了。代码整体上来看还是挺简单的，所以我们后期的任务比之前轻松许多。还有最后一部分，加油吧
