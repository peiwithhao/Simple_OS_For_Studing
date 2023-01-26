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




