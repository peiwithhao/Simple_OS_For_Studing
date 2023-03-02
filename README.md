祝大家新年快乐！
## 0x00 基础知识们
今天我们来介绍一个新的知识那就是线程，大家如果有开发经验的话应该对这个线程都熟悉的，咱们都会通过多线程来提升我们处理器的效率，也提高了咱们程序运行的效率。这里的多线程默认是指并发，而不是并行，真正的并行是同一时间有多个线程并排运作，而并发是指在一段极短的时间内，多个线程交替使用，给人的感觉就好像这几个线程都是一起在运行。
所以我们采取的做法就是每个任务在处理器上执行一小会儿，然后使用调度器切换下一个任务到处理器上面运行。
![](http://imgsrc.baidu.com/forum/pic/item/e824b899a9014c087174a7604f7b02087af4f4ac.jpg)
上面的图说明了任务调度的情况，咱们的调度器也就是实现这一功能。简单来说，咱们的调度器就是操作系统中用于把任务轮流调度上处理器运行的一个软件模块，是操作系统的一部分。调度器在内核中维护一个任务表，然后按照一定的算法将该任务放到处理器上面执行。正是有了任务调度器，多任务操作系统才能够实现。
但是多线程也不是全是好处，就跟动态链接与静态链接一样，他俩都有优势与劣势，只不过看谁的优势更大而已。这里如果实现多线程，确实满足了多个线程同时推进，避免了一个运行时间短的线程始终等待一个运行时间长的线程，但是我们使用多线程在切换线程的时候会产生一些切换的时间，这让我也想到前面我们讲过的流水线了，如果设计的不合理，则流水线还不如指令串行。
![](http://imgsrc.baidu.com/forum/pic/item/dc54564e9258d109157126779458ccbf6d814d61.jpg)

### 1.线程的概念
线程如果我们简单来讲，在高级语言中，线程是运行函数的另一种方式，也就是说，构建一套线程方法，就比如C语言库中的POSIX线程库，让函数在此线程中调用，然后处理器去执行这个函数，因此线程实际的功能就是相当与调用了这个函数，从而让函数执行。
说到这里，那他和普通的函数调用有什么区别呢。
在一般的函数调用中，它是随着此函数所在的调度单元一块上处理器运行的，这个调度单元可能是这个进程，但也可能是某个线程，你可以当他是顺便执行的，也就是说咱们的处理器并不是单独的执行他。可是在线程就不一样了，他可以为一般的代码快创造他所依赖的上下文环境，从而让代码快具有独立性，因此在原理上线程能使一段函数成为调度单元，从而被调度器专门调度到处理器上执行，也就是说摆脱了函数调用时的依赖性，而真正翻身做了主人。
![](http://imgsrc.baidu.com/forum/pic/item/30adcbef76094b36d9569528e6cc7cd98c109d2b.jpg)

这里我默认大家曾经学过操作系统这门课程，就简单的给个结论，如果开启了多线程，那么线程就是CPU调度的最小单位，而CPU资源分配的最小单位是进程而不是线程，也就是说线程是没有属于自己的资源的，他的一切可用资源都是从所属进程获取的，地址空间也是。
这里进程、线程、资源之间的关系可以这样表达:进程 = 线程 + 资源

### 2.进程、线程的状态
这里给出现在常用的状态，当然别忘了咱们的工作是写操作系统，所以这里咱们的状态可以自定义，并不一定非得按照他的来，这里只是给个参考。
![](http://imgsrc.baidu.com/forum/pic/item/f636afc379310a554a285731f24543a9832610e3.jpg)
+ 就绪态是指该进程已经在可调度队列当中，随时可以调度他到处理器上
+ 运行态指该进程正在处理器上运行
+ 阻塞态是值该进程需要某种资源但是暂未得到，此时他无法加入调度队列进行调度

注意这里的状态是给调度器用的，而调度器他不会在意你到底属于进程还是线程，所以上述状态也使用于线程。

### 3.进程的身份证——PCB
操作系统为每个进程提供了一个PCB,Process Control Block，也就是程序控制块，用来记录与此进程相关的信息，比如进程状态、PID、优先级等。一般的PCB结构如下图所示：
![](http://imgsrc.baidu.com/forum/pic/item/7dd98d1001e939015a5904fb3eec54e737d19698.jpg)

每个进程都有自己的PCB，所有PCB放到一张表格中维护，这就是进程表，调度器可以根据这张进程表来选择上处理器运行的进程。因此PCB又可以称为进程表项。进程表如下：
![](http://imgsrc.baidu.com/forum/pic/item/728da9773912b31bb75445ecc318367adab4e13f.jpg)

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
![](http://imgsrc.baidu.com/forum/pic/item/cb8065380cd79123f1e91eb1e8345982b3b780ee.jpg)

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
![](http://imgsrc.baidu.com/forum/pic/item/b219ebc4b74543a97baf0f095b178a82b80114bb.jpg)
可以看到成功打印出来了argA，然后我们也可以看到这里确实也出现了一个申请的块，而这个块内就存放着咱们这个线程的PCB。
![](http://imgsrc.baidu.com/forum/pic/item/5fdf8db1cb13495492ad1da1134e9258d0094a40.jpg)

## 0x02 核心数据结构，双向链表
相信大家肯定很多人学过数据结构这门课程，今天我们就来实现他，并将其运用在实战当中，首先我们定义一些头文件，为lib/kernel/list.h
```
#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H
#include "global.h"

#define offset(struct_type, member) (int)(&((struct_type*)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) (struct_type*)((int)elem_ptr - offset(struct_type, stuct_member_name))

/********** 定义链表结点成员结构 **********
 * 节点中不需要数据元，只需要前后指针即可*/
struct list_elem{
  struct list_elem* prev;   //前驱节点
  struct list_elem* next;   //后继节点
};

/* 链表结构，用来实现队列 */
struct list{
  /* head是队首，固定不变，不是第1个元素，第1个元素是head.next */
  struct list_elem head;
  /* tail是队尾，同样固定不变 */
  struct list_elem tail;
};

/* 自定义函数function，用于在list_traversal中做回调函数 */
typedef bool (function)(struct list_elem*, int arg);

void list_init(struct list*);
void list_insert_before(struct list_elem* before, struct list_elem* elem);
void list_push(struct list* plist, struct list_elem* elem);
void list_iterate(struct list* plist);
void list_append(struct list* plist, struct list_elem* elem);
void list_remove(struct list_elem* pelem);
struct list_elem* list_pop(stuct list* plist);
bool list_empty(struct list* plist);
uint32_t list_len(struct list* plist);
struct list_elem* list_traversal(struct list* plist, function func, int arg);
bool emel_find(struct list* plist, struct list_elem* obj_elem);
#endif
```

以上代码大家只需要注意链表和链表结点的数据结构，其他的我们之后会讲到。然后接下来我们逐个实现这些函数就行
```
#include "list.h"
#include "interrupt.h"

/* 初始化双向链表list */
void list_init(struct list* list){
  list->head.prev = NULL;
  list->tail.next = NULL;
  list->head.next = &list->tail;
  list->tail.prev = &list->head;
}

/* 把链表元素elem插入在元素before之前 */
void list_insert_before(struct list_elem* before, struct list_elem* elem){
  enum intr_status old_status = intr_disable();     //关中断避免产生冲突，因为我们需要实现多线程
  /* 将before前驱元素的后继元素更新为elem,暂时使before脱离链表 */
  before->prev->next = elem;

  /* 更新elem自己的前驱结点为before的前驱，跟新elem自己的后继为before，更新before的前驱为elem */
  elem->prev = before->prev;
  elem->next = before;
  before->prev = elem;

  intr_set_status(old_status);      //恢复以前的中断状态
}

/* 添加元素到队列队首，类似栈push操作 */
void list_push(struct list* plist, struct list_elem* elem){
  list_insert_before(plist->head.next, elem);
}

/* 追加元素到链表队尾，类似队列的先进先出 */
void list_append(struct list* plist, struct list_elem* elem){
  list_insert_before(&plist->tail, elem);
}

/* 使元素pelem脱离链表 */
void list_remove(struct list_elem* pelem){
  enum intr_status old_status = intr_disable();

  pelem->prev->next = pelem->next;
  pelem->next->prev = pelem->prev;

  intr_set_status(old_status);
}

/* 将链表第一个元素弹出并返回，类似栈的pop操作 */
struct list_elem* list_pop(struct lsit* plist){
  struct list_elem* elem = plist->head.next;
  list_remove(elem);
  return elem;
}

/* 从链表中查找元素obj_elem，成功时返回true,失败返回false */
bool elem_find(struct list* plist, struct list_elem* obj_elem){
  struct list_elem* elem = plist->head.next;
  while(elem != &plist->tail){
    if(elem == obj_elem){
      return true;
$(BUILD_DIR)/
    }
    elem = elem->next;
  }
  return false;
}

/* 从列表plist中的每个元素elem和arg传给回调函数func，
 * arg给func用来判断elem是否符合条件。
 * 本函数的功能是遍历列表内所有元素，逐个判断是否有符合条件的元素。
 * 找到符合条件的元素返回元素指针，否则返回NULL */
struct list_elem* list_traversal(struct list* plist, function func, int arg){
  struct list_elem* elem = plist->head.next;
  /* 如果队列为空，就必然没有符合条件的结点，直接返回NULL */
  if(list_empty(plist)){
    return NULL;
  }
  while(elem != &plist->tail){
    if(func(elem, arg)){
      //返回true，则认为该元素在回调函数中符合条件，命中，故停止继续遍历
      return elem;
    }
    elem = elem->next;
  }
  return NULL;
}

/* 返回链表长度 */
uint32_t list_len(struct list* plist){
  struct list_elem* elem = plist->head.next;
  uint32_t length = 0;
  while(elem != &plist->tail){
    length ++;
    elem = elem->next;
  }
  return length;
}

/* 判断链表是否为空，空时返回true，否则返回false */
bool list_empty(struct list* plist){
  return (plist->head.next == &plist->tail ? true : false);
}

```

上面的代码都十分基础，让我回想到了考研408时候写代码题啊哈哈哈，这里的代码都十分基础，没注释大伙甚至都一看就能懂，所以这里不过多讲解。

## 0x03 多线程调度
要实现多线程调度，我们这里需要在thread.h中的PCB结构中新加几个数据，最新的struct task_struct如下:
```
/* 进程或线程的PCB */
struct task_struct{
  uint32_t* self_kstack;        //各内核线程都用自己的内核栈
  enum task_status status;
  char name[16];
  uint8_t priority;             //线程优先级
  uint8_t ticks;                //每次在处理器上执行的时间滴答数

  /* 此任务自从上cpu运行后至今占用了多少cpu滴答数，
   * 也就是此任务执行了多久 */
  uint32_t elapsed_ticks;

  /* general_tag的作用是用于线程在一般的队列中的结点 */
  struct list_elem general_tag;

  /* all_list_tag的作用是用于线程队列thread_all_list中的节点 */
  struct list_elem all_list_tag;

  uint32_t* pgdir;              //进程自己页表的虚拟地址
  uint32_t stack_magic;         //栈的边界标记，用于检测栈的溢出
};

```

这里我们的ticks元素需要与priority配合使用，优先级越高，则处理器上执行该任务的时间片就越长，每次时钟中断都会将当前任务的ticks减1,ticks也就是咱们经常说的时间片。
还有就是这里的pgdir，这里是指向的该进程任务自己的页表（虚拟地址，需要转换为物理地址），但如果是线程的话那么这里就是NULL。
这里反倒不好讲解了，因为都是之前修改了点的代码，但是权衡再三还是全贴出来，不然突然来一句实现什么显得很让人莫不找头脑：
```
#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "list.h"
#include "interrupt.h"
#include "debug.h"

#define PG_SIZE 4096
struct task_struct* main_thread;    //主线程PCB
struct list thread_ready_list;      //就绪队列
struct list thread_all_list;        //所有任务队列
static struct list_elem* thread_tag;    //用于保存队列中的线程结点

extern void switch_to(struct task_struct* cur, struct task_struct* next);

/* 获取当前线程PCB指针 */
struct task_struct* ruinning_thread(){
  uint32_t esp;
  asm("mov %%esp, %0" : "g="(esp));
  /* 取esp整数部分，也就是PCB起始地址 */
  return (struct task_struct*)(esp & 0xfffff000);       //这里因为我们PCB肯定是放在一个页最低端，所以这里取栈所在页面起始地址就行
}


/* 由kernel_thread去执行function(func_arg) */
static void kernel_thread(thread_func* function, void* func_arg){
  /* 执行function前要开中断，避免后面的时钟中断被屏蔽，而无法调度其他程序 */
  intr_enable();
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
  if(pthread == main_thread){
    /* 由于把main函数也封装成一个线程，并且他是一直运行的，故将其直接设为TASK_RUNNING*/
    pthread->status = TASK_RUNNING;
  }else{
    pthread->status = TASK_READY;
  }

  /* self_kstack是线程自己在内核态下使用的栈顶地址 */
  pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
  pthread->priority = prio;
  pthread->ticks = prio;
  pthread->elapsed_ticks = 0;
  pthread->pgdir = NULL;
  pthread->stack_magic = 0xdeadbeef;    //自定义魔数
}

/* 创建一优先级为prio的线程，线程名为name，线程所执行的函数是function(func_arg) */
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg){
  /* PCB都位于内核空间，包括用户进程的PCB也在内核空间 */
  struct task_struct* thread = get_kernel_pages(1);
  init_thread(thread, name, prio);
  thread_create(thread, function, func_arg);

  /* 确保之前不在队列中 */
  ASSERT(!elem_find(&thread_all_list, &thread->general_tag));
  /* 加入就绪线程队列 */
  list_append(&thread_ready_list, &thread->general_tag);

  /* 确保之前不在队列中 */
  ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
  /* 加入全部线程队列 */
  list_append(&thread_all_list, &thread->all_list_tag);
  return thread;
}

/* 将kernel中的main函数完善为主线程 */
static void make_main_thread(void){
  /* 因为main线程早已运行，
   * 咱们在loader.S中进入内核时的mov esp,0xc009f000,
   * 就是为其预留PCB，所以PCB的地址就是0xc009e000
   * 不需要通过get_kernel_page另分配一页*/
  main_thread = running_thread();
  init_thread(main_thread, "main", 31);

  /* main函数只是当前线程，当前线程不在thread_ready_list中，所以将其加入thread_all_list中*/
  ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
  list_append(&thread_all_list, &main_thread->all_list_tag);
}

```

上面咱们添加的代码是将main函数封装成了一个线程，实际上也就是为其加上PCB而已，然后修改了一些初始化代码，让其中的tag标签链接到咱们之前写的双向链表当中。如图：
![](http://imgsrc.baidu.com/forum/pic/item/738b4710b912c8fc03760708b9039245d788216a.jpg)

---
在咱们实现了进化版线程创建后，我们需要开始今天的重点，也就是线程调度了。
这里的调度原理也十分简单，当我们线程的PCB中ticks降为0的时候就进行任务调度，那ticks什么时候降呢，想必聪明的各位已经知道了，就是时钟每发生一次中断，那么他就会将ticks减1,然后时钟的中断处理程序调用调度器schedule，由他来将切换下一个线程，而调度器的主要任务就是读写就绪队列（也就是上面的thread_ready_list），增删其中的节点，修改其中的状态，注意这里我们采取队列的先进先出(FIFO).
调度器按照队列先进先出的顺序，把就绪队列中的第1个节点作为下一个要运行的新线程，将该线程的状态设置为TASK_RUNNING,之后通过函数switch_to将新线程的寄存器环境恢复，这样新线程就开始执行。
完整的调度过程需要三部分的配合：
1. 时钟中断处理函数
2. 调度器schedule
3. 任务切换函数switch_to

首先我们去注册时钟中断处理函数，大家应该还记得之前咱们的时钟中断函数是在C中写的一个打印字符串以及中断向量号的函数模板，现在我们就来改进一下：
```
/* 通用的中断处理函数，一般用在出现异常的时候处理 */
static void general_intr_handler(uint64_t vec_nr){
  /* IRQ7和IRQ15会产生伪中断，IRQ15是从片上最后一个引脚，保留项，这俩都不需要处理 */
  if(vec_nr == 0x27 || vec_nr == 0x2f){
    return;
  }
  /* 将光标置为0,从屏幕左上角清出一片打印异常信息的区域方便阅读 */
  set_cursor(0);    //这里是print.S中的设置光标函数，光标值范围是0～1999
  int cursor_pos = 0;
  while(cursor_pos < 320){
    put_char(' ');
    cursor_pos++;
  }
  set_cursor(0);    //重置光标值
  put_str("!!!!!!!!  exception message begin  !!!!!!!!");
  set_cursor(8);    //从第二行第8个字符开始打印
  put_str(intr_name[vec_nr]);
  if(vec_nr == 14){         //若为PageFault,将缺失的地址打印出来并悬停
    int page_fault_vaddr = 0;
    asm("movl %%cr2, %0" : "=r"(page_fault_vaddr));     //cr2存放造成PageFault的地址
    put_str("\npage fault addr is ");
    put_int(page_fault_vaddr);
  }
  put_str("\n!!!!!!!!   exception message end !!!!!!!");
  /* 能进入中断处理程序就表示已经在关中断情况下了，不会出现调度进程的情况，因此下面的死循环不会被中断 */
  while(1);
}

/* 在中断处理程序数组第vector_no个元素中注册安装中断处理程序function */
void register_handler(uint8_t vector_no, intr_handler function){
  /* idt_table数组中的函数是在进入中断后根据中断向量号调用的 */
  idt_table[vector_no] = function;
}
```

这里我们仅仅修改了interrupt.c中的general_intr_handler函数，也就是咱们之前的中断程序模板，在我们进入中断处理程序后，处理器会自动将eflags中的IF位置0,也就是关闭中断，此时我们的while循环就不会受到其他中断的打扰了。然后我们还需要添加一个注册函数入口函数register_handler
修改完interrupt后我们需要在时钟中断那儿注册咱们专门的处理函数，下面我们需要改进device/timer.c
```
#include "timer.h"
#include "io.h"
#include "print.h"
#include "thread.h"
#include "debug.h"

#define IRQ0_FREQUENCY 100                      //咱们所期待的频率
#define INPUT_FREQUENCY 1193180                 //计数器平均CLK频率
#define COUNTRE0_VALUE  INPUT_FREQUENCY / IRQ0_FREQUENCY    //计数器初值
#define CONTRER0_PORT   0x40                    //计数器0的端口号
#define COUNTER0_NO     0                       //计数器0
#define COUNTER_MODE    2                       //方式2
#define READ_WRITE_LATCH 3                      //高低均写
#define PIT_CONTROL_PORT 0x43                   //控制字端口号

/* 把操作的计数器counter_no,读写锁属性rwl,计数器模式counter_mode
 * 写入模式控制寄存器并赋予初值 counter_value
 *  */
uint32_t ticks;         //ticks是内核自开中断开启以来总共的滴答数

static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl, uint8_t counter_mode, uint16_t counter_value){
  /* 往控制字寄存器端口0x43写入控制字 */
  outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1)); //计数器0,rwl高低都写，方式2,二进制表示
  /* 先写入counter_value的低8位 */
  outb(CONTRER0_PORT, (uint8_t)counter_value);
  /* 再写入counter_value的高8位 */
  outb(COUNTER_MODE, (uint8_t)counter_mode >> 8);
}

/* 时钟的中断处理函数 */
static void intr_timer_handler(void){
  struct task_struct* cur_thread = running_thread();
  ASSERT(cur_thread->stack_magic == 0xdeadbeef);    //检查栈是否溢出
  cur_thread->elapsed_ticks++;          //记录此线程占用的CPU时间
  ticks++;          //从内核第一次处理时间中断后开始至今的滴答数，内核态和用户态总共的滴答数
  if(cur_thread->ticks == 0){   //查看时间片是否用完
    schedule();
  }else{
    cur_thread->ticks--;
  }
}

/* 初始化PIT8253 */
void timer_init(){
  put_str("timer_init start\n");
  /* 设置8253的定时周期，也就是发中断的周期 */
  frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTRE0_VALUE);
  register_handler(0x20, intr_timer_handler);
  put_str("timer_init_done\n");

}

```

这里的注册实际上也就是往咱们之前的函数表中填地址而已。
一切基本就绪，这里我们开始实现上面的第二布，那就是咱们的调度器函数schedule,这个函数仍位于thread.c当中。
```
/* 实现任务调度 */
void schedule(){
  ASSERT(intr_get_status() == INTR_OFF);

  struct task_struct* cur = running_thread();
  if(cur->status == TASK_RUNNING){
    //这里若是从运行态调度，则是其时间片到了的正常切换，因此将其改变为就绪态
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag);
    cur->ticks = cur->priority;
    //重新将当前线程的ticks再重置为其priority
    cur->status = TASK_READY;
  }else{
    /* 说明可能是阻塞自己 */
  }

  ASSERT(!list_empty(&thread_ready_list));
  thread_tag = NULL;    //将thread_tag清空
  /* 将thread_ready_list队列中的地一个就绪线程弹出，准备将他调入CPU运行*/
  thread_tag  list_pop(&thread_ready_list);
  struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
  next->status = TASK_RUNNING;
  switch_to(cur, next);
}

```

我简单讲解一下上面的函数，任务调度我们首先判断其是从运行态还是什么态发生调度，若是运行态的话那么将他的状态转为就绪态然后加入我们的就绪队列，但是注意这里就绪队列中结点并不是一个PCB，而是PCB中的一个字段，所以我们需要通过我们的general_tag来找到我们的PCB
```
#define offset(struct_type, member) (int)(&((struct_type*)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) (struct_type*)((int)elem_ptr - offset(struct_type, stuct_member_name))

```

这俩我在这儿给出解释，其中的elem2entry通过名字我们可以看出他的功能是由咱们的elem结点转换到PCB入口。
然后这里我们来逐个解释其中含义：
+ elem_ptr是待转换的地址
+ struct_member_name是elem_ptr所在结构体中对应地址的成员名字
+ struct_type是elem_ptr所属的结构体类型。

这里当然也可以使用咱们之前编写running_thread函数时的与0xfffff000相与，但是这里我们采用另一种方法来获取PCB首地址。
这另一种方式就是我们使用elem的减去固定结构体的偏移就得到了咱们的PCB首地址，而offset宏就是计算偏移的方式，这样讲了之后想必大家就明白了。

---
我们目前已经实现了注册中断处理程序，编写调度器，剩下还有最后一个步骤那就是实现切换函数switch_to，我们在调度器的最后一行是使用了这个函数来进行任务切换。
在咱们的系统当中，任务调度是由时钟中断发起的，然后由中断处理程序调用switch_to函数实现。因此在进入中断前我们需要保护进入中断前的任务状态，而当我们进入中断后，当我们内核执行中断处理程序的时候又要调用任务切换函数switch_to，当前的中断处理程序有要被中断，因此我们需要再次保护中断处理过程中的任务状态。

这里我们的第一层保护已经由kernel.S中的intr%1entry完成，所以我们需要实现第二部分，我们在thread目录下创建switch.S文件：
```
[bits 32]
section .text
global switch_to
switch_to:
  ;栈中此处是返回地址
  push esi
  push edi
  push edx
  push ebp
  mov eax, [esp + 20]   ;得到栈中的参数cur, cur = [esp + 20]
  mov [eax], esp        ;保存栈顶指针esp,task_struct的self_kstack字段
                        ;self_kstack在task_struct中的偏移为0
                        ;所以直接往thread开头处存4字节即可

  ;----------- 以上是备份当前线程的环境，下面是恢复下一个线程的环境 -------------
  mov eax, [esp + 24]   ;的到栈中的参数next
  mov esp, [eax]        ;恢复栈顶
  pop ebp
  pop ebx
  pop edi
  pop esi
  ret                   ;返回到上面switch_to下面的那句注释的返回地址
                        ;如果未由中断进入，第一次执行的时候会返回到kernel_thread
```

代码十分简单，这里只是保存上下文而已，注意上面的最后一行，如果我们的某个线程是第一次运行，则他的PCB中的kstack字段会指向压入的四个寄存器然后就是kernel_thread从而调用function,这点在我看来十分巧妙。

这里我们再到thread.c中添加一段初始化代码然后再通过init.c调用
```
/* 初始化线程环境 */
void thread_init(void){
  put_str("thread_init start\n");
  list_init(&thread_ready_list);
  list_init(&thread_all_list);
  /* 将当前main函数创建为线程 */
  make_main_thread();
  put_str("thread_init done\n");
}

```


我们万事具备，现在就去main函数中尝试一下:
```
#include "print.h"
#include "memory.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*); 

int main(void){
  put_str("I am Kernel\n");
  init_all();
  thread_start("k_thread_a", 31, k_thread_a, "argA");
  thread_start("k_thread_b", 8, k_thread_b, "argB");
  intr_enable();
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
void k_thread_b(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  while(1){
    put_str(para);
  }
}

```


这里遇到个问题，这几天实在没法子，是这样的，在之前我们实现中断的时候，可以看到我们的8253计数器是成功发出中断的，但是这两天我重新编译发现他突然无法发出时钟中断了，且这里绝对不是代码的问题，我将之前中断部分的代码重新编译一遍发现就无法发起时钟中断，但是我使用以前的编译结果就是正确的，这两天排查的很是烦人，且使用不同版本的gcc也没进展，换环境也无法成功，这里首先给出运行成功的正确结果，我估计得过几天才再来解决这个问题，这两天搞得心力憔悴
![](http://imgsrc.baidu.com/forum/pic/item/d1a20cf431adcbef32e0a1c3e9af2edda2cc9f1b.jpg)


## 0x04 总结
本次撇开最后结果的问题，整体代码弄懂了是十分值得的，你会从中知道调度器是如何运作，线程切换是如何工作的，如何识别线程等十分有趣的知识，但是就因为最近几天机子上的编译问题导致计数器始终不工作，搞得我过年都过不安逸，并且他是最近几天出的问题，年前实现中断的时候根本没这种困扰。

## 0x05 问题解决
排查到最后发现是timer.c代码出了问题，其中PIT的初始过程给写错了，搞到最后各种编译各种改版本发现最终是代码问题哈哈,修改的部分如下：
```
static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl, uint8_t counter_mode, uint16_t counter_value){
  /* 往控制字寄存器端口0x43写入控制字 */
  outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1)); //计数器0,rwl高低都写，方式2,二进制表示
  /* 先写入counter_value的低8位 */
  outb(counter_port, (uint8_t)counter_value);
  /* 再写入counter_value的高8位 */
  outb(counter_port, (uint8_t)counter_value >> 8);
}

```

然后这里给出我自己的运行结果
![](http://imgsrc.baidu.com/forum/pic/item/c8177f3e6709c93d05773f31da3df8dcd000546a.jpg)

这里我们看到了最上面是我们修改了的普通中断处理函数的异常报错，也就是"#GP General Protection Exception".这正是我们之前定义的中断名，这里我们可以去看看为什么爆出这个异常，所以我们进入bochs调试看看，我们可以使用show int来打印程序中所出现的中断，但是由于我们目前没有实现软中断，所以我们只需要在bochs调试页面使用show extint指令，这里我们可以首先使用nm指令查看thread_start的地址使得我们尽量距离GP异常近一点。

![](http://imgsrc.baidu.com/forum/pic/item/c9fcc3cec3fdfc031050d37e913f8794a5c226cf.jpg)

然后我们就先在这儿打断点然后使用show extint定位GP异常的地方就好了,下面就是我定位到的发生异常的指令
![](http://imgsrc.baidu.com/forum/pic/item/a8ec8a13632762d015199479e5ec08fa503dc6d5.jpg)
从这里我们可以看出，他是将cl移到gs的选择子当中，也就是咱们的视频段选择子，也就是咱们的显存段，但是我们可以回忆一下我们的显存段的物理地址范围为0xb8000~0xbffff，总共大小为0x7fff,而我们这里的ebx也就是偏移他是0x9f9e明显超出了这个范围，所以他明显越界了所以爆出了GP异常
这里我们使用x来查看内存情况，发现确实提示越界

![](http://imgsrc.baidu.com/forum/pic/item/a1ec08fa513d2697b1c9389b10fbb2fb4216d8da.jpg)
至于这里为什么产生这样的错误，我们在之后同步环节进行讲解。
