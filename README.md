## 0x00 基础知识们
基础知识这里需要讲的不多，我们都知道系统调用吧，使用过的同学可能知道这是用户使用内核资源的一种方式，他的底层过程就是先将系统调用号写入eax(咱们目前是32位系统)，然后使用int 0x80中断指令来进行系统调用，但是大伙可能不清楚的是这里只是C库提供的系统调用接口，我们可以使用以下命令来查看：
```
man syscall
```

我们可以看到如下帮助手册
![](http://imgsrc.baidu.com/forum/pic/item/9213b07eca806538b7eaf9d5d2dda144ac3482f0.jpg)
下面还有例子
![](http://imgsrc.baidu.com/forum/pic/item/3ac79f3df8dcd100ace7a5e7378b4710b8122ff8.jpg)
上面的方式可能就是咱们系统编程的时候一直使用的方式。
这里我们看到一个indirect说明这个syscall是间接方式，所以说这里咱们自然还存在直接方式啦，使用下面指令：
```
man _syscall
```
![](http://imgsrc.baidu.com/forum/pic/item/43a7d933c895d143931db9e536f082025baf0788.jpg)
这里我们看到了without library support，说明咱们摆脱了库的支持，所以是直接使用的系统调用，但是后面还有个OBSOLETE，这个单词是过时的的意思，至于为什么直接的过时了，这是因为使用直接的系统调用我们用户需要自行传递很多参数，而正常情况下这些参数十分影响我们正常的编程，所以现在大多数都是采用上面的间接系统调用，咱们只需要传递中断调用号就行了。
还有个点我得提出来一下，那就是Linux中参数传递是采用寄存器的，这里大伙可能会有这么点疑问，就是64位好像确实使用寄存器传递参数，但是32位似乎是使用栈来传递参数吧，有这种疑问说明您平时的一些基础知识十分扎实，但是对于底层的了解少了点，我们平时不论是在编程或者说是pwn的过程中都是使用了库函数，在这一层我们确实是使用栈（64位前6个参数是寄存器，后面是栈），但是到了底层，我们都是使用寄存器进行参数传递，一方面是因为寄存器肯定快于位于内存的栈，另一方面是因为涉及到用户系统调用的栈切换，当位于内核栈我们还需要从用户栈上获取其中的参数的话会十分繁琐，因此在底层我们采取寄存器传参，这一点在我们编写自己的打印函数put_char的时候大伙肯定有所体会。

## 0x01 系统调用实现
基础知识讲解的十分快速，接下来我们正式来实现系统调用，一个系统功能调用咱们需要分为两个部分，一部分是暴露给用户进程的接口函数，它属于用户空间，而另一部分是与之对应的内核具体实现函数，他才正式做了用户需要效果的函数，为了区分这两个函数我们一般使用函数名前加上`sys_`_来进行区分，比如`getpid()`是咱们用户可以调用的获取pid的函数，但是内核执行的函数名就是`sys__getpid()`,由他来真正获取内核pid来返回给用户。
这里我们简单梳理一下系统调用的实现思路：
1. 用中断门实现系统调用，使用0x80号中断作为系统调用号，使用这个没什么特殊含义，仅仅是熟悉了
2. 在IDT中安装0x80对应的中断描述符，在这里面注册中断处理例程，这里咱们之前已经写了时钟中断和键盘中断，所以这里对大家肯定很好理解
3. 建立系统调用子功能表syscall_table, 利用eax寄存器中的子功能号在该表中索引相应的处理函数
4. 用宏实现用户空间系统调用接口_syscall, 最大支持3个参数的系统调用。其中eax用来保存功能号，ebx保存第一个参数，之后按顺序为ecx， edx。

### 1.增加0x80号中断描述符
这里简单的呀皮啊，直接该interrupt.c就行啦，咱们将中断数增加到0x81,然后增加如下代码即可
```
extern uint32_t syscall_handler(void);              //单独的系统调用中断处理函数例程
/*初始化中断描述符表*/
static void idt_desc_init(void){
  int i, lastindex = IDT_DESC_CNT -1;
  for(i = 0;i < IDT_DESC_CNT; i++){
    make_idt_desc(&idt[i],IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
  }

  /* 单独处理系统调用，因为这里要使得用户能直接使用，所以系统调用对应的中断门dpl应为3,
   * 中断处理程序为单独的syscall_handler*/
  make_idt_desc(&idt[lastindex], IDT_DESC_ATTR_DPL3, syscall_handler);
  put_str("  idt_desc_init_done!\n");
}

```

### 2.实现系统调用接口
我们将调用接口的宏实现到函数lib/user/syscall.c当中：
```
#include "syscall.h"

/* 无参数的系统调用 */
#define _syscall0(NUMBER) ({    \
    int retval;                 \
    asm volatile(               \
        "int 0x80"              \
        : "=a"(retval)          \
        : "a"(NUMBER)           \
        : "memory"              \
        );                      \
    retval;                     \
    })

/* 一个参数的系统调用 */
#define _syscall0(NUMBER, ARG1) ({    \
    int retval;                 \
    asm volatile(               \
        "int 0x80"              \
        : "=a"(retval)          \
        : "a"(NUMBER), "b"(ARG1)           \
        : "memory"              \
        );                      \
    retval;                     \
    })

/* 两个参数的系统调用 */
#define _syscall0(NUMBER, ARG1, ARG2) ({    \
    int retval;                 \
    asm volatile(               \
        "int 0x80"              \
        : "=a"(retval)          \
        : "a"(NUMBER), "b"(ARG1), "c"(ARG2)           \
        : "memory"              \
        );                      \
    retval;                     \
    })

/* 三个参数的系统调用 */
#define _syscall0(NUMBER, ARG1, ARG2, ARG3) ({    \
    int retval;                 \
    asm volatile(               \
        "int 0x80"              \
        : "=a"(retval)          \
        : "a"(NUMBER), "b"(ARG1), "c"(ARG2), "d"(ARG3)           \
        : "memory"              \
        );                      \
    retval;                     \
    })

```
由于咱们最多支持3个参数，所以这里咱们定义4个宏，这里咱们使用大括号中最后一个语句的值会当作返回值，剩下的就不需多说了。

### 3.增加中断0x80处理例程
在我们修改interrupt.c中引用了一个外部函数syscall_handler,在这里我们就到kernel.S中实现他
```
;;;;;;;;;;;;;;; 0x80号中断 ;;;;;;;;;;;;;;;;;
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
;1 保存上下文环境
  push 0    ;压入0,使得栈中格式统一
  push ds
  push es
  push fs
  push gs
  pushad    ;PUSHAD压入8个寄存器，类似上面的中断函数例程
  push 0x80     ;同样为了保持统一的栈格式

;2 为系统调用子功能传入参数
  push edx  ;第3个参数，下面依次递减
  push ecx
  push ebx

;3 调用子功能处理函数
  call [syscall_table + eax*4]
  add esp, 12   ;跨过上面的三个参数

;4 将call调用后的返回值存入当前内核栈中的eax的位置
  mov [esp + 8*4], eax
  jmp intr_exit     ;恢复上下文


```

添加的函数十分简单，就是调用我们即将实现的子功能处理函数，这里注意我们的abi约定是将返回值存入eax，在这里我们将eax的值存入之前保存eax值的栈上面，然后最后调用恢复函数。
之后我们顺势来构造syscall_table并且初始化其中的值，我们构造函数userprog/syscall-init.c

```
#include "syscall-init.h"
#include "print.h"
#include "syscall.h"
#include "thread.h"
#define syscall_nr 32
typedef void* syscall;
syscall syscall_table[syscall_nr];

/* 返回当前任务的pid */
uint32_t sys_getpid(void){
  return running_thread()->pid;
}

/* 初始化系统调用 */
void syscall_init(void){
  put_str("syscall_init_start\n");
  syscall_table[SYS_GETPID] = sys_getpid;
  put_str("syscall_init done\n");
}
```

这里我们是只定义了最大子功能个数，为32,然后就是sys_getpid()函数了，十分简单，但是我们目前还需要修改thread.h函数,其中也仅仅在task_struct中添加了`pid_t pid`,这里的pid_t就是类型int16_t, 这里是用typedef
定义了。然后我们再到thread.c中添加初始化函数，这里我们需要初始化的时候添加pid的初始化，然后添加以下函数
```
struct lock pid_lock;       //分配pid锁

/* 分配pid */
static pid_t allocate_pid(void){
  static pid_t next_pid = 0;        //静态局部变量，相似于全局变量，所以这里函数结束后并不会消失
  lock_acquire(&pid_lock);
  next_pid++;
  lock_release(&pid_lock);
  return next_pid;
}

```

这里为了防止多个任务同时分配pid，所以我们需要定义一个pid锁来进行互斥。

### 4.添加系统调用getpid
这里添加lib/user/syscall.h
```
#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H
#include "stdint.h"
enum SYSCALL_NR{
  SYS_GETPID
};
uint32_t getpid(void);
#endif

```

其中定义了一个枚举类型，这里就是getpid的功能号为0.之后就是实现函数syscall.c中添加对应的函数实现
```
/* 返回当前任务的pid */
uint32_t getpid(){
  return _syscall0(SYS_GETPID);
}

```

这里我们仅仅是调用了刚刚咱们定义的宏函数.

### 5 测试系统调用
这里我们修改main函数来测试，如下代码：
```
#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
int prog_a_pid = 0, prog_b_pid = 0;

int main(void){
  put_str("I am Kernel\n");
  init_all();

  process_execute(u_prog_a, "user_prog_a");
  process_execute(u_prog_b, "user_prog_b");

  intr_enable();
  console_put_str(" main_pid:0x");
  console_put_int(sys_getpid());
  console_put_char('\n');

  thread_start("k_thread_a", 31, k_thread_a, " A_");
  thread_start("k_thread_b", 31, k_thread_b, " B_");
  while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  console_put_str(" thread_a_pid:0x");
  console_put_int(sys_getpid());
  console_put_char('\n');
  console_put_str(" prog_a_pid:0x");
  console_put_int(prog_a_pid);
  console_put_char('\n');
  while(1);
}
void k_thread_b(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  console_put_str(" thread_b_pid:0x");
  console_put_int(sys_getpid());
  console_put_char('\n');
  console_put_str(" prog_b_pid:0x");
  console_put_int(prog_b_pid);
  console_put_char('\n');
  while(1);
}

void u_prog_a(void){
  prog_a_pid = getpid();
  while(1);
}

void u_prog_b(void){
  prog_b_pid = getpid();
  while(1);
}

```

这里十分逻辑十分简单，那就是内核线程照例使用sys_getpid()函数来获取pid打印，而用户进程则使用getpid();来进行系统调用，然后最后会执行sys_getpid()，但是目前他还不会打印，所以让另外两个线程帮忙打印，下面就是咱们的实现结果：
![](http://imgsrc.baidu.com/forum/pic/item/50da81cb39dbb6fd791bbbe04c24ab18962b37b7.jpg)
这里看出我们确实打印出来了各自的pid，十分顺利。（这里pid的值从1开始的原因在以后我们讲解pid的时候再次进行解释）

## 0x02 实现进程打印的好帮手-printf
接下来大伙可能会碰到很多老朋友了，比如说printf，write,stdio等，大家以前应该就了解过，所以接下来的工作不要太简单。
首先我们知道printf跟咱们内核实现的put_str肯定是有所不同的，其中还包括了格式化的功能，也就是%d等，然后解析相应参数。而最终真正实现打印的是咱们的write系统调用，这里由于目前我们并没有实现文件系统，所以先编写一个简单的write系统调用，这里我们跟上面添加getpid系统调用即可。
首先是修改一下syscall.h,在里面的枚举类型中添加一下SYS_WRITE,这里比较简单就不贴代码，然后我们到syscall.c中添加相应函数
```
/* 系统调用write */
uint32_t write(char* str){
  return _syscall1(SYS_WRITE, str);
}

```

之后我们再修改syscall-init.c即可
```
/* 打印字符串（未实现文件系统版本） */
uint32_t sys_write(char* str){
  console_put_str(str);
  return strlen(str);
}

```

这里还是十分简单的，接下来正式开始咱们的格式化工作。
### 1.vsprintf
调用printf后真正实现格式化的工作的是vsprintf，这里我们使用man vsprintf来查看他的功能。
![](http://imgsrc.baidu.com/forum/pic/item/9213b07eca806538b888fed5d2dda144ac348292.jpg)
该函数的功能就是把ap指向的可变参数，以字符串格式format中的符号%为替换标记，不修改原格式字符串format，将format中除了%类型字符以外的内容复制到str，解释完之后我们立刻开始实现，我们创建文件lib/stdio.c
```
#include "stdio.h"
#include "stdint.h"
#define va_start(ap, v) ap = (va_list)&v    //把ap指向第一个固定参数v
#define va_arg(ap, t) *((t*)(ap += 4))      //ap指向下一个参数并返回其值，由于是32位程序，所以栈上一个参数占4字节
#define va_end(ap) ap = NULL                //清楚ap

/* 将整型转换成字符 */
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base){
  uint32_t m = value % base;        //取余
  uint32_t i = value / base;        //取整
  if(i){                            //如果倍数部位0,则递归调用
    itoa(i, buf_ptr_addr, base);
  }
  if(m < 10){                       //如果余数是0～9
    *((*buf_ptr_addr)++) = m + '0';     //将数字0～9转化为字符'0'～'9'
  }else{
    *((*buf_ptr_addr)++) = m -10 + 'A'; //将数字A~F转化为字符'A'～'F'
  }
}

/* 将参数ap按照格式format输出到字符串str，并返回替换后的str长度 */
uint32_t vsprintf(char* str, const char* format, va_list ap){
  char* buf_ptr = str;
  const char* index_ptr = format;
  char index_char = *index_ptr;
  int32_t arg_int;
  while(index_char){
    if(index_char != '%'){
      *(buf_ptr++) = index_char;
      index_char = *(++index_ptr);
      continue;
    }
    index_char = *(++index_ptr);    //获取%后面的字符
    switch(index_char){
      case 'x':
        arg_int = va_arg(ap, int);  //最开始ap是指向format，然后每次取下一个参数
        itoa(arg_int, &buf_ptr, 16);
        index_char = *(++index_ptr); //跳过格式字符并且更新index_char
        break;
    }
  }
  return strlen(str);
}

/* 格式化输出字符串format */
uint32_t printf(const char* format, ...){
  va_list args;
  vs_start(args, format);
  char buf[1024] = {0};
  vsprintf(buf, format, args);
  va_end(args);
  return write(buf);
}

```

这里的函数个人认为还是仔细点看比较好，其中va_list我是在stdio.h中定义为`char *`变量，这里我们的工作就是将原本的格式化字符串一个字符一个字符的写入我们提前预留的字符缓冲区当中，然后进行write系统调用，接下来我们就来main函数中测试一下：
```
void k_thread_b(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  console_put_str(" thread_b_pid:0x");
  console_put_int(sys_getpid());
  console_put_char('\n');
  while(1);
}

void u_prog_a(void){
  printf(" prog_a_pid:0x%x\n",getpid());
  while(1);
}

void u_prog_b(void){
  printf(" prog_b_pid:0x%x\n",getpid());
  while(1);
}

```

这里可以看到咱们进程不再依赖于线程打印了，而是使用printf进行输出，我们来看看效果
![](http://imgsrc.baidu.com/forum/pic/item/8ad4b31c8701a18bd8365789db2f07082938fe13.jpg)
这里可以看到咱们进程自己实现的pid成功打印！！

### 2.完善printf
上面我们在vsprintf当中仅仅处理了%x,但其他的类型却没有实现，这里我们将他们补充完整，如下：
```
/* 将参数ap按照格式format输出到字符串str，并返回替换后的str长度 */
uint32_t vsprintf(char* str, const char* format, va_list ap){
  char* buf_ptr = str;
  const char* index_ptr = format;
  char index_char = *index_ptr;
  int32_t arg_int;
  char* arg_str;
  while(index_char){
    if(index_char != '%'){
      *(buf_ptr++) = index_char;
      index_char = *(++index_ptr);
      continue;
    }
    index_char = *(++index_ptr);    //获取%后面的字符
    switch(index_char){
      case 'x':             //十六进制打印
        arg_int = va_arg(ap, int);  //最开始ap是指向format，然后每次取下一个参数
        itoa(arg_int, &buf_ptr, 16);
        index_char = *(++index_ptr); //跳过格式字符并且更新index_char
        break;
      case 's':             //字符串打印
        arg_str = va_arg(ap, char*);
        strcpy(buf_ptr, arg_str);
        buf_ptr += strlen(arg_str);
        index_char = *(++index_ptr);
        break;
      case 'c':
        *(buf_ptr++) = va_arg(ap, char);
        index_char = *(++index_ptr);
        break;
      case 'd':
        arg_int = va_arg(ap, int);
        /* 如果是负数，将其转换为正数后，在正数前面输出一个负号‘-’ */
        if(arg_int < 0){
          arg_int = 0 - arg_int;
          *buf_ptr++ = '-';
        }
        itoa(arg_int, &buf_ptr, 10);
        index_char = *(++index_ptr);
        break;
    }
  }
  return strlen(str);
}

```


我们使用printf测试一下：
```
  printf(" prog_a_pid:0x%x,and integer is %d, string is %s ,char is %c\n",getpid(),22,"hello world", 'S');
```
结果仍然十分成功
![](http://imgsrc.baidu.com/forum/pic/item/f3d3572c11dfa9ecf93112d127d0f703908fc1dd.jpg)


## 0x03 堆内存管理
首先我们需要了解一点arena的知识，因为咱们目前分配的粒度是4KB，但是真正用户进程开始工作之后一般就是多少字节的分配，所以咱们必须实现更小的粒度分配，因此咱们有必要对于这种情况进行管理，大伙可能都对main_arena比较熟悉，这里我们仅仅实现简单的版本，每一个arena都提供同种大小的内存块供用户分配。所以我们只需要知道他是一个内存供应商就行了.
### 1.底层初始化
首先我们先到memory.h中添加相应的数据结构：
```
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
```

这里free_list是存放对应arena中空闲的块链表。然后就是添加的memory.c函数
```
/* 内存仓库 */
struct arena{
  struct mem_block_desc* desc;  //此arena关联的mem_block_desc
  /* large为true时，cnt表示的是页框数。
   * 否则cnt表示空闲的mem_block数量 */
  uint32_t cnt;
  bool large;
};

struct mem_block_desc k_block_descs[DESC_CNT];  //内核内存块描述符数组

/* 为malloc做准备 */
void block_desc_init(struct mem_block_desc* desc_array){
  uint16_t desc_idx,block_size = 16;
  /* 初始化每个mem_block_desc描述符 */
  for(desc_idx = 0; desc_idx < DESC_CNT; desc_idx++){
    desc_array[desc_idx].block_size = block_size;
    /* 初始化arena中的内存块数量 */
    desc_array[desc_idx].block_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;
    list_init(&desc_array[desc_idx].free_list);
    block_size *= 2;        //更新为下一个规格内存块
  }
}

/* 内存管理部分初始化入口 */
void mem_init(){
  put_str("mem_init start\n");
  uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));     //这里是咱们之前loader.S中存放物理总内存的地址
  mem_pool_init(mem_bytes_total);
  /* 初始化mem_block_desc数组descs，为malloc做准备 */
  block_desc_init(k_block_descs);
  put_str("mem_init done\n");
}

```

这里给出一个图来解释mem_block_desc和arena和mem_block的关系
![](http://imgsrc.baidu.com/forum/pic/item/0df3d7ca7bcb0a467954cdff2e63f6246a60af86.jpg)
也就是说arena是存在于已分配的页面中的，所以这里我们内存块的数量需要减去他结构体的大小再进行计算,其余就是一些普通的初始化过程。

### 2.实现sys_malloc
大伙一看前面的sys_应该就反应到需要改写syscall了，sys_malloc的功能是分配并维护内存块资源，动态创建arena以满足内存块的分配，为了完成sys_malloc，我们这里还需要做一些细小的改动,首先就是让咱们的线程以及用户进程都支持内存管理，所以先添加下面task_struct中的变量，然后在process.c中添加初始化资源即可（内核管理模块我们在memory.c已经写了）
```
struct mem_block_desc u_block_desc[DESC_CNT];     //用户进程的内存块描述符
```

然后就是最繁琐的memory.c的改进:
```
/* 返回arena中第idx个内存块的地址 */
static struct mem_block* arena2block(struct arena* a, uint32_t idx){
  return (struct mem_block*) ((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

/* 返回内存块b所在的arena地址 */
static struct arena* block2arena(struct mem_block* b){
  return (struct arena*)((uint32_t)b & 0xfffff000);
}

/* 在堆中申请size字节内存 */
void* sys_malloc(uint32_t size){
  enum pool_flags PF;
  struct pool* mem_pool;
  uint32_t pool_size;
  struct mem_block_desc* descs;
  struct task_struct* cur_thread = running_thread();
  /* 判断使用哪个内存池 */
  if(cur_thread->pgdir == NULL){    //若为内核线程
    PF = PF_KERNEL;
    pool_size = kernel_pool.pool_size;
    mem_pool = &kernel_pool;
    descs = k_block_descs;
  }else{
    PF = PF_USER;
    pool_size = user_pool.pool_size;
    mem_pool = &user_pool;
    descs = cur_thread->u_block_desc;
  }

  /* 若申请的内存不再内存池容量范围内，则直接返回NULL */
  if(!(size > 0 && size < pool_size)){
    return NULL;
  }
  struct arena* a;
  struct mem_block* b;
  lock_acquire(&mem_pool->lock);
  /* 超过最大内存块，就分配页框 */
  if(size > 1024){
    uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);     //向上取整需要的页框数
    a = malloc_page(PF, page_cnt);
    if(a != NULL){
      memset(a, 0, page_cnt * PG_SIZE);     //将分配的内存清0
      /* 对于分配的大块页框，将desc置为NULL,
      * cnt置为页框数，large置为true */
      a->desc = NULL;
      a->cnt = page_cnt;
      a->large = true;
      lock_release(&mem_pool->lock);
      return (void*)(a + 1);  //跨过arena大小，把剩下的内存返回
    }else{
      lock_release();
      return NULL;
    }
  }else{    //若申请的内存小于等于1024,则可在各种规格的mem_block_desc中去适配
    uint8_t desc_idx;
    /* 从哦你内存块描述符中匹配合适的内存块规格 */
    for(desc_idx = 0; desc_idx < DESC_CNT; desc_idx++){
      if(size <= descs[desc_idx].block_size){
        break;          //从小往大找
      }
    }
    /* 若mem_block_desc的free_list中已经没有可用的mem_block,
     * 就创建新的arena提供mem_block */
    if(list_empty(&descs[desc_idx].free_list)){
      a = malloc_page(PF, 1);       //分配1页框作为arena
      if(a == NULL){
        lock_release(&mem_pool->lock);
        return NULL;
      }
      memset(a, 0, PG_SIZE);
      /* 对于分配的小块内存，将desc置为相应内存块描述符，
       * cnt置为此arena可用的内存块数 ，large置为false */
      a->desc = &descs[desc_idx];
      a->large = false;
      a->cnt = descs[desc_idx].blocks_per_arena;
      uint32_t block_idx;
      enum intr_status old_status = intr_disable();
      /* 开始将arena拆分成内存块，并添加到内存块描述符的free_list当中 */
      for(block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena; block_idx++){
        b = arena2block(a, block_idx);
        ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
        list_append(&a->desc->free_list, &b->free_elem);
      }
      intr_set_status(old_status);
    }
    /* 开始分配内存块 */
    b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_idx].free_list)));
    memset(b, 0, descs[desc_idx].block_size);
    a = block2arena(b);     //获取所在arena
    a->cnt--;
    lock_release(&mem_pool->lock);
    return (void*)b;
  }
}

```

这一长串代码我来画图结合理解：
![](http://imgsrc.baidu.com/forum/pic/item/4e4a20a4462309f712d9edd4370e0cf3d6cad646.jpg)
当然其中分配物理页框之前我们需要判断咱们此次使用的是内核线程还是用户进程，这个判断就不多说了。然后我们到main函数中试看看。我们只需要修改一下两个线程测试函数即可：
```
/* 在线程中运行的函数 */
void k_thread_a(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  void* addr = sys_malloc(33);
  console_put_str(" I am thread_a,sys_malloc(33),addr is :0x");
  console_put_int((int)addr);
  console_put_char('\n');
  while(1);
}
void k_thread_b(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  void* addr = sys_malloc(63);
  console_put_str(" I am thread_b,sys_malloc(63),addr is :0x");
  console_put_int((int)addr);
  console_put_char('\n');
  while(1);
}


```

上面就是打印出自己申请内存快的首地址，接下来我们看看效果
![](http://imgsrc.baidu.com/forum/pic/item/730e0cf3d7ca7bcb82f387e9fb096b63f724a868.jpg)
从中可以看到咱们确实正常的分配了33和63字节大小的块，并且这俩同属于64字节的arena，所以这里暂时只存在一片arena，且全是大小为64字节的block。

### 3.释放内存
我们先来回顾一下之前分配内存的过程：
1. 在虚拟地址池中分配内存，相关函数是vaddr_get()
2. 在物理内存池中分配内存，相关函数是palloc
3. 完成虚拟地址到物理地址的映射，就是填页表，相关函数是page_table_add()

上面三个函数封装在malloc_page当中，我们释放内存就是他们的逆过程，步骤如下：
1. 在物理地址池当中释放物理页地址，相关函数是pfree
2. 在页表当中去除虚拟地址映射，原理就是将pte的P位置0,相关函数是page_table_pte_remove
3. 在虚拟内存池当中释放虚拟地址，相关函数是vaddr_remove

说完思路我们立刻开始实现:
```
/* 将物理地址pg_phy_addr回收到物理内存池 */
void pfree(uint32_t pg_phy_addr){
  struct pool* mem_pool;
  uint32_t bit_idx = 0;
  if(pg_phy_addr >= user_pool.phy_addr_start){      //用户物理内存池
    mem_pool = &user_pool;
    bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
  }else{
    mem_pool = &kernel_pool;
    bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
  }
  bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

/* 去掉页表中虚拟地址vaddr的映射，只用去掉vaddr对应的pte */
static void page_table_pte_remove(uint32_t vaddr){
  uint32_t* pte = pte_ptr(vaddr);
  *pte &= ~PG_P_1;  //将页表项pte的P位置为0
  asm volatile("invlpg %0" : : "m"(vaddr) : "memory");  //更新tlb，这里因为以前的页表会存在高速缓存，现在咱们修改了所以需要刷新一下tlb对应的条目
}

/* 在虚拟地址池当中释放以_vaddr起始的连续pg_cnt个虚拟地址页 */
static void vaddr_remove(enum pool_flag pf, void* _vaddr, uint32_t pg_cnt){
  uint32_t bit_idx_start = 0, vaddr = (uint32_t)_vaddr, cnt = 0;
  if(pf == PF_KERNEL){      //虚拟内核池
    bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
    while(cnt < pg_cnt){
      bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
    }
  }else{
    struct task_struct* cur_thread = running_thread();
    bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
    while(cnt < pg_cnt){
      bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
    }
  }
}

```

函数的注释已经将需要了解的点说清楚了，剩下的代码逻辑很简单，然后我们跟之前分配一样将这三者整合到一起，再次到memory.c中添加函数
```
/* 释放虚拟地址vaddr为起始的cnt个物理页框 */
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt){
  uint32_t pg_phy_addr;
  uint32_t vaddr = (int32_t)_vaddr, page_cnt = 0;
  ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);
  pg_phy_addr = addr_v2p(vaddr);    //获取虚拟地址vaddr对应的物理地址

  /* 确保待释放的物理内存在低端1MB + 1KB大小的页目录 + 1KB大小的页表地址范围外 */
  ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);

  /* 判断pg_phy_addr属于用户物理内存池还是内核物理内存池 */
  if(pg_phy_addr >= user_pool.phy_addr_start){      //位于user_pool内存池
    vaddr -= PG_SIZE;
    while(page_cnt < pg_cnt){
      vaddr += PG_SIZE;
      pg_phy_addr = addr_v2p(vaddr);
      /* 确保此物理地址属于用户物理内存池 */
      ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);
      /* 先将对应的物理页框归还到内存池 */
      pfree(pg_phy_addr);
      /* 再从页表中清楚此虚拟地址所在的页表项pte */
      page_table_pte_remove(vaddr);
      page_cnt++;
    }
  /* 清空虚拟地址位图中的相应位 */
    vaddr_remove(pf, _vaddr, pg_cnt);
  }else{
    vaddr -= PG_SIZE;
    while(page_cnt < pg_cnt){
      vaddr += PG_SIZE;
      pg_phy_addr = addr_v2p(vaddr);
      /* 确保此物理地址属于内核物理内存池 */
      ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr < user_pool.phy_addr_start);
      /* 先将对应的物理页框归还到内存池 */
      pfree(pg_phy_addr);
      /* 再从页表中清楚此虚拟地址所在的页表项pte */
      page_table_pte_remove(vaddr);
      page_cnt++;
    }
    /* 清空虚拟地址位图中的相应位 */
    vaddr_remove(pf, _vaddr, pg_cnt);
  }
}

```

这里仅仅是将上面实现的三个函数整合起来而已，不必多说。
我们之前是觉得分配内存的粒度太大，所以实现了sys_malloc,现在我们因为同样的原因也来实现缩小粒度的sys_free，如下:
```
/* 回收内存ptr */
void sys_free(void* ptr){
  ASSERT(ptr != NULL);
  if(ptr != NULL){
    enum pool_flags PF;
    struct pool* mem_pool;

    /* 判断是线程还是进程 */
    if(running_thread()->pgdir == NULL){
      ASSERT((uint32_t)ptr > K_HEAP_START);
      PF = PF_KERNEL;
      mem_pool = &kernel_pool;
    }else{
      PF = PF_USER;
      mem_pool = &user_pool;
    }

    lock_acquire(&mem_pool->lock);
    struct mem_block* b = ptr;
    struct arena* a = block2arena(b);
    //把mem_block换成arena，获取元信息
    ASSERT(a->large == 0 || a->large == 1);
    if(a->desc == NULL && a->large == true){    //大于1024的内存
      mfree_page(PF, a, a->cnt);
    }else{                                      //小于1024的内存
      /* 先将内存块回收到free_list */
      list_append(&a->desc->free_list, &b->free_elem);
      /* 再判断arena中的块是否都空闲，若是则收回整个块 */
      if(++a->cnt == a->desc->blocks_per_arena){
        uint32_t block_idx;
        for(block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++){
          struct mem_block* b = arena2block(a, block_idx);
          ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
          list_remove(&b->free_elem);
        }
        mfree_page(PF, a, 1);
      }
    }
    lock_release(&mem_pool->lock);
  }
}

```

这里我们在碰到arena被咱们释放空的情况下要注意我们先脱链然后再释放页。这里就不跟大家进行释放的演示了，十分抽象，大家自行分配然后释放即可。

### 4.实现系统调用malloc和free
跟之前一样添加系统调用,首先就是stdio.h头文件得改
```
#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H
#include "stdint.h"
enum SYSCALL_NR{
  SYS_GETPID,
  SYS_WRITE,
  SYS_MALLOC,
  SYS_FREE
};
uint32_t getpid(void);
uint32_t write(char* str);
void* malloc(uint32_t size);
void free(void* ptr);
#endif

```

然后我们到syscall.c中添加相应函数
```
/* 系统调用malloc */
void* malloc(uint32_t size){
  return (void*)_syscall1(SYS_MALLOC, size);
}

/* 系统调用free */
void free(void* ptr){
  _syscall1(SYS_FREE, ptr);
}

```

最后别忘记到syscall-init.c中注册。
然后我们修改main函数来测试效果,下面是我们修改的main函数中的内核线程与用户进程
```
/* 在线程中运行的函数 */
void k_thread_a(void* arg){
  void* addr1 = sys_malloc(256);
  void* addr2 = sys_malloc(256);
  void* addr3 = sys_malloc(256);

  console_put_str("thread_a malloc addr:0x");
  console_put_int((int)addr1);
  console_put_char(',');
  console_put_int((int)addr2);
  console_put_char(',');
  console_put_int((int)addr3);
  console_put_char('\n');
  int cpu_delay = 100000;
  while(cpu_delay-- > 0);
  sys_free(addr1);
  sys_free(addr2);
  sys_free(addr3);
  while(1);
}
void k_thread_b(void* arg){
  void* addr1 = sys_malloc(256);
  void* addr2 = sys_malloc(256);
  void* addr3 = sys_malloc(256);

  console_put_str("thread_b malloc addr:0x");
  console_put_int((int)addr1);
  console_put_char(',');
  console_put_int((int)addr2);
  console_put_char(',');
  console_put_int((int)addr3);
  console_put_char('\n');
  int cpu_delay = 100000;
  while(cpu_delay-- > 0);
  sys_free(addr1);
  sys_free(addr2);
  sys_free(addr3);
  while(1);
}

void u_prog_a(void){
  void* addr1 = malloc(256);
  void* addr2 = malloc(256);
  void* addr3 = malloc(256);
  printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);
  int cpu_delay = 100000;
  while(cpu_delay-- > 0);
  free(addr1);
  free(addr2);
  free(addr3);
  while(1);
}

void u_prog_b(void){
  void* addr1 = malloc(256);
  void* addr2 = malloc(256);
  void* addr3 = malloc(256);
  printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);
  int cpu_delay = 100000;
  while(cpu_delay-- > 0);
  free(addr1);
  free(addr2);
  free(addr3);
  while(1);
}

```

然后我们来查看一下虚拟机中的效果
![](http://imgsrc.baidu.com/forum/pic/item/f7246b600c338744608260ba140fd9f9d62aa0b5.jpg)
这里咱们可以看到进程ab中分配的虚拟地址都是同样的，这是因为用户虚拟地址是独占的，并且我们这里一下分配256,也就是0x100,这也是十分符合咱们的分配过程，之后我们发现线程ab却不一样，这是因为线程是共享空间的。

## 0x04 总结
咱们本节完成了对于系统调用与内存管理的完善，这里大伙有兴趣的也可以多实现几个系统调用，与添加malloc等都是类似的.
