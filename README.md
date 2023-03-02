## 0x00 加载用户进程
我们目前已经可以使用shell执行命令了，但是这还不是咱们的最后一步，我们应该要做到从硬盘上面加载程序然后执行，不出意外的话这就是咱们的最后一章了，加油吧各位
### 1.实现exec
首先我们来解释一下exec的作用：
shell的内建命令exec将并不启动新的shell，而是用要被执行命令替换当前的shell进程，并且将老进程的环境清理掉，而且exec命令后的其它命令将不再执行。
简单来说就是exec会将一个新进程的进程体装进一个旧进程当中，类似灵魂附体，而由于你这个只是替换灵魂（程序体，例如代码段、数据段、堆、栈），所以身体（pid）不变。
而根据咱们的回忆，我们shell中的内建函数都是使用`if-else if`来实现的，这就导致我们只要添加或者换一个命令就需要重新修改shell.c函数，因此有人就想出使用exec来达成用外部程序来执行命令。
![](http://imgsrc.baidu.com/forum/pic/item/d1a20cf431adcbefcb7f3ac1e9af2edda2cc9faa.jpg)
我们可以查看exec的帮助手册来查看Linux上exec的使用方法如上图，这几个exec函数功能类似，区别在于参数。这里我们并不全部实现，单独实现execv就足够了。
这里我们的实现有个点得指出，那就是execv失败返回-1,成功无返回，这里不返回任何值的原因是exec是去执行一个新进程，也就是jmp过去，他不会再需要返回到哪儿。
这里我们创建userprog/exec.c
```
#include "stdint.h"
#include "global.h"

extern void init_exit(void);
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint32_t Elf32_Half;

/* 32位elf头 */
struct Elf32_Ehdr{
  unsigned char e_ident[16];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
};

/* 程序头表Program header就是段描述头 */
struct Elf32_Phdr{
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

/* 段类型 */
enum segment_type{
  PT_NULL,      //草、走、忽略
  PT_LOAD,      //可加载程序段
  PT_DYNAMIC,   //动态加载信息
  PT_INTERP,    //动态加载器名称
  PT_NOTE,      //一些辅助信息
  PT_SHLIB,     //保留
  PT_PHDR       //程序头表
};

/* 将文件描述符fd指向的文件中，偏移为offset，大小为filesz的段加载到虚拟地址为vaddr的内存 */
static bool segment_load(int32_t fd, uint32_t offset, uint32_t filesz, uint32_t vaddr){
  uint32_t vaddr_first_page = vaddr & 0xfffff000;   //获取vaddr所在的第一个页框
  uint32_t size_in_first_page = PG_SIZE - (vaddr & 0x00000fff);     //加载到内存后文件在第一个页框中占用的字节大小
  uint32_t occupy_pages = 0;        //这里存放该段占用的页面数
  /* 若第一个页框容不下该段 */
  if(filesz > size_in_first_page){
    uint32_t left_size = filesz - size_in_first_page;
    occupy_pages = DIV_ROUND_UP(left_size, PG_SIZE) + 1;
  }else{
    occupy_pages = 1;
  }

  /* 为进程分配内存 */
  uint32_t page_idx = 0;
  uint32_t vaddr_page = vaddr_first_page;
  while(page_idx < occupy_pages){
    uint32_t* pde = pde_ptr(vaddr_page);
    uint32_t* pte = pte_ptr(vaddr_page);
    /* 如果pde不存在，或者pte不存在就分配内存，
     * pde的判断要在pte之前，否则pde若不存在会导致判断pte时缺页异常 */
    if(!(*pde & 0x00000001) || !(*pte & 0x00000001)){
      if(get_a_page(PF_USER, vaddr_page) == NULL){
        return false;
      }
    }//如果原进程的页表已经分配了，利用现有的物理页
    //直接覆盖进程体
    vaddr_page += PG_SIZE;
    page_idx++;     //处理下一页
  }
  sys_lseek(fd, offset, SEEK_SET);      //设置文件读写指针
  sys_read(fd, (void*)vaddr, filesz);   //读取内容到内存vaddr
  return true;
}

/* 从文件系统上加载用户程序pathname,成功则返回程序的起始地址，否则返回-1 */
static int32_t load(const char* pathname){
  int32_t ret = -1;
  struct Elf32_Ehdr elf_header;     //ELF头
  struct Elf32_Phdr prog_header;    //程序头
  memset(&elf_header, 0, sizeof(struct Elf32_Ehdr));
  int32_t fd = sys_open(pathname, O_RDONLY);    //只读打开文件
  if(fd == -1){
    return -1;
  }
  if(sys_read(fd, &elf_header, sizeof(struct Elf32_Ehdr)) != sizeof(struct Elf32_Ehdr)){
    ret = -1;
    goto done;
  }
  /* 校验elf头 */
  if(memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7) \           //\177是八进制，这里我们是检验一下文件是否为elf格式
      || elf_header.e_type != 2 \
      || elf_header.e_machine != 3 \
      || elf_header.e_version != 1 \
      || elf_header.e_phnum > 1024 \
      || elf_header.e_phentsize != sizeof(struct Elf32_Phdr)){
    ret = -1;
    goto done;
  }
  Elf32_Off prog_header_offset = elf_header.e_phoff;
  Elf32_Half prog_header_size = elf_header.e_phentsize;
  /* 遍历所有程序头 */
  uint32_t prog_idx = 0;
  while(prog_idx < elf_header.e_phnum){
    memset(&prog_header, 0, prog_header_size);
    /* 将文件的指针定位到程序头 */
    sys_lseek(fd, prog_header_offset, SEEK_SET);
    /* 只获取程序头 */
    if(sys_read(fd, &prog_header, prog_header_size) != prog_header_size){
      ret = -1;
      goto done;
    }
    /* 如果是可加载段就调用segment_load加载到内存 */
    if(PT_LOAD == prog_header.p_type){
      if(!segment_load(fd, prog_header.p_offset, prog_header.p_filesz, prog_header.p_vaddr)){
        ret = -1;
        goto done;
      }
    }
    /* 更新下一个程序头的偏移 */
    prog_header_offset += elf_header.e_phentsize;
    prog_idx++;
  }
  ret = elf_header.e_entry;
done:
  sys_close(fd);
  return ret;
}

/* 用path指向的程序替换当前进程 */
int32_t sys_execv(const char* path, const char* argv[]){
  uint32_t argc = 0;
  while(argv[argc]){
    argc++;
  }
  int32_t entry_point = load(path);
  if(entry_point == -1){    //如果加载失败，则返回-1
    return -1;
  }
  struct task_struct* cur = running_thread();
  /* 修改进程名 */
  memcpy(cur->name, path, TASK_NAME_LEN);
  cur->name[TASK_NAME_LEN - 1] = 0;
  struct intr_stack* intr_0_stack = (struct intr_stack*)((uint32_t)cur + PG_SIZE - sizeof(struct intr_stack));  //获取中断栈
  /* 参数传递给用户进程 */
  intr_0_stack->ebx = (int32_t)argv;    //ebx经常作为基址寄存器
  intr_0_stack->ecx = argc;             //ecx经常用作循环控制，这俩刚好适配
  intr_0_stack->eip = (void*)entry_point;
  /* 使新用户进程的栈地址为最高用户空间地址 */
  intr_0_stack->esp = (void*)0xc0000000;
  /* 与fork不同，fork是等待被调度，而exec是立即中断返回 */
  asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(intr_0_stack) : "memory");
  return 0;     //这条语句其实没法执行，因为上面已经jmp了
}

```

上面的函数意思就是将可加载的段加载到内存，然后修改咱们的栈信息使得中断返回的时候到达可加载段就行。然后我们像往常一样添加对应的系统调用即可


### 2.使shell支持外部命令
这里我们只需要在shell的`if-else if`判断结尾加上一个分支即可，也就是所有不属于内建命令的分支都会走这里，如下：
```
else{  //如果是外部命令，则需要从磁盘上面加载
      int32_t pid = fork();
      printf("fork\n");
      if(pid){
        /* 这里加上while循环使父进程在子进程后进行，这里是因为一般父进程会比子进程更早执行，然后进行下一轮清空final_path */
        while(1);
      }else{    //子进程
        printf("i am son\n");
        make_clear_abs_path(argv[0], final_path);
        argv[0] = final_path;
        /* 首先判断文件是否存在 */
        struct stat file_stat;
        memset(&file_stat, 0, sizeof(struct stat));
        if(stat(argv[0], &file_stat) == -1){
          printf("my_shell: cannot access %s: No such file or directory\n", argv[0]);
        }else{
          execv(argv[0], argv);
        }
        while(1);
      }
    }
    int32_t arg_idx = 0;
    while(arg_idx < MAX_ARG_NR){
      argv[arg_idx] = NULL;
      arg_idx++;
    }

```

现在由于咱们还没有将外部用户程序写入硬盘，所以咱们下节再进行测试

## 0x01 加载硬盘上的用户程序执行
我们要将自己实现的用户程序写入硬盘，先完成三相工作：
1. 编写一个真正的用户程序
2. 将用户程序写入文件系统
3. 在shell中执行用户程序，也就是外部命令

首先我们来编写用户程序，新建函数`command/prog_no_arg.c`,我们先实现一个比较简单无参数的
```
#include "stdio.h"
int main(void){
  printf("prog_no_arg from disk\n");
  while(1);
  return 0;
}

```

还是十分简单的，这里加上while循环来防止程序乱跑，接下来我们的编译也要重新设置，编写函数command/compile.sh
```
######## 应在command目录下执行 #########
if[[ ! -d "../lib" || ! -d "../build"]];then
  echo "dependent dir do not exist!"
  cwd=$(pwd)
  cwd=${cwd##*/}
  cwd=${cwd%/}
  if[[ $cwd != "command" ]];then
    echo -e "you would better in command dir\n"
  fi
  exit
fi

BIN="prog_no_arg"
CFLAGS="-m32 -fno-stack-protector -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers"
LIB="../lib/"
OBJS="../build/string.o ../build/syscall.o ../build/stdio.o ../build/assert.o"
DD_IN=$BIN
DD_OUT="/home/dawn/repos/OS_learning/bochs/hd60M.img"

gcc $CFLAGS -I $LIB -o $BIN".o" $BIN".c"
ld -e main $BIN".o" $OBJS -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d", ($5+511)/512)}')

if[[ -f $BIN ]];then
  dd if=./$DD_IN of=./$DD_OUT bs=512 count=$SEC_CNT seek=300 conv=notrunc
fi

```

这里我们的shell脚本是首先将我们的程序编译打入hd60M.img，然后再通过主线程将其写入hd80M.img，这样就成功加载了用户程序.
然后我们再修改kernel/main.c来进行写入hd80M.img
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  /************** 写入应用程序 ************/
  uint32_t file_size = 10236;
  uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512);
  struct disk* sda = &channels[0].devices[0];
  void* prog_buf = sys_malloc(file_size);
  ide_read(sda, 300, prog_buf, sec_cnt);
  int32_t fd = sys_open("/prog_no_arg", O_CREAT|O_RDWR);
  if(fd != -1){
    if(sys_write(fd, prog_buf, file_size) == -1){
      printk("file write error!\n");
      while(1);
    }
  }
  /****×***** 写入应用程序结束  *********/
  cls_screen();
  console_put_str("[peiwithhao@localhost /]$ ");
  while(1);
  return 0;
}

