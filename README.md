## 0x00 上一节的遗留问题
大伙从上一节可以看出我们在实现多线程的途中会出现很多空格以及最后会出现GP异常，这个GP异常我们上一节经过调试也发现是因为我们显存段输入的值超过了我们预计的值所以生成异常，这里我来解释下为什么会产生这么多的问题。
首先来整体看待一下，我们实现多线程是要保证两个环境的保存，其一就是由线程转向时钟中断的过程，此时我们的线程都是输出字符串以及其他功能，所以这里我们需要保存的环境是比较多的，有那几个通用寄存器和一些段寄存器，我们从打印的效果可以看出我们打印字符也是成功打印了没有出现失误的情况（这里单指一个线程内）。然后其二就是通过时钟中断转换为其他线程，然后需要转而执行到kernel.S中的intr_exit函数，我们发现我们输出的空格就是出现在线程切换后的时刻，所以我们有充分的理由认为是第二个环境的保存中出现了错误。
我们在学习操作系统的时候都会知道同步互斥的概念，也就是说我们在访问临界区的时候，如果我们的代码没有按照正常的顺序走，将会出现十分不同的情况。而在我们这个例子中，打印字符也被分为了三个步骤：
1. 获取光标值
2. 将光标值转换为字节地址
3. 更新光标值

而在上述几个步骤中，由于咱们之前并没有实现互斥的机制，所以这导致了可能a线程刚执行完第二步，此时中断发生切换b线程恰好执行了第三步，而咱们的光标值是保存在显存寄存器中的，这属于是临界资源，这也就导致了咱们的字符被覆盖，也就会出现少字符的情况。

说完上面的我们来尝试在打印字符串过程中关中短来保证上面三步不可被打断然后尝试：
```
#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*);

int main(void){
  put_str("I am Kernel\n");
  init_all();
  thread_start("k_thread_a", 31, k_thread_a, "argA ");
  thread_start("k_thread_b", 8, k_thread_b, "argB ");
  intr_enable();
  while(1){
    intr_disable();
    put_str("Main ");
    intr_enable();
  };
  return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  while(1){
    intr_disable();
    put_str(para);
    intr_enable();
  }
}
void k_thread_b(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  while(1){
    intr_disable();
    put_str(para);
    intr_enable();
  }
}

```

