提前祝大家新年快乐！
## 0x00 基础知识们
今天我们来介绍一个新的知识那就是线程，大家如果有开发经验的话应该对这个线程都熟悉的，咱们都会通过多线程来提升我们处理器的效率，也提高了咱们程序运行的效率。这里的多线程默认是指并发，而不是并行，真正的并行是同一时间有多个线程并排运作，而并发是指在一段极短的时间内，多个线程交替使用，给人的感觉就好像这几个线程都是一起在运行。
所以我们采取的做法就是每个任务在处理器上执行一小会儿，然后使用调度器切换下一个任务到处理器上面运行。
![](http://imgsrc.baidu.com/super/pic/item/e824b899a9014c087174a7604f7b02087af4f4ac.jpg)
上面的图说明了任务调度的情况，咱们的调度器也就是实现这一功能。简单来说，咱们的调度器就是操作系统中用于把任务轮流调度上处理器运行的一个软件模块，是操作系统的一部分。调度器在内核中维护一个任务表，然后按照一定的算法将该任务放到处理器上面执行。正是有了任务调度器，多任务操作系统才能够实现。
但是多线程也不是全是好处，就跟动态链接与静态链接一样，他俩都有优势与劣势，只不过看谁的优势更大而已。这里如果实现多线程，确实满足了多个线程同时推进，避免了一个运行时间短的线程始终等待一个运行时间长的线程，但是我们使用多线程在切换线程的时候会产生一些切换的时间，这让我也想到前面我们讲过的流水线了，如果设计的不合理，则流水线还不如指令串行。
![](http://imgsrc.baidu.com/super/pic/item/dc54564e9258d109157126779458ccbf6d814d61.jpg)

### 1.线程的概念
线程如果我们简单来讲，在高级语言中，线程是运行函数的另一种方式，也就是说，构建一套线程方法，就比如C语言库中的POSIX线程库，让函数在此线程中调用，然后处理器去执行这个函数，因此线程实际的功能就是相当与调用了这个函数，从而让函数执行。
说到这里，那他和普通的函数调用有什么区别呢。
在一般的函数调用中，它是随着此函数所在的调度单元一块上处理器运行的，这个调度单元可能是这个进程，但也可能是某个线程，你可以当他是顺便执行的，也就是说咱们的处理器并不是单独的执行他。可是在线程就不一样了，他可以为一般的代码快创造他所依赖的上下文环境，从而让代码快具有独立性，因此在原理上线程能使一段函数成为调度单元，从而被调度器专门调度到处理器上执行，也就是说摆脱了函数调用时的依赖性，而真正翻身做了主人。
![](http://imgsrc.baidu.com/super/pic/item/30adcbef76094b36d9569528e6cc7cd98c109d2b.jpg)

这里我默认大家曾经学过操作系统这门课程，就简单的给个结论，如果开启了多线程，那么线程就是CPU调度的最小单位，而CPU资源分配的最小单位是进程而不是线程，也就是说线程是没有属于自己的资源的，他的一切可用资源都是从所属进程获取的，地址空间也是。
这里进程、线程、资源之间的关系可以这样表达:进程 = 线程 + 资源

### 2.进程、线程的状态
这里给出现在常用的状态，当然别忘了咱们的工作是写操作系统，所以这里咱们的状态可以自定义，并不一定非得按照他的来，这里只是给个参考。
![](http://imgsrc.baidu.com/super/pic/item/f636afc379310a554a285731f24543a9832610e3.jpg)
+ 就绪态是指该进程已经在可调度队列当中，随时可以调度他到处理器上
+ 运行态指该进程正在处理器上运行
+ 阻塞态是值该进程需要某种资源但是暂未得到，此时他无法加入调度队列进行调度

注意这里的状态是给调度器用的，而调度器他不会在意你到底属于进程还是线程，所以上述状态也使用于线程。

### 3.进程的身份证——PCB
操作系统为每个进程提供了一个PCB,Process Control Block，也就是程序控制块，用来记录与此进程相关的信息，比如进程状态、PID、优先级等。一般的PCB结构如下图所示：
![](http://imgsrc.baidu.com/super/pic/item/7dd98d1001e939015a5904fb3eec54e737d19698.jpg)

每个进程都有自己的PCB，所有PCB放到一张表格中维护，这就是进程表，调度器可以根据这张进程表来选择上处理器运行的进程。因此PCB又可以称为进程表项。进程表如下：
![](http://imgsrc.baidu.com/super/pic/item/728da9773912b31bb75445ecc318367adab4e13f.jpg)

这里里面几个比较重要的字段我拿出来单独说：
+ 进程状态：是指上面所说的运行、就绪、阻塞等，这样调度器就知道他是否可以进行调度
+ 时间片：时间片为0的时候说明这个进程在处理器上的时间已经到了，这时候需要调度器进行调度
+ 页表：表示进程的地址空间，这个是进程独享的
+ 寄存器映像：用来保存进程的现场，进程在处理器上运行的时候，所有寄存器的值都将保存在这里。
+ 0级内核特权栈位于PCB中，而寄存器映像一般保存在这里

## 0x01 内核线程的实现
在标题中我提出了一个内核线程的概念，当然还有另外一种那就是用户线程，这里我解释一下他们两个的区别：
1. 内核线程：调度器由操作系统实现，但是切换线程需要压栈出栈来保护现场恢复现场
2. 用户线程：调度器由用户实现，若一个线程阻塞，但整个进程跟着阻塞

基础知识已经讲完，这里我们立马开始实现，我们定义thread/thread.h文件
```
#ifndef __THREAD_H
#define __THREAD_H
#include "stdint.h"

/* 自定义通用函数类型，它将在很多线程函数中作为形参类型 */
typedef void thread_func(void*);

/* 进程或线程的状态 */
enum task_status{
  TASK_RUNNING,
  TASK_READY,
  TASK_BLOCKED,
  TASK_WAITING,
  TASK_HANGING,
  TASK_DIED
};

/********** 中断栈 intr_stack ***********
 * 此结构用于中断发生时保护程序（线程或进程）的上下文环境
 * 进程或线程被外部中断或软中断打断时，会按照此结构呀如上下文
 * 寄存器，intr_exit中的出栈操作是此结构的逆操作
 * 此栈在线程自己的内核栈中位置固定，所在页的最顶端
 * **************************************/
struct intr_stack{
  uint32_t vec_no;
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp_dummy;       //虽然pushad把esp也压入栈中，但esp是不断变化的，所以被popad忽略
  uint32_t ebx;
  uint32_t edx;
  uint32_t edx;
  uint32_t eax;
  uint32_t gs;
  uint32_t fs;
  uint32_t es;
  uint32_t ds;

  /* 以下由cpu从低特权级进入高特权级时压入 */
  uint32_t err_code;        //err_code会被压入eip之后
  void (*eip) (void);
  uint32_t cs;
  uint32_t eflags;
  void* esp;
  uint32_t ss;
};

/************** 线程栈thread_stack ****************
 * 线程自己的栈，用于存储线程中待执行的函数
 * 此结构在线程自己的内核栈中位置不固定
 * 仅用在switch_to时保存线程环境
 * 实际位置取决于实际运行情况
 * ************************************************/
struct thread_stack{
  uint32_t ebp;
  uint32_t ebx;
  uint32_t edi;
  uint32_t esi;

  /* 线程第一次执行的时候，eip指向待调用的函数kernel_thread
   * 其他时候，eip指向switch_to的返回地址*/
  void (*eip) (thread_func* func, void* func_arg);  //表示一个指地变量

  /******* 以下仅供第一次被调度上CPU时使用 *******/

  /* 参数unused_ret 只为占位置充数为返回地址 */
  void (*unused_retaddr);
  thread_func* function;        //由kernel_thread所调用的函数名
  void* func_arg;               //kernel_thread所调用的函数所需要的参数
};

/* 进程或线程的PCB */
struct task_struct{
  uint32_t* self_kstack;        //各内核线程都用自己的内核栈
  enum task_status status;
  uint8_t priority;             //线程优先级
  char name[16];                //任务的名字
  uint32_t stack_magic;         //栈的边界标记，用于检测栈的溢出
};


#endif

```

这里我们来看咱们定义的thread_stack结构体，我们首先定义了4个寄存器，至于为什么是4个寄存器，这里涉及到ABI的内容，也就是应用程序二进制接口，他规定了底层的一套规则，属于编译方面的约定，比如说就是参数如何传递等等。而我们这里采用的规约决定5个寄存器ebp，ebx，edi，esi和esp归主调函数使用，其余的寄存器归被调函数使用，也就是说不管被调函数是否使用了这5个寄存器，在被调函数执行完后，这5个寄存器的值不该被改变。
因此被调函数必须为主调函数保护好这5个寄存器，在被调函数运行完之后，他必须在自己的栈中存储这些寄存器的值。
这里强调ABI是因为C编译器就是按照这套ABI来编译C的，但是咱们写的汇编程序则不同，他是咱们自行定义的，因此如果我们要写一个汇编函数且通过C调用的话，那么我们必须要按照ABI的规则来写汇编才行，上面的switch_to就是咱们即将写的汇编函数，当然这里如果不放心，大伙也可以保存所有的32位通用寄存器，比如使用pushad。
上面我们没管esp是因为他的值会用调用约定来保证，因此我们不需要手动保护。

我们继续来回顾一下函数调用发生的情况：
首先我们的eip指向的是被调用函数kernel_thread函数，当kernel_thread函数开始执行的时候，我们的栈应该如下：
![](http://imgsrc.baidu.com/super/pic/item/cb8065380cd79123f1e91eb1e8345982b3b780ee.jpg)

而我们kernel_thread函数的大致作用如下：
```
kernel_thread(thread_func* func, void* func_arg){
  func(func_arg);
}
```

大伙都知道我们一般调用函数都是只能使用call，jmp，ret，这里我们的kernel_thread的调用并不是采用call，而是采用ret，也就是说kernel_thread函数作为某个函数的返回地址，通过ret指令来调用的。这里为什么要这么设计我在下面讲解：
首先我们得知道刚刚定义的struct thread_stack有两个作用：
1. 线程首次运行的时候，线程栈用于存储创建线程所需要的相关数据。和线程有关的数据应该都在该PCB中，这样便于管理线程，避免为他们再单独维护数据空间。创建线程之初，要指定在线程中运行的函数以及参数，因此将他存放在我们的内核栈当中，而其中eip便保存的是需要执行的函数
2. 任务切换函数switch_to中，这是线程已经处于正常运行后线程所体现的作用，由于switch_to函数是汇编程序，从其返回时，必然要用到ret指令，因此为了同时满足这两个作用，我们最初先在线程栈中装入适合的返回地址以及参数。使得作用2中的switch_to的ret指令也满足创建线程时的作用1.

这里虽然说咱们是通过ret指令来调用kernel_thread的，但是他自身不知道啊，他还以为自己是正常调用call过来的，所以他访问参数还是会是从栈顶地址esp + 4开始访问地一个参数，所以这里我们需要填充一个4字节的无意义数据，这里充当他的返回地址，但实际上此时他只会一路前进不会返回。
但注意上述的一路前进只是第一次在线程中执行的情况，在第2个作用中，会由调度函数switch_to为其留下返回地址，这时才需要返回。

这里最后说明一下中断栈和线程栈都位于线程的内核栈中，也就是PCB的高地址处。

---
这里我们已经讲解完了内核线程的数据结构，这里我们开始实现thread.c
```
#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"

#define PG_SIZE 4096

/* 由kernel_thread去执行function(func_arg) */
static void kernel_thread(thread_func* function, void* func_arg){
  function(func_arg);
}

/* 初始化线程栈thread_stack, 将待执行的函数和参数放到thread_stack中的相应位置*/
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg){
  /* 先预留中断使用栈的空间 */
  pthread->self_kstack -= sizeof(struct intr_stack);

  /* 再留出线程栈空间 */
  pthread->self_kstack -= sizeof(struct thread_stack);
  struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
  kthread_stack->eip = kernel_thread;
  kthread_stack->function = function;
  kthread_stack->func_arg = func_arg;
  kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}

/* 初始化线程基本信息 */
void init_thread(struct task_struct* pthread, char* name, int prio){
  memset(pthread, 0, sizeof(*pthread));
  strcpy(pthread->name, name);
  pthread->status = TASK_RUNNING;
  pthread->priority = prio;
  /* self_kstack是线程自己在内核态下使用的栈顶地址 */
  pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
  pthread->stack_magic = 0xdeadbeef;    //自定义魔数
}

/* 创建一优先级为prio的线程，线程名为name，线程所执行的函数是function(func_arg) */
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg){
  /* PCB都位于内核空间，包括用户进程的PCB也在内核空间 */
  struct task_struct* thread = get_kernel_pages(1);
  init_thread(thread, name, prio);
  thread_create(thread, function, func_arg);

  /* 这里当我们即将执行ret时，这里恰好就是咱们的eip所指向的函数地址 */
  asm volatile ("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret": : "g"(thread->self_kstack) : "memory");
  return thread;
}

```

如果说上面的解释没看懂，当你仔细看这个线程函数的实现的时候你会发现或然开朗，推荐大家跟着函数理顺一下思路
然后我们再到main.c中进行测试看看
```
#include "print.h"
#include "init.h"
#include "debug.h"
#include "string.h"
#include "memory.h"
#include "thread.h"

void k_thread_a(void*);     //自定义线程函数

int main(void){
  put_str("I am Kernel\n");
  init_all();
  thread_start("k_thread_a", 31, k_thread_a, "argA");
  while(1);
  return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  while(1){
    put_str(para);
  }
}

```

这里我们简单的构造一个打印的线程，这里我们还没实现线程的切换，所以这里就只有这个线程在不断运行输出字符串，我们编译链接后上虚拟机查看一下效果：
![](http://imgsrc.baidu.com/super/pic/item/b219ebc4b74543a97baf0f095b178a82b80114bb.jpg)
可以看到成功打印出来了argA，然后我们也可以看到这里确实也出现了一个申请的块，而这个块内就存放着咱们这个线程的PCB。
![](http://imgsrc.baidu.com/super/pic/item/5fdf8db1cb13495492ad1da1134e9258d0094a40.jpg)

## 0x02 核心数据结构，双向链表