```

这里我们会碰到一个错误，就是我之前写的switch.c中有个错误，把ebx写成了edx，导致我调试了一下午，这时间真白费了，然后上面main函数中的10236是咱们用户程序的大小，我们使用ls命令就可以获得
```
dawn@dawn-virtual-machine:~/repos/OS_learning$ ls -l command/prog_no_arg
-rwxrwxr-x 1 dawn dawn 10236 Feb 18 23:06 command/prog_no_arg

```

然后我们到虚拟机中测试看是否真的可以运行这个程序了，结果如下：
![](http://imgsrc.baidu.com/forum/pic/item/0b7b02087bf40ad10ca08324122c11dfa8ecceb5.jpg)

### 3. 支持参数传递
我们在平时的程序中都是依靠栈来传递参数，但是对于我们现在的用户程序来说创建之初还没有栈怎么办呢，实际上我们知道用户程序都有一个main函数，但是这个main函数并不是第一个运行的函数，因此他的参数就是由其他人来传递，而这里那些计算机的大佬就使用库函数对其进行协助，这个库函数实际上也就是一些通用的函数，写道一起方便调用，而后来为了规定一个标准，于是就有了CRT，也就是C运行库，而他的主要功能就是初始化运行环境，也就是进入main函数之前为他准备环境，传递参数等，然后我们再调用main函数才能正确跑起来。
而目前咱们的用户程序大家都可以看到最后是使用while(1)来防止程序不会到处乱跑，所以我们还要实现exit系统调用来使得main函数结束后陷入内核来将处理器的控制权返回内核。
下面给出CRT与用户程序的关系，如下:
![](http://imgsrc.baidu.com/forum/pic/item/7e3e6709c93d70cf320deecabddcd100bba12b05.jpg)

接下来我们首先实现一个CRT的简易版本：
```
[bits 32]
extern main
section .text
global _start
_start:
  ;下面这两个要和execv中load之后指定的寄存器一致，也就是ebx和ecx啦
  push ebx  ;压入argv
  push ecx  ;压入argc
  call main
```

这里使用call而不是jmp就是让main有去有回，不会到处乱跑，我们刚写的exec.c大伙应该还记得，当时我们将新进程的参数压入内核中相应的寄存器，因此在这里我们将寄存器中的值压入栈中，这样下去我们的main函数就会有参数可以调用了。
至此我们来重新编写一个有参数的用户程序
```
#include "stdio.h"
#include "syscall.h"
#include "string.h"