![](http://imgsrc.baidu.com/super/pic/item/0e2442a7d933c8954ca7a5bd941373f08302001e.jpg)
这里发现运行是完全没问题的，并且也不会报出GP异常，这里我再来解释一下GP异常的发生。
我们在上一节的末尾是给出了产生GP异常的直接原因的，那就是偏移地址超过了咱们预定的范围，如下：
![](http://imgsrc.baidu.com/super/pic/item/a8ec8a13632762d015199479e5ec08fa503dc6d5.jpg)
这里可以发现他这条指令是位于咱们print.S中的.put_other底下的一条指令：
```
.put_other:
  shl bx,1          ;表示对应显存中的偏移字节
  mov [gs:bx], cl   ;ASCII字符本身
  inc bx
  mov byte[gs:bx], 0x07     ;字符属性
  shr bx, 1                 ;下一个光标值
  inc bx
  cmp bx, 2000              ;若小于2000,则表示没写道显存最后，因为80×25=2000
  jl .set_cursor            ;若大于2000,则换行处理,也就是进行下一行的换行处理

```

也就是其中的 `mov [gs:bx], cl`,这里我们的bx本身是存放偏移字符的，但由于我们一个字符是按照两字节来计算，所以这里我们需要先将bx左移1位来乘二，这里我们的bx乘二之后却变成了0x9f9e，这里我们关注一下标志位i寄存器eflags，我们之前提过这个CF标志位是表明了进位/借位，这里大写表明这个标志位为1,所以说明了咱们的bx左移的时候是进位了的，所以我们的bx正确值应该是0x19f9e，这里我们除以二进行还原得出0xcfcf,这个0xcfcf就是他传入的光标数，我们可以计算得出这里就已经越界了，因为咱们的光标值范围应该是0～1999,也就是0x0～0x7cf，这里说明很可能是我们在设置光标值的过程中出现了条件竞争的错误，这也只有在多线程的过程中才会出现的。
我们将光标的设置分为如下四种操作：
1. 通知光标寄存器要设置高8位;
2. 输入光标值的高8位
3. 通知光标寄存器要设置低8位
4. 输入光标值的低8位

相信跟我一起写过代码的同学会熟悉这些步骤，因为这就是咱们写汇编print.S的代码顺序，接下来我们来正式解释为何会产生0x9f9e
这里我们注意这里的临界资源为光标寄存器，也就是咱们存放光标位置的端口。
这里假设A线程正在CPU上运行，并且A线程要设置的光标值为0x7cf，也就是刚好是我们能设置的最大值1999,即屏幕右下角，在完成上述过程的第三步后，这时发生中断，切换了线程B运行，这里我们知道刚刚光标寄存器仅仅设置了高8位，也就是0x7,但其并不影响接下来的操作，此时光标寄存器的值仍然是0x7ce，因此此时B线程开始进行操作，当他在0x7ce输出一个字符后准备更新光标寄存器也就是将其加一得到2000,因此需要进行滚屏，而滚屏的操作相对来说比较繁琐，所以当B线程完成滚屏然后去更新光标寄存器的时候，这时候刚好完成第一步，第二步还没开始的时候处理器切换到运行A线程，这时候A线程继续刚刚的过程也就是进行第四布，可是此时光标寄存器以为你是要设置高8位，所以此时我们设置的0xcf就会输入到光标寄存器的高8位，就会得到0xcfcf，然后依次再进行输入字符操作就会出现GP异常。

---
上述说了这么多，相信大家已经了解了保持互斥的必要性，但是我们不能每次都在临界区代码前后加上开关中断的手段，这里我们接下来会介绍锁的概念。


## 0x01 同步与互斥
这里先介绍点基础的术语知识：
+ 公共资源：被所有任务共享的一套资源
+ 临界区：使用公共资源的代码程序
+ 互斥：值某一时刻公共资源只能被1个任务独享
+ 竞争条件：指多个任务同时进入临界区，大家对公共资源的访问是以竞争的方式并行存在的，因此公共资源的最终状态依赖于这些任务的临界区中的微操作执行顺序

从上面的解释中我们基本可以一一定位咱们程序中的这几点对应，其中公共资源不必多说，那自然是咱们的显卡的光标寄存器了，而因为咱们每个线程都会使用put_char函数，且这个函数是操作光标寄存器的，所以这段代码也就是咱们每个线程的临界区。
而这些机制所造成的问题都可以通过在临界区开关中断来实现，这里我们需要解决的操作就是在哪儿进行关中断的操作，如果说我们关中断的操作距离临界区太远就会造成多任务调度的低效，这也就违背了我们当初设计多线程的初衷。
然后我们来介绍锁的概念，相信大家一定上操作系统这门课的时候会听说过PV操作，也就是信号量的计算，这个信号量最初是来源于铁轨上的信号灯，因为铁轨要满足任何时候铁轨上只能有一辆火车，所以他也存在着信号量系统。在计算机当中，信号量为0就表示没有可用信号。
信号量也就是个计数器，他可以被看作资源的剩余量，而P操作就是减少信号量来获取资源，而V操作就是释放资源增加信号量，下面是PV操作分别的微操作：
V操作：
1. 将信号量的值加1
2. 唤醒在此信号量上等待的线程

P操作：
1. 判断信号量是否大于0
2. 若大于0,则将信号量减1
3. 若等于0,则线程将自己阻塞，以在此信号量上等待

其中若我们的信号量初值为1的话，那么称他为二元信号量，此时P操作就是获得锁，V操作就是释放锁，从而借此来保证只有1个线程进入临界区从而实现互斥，具体步骤如下：
1. 线程A通过P操作获得锁，此时信号量减一得0
2. 线程B想通过P操作获取锁，但发现信号量为0则阻塞自己
3. 线程A运行完毕通过V操作释放锁，此时信号量加1得1,然后线程A将线程B唤醒
4. 线程B获取了锁，进入临界区

基础知识介绍完毕，此刻我们来进行代码实现，首先我们需要知道阻塞是个什么，之前我们调度器都是将运行任务转到就绪队列，然后从就绪队列调度一个任务上处理器运行，如果说我们不想让他运行的话应该怎么办呢，那就只能别让他出现在就绪队列了，这样调度器根本不会让他上处理器运行，其实说开了十分简单，这里我们先在thread.c中增加一个阻塞函数以及解除阻塞函数：
```
/* 当前线程将自己阻塞，标志其状态为stat. */
void thread_block(enum task_status stat){
  /* stat取值为TASK_BLOCKED,TASK_WAITING,TASK_HANGING才不会被调度 */
  ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || (stat == TASK_HANGING)));
  enum intr_status old_status = intr_disable();
  struct task_struct* cur_thread = running_thread();
  cur_thread->status = stat;    //设置状态为stat
  schedule();                   //将当前线程换下处理器
  /* 待当前线程被接触阻塞后才会继续运行下面的intr_set_status */
  intr_set_status(old_status);
}

/* 将线程pthread解除阻塞 */
void thread_unblock(struct task_struct* pthread){
  enum intr_status old_status = intr_disable();
  ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)));
  if(pthread->status != TASK_READY){
    ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
    if(elem_find(&thread_ready_list, &pthread->general_tag)){
      PANIC("thread unblock: blocked thread in ready_list\n");
    }
    list_push(&thread_ready_list, &pthread->general_tag);   //放到队列首，使得其尽快被调度
    pthread->status = TASK_READY;
  }
  intr_set_status(old_status);
}
```

这里的代码很简单，值得注意的是一般阻塞行为是线程自己阻塞自己，而唤醒行为则是其他线程好心给你唤醒，也就是说自己阻塞自己，但要等别人来唤醒你。
以上就是锁的基础部件，这里我们来继续实现锁,首先我们定义一些结构体，他位于thread/sync.h：
```
#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H

#include "list.h"
#include "stdint.h"
#include "thread.h"

/* 信号量结构 */
struct semaphore{
  uint8_t value;                //记录信号量的值
  struct list waiters;          //记录等待线程
};

/* 锁结构 */
struct lock{
  struct task_struct* holder;   //锁的持有者
  struct semaphore semaphore;   //用二元信号量来实现锁
  uint32_t holder_repeat_nr;    //锁的持有者重复申请锁的次数
};


#endif
```

上面的重复申请次数是指一般情况下我们进入临界区之前会加锁，但有时候哦我们可能持有锁之后还会再申请此锁，这样以来会导致释放两次锁，为了避免这种问题，我们使用holder_repeat_nr来记录此次数防止我们过多释放锁。
接下来我们查看实现文件thread/sync.c
```
#include "sync.h"
#include "stdint.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"

/* 初始化信号量 */
void sema_init(struct semaphore* psema, uint8_t value){
  psema->value = value;     //信号量赋予初值
  list_init(&psema->waiters);   //初始化信号量的等待队列
}

/* 初始化锁plock */
void lock_init(struct lock* plock){
  plock->holder = NULL;
  plock->holder_repeat_nr = 0;
  sema_init(&plock->semaphore, 1);
}

/* 信号量down操作 */
void sema_down(struct semaphore* psema){
  /* 关中断来保证原子操作 */
  enum task_status old_status = intr_disable();
  while(psema->value == 0){         //使用while是因为被唤醒后仍需要继续竞争条件，而不是直接向下执行
    ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));    //当前线程不应该已在等待队列当中
    if(elem_fine(&psema->waiters, &running_thread()->general_tag)){
      PANIC("sema_down: thread blocked has been in waiters_list\n");
    }
    /* 若信号量的值等于0,则将自己加入该锁的等待队列中，然后阻塞自己 */
    list_append(&psema->waiters, &running_thread()->general_tag);
    thread_block(TASK_BLOCKED);     //阻塞自己，直到被唤醒,这里会调度别的线程，所以不必担心关中断
  }
  /* 若value为1或被唤醒之后，会执行下面代码，也就是获得了锁 */
  psema->value--;
  ASSERT(psema->value == 0);
  /* 恢复之前的中断状态 */
  intr_set_status(old_status);
}

/* 信号量的up操作 */
void sema_up(struct semaphore* psema){
  /* 关中断来保证原子操作 */
  enum task_status old_status = intr_disable();
  ASSERT(psema->value == 0);
  if(!list_empty(&psema->waiters)){
    struct task_struct* thread_blocked = elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
    thread_unblock(thread_blocked);
  }
  psema->value++;
  ASSERT(psema->value == 1);
  /* 恢复之前的状态 */
  intr_set_status(old_status);
}

/* 获取锁plock */
void lock_acquire(struct lock* plock){
  /* 排除曾经自己已经持有锁但还未将其释放的状态 */
  if(plock->holder != running_thread()){
    sema_down(&plock->semaphore);   //对信号量P操作
    plock->holder = running_thread();
    ASSERT(plock->holder_repeat_nr == 0);
    plock->holder_repeat_nr = 1;
  }else{
    plock->holder_repeat_nr++;
  }
}

/* 释放锁plock */
void lock_release(struct lock* plock){
  ASSERT(plock->holder == running_thread());
  if(plock->holder_repeat_nr > 1){
    plock->holder_repeat_nr--;
    return ;
  }
  ASSERT(plock->holder_repeat_nr == 1);
  plock->holder = NULL;     //把锁的持有者置空放在V操作之前
  plock->holder_repeat_nr = 0;
  sema_up(&plock->semaphore);   //对信号量V操作
}
```

这里代码的具体含义就是定义了获取锁和释放锁，而对锁的操作中包含着对信号量的操作，然后在其中进行阻塞和接触阻塞功能,这样我们就完成了锁的实现工作。

## 0x02 使用锁实现终端输出
大家可能都听说过tty，也就是虚拟终端，我们可以通过ALT+F1等操作来切换不同的控制终端。我们本节所要实现的终端其实也就是一个输出设备，在这里我们使用锁来将其输出变得整齐点。
首先是device/console.c文件了:
```
#include "console.h"
#include "print.h"
#include "stdint.h"
#include "sync.h"
#include "thread.h"

static struct lock console_lock;    //控制台锁

/* 初始化终端 */
void console_init(){
  lock_init(console_lock);
}

/* 获取终端 */
void console_acquire(){
  lock_acquire(&console_lock);
}

/* 释放终端 */
void console_release(){
  lock_release(&console_lock);
}

/* 终端输出字符串 */
void console_put_str(char* str){
  console_acquire();
  put_str(str);
  console_release();
}

/* 终端输出字符 */
void console_put_char(uint8_t char_asci){
  console_acquire();
  put_char(char_asci);
  console_release();
}

/* 终端输出十六进制整数 */
void console_put_int(uint32_t num){
  console_acquire();
  put_int(num);
  console_release();
}

```

这里大家也可以直观的看到他其实就是封装了我们的锁处理而已，此时我们再将其初始化操作放入init.c中,然后我们的main函数进行输出的时候就是用console_put_str等函数即可，此时是他内部帮咱们实现了互斥机制，我们上机实现一下发现确实正确无误。
![](http://imgsrc.baidu.com/super/pic/item/0dd7912397dda144aaaa7974f7b7d0a20df48650.jpg)
但是这里我的gcc9.4编译的话还是会有GP问题，这里我就直接同作者的环境保持一致使用gcc4.4编译发现一遍过，建议大家也保持环境一致。

## 0x03 从键盘获取输入
我们上面已经实现了正常的多线程输出，但我们今天的工作还有输入，所以我们需要稍微了解一下键盘输入的过程，这里我简单讲解一下，我们的键盘内部存在着一个叫做键盘编码器的芯片，这个芯片实际上就是向键盘控制器报告哪个建被按下，按键是否弹起，通常是Intel 8048或兼容芯片，而键盘控制器一般位于主板，通常是Intel 8042或兼容芯片.具体结构如下图：
![](http://imgsrc.baidu.com/super/pic/item/bf096b63f6246b60eecea6b2aef81a4c500fa230.jpg)

也就是当我们键盘按键的时候，8048会维护一个键值对的表，我们所按的键对应的扫描码会传给8042芯片，然后8042向8259A发送中断信号，这样处理器就会去执行键盘中断处理程序。而这个中断处理程序就是由我们自己实现。
这里的扫描码又分为两类，一个是叫做通码，指的是按下按键产生的扫描码，还一个是断码，指的是松开按键产生的扫描码。
由于我们根据不同的编码方案，键盘扫描码分为三类（注意这里并不是指ASCII码，而是单指标识没一个键的码，就比如说空格的ASCII码是0x20,但是扫描码却是0x39，我们中断处理程序就是要将扫描码转化为ASCII码进行输出），这里我们规定使用第一类，但是如果说咱们的键盘使用的是第二类或第三类怎么办呢，这个也并不需要我们关心，因为8042存在的意义之一就是兼容这三套方案，也就是说即使我们键盘是第二类方案，他也会转换为第一类方案进行信号传递。
下面就是第一套键盘扫描码：
![](http://imgsrc.baidu.com/super/pic/item/4e4a20a4462309f7454bbad4370e0cf3d6cad6d4.jpg)
![](http://imgsrc.baidu.com/super/pic/item/0824ab18972bd4074fe7c43b3e899e510eb309d7.jpg)
![](http://imgsrc.baidu.com/super/pic/item/7a899e510fb30f24c286fbc58d95d143ac4b03d3.jpg)
![](http://imgsrc.baidu.com/super/pic/item/5366d0160924ab18ef0ff6f170fae6cd7a890bde.jpg)

我们可以观察到一般通码和断码都是1字节大小，且断码 = 通码 + 0x80， 这里是因为一般咱们的扫描码的最高1位用来标识是通码还是断码，若是0则表示通码，为1则表示断码。
为了让我们可以获取击键的过程，我们将每一次击键过程分为“按下”，“按下保持”，“弹起”三个阶段，其中每次8048向8042发送扫描码的时候，8042都会向8059A发起中断并且将扫描码保存在自己的缓冲区中，此时再调用我们准备好的键盘中断处理程序，从8042缓冲区获得传递来的扫描码。
下面给出第一套扫描码的更加直观图：
![](http://imgsrc.baidu.com/super/pic/item/503d269759ee3d6d983c900d06166d224e4ade7a.jpg)

---
说完一些键盘的基本知识，我们现在来简单介绍一下8042,他本身比较简单，我们只使用了他的一个端口接收扫描码而已，他有4个8位寄存器，如下：
![](http://imgsrc.baidu.com/super/pic/item/0b46f21fbe096b6329f3499649338744eaf8ac0a.jpg)
四个寄存器共用两个端口，说明在不同场合下同一个端口有不同的作用，这里咱们已经熟门熟路了应该，这里图上面表示的很清楚，也就是当CPU读0x64的时候，这个端口反映的是状态寄存器，反映了8048的工作状态，而当CPU向0x64端口写入的时候则表明了即将发给8048的一些控制命令，而0x60端口则分别是写入和读出扫描码而已。这里我们主要介绍一下0x64端口：
状态寄存器，只读：
+ 0：为1表示输出缓冲区寄存器已满，处理器通过in指令后自动置0
+ 1：为1表示输入缓冲区寄存器已满，8042将值读取后该位自动为0
+ 2：系统标志位，最初加点为0,自检通过后置为1
+ 3：为1表示输入缓冲区中的是命令，为0则表示是普通数据
+ 4：为1表示键盘启动，为0表示禁用
+ 5：为1则表示发送超时
+ 6：为1表示接收超时
+ 7：来自8048的数据在奇偶校验时出错

控制寄存器，只写：
+ 0：为1表示启用键盘中断
+ 1：为1启用鼠标中断
+ 2：设置状态寄存器的位2
+ 3：为1表示状态寄存器的位4无效
+ 4：为1表示禁止键盘
+ 5：为1禁止鼠标
+ 6：将第二套转为第一套扫描码
+ 7：默认为0

介绍完基本端口信息后我们就开始实战，首先我们将kernel.S中的8259A中断控制器的其他引脚IRQ补充完整:
```
VECTOR 0x20,ZERO            ;时钟中断
VECTOR 0x21,ZERO            ;键盘中断
VECTOR 0x22,ZERO            ;级联从片
VECTOR 0x23,ZERO            ;串口2对应入口
VECTOR 0x24,ZERO            ;串口1对应入口
VECTOR 0x25,ZERO            ;并口2对应入口
VECTOR 0x26,ZERO            ;软盘对应入口
VECTOR 0x27,ZERO            ;并口1对应入口
VECTOR 0x28,ZERO            ;实时时钟中断对应入口
VECTOR 0x29,ZERO            ;重定向
VECTOR 0x2a,ZERO            ;保留
VECTOR 0x2b,ZERO            ;保留
VECTOR 0x2c,ZERO            ;ps/2鼠标
VECTOR 0x2d,ZERO            ;fpu浮点单元异常
VECTOR 0x2e,ZERO            ;硬盘
VECTOR 0x2f,ZERO            ;保留
```

这里我们为了简单方便，我们首先将时钟中断关闭转而开启键盘中断，当然这里我们需要修改interrupt.c文件，注意其中的支持的中断数也需要修改：
```
/* 初始化可编程中断控制器 */
static void pic_init(void){
  /* 初始化主片 */
  outb(PIC_M_CTRL, 0x11);           //ICW1:边沿触发，级联8259,需要ICW4
  outb(PIC_M_DATA, 0x20);           //ICW2:起始中断向量号为0x20,也就是IRQ0的中断向量号为0x20
  outb(PIC_M_DATA, 0x04);           //ICW3:设置IR2接从片
  outb(PIC_M_DATA, 0x01);           //ICW4:8086模式，正常EOI，非缓冲模式，手动结束中断

  /* 初始化从片 */
  outb(PIC_S_CTRL, 0x11);           //ICW1:边沿触发，级联8259,需要ICW4
  outb(PIC_S_DATA, 0x28);           //ICW2：起始中断向量号为0x28
  outb(PIC_S_DATA, 0x02);           //ICW3:设置从片连接到主片的IR2引脚
  outb(PIC_S_DATA, 0x01);           //ICW4:同上

  /* 打开主片上的IR0,也就是目前只接受时钟产生的中断 */
  outb(PIC_M_DATA, 0xfd);           //OCW1:IRQ0外全部屏蔽
  outb(PIC_S_DATA, 0xff);           //OCW1:IRQ8~15全部屏蔽

  put_str("     pic init done!\n");
}

```

这里实际上也仅仅是修改了初始化而已，而我们将可支持的中断改为0x30，此时已经完全支持了8259A所支持的全部中断。然后我们开始编写device/keyboard.c，他是用来定义键盘中断处理程序的
```
#include "keyboard.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"

#define KBD_BUF_PORT 0x60

/* 键盘中断处理程序 */
static void intr_keyboard_handler(void){
  put_char('k');
  /* 必须要读取输出缓冲区寄存器，否则8042不再继续响应键盘中断 */
  inb(KBD_BUF_PORT);
  return;
}

/* 键盘初始化 */
void keyboard_init(){
  put_str("keyboard init start \n");
  register_handler(0x21, intr_keyboard_handler);
  put_str("keyboard init done \n");
}

```

这里的中断处理程序十分简单，也就是接收键盘输入然后打印K字符，我们直接上bochs查看:

![](http://imgsrc.baidu.com/super/pic/item/8718367adab44aed89d5a822f61c8701a08bfb59.jpg)
图片可能看不太明白，但是确实实现了键盘中断，当我任意按键的时候，屏幕会输出字符’k‘,但是你如果尝试了会发现为啥我按一个键他出来两个甚至好几个K呢，这是因为你按下跟弹出是两个中断，有的通码和断码各有2字节，此时你按下加弹出他会产生4次中断，也就是打印4个K。
接下来我们改写一下中断处理程序，我们将其改为打印扫描码。
```
/* 键盘中断处理程序 */
static void intr_keyboard_handler(void){
  /* 必须要读取输出缓冲区寄存器，否则8042不再继续响应键盘中断 */
  uint8_t scancode = inb(KBD_BUF_PORT);
  put_int(scancode);
  return;
}

```
![](http://imgsrc.baidu.com/super/pic/item/03087bf40ad162d94f3574da54dfa9ec8b13cd19.jpg)
这里我是按了一个A键，我们对比前面的第一套得出确实如此


## 0x04 编写键盘驱动
我们已经成功打印出来扫描码，但是我们输入并不是为了看扫描码而是那个扫描码对应的字符，所以这里我们的键盘驱动程序就是将扫描码转换为对应字符的过程。
而对于转换过程其实也是写映射的过程，这个部分是程序给你固定住的映射关系，下面我们来进行实现：
```
/* 用转义字符定义一部分控制字符 */
#define esc '\x1b'      //十六进制表示
#define backspace '\b'
#define tab     '\t'
#define enter '\r'
#define delete '\x7f'

/* 以下不可见字符一律定义为0 */
#define char_invisible 0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible

/* 定义控制字符的通码和断码 */
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define ctrl_l_make  0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a

/* 定义以下变量来记录相应键是否按下的状态，
 * ext_scancode用于记录makecode是否以0xe0开头 */
static bool ctrl_status, shift_status, alt_status, caps_lock_status, ext_scancode;

/* 以通码make_code为索引的二维数组 */
static char keymap[][2] = {
  /* 扫描码未与shift组合 */
  /* --------------------------------------------- */
  /* 0x00 */    {0, 0},
  /* 0x01 */    {esc, esc},
  /* 0x02 */    {'1', '!'},
  /* 0x03 */    {'2', '@'},
  /* 0x04 */    {'3', '#'},
  /* 0x05 */    {'4', '$'},
  /* 0x06 */    {'5', '%'},
  /* 0x07 */    {'6', '^'},
  .......
  /** 0x3A /

```

这里注意我们需要对照之前的表格来看，这是我们说好的目前所支持的主键盘区的按键范围，这里是分别搭配shift使用。我们定义了一些映射，接下来就是编写正式的键盘中断处理程序了。
