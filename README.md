## 0x00 基础知识们
基础知识这里需要讲的不多，我们都知道系统调用吧，使用过的同学可能知道这是用户使用内核资源的一种方式，他的底层过程就是先将系统调用号写入eax(咱们目前是32位系统)，然后使用int 0x80中断指令来进行系统调用，但是大伙可能不清楚的是这里只是C库提供的系统调用接口，我们可以使用以下命令来查看：
```
man syscall
```

我们可以看到如下帮助手册
![](http://imgsrc.baidu.com/super/pic/item/9213b07eca806538b7eaf9d5d2dda144ac3482f0.jpg)
下面还有例子
![](http://imgsrc.baidu.com/super/pic/item/3ac79f3df8dcd100ace7a5e7378b4710b8122ff8.jpg)
上面的方式可能就是咱们系统编程的时候一直使用的方式。
这里我们看到一个indirect说明这个syscall是间接方式，所以说这里咱们自然还存在直接方式啦，使用下面指令：
```
man _syscall
```
![](http://imgsrc.baidu.com/super/pic/item/43a7d933c895d143931db9e536f082025baf0788.jpg)
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
![](http://imgsrc.baidu.com/super/pic/item/50da81cb39dbb6fd791bbbe04c24ab18962b37b7.jpg)
这里看出我们确实打印出来了各自的pid，十分顺利。（这里pid的值从1开始的原因在以后我们讲解pid的时候再次进行解释）

## 0x02 实现进程打印的好帮手-printf
接下来大伙可能会碰到很多老朋友了，比如说printf，write,stdio等，大家以前应该就了解过，所以接下来的工作不要太简单。