int main(int argc, char** argv){
  int arg_idx = 0;
  while(arg_idx < argc){
    printf("argv[%d] is %s\n", arg_idx, argv[arg_idx]);
    arg_idx++;
  }
  int pid = fork();
  if(pid){
    int delay = 900000;
    while(delay--);
    printf("\n I am father prog, my pid : %d, I will show procerss list\n", getpid());
    ps();
  }else{
    char abs_path[512] = {0};
    printf("\n  I am child prog, my pid: %d, I will exec %s right now\n", getpid(), argv[1]);
    if(argv[1][0] != '/'){
      getcwd(abs_path, 512);
      strcat(abs_path, "/");
      strcat(abs_path, argv[1]);
      execv(abs_path, argv);
    }else{
      execv(argv[1], argv);
    }
  }
  while(1);
  return 0;
}

```

该程序的作用就是父进程来延迟显示操作系统的进程信息，而子进程来运行argv[1]指向的用户进程
```
######## 应在command目录下执行 #########
if [[ ! -d "../lib" || ! -d "../build" ]];then
  echo "dependent dir do not exist!"
  cwd=$(pwd)
  cwd=${cwd##*/}
  cwd=${cwd%/}
  if [[ $cwd != "command" ]];then
    echo -e "you would better in command dir\n"
  fi
  exit
fi

BIN="prog_arg"
CFLAGS="-m32 -fno-stack-protector -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers"
LIBS="-I ../lib/ -I ../lib/user -I ../fs"
OBJS="../build/string.o ../build/syscall.o ../build/stdio.o ../build/assert.o start.o"
DD_IN=$BIN
DD_OUT="/home/dawn/repos/OS_learning/bochs/hd60M.img"

nasm -f elf ./start.S -o ./start.o
ar rcs simple_crt.a $OBJS start.o
gcc $CFLAGS $LIBS -o $BIN".o" $BIN".c"
ld -m elf_i386 $BIN".o" simple_crt.a -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d", ($5+511)/512)}')

if [[ -f $BIN ]];then
  dd if=./$DD_IN of=$DD_OUT bs=512 count=$SEC_CNT seek=300 conv=notrunc
fi

```

这里我们重新编写一下compile.sh来编译`prog_arg.c`,我们首先将start.S编译为start.o,然后使用ar命令将string.o,syscall.o,stdio.o, assert.o和start.o打包成为一个静态库文件`simple_crt.a`,这就是我们刚刚讲到的简单的CRT了，然后我们再修改main函数
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  /************** 写入应用程序 **********/
  uint32_t file_size = 10384;
  uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512);
  struct disk* sda = &channels[0].devices[0];
  void* prog_buf = sys_malloc(file_size);
  ide_read(sda, 300, prog_buf, sec_cnt);
  int32_t fd = sys_open("/prog_arg", O_CREAT|O_RDWR);
  if(fd != -1){
    if(sys_write(fd, prog_buf, file_size) == -1){
      printk("file write error!\n");
      while(1);
    }
  }
  sys_close(fd);
   /******* 写入应用程序结束  *********/
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

这里我们父进程来答应ps信息，子进程使用execv来执行`prog_no_arg`,还是十分简单的，然后执行结果如下，我们确实正确的执行
![](http://imgsrc.baidu.com/forum/pic/item/4034970a304e251f9542a33fe286c9177e3e53bb.jpg)

## 0x01实现系统调用exit和wait
### 1.wait和exit的作用
exit的作用十分直白，那就是退出进程，在C运行库当中会调用main函数执行，main函数结束后程序流程会回到C运行库，C运行库结束之后就会调用exit系统调用。
而wait的作用是阻塞父进程自己，直到任意一个子进程运行结束，也就是说某个进程调用wait的时候，内核就回去寻找他的子进程，如果没有子进程，这里wait会返回-1,如果有子进程，那么内核就要去遍历其所有的子进程并查找哪个子进程已经推出了，并且将子进程推出时的返回值传递给父进程，然后将父进程唤醒，这里主要的功能就是阻塞父进程，像我们上一节那样为了避免父进程和子进程的输出混在一起，我们让父进程循环了900000,这样不仅不够优雅，且十分浪费cpu资源，因此咱们迫切使用wait来阻塞自身。

### 2.孤儿进程和僵尸进程
首先来说说孤儿进程，简单来说也就是子进程没父进程了，也就是说在子进程还都没有完全执行完毕的时候，此时父进程却因某种事情终止了，此时这些子进程便被成为父进程
这里僵尸进程就是子进程调用exit系统调用进行退出，但是父进程并没有调用wait系统调用（用来接收子进程的返回值以及收回子进程所占用的资源），这样就没人帮子进程收尸，那么此时他就变成了僵尸进程。
这俩解释都挺直观的，我们接下来总结两句：
+ exit由子进程调用，表面上功能是使自己结束并且传递返回值给内核，本质上是内核在幕后将进程除pcb外的一切资源回收
+ wait由父进程调用，表面上是父进程阻塞自己，直到子进程唤醒父进程，然后获得子进程的返回值，本质上是内核在幕后将子进程的返回值传递给父进程并唤醒他，然后回收PCB

### 3.基础代码们
首先我们来写一点基础代码来方便这两个系统调用
我们刚刚知道子进程的返回值是存放在pcb当中的，所以这里我们在pcb当中增加一个成员来记录返回值
```
int8_T exit_status
```

这里我们再到kernel/memory.c当中添加一个内存释放的函数
```
/* 根据物理页框地址pg_phy_addr在相应的内存池位图清0,不改动页表 */
void free_a_phy_page(uint32_t pg_phy_addr){
  struct pool* mem_pool;
  uint32_t bit_idx = 0;
  if(pg_phy_addr >= user_pool.phy_addr_start){  //如果是用户内存池
    mem_pool = &user_pool;
    bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
  }else{
    mem_pool = &kernel_pool;
    bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
  }
  bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

```

我们使用他来回收某个指定的物理页框,所以这里我们介绍一下重点函数，我们添加在thread/thread.c
```
/* pid的位图，最大支持1024个pid */
uint8_t pid_bitmap_bits[128] = {0};

/* pid池 */
struct pid_pool{
  struct bitmap pid_bitmap;     //pid位图
  uint32_t pid_start;       //起始pid
  struct lock pid_lock;     //分配pid锁
}pid_pool;

/* 初始化pid池 */
static void pid_pool_init(void){
  pid_pool.pid_start = 1;
  pid_pool.pid_bitmap.bits = pid_bitmap_bits;
  pid_pool.pid_bitmap.btmp_bytes_len = 128;
  bitmap_init(&pid_pool.pid_bitmap);
  lock_init(&pid_pool.pid_lock);
}

/* 分配pid */
static pid_t allocate_pid(void){
  lock_acquire(&pid_pool.pid_lock);
  int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
  bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
  lock_release(&pid_pool.pid_lock);
  return (bit_idx + pid_pool.pid_start);
}

/* 释放pid */
void release_pid(pid_t pid){
  lock_acquire(&pid_pool.pid_lock);
  int32_t bit_idx = pid - pid_pool.pid_start;
  bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
  lock_release(&pid_pool.pid_lock);
}

/* 回收thread_over的pcb和页表，并将其从调度队列去掉 */
void thread_exit(struct task_struct* thread_over, bool need_schedule){
  /* 这里要确保schedule在关中断下使用 */
  intr_disable();
  thread_over->status = TASK_DIED;
  /* 如果thread_over不是当前线程，那么就从就绪队列中移除 */
  if(elem_find(&thread_ready_list, &thread_over->general_tag)){
    list_remove(&thread_over->general_tag);
  }
  if(thread_over->pgdir){   //如果是进程，那么回收他的页表
    mfree_page(PF_KERNEL, thread_over->pgdir, 1);
  }

  /* 从all_thread_list当中去掉此任务 */
  list_remove(&thread_over->all_list_tag);

  /* 回收pcb所在的页，主线程的pcb并不在堆中，跨过 */
  if(thread_over != main_thread){
    mfree_page(PF_KERNEL, thread_over, 1);
  }

  /* 归还pid */
  release_pid(thread_over->pid);
  /* 如果需要下一轮调度则主动调用schedule */
  if(need_schedule){
    schedule();
    PANIC("thread_exit: should not be here\n");
  }
}

/* 比对任务的pid */
static bool pid_check(struct list_elem* pelem, int32_t pid){
  struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
  if(pthread->pid == pid){
    return true;
  }
  return false;
}

/* 根据pid找到pcb，如果找到则返回这个pcb，否则返回NULL */
struct task_struct* pid2thread(int32_t pid){
  struct list_elem* pelem = list_traversal(&thread_all_list, pid_check, pid);
  if(pelem == NULL){
    return NULL;
  }
  struct task_struct* thread = elem2entry(struct task_struct, all_list_tag, pelem);
  return thread;
}

```

上面我们实现了回收pid以及一些基础函数，这里pid我们干脆用位图来保证他是全局分配

### 4.实现wait和exit
接下来我们终于要打破进程无法正常结束的死局了，我们创建函数`userprog/wait_exit.c`
```
#include "wait_exit.h"
#include "global.h"
#include "debug.h"
#include "thread.h"
#include "list.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "bitmap.h"
#include "fs.h"

/* 释放用户进程资源 *
 * 1. 页表中对应的物理页
 * 2. 虚拟内存池占的物理页框
 * 3. 关闭打开的文件 */
static void release_prog_resource(struct task_struct* release_thread){
  uint32_t* pgdir_vaddr = release_thread->pgdir;
  uint16_t user_pde_nr = 768, pde_idx = 0;      //表示用户空间的pde数量，有768个
  uint32_t pde = 0;
  uint32_t* v_pde_ptr = NULL;   //表示var,和函数pde_ptr区分
  uint16_t user_pte_nr = 1024, pte_idx = 0;
  uint32_t pte = 0;
  uint32_t* v_pte_ptr = NULL;   //表示var,和函数pte_ptr区分
  uint32_t* first_pte_vaddr_in_pde = NULL;  //用来记录pde当中第0个pte的地址
  uint32_t pg_phy_addr = 0;

  /* 回收页表中用户空间的页框 */
  while(pde_idx < user_pde_nr){
    v_pde_ptr = pgdir_vaddr + pde_idx;  //页目录表项指针
    pde = *v_pde_ptr;           //页目录项值
    if(pde & 0x00000001){   //如果页目录项的p位为1,表示该页目录项下可能有页表项
      first_pte_vaddr_in_pde = pte_ptr(pde_idx * 0x400000);     //一个页表表示的内容容量是4M,通过这里我们就可以得到地一个pte地址
      pte_idx = 0;
      while(pte_idx < user_pte_nr){     //在页表当中遍历
        v_pte_ptr = first_pte_vaddr_in_pde + pte_idx;
        pte = *v_pte_ptr;    //获取页表项内容
        if(pte & 0x00000001){
          /* 将pte中记录的物理页框直接在相应内存池的位图中清0 */
          pg_phy_addr = pte & 0xfffff000;
          free_a_phy_page(pg_phy_addr);
        }
        pte_idx++;
      }
      /* 将pde中记录的物理页框直接在相应内存池的位图中清0 */
      pg_phy_addr = pde & 0xfffff000;
      free_a_phy_page(pg_phy_addr);
    }
    pde_idx++;
  }
  /* 回收用户虚拟地址池所占的物理内存 */
  uint32_t bitmap_pg_cnt = (release_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len) / PG_SIZE;
  uint8_t* user_vaddr_pool_bitmap = release_thread->userprog_vaddr.vaddr_bitmap.bits;
  mfree_page(PF_KERNEL, user_vaddr_pool_bitmap, bitmap_pg_cnt);

  /* 关闭进程打开的文件 */
  uint8_t fd_idx = 3;
  while(fd_idx < MAX_FILES_OPEN_PER_PROC){
    if(release_thread->fd_table[fd_idx] != -1){
      sys_close(fd_idx);
    }
    fd_idx++;
  }
}

/* list_traversal回调函数，查找pelem的parent_pid是否是ppid，成功返回true,失败返回false */
static bool find_child(struct list_elem* pelem, int32_t ppid){
  /* elem2entry中间的参数all_list_tag取决于pelem对应的变量名 */
  struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
  if(pthread->parent_pid == ppid){  //如果该任务的parent_pid为ppid,则返回
    return true;
  }
  return false;     //继续遍历
}

/* list_traversal毁掉函数，查找状态为TASK_HANGING的函数 */
static bool find_hanging_child(struct list_elem* pelem, int32_t ppid){
  struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
  if(pthread->parent_pid == ppid && pthread->status == TASK_HANGING){
    return true;
  }
  return false;
}

/* list_traversal回调函数，将一个子进程过继给init */
static bool init_adopt_a_child(struct list_elem* pelem, int32_t pid){
  struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
  if(pthread->parent_pid == pid){   //若该进策划那个的parent_pid为pid,返回
    pthread->parent_pid = 1;
  }
  return false;
}

/* 等待子进程调用exit，将子进程的推出状态保存到status指向的变量，成功则返回子进程的pid，失败则返回-1 */
pid_t sys_wait(int32_t* status){
  struct task_struct* parent_thread = running_thread();
  while(1){
    /* 优先处理已经是挂起状态的任务 */
    struct list_elem* child_elem = list_traversal(&thread_all_list, find_hanging_child, parent_thread->pid);
    /* 若有挂起的子进程 */
    if(child_elem != NULL){
      struct task_struct* child_thread = elem2entry(struct task_struct, all_list_tag, child_elem);
      *status = child_thread->exit_status;
      /* thread_exit后，pcb会被回收，因此提前获取pid */
      uint16_t child_pid = child_thread->pid;
      /* 从就绪队列和全部队列中删除进程表项 */
      thread_exit(child_thread, false);   //传入false,使thread_exit调用后回到此处
      /* 进程表项是进程或线程最后保留的资源，至此该进程彻底消失了 */
      return child_pid;
    }
    /* 判断是否有子进程 */
    child_elem = list_traversal(&thread_all_list, find_child, parent_thread->pid);
    if(child_elem == NULL){     //若没有子进程则出错返回
      return -1;
    }else{
      /* 若子进程还没运行完，也就是还没调用exit，则就爱那个自己挂起，直到子进程唤醒 */
      thread_block(TASK_WAITING);
    }
  }
}

/* 子进程用来结束自己时调用 */
void sys_exit(int32_t status){
  struct task_struct* child_thread = running_thread();
  child_thread->exit_status = status;
  if(child_thread->parent_pid == -1){
    PANIC("sys_exit: child_thread->parent_pid is -1\n");
  }
  /* 将进程child_thread的所有子进程都过继给init */
  list_traversal(&thread_all_list, init_adopt_a_child, child_thread->pid);

  /* 回收进程child_thread的资源 */
  release_prog_resource(child_thread);

  /* 如果父进程正在等待子进程推出，将父进程唤醒 */
  struct task_struct* parent_thread = pid2thread(child_thread->parent_pid);
  if(parent_thread->status == TASK_WAITING){
    thread_unblock(parent_thread);
  }

  /* 将自己挂起，等待父进程获取其status,并回收其pcb */
  thread_block(TASK_HANGING);
}

```

这里给大家梳理一下整体过程：
1. 首先父进程会调用wait，首先就是循环查找整体链表里面是否有`TASK_HANGING`的子进程，如果找到了，就会调用`thread_exit`来释放其pcb和将其从链表上脱链
2. 但如果没找到相对应状态的子进程，则会将自己的状态置为`TASK_WAITING`,然后阻塞自己
3. 如果此时子进程再调用exit，则会首先查找自己是否有子进程，如果有的话那么将其全部托管给init进程，然后回收自己的一些物理页框资源和虚拟地址位图资源，还有自己打开的文件
4. 然后查看父进程是否为waiting状态，如果是那就唤醒父进程，否则将自己的状态置为`TASK_HANGING`

这里清楚之后我们自行添加系统调用，这里我就不多讲了

### 5.实现cat命令
我们现在虽然可以创建文件以及执行，但是还不能进行查看，所以这里我们需要实现cat命令来查看文件，在实现之前我们先在start.S当中添加exit系统调用
```
[bits 32]
extern main
extern exit
section .text
global _start
_start:
  ;下面这两个要和execv中load之后指定的寄存器一致，也就是ebx和ecx啦
  push ebx  ;压入argv
  push ecx  ;压入argc
  call main
  ;将main的返回值通过栈传递给exit，gcc用eax存储返回值，这是ABI规定的
  push eax
  call exit
  ;exit不会返回
```

这样在我们的用户程序main函数执行完毕后就会返回到exit系统调用，然后就是正常的回收资源，然后父进程获取子进程的返回值，然后回收子进程pcb了

然后我们正式开始实现,我们创建文件command/cat.c
```
#include "syscall.h"
#include "stdio.h"
#include "string.h"
int main(int argc, char** argv){
  if(argc > 2 || argc == 1){
    printf("cat: only support 1 argument.\neg: cat filename\n");
    exit(-2);
  }
  int buf_size = 1024;
  char abs_path[512] = {0};
  void* buf = malloc(buf_size);
  if(buf == NULL){
    printf("cat: malloc memory failed\n");
    return -1;
  }
  if(argv[1][0] != '/'){    //如果输入的不是绝对路径
    getcwd(abs_path, 512);
    strcat(abs_path, "/");
    strcat(abs_path, argv[1]);
  }else{
    strcpy(abs_path, argv[1]);
  }
  int fd = open(abs_path, O_RDONLY);
  if(fd == -1){
    printf("cat: open: open %s failed\n",argv[1]);
    return -1;
  }
  int read_bytes = 0;
  while(1){
    read_bytes = read(fd, buf, buf_size);
    if(read_bytes == -1){
      break;
    }
    write(1, buf, read_bytes);
  }
  free(buf);
  close(fd);
  return 66;

}

```

简单的读取文件代码，想必不需要多说，然后我们来修改compile.c就可以了,如下：
```
######## 应在command目录下执行 #########
if [[ ! -d "../lib" || ! -d "../build" ]];then
  echo "dependent dir do not exist!"
  cwd=$(pwd)
  cwd=${cwd##*/}
  cwd=${cwd%/}
  if [[ $cwd != "command" ]];then
    echo -e "you would better in command dir\n"
  fi
  exit
fi

BIN="cat"
CFLAGS="-m32 -fno-stack-protector -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers"
LIBS="-I ../lib/ -I ../lib/kernel/ -I ../lib/user/ -I ../kernel -I ../device -I ../thread -I ../userprog -I ../fs -I ../shell/"
OBJS="../build/string.o ../build/syscall.o ../build/stdio.o ../build/assert.o start.o"
DD_IN=$BIN
DD_OUT="/home/dawn/repos/OS_learning/bochs/hd60M.img"

nasm -f elf ./start.S -o ./start.o
ar rcs simple_crt.a $OBJS start.o
gcc $CFLAGS $LIBS -o $BIN".o" $BIN".c"
ld -m elf_i386 $BIN".o" simple_crt.a -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d", ($5+511)/512)}')

if [[ -f $BIN ]];then
  dd if=./$DD_IN of=$DD_OUT bs=512 count=$SEC_CNT seek=300 conv=notrunc
fi

```

我们将cat写入hd60M.img后，再修改一下shell.c，因为咱们现在已经实现了wait，所以就把之前比较笨的while(1)给去掉
```
else{  //如果是外部命令，则需要从磁盘上面加载
      int32_t pid = fork();
      if(pid){
        int32_t status;     //用来获取子进程返回值
        int32_t child_pid = wait(&status);  //此时若子进程没有执行exit，则父进程会自行阻塞
        if(child_pid == -1){    //这里添加一个错误判断
          panic("my shell: no child\n");
        }
        printf("child_pid %d, it's status: %d\n", child_pid, status);
      }else{    //子进程
        make_clear_abs_path(argv[0], final_path);
        argv[0] = final_path;
        /* 首先判断文件是否存在 */
        struct stat file_stat;
        memset(&file_stat, 0, sizeof(struct stat));
        if(stat(argv[0], &file_stat) == -1){
          printf("my_shell: cannot access %s: No such file or directory\n", argv[0]);
          exit(-1);
        }else{
          execv(argv[0], argv);
        }
      }
    }
```

这里我只贴出修改的部分，也就是外部命令判断的那部分
然后我们可以检测一下，这里为了方便，我提前写入了文件`file1`和`/dir1/file2`，这样是为了方便测试我们的`cat`命令，情况如下图
![](http://imgsrc.baidu.com/forum/pic/item/77094b36acaf2edd14ec072ec81001e9380193dc.jpg)
可以看到我们读取文件的功能是正常的，还有就是你可以发现每次任务的pid始终是2，因为此时主线程已经被咱们抛弃了，所以空出了2号pid，这样就每次分配给子进程，然后子进程结束后调用exit来唤醒wait的父进程，所以每次打印都是子进程号为2,这里我们在子进程加过一个判断，那就是访问错误的路径会调用`exit(1)`，这里父进程获取子进程返回的-1也确实无误。
还有一点就是可以发现咱们的idle线程卷起来辣，这里TICKS我们知道记录的是该线程运行的总拍数，以前idle线程都入不了咱们的法眼，但此时我们添加了wait，就不用一直while(1)循环占用CPU资源，每次都是直接阻塞自身，这样idle的线程就功能大了起来，你可以这么理解，假如我们自己是CPU，我们以前没事的时候只能不停跑步，我们想闲下来抖个腿（idle线程）都不行，但是当我们得知自己可以让自己停下来（wait阻塞）的时候，我们终于可以一直抖腿了，这样我们抖退的时间就会增加，这对我们身体的负担也会小很多。

## 0x02 管道
终于到了最后一步，我们将要实现管道，以此来支持父子之间的通信，并且在shell当中支持管道操作
### 1.原理
进程间通信有很多种，比如消息队列，共享内存，socket网络通信，还有就是咱们的管道，等等，这里我们同书上的作者一样只实现管道
在Linux一切皆文件，因此管道也被看作文件，但是这个文件并不存在于文件系统，而是只存在于内存，这里我们其实已经知道了，根据之前的经验，这个管道当然是存储在内核空间的，因为只有这样才能实现进程间共享。
当然实现进程资源共享还可以写同一个文件，但是这样涉及到硬盘就太慢了。
这里还有一个问题，既然管道存在于内核空间，那我们该分配多少字节的空间呢，这个大小我们是很难把控的，这也根据你具体传递信息来决定，但是我们曾经写过一个东西可以巧妙的解决这个问题，那就是咱们的环形缓冲区，使用他理论上可以获得无限大的空间，同时也满足小字节的分配，这是由于它采用生产者消费者的设计思路，一方写满就阻塞，另一方再读。
![](http://imgsrc.baidu.com/forum/pic/item/5ab5c9ea15ce36d34c46cdf77ff33a87e850b1b3.jpg)
上面是一个管道的示意图，管道有两端，一端用于从管道中读入数据，另一端往管道中写入数据，这两端使用文件描述符来读取，一个描述符用来读，一个用来写，通常情况下是用户进程为内核提供一个长度为2的文件描述符数组，然后内核会在该数组中写入管道操作的两个描述符，也就是上面的fd[0]和fd[1]
我们要实现父子进程之间的通信，首先就是在管道创建之后，父进程立马fork出一个子进程，然后父进程和子进程一个读一个写，不会出现同一个进程又读又写的情况，比如说父进程负责写，那么他会关闭读描述符，然后子进程负责写，那么他就会关闭读描述符，而由于管道也视作文件，因此文件指针的更新对与父子来说是同步的。大郅情况如下:
![](http://imgsrc.baidu.com/forum/pic/item/c9fcc3cec3fdfc039eeead7f913f8794a5c22666.jpg)
上面是理论上的版本，下面则是实际上的使用：
![](http://imgsrc.baidu.com/forum/pic/item/eaf81a4c510fd9f959871bae602dd42a2934a461.jpg)
这里还得说一句，管道分为两种：匿名管道和命名管道，这里从他名字也知道，区别就在于有无名字，但是他俩也有适用范围，因为如果是匿名管道，他没有名字，创建之后只能通过内核为其返回的文件描述符来访问，所以此管道只会对创建他的进程或者其子进程开放，因此匿名管道只能用于父子进程通信。
而命名管道则是真正的文件，他存在与文件系统，因此他对任何进程都可见，所以进程间的通信就可以靠他。

### 2.管道的设计
我们都知道文件系统分为很多种，ext2、ext3等，为了向上提供统一的接口，Linux加了一个中间曾VFS，也就是虚拟文件系统，向用户屏蔽一些实现的细节，所以用户只和VFS打交道
但是咱们目前的操作系统是只用支持咱们自己写的文件系统了，所以并不需要VFS来进行标准化，因此我们在文件结构上面来做个手脚，这里先给出文件结构防止大家忘了
```
/* 文件结构 */
struct file {
  uint32_t  fd_pos;     //记录当前文件操作的偏移地址，以0为起始，最大为文件大小-1
  uint32_t fd_flag;
  struct inode* fd_inode;
};

```

这是我们之前写的文件结构，这个fd_inode其实我们并没有必要使用了，因为咱们的管道并不是真正存在与文件系统上面，但是如果说现在我们来修改文件系统势必会十分麻烦，因此我们来使用一个标识来表示他是一个管道，我们之前使用`fd_flag`来说明该文件的类型，比如说是`O_RDONLY, O_WRONLY`等，我们可以使用他来标识该文件是一个管道，因此我们就让`fd_flag`的值为0xFFFF的时候，表示该结构是一个管道，然后我们的`fd_inode`就用来指向存储数据的内核缓冲区，然后`fd_pos`就表示管道的打开数

大致实现结构如下：
![](http://imgsrc.baidu.com/forum/pic/item/574e9258d109b3de7a6baaaf89bf6c81810a4c25.jpg)

### 3.管道的实现
首先我们到唤醒缓冲区补充一下基本函数，他是为了获取当前缓冲区数据大小
```
/* 返回环形缓冲区中的数据长度 */
uint32_t ioq_length(struct ioqueue* ioq){
  uint32_t len = 0;
  if(ioq->head >= ioq->tail){
    len = ioq->head - ioq->tail;
  }else{
    len = bufsize - (ioq->tail - ioq->head);
  }
  return len;
}

```

然后我们来定义管道的函数，我们创建函数shell/pipe.c，如下：
```
#include "pipe.h"
#include "memory.h"
#include "fs.h"
#include "file.h"
#include "ioqueue.h"
#include "thread.h"

/* 判断文件描述符local_fd是否管道 */
bool is_pipe(uint32_t local_fd){
  uint32_t global_fd = fd_local2global(local_fd);
  return file_table[global_fd].fd_flag == PIPE_FLAG;
}

/* 创建管道，成功返回0,失败返回-1 */
int32_t sys_pipe(int32_t pipefd[2]){
  int32_t global_fd = get_free_slot_in_global();    //返回全局文件表空闲位
  /* 申请一页内核内存做环形缓冲区 */
  file_table[global_fd].fd_inode = get_kernel_pages[1];
  /* 初始化环形缓冲区 */
  ioqueue_init((struct ioqueue*)file_table[global_fd].fd_inode);
  if(file_table[global_fd].fd_inode == NULL){
    return -1;
  }
  /* 将fd_flag复用为管道标志 */
  file_table[global_fd].fd_flag = PIPE_FLAG;
  /* 将fd_pos复用为管道打开数 */
  file_table[global_fd].fd_pos = 2;
  pipefd[0] = pcb_fd_install(global_fd);
  pipefd[1] = pcb_fd_install(global_fd);
  return 0;
}

/* 从管道当中读取数据 */
uint32_t pipe_read(int32_t fd, void* buf, uint32_t count){
  char* buffer = buf;
  uint32_t bytes_read = 0;
  uint32_t global_fd = fd_local2global(fd);
  /* 获取管道的环形缓冲区 */
  struct ioqueue* ioq = (struct ioqueue*)file_table[global_fd].fd_inode;
  /* 选择较小的数据写入量、避免阻塞 */
  uint32_t ioq_len = ioq_length(ioq);
  uint32_t size = ioq_len > count ? count : ioq_len;
  while(bytes_read < size){
    *buffer = ioq_getchar(ioq);
    bytes_read++;
    buffer++;
  }
  return bytes_read;
}

/* 从管道当中写入数据 */
uint32_t pipe_write(int32_t fd, const void* buf, uint32_t count){
  uint32_t bytes_write = 0;
  uint32_t global_fd = fd_local2global(fd);
  /* 获取管道的环形缓冲区 */
  struct ioqueue* ioq = (struct ioqueue*)file_table[global_fd].fd_inode;
  /* 选择较小的数据写入量、避免阻塞 */
  uint32_t ioq_left = bufsize - ioq_length(ioq);
  uint32_t size = ioq_len > count ? count : ioq_left;
  const char* buffer = buf;
  while(bytes_write < size){
    ioq_putchar(ioq, *buffer);
    bytes_write++;
    buffer++;
  }
  return bytes_write;
}

```

这里因为我们需要避免写入过多或者读取过多因此每次都是使用较小的数据进行读写，因此不会发生缓冲区满或空导致阻塞的情况。
我们将管道操作函数完成后就需要修改文件系统相关代码了，我们修改fs/fs.c
```
/* 从文件描述符fd指向的文件中读取count个字节到buf,若成功则返回读出的字节数,到文件尾则返回-1 */
int32_t sys_read(int32_t fd, void* buf, uint32_t count) {
  ASSERT(buf != NULL);
  int32_t ret = -1;
  uint32_t global_fd = 0;
  if(fd < 0 || fd == stdout_no || fd == stderr_no) {
    printk("sys_read: fd error\n");
  }else if (fd == stdin_no) {   //如果是从键盘获取
    /* 标准输入有可能被重定向为管道缓冲区 */
    if(is_pipe(fd)){
      ret = pipe_read(fd, buf, count);
    }else{
      char* buffer = buf;
      uint32_t bytes_read = 0;
      while (bytes_read < count) {
	      *buffer = ioq_getchar(&kbd_buf);
	      bytes_read++;
	      buffer++;
      }
      ret = (bytes_read == 0 ? -1 : (int32_t)bytes_read);
    }
  }else if(is_pipe(fd)){    /* 若是管道就调用管道 */
    ret = pipe_read(fd, buf, count);
  }else{
    uint32_t _fd = fd_local2global(fd);
    ret = file_read(&file_table[global_fd], buf, count);
  }
  return ret;
}

int32_t sys_write(int32_t fd, const void* buf, uint32_t count) {
  if (fd < 0) {
    printk("sys_write: fd error\n");
    return -1;
   }
  if (fd == stdout_no) {  
    char tmp_buf[1024] = {0};
    memcpy(tmp_buf, buf, count);
    console_put_str(tmp_buf);
    return count;
  }else if(is_pipe(fd)){   /* 若是管道就调用管道方法 */
    return pipe_write(fd, buf, count);
  }else{    //普通文件
    uint32_t _fd = fd_local2global(fd);
    struct file* wr_file = &file_table[_fd];
    if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR) {
      uint32_t bytes_written  = file_write(wr_file, buf, count);
      return bytes_written;
    }else{
      console_put_str("sys_write: not allowed to write file without flag O_RDWR or O_WRONLY\n");
      return -1;
    }
  }
}

int32_t sys_close(int32_t fd) {
  int32_t ret = -1;   // 返回值默认为-1,即失败
   if (fd > 2) {
      uint32_t global_fd = fd_local2global(fd);
      if(is_pipe(fd)){
        /* 如果此管道上的描述符都被关闭，释放管道的环形缓冲区 */
        if(--file_table[global_fd].fd_pos == 0){
          mfree_page(PF_KERNEL, file_table[global_fd].fd_inode, 1);
          file_table[global_fd].fd_inode = NULL;
        }
        ret = 0;
      }else{
        ret = file_close(&file_table[global_fd]);
      }
      running_thread()->fd_table[fd] = -1; // 使该文件描述符位可用
    }
  return ret;
}

```

由于我们的管道也算文件，因此我们需要修改其中的read，write以及close,然后还有就是我们这个算是匿名管道，是由父子进程共享的，因此在fork也要增加管道的打开数，我们修改fork.c如下
```
/* 更新inode打开数 */
static void update_inode_open_cnts(struct task_struct* thread) {
  int32_t local_fd = 3, global_fd = 0;
  while (local_fd < MAX_FILES_OPEN_PER_PROC) {
    global_fd = thread->fd_table[local_fd];
    ASSERT(global_fd < MAX_FILE_OPEN);
    if (global_fd != -1) {
      if(is_pipe(local_fd)){
        file_table[global_fd].fd_pos++;
      }else{
	      file_table[global_fd].fd_inode->i_open_cnts++;
      }
    }
    local_fd++;
  }
}

```

由于有了管道，我们还需要到wait_exit.c当中修改一下
```
  /* 关闭进程打开的文件 */
  uint8_t local_fd = 3;
  while(local_fd < MAX_FILES_OPEN_PER_PROC){
    if(release_thread->fd_table[local_fd] != -1){
      if(is_pipe(local_fd)){
        uint32_t global_fd = fd_local2global(local_fd);
        if(--file_table[global_fd].fd_pos == 0){
          mfree_page(PF_KERNEL, file_table[global_fd].fd_inode, 1);
          file_table[global_fd].fd_inode = NULL;
        }
      }else{
        sys_close(local_fd);
      }
    }
    local_fd++;
  }
```

我们在收回子进程资源的时候管道这里记得判断一下，就是如此，然后pipe的系统调用我们自行加上就可以了

### 4.利用管道实现通信
我们现在来编写用户进程来验证一下父子之间的通信，如下
```
#include "stdio.h"
#include "syscall.h"
#include "string.h"
int main(int argc, char** argv) {
   int32_t fd[2] = {-1};
   pipe(fd);
   int32_t pid = fork();
   if(pid) {	  // 父进程
      close(fd[0]);  // 关闭输入
      write(fd[1], "Hi, my son, I love you!", 24);
      printf("\nI`m father, my pid is %d\n", getpid());
      return 8;
   } else {
      close(fd[1]);  // 关闭输出
      char buf[32] = {0};
      read(fd[0], buf, 24);
      printf("\nI`m child, my pid is %d\n", getpid());
      printf("I`m child, my father said to me: \"%s\"\n", buf);
      return 9;
   }
}

```

这里就是父进程往管道里面写入数据，然后子进程读出，然后我们按照原先的方法写入文件系统
然后我们运行虚拟机来查看结果：
![](http://imgsrc.baidu.com/forum/pic/item/d50735fae6cd7b89bf6671444a2442a7d8330eea.jpg)
可以看到正如我们所料，完全是一致的，这里父进程首先返回，然后子进程就变成了孤儿进程，接下来就会过继给init进程领养，这里比较重要的一点是子进程确实从管道获取了父进程发给他的消息。


### 5.在shell中支持管道
我们平时Linux操作中肯定少不了"cat|grep"操作，这个竖线表示的就是管道，也就是通过这个管道我们实现数据过滤功能，这里之所以可以这样使用是因为我们增加了输入输出重定向的功能，大伙应该还记得刚刚我们修改read和write系统调用的时候在标准情况下还是加上了管道的判断，这就是为了现在这一步，接下来我们来完善shell/pipe.c

```
/* 将文件描述符old_local_fd重定位为new_local_fd */
void sys_fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd){
  struct task_struct* cur = running_thread();
  /* 针对恢复标准描述符 */
  if(new_local_fd < 3){
    cur->fd_table[old_local_fd] = new_local_fd;
  }else{
    uint32_t new_global_fd = cur->fd_table[new_local_fd];
    cur->fd_table[old_local_fd] = new_global_fd;
  }
}


```

这里就是修改本进程的表而已，然后我们在shell.c当中新增代码,这里注意上面的我们还需要添加个系统调用。下面就是shell的新增代码，如下：
```
/* 简单的shell */
void my_shell(void){
  cwd_cache[0] = '/';
  while(1){
    print_prompt();
    memset(cmd_line, 0, MAX_PATH_LEN);
    memset(final_path, 0, MAX_PATH_LEN);
    readline(cmd_line, MAX_PATH_LEN);
    if(cmd_line[0] == 0){ //如果只输入了一个回车
      continue;
    }
    /* 针对管道的处理 */
    char* pipe_symbol = strchr(cmd_line, '|');  //获取第一个管道地址
    if(pipe_symbol){
      /* 支持多重管道操作，如cmd1|cmd2|..|cmdn,
       * 这里cmd1和cmdn的标准输出输入需要单独处理*/
      /* 1 生成管道 */
      int32_t fd[2] = {-1};     //fd[0]输入，fd[1]输出
      pipe(fd);
      /* 将标准输出重定向到fd[1],使后面的输出信息重定向到内核环形缓冲区 */
      fd_redirect(1, fd[1]);
      /* 2 第一个命令 */
      char* each_cmd = cmd_line;
      pipe_symbol = strchr(each_cmd, '|');
      *pipe_symbol = 0;
      /* 执行第一个命令，命令的输出会写入环形缓冲区 */
      argc = -1;
      argc = cmd_parse(each_cmd, argv, ' ');
      cmd_execute(argc, argv);  //由于子进程会拷贝父进程信息，所以这里一起重定向了
      /* 跨过'|',处理下一个命令 */
      each_cmd = pipe_symbol + 1;
      /* 将标准输入重定向到fd[0],使之指向内核环形缓冲区 */
      fd_redirct(0, fd[0]);
      /* 3 中间的命令，输入和输出都是指向环形缓冲区 */
      while((pipe_symbol = strchr(each_cmd, '|'))){
        *pipe_symbol = 0;
        argc = -1;
        argc = cmd_parse(each_cmd, argv, ' ');
        cmd_execute(argc, argv);
        each_cmd = pipe_symbol + 1;
      }
      /* 4 处理管道中最后一个命令 */
      /* 这里将标准输出恢复屏幕 */
      fd_redirect(1, 1);
      /* 执行最后一个命令 */
      argc = -1;
      argc = cmd_parse(each_cmd, argv, ' ');
      cmd_execute(argc, argv);

      /* 5 将标准输入恢复为键盘 */
      fd_redirect(0, 0);
      /* 6 关闭管道 */
      close(fd[0]);
      close(fd[1]);
    }else{  //一般的无管道操作
      argc = -1;
      argc = cmd_parse(cmd_line, argv, ' ');
      if(argc == -1){
        printf("num of argument exceed %d\n", MAX_ARG_NR);
        continue;
      }
      cmd_execute(argc, argv);
    }
  }
  panic("my_shell: should not be here");
}

```

这是将命令的选择和执行单独封装为`cmd_execute`，然后根据管道符号`|`依次进行重定位。接下来我们修改一下cat命令来方便测试管道shell
```
#include "syscall.h"
#include "stdio.h"
#include "string.h"
int main(int argc, char** argv){
  if(argc > 2){
    printf("cat: only support 1 argument.\neg: cat filename\n");
    exit(-2);
  }
  if(argc == 1){
    char buf[512] = {0};
    read(0, buf, 512);
    printf("%s", buf);
    exit(0);
  }
  int buf_size = 1024;
  char abs_path[512] = {0};
  void* buf = malloc(buf_size);
  if(buf == NULL){
    printf("cat: malloc memory failed\n");
    return -1;
  }
  if(argv[1][0] != '/'){    //如果输入的不是绝对路径
    getcwd(abs_path, 512);
    strcat(abs_path, "/");
    strcat(abs_path, argv[1]);
  }else{
    strcpy(abs_path, argv[1]);
  }
  int fd = open(abs_path, O_RDONLY);
  if(fd == -1){
    printf("cat: open: open %s failed\n",argv[1]);
    return -1;
  }
  int read_bytes = 0;
  while(1){
    read_bytes = read(fd, buf, buf_size);
    if(read_bytes == -1){
      break;
    }
    write(1, buf, read_bytes);
  }
  free(buf);
  close(fd);
  return 66;
}

```

这里只需要添加一个简单的判断即可，因为当只有一个cat的时候我们的标准输入输出已经被重定向，所以这里我们直接输出直接读标准fd就行了。这里我们还需要重新用compile.sh来重新编译。然后写入文件系统。
最后我们实现一个简单的帮助系统调用，实现在fs/fs.c中，这样能帮助我们更贴近现实的操作系统，如下
```
/* 显示系统支持的内部命令 */
void sys_help(void){
  printk("buildin commands:\n\
      ls: show directory of file\n\
      cd: change curent work directory\
      mkdir: create a directory\
      rmdir: remove a empty directory\n\
      rm: remove a regular file\n\
      pwd: show current work directory\n\
      ps: show process information\n\
      clear: clear screen\n\
      shortcut key:\n\
      ctrl+l: clear screen\n\
      ctrl+u: clear input\n\n");
}

```

可以看到这是个简单到不能再简单的系统调用，然后我们立刻来最后一次上机测试
![](http://imgsrc.baidu.com/forum/pic/item/b90e7bec54e736d192bbc6cfde504fc2d4626949.jpg)
从中可以看到我们使用`ls -l|cat|./cat|../cat`来不断测试发现确实照常输出了目录内容

## 0x03 总结
今天对于管道大家可能会有更深的认识，这里我们整个操作系统已经基本上完善了，这也加深了我对于内核的认识，不知道有没有人跟我一起实现到这里，不过之前有位友友跟我一起实现但是后面不知道是不是有事还是什么没看到动静了，但是整体跟下来对于操作系统确实不会局限与以往的书本了，还是挺有用的。终于整个操作系统实现完毕，这两天加班是真累，等会儿出门吃个烤鸡晚上回来写一篇整体总结，就这样了。
