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
