#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*); 

int main(void){
  put_str("I am Kernel\n");
  init_all();
  thread_start("k_thread_a", 31, k_thread_a, " A_");
  thread_start("k_thread_b", 31, k_thread_b, " B_");
  intr_enable();
  while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  while(1){
    enum intr_status old_status = intr_disable();
    if(!ioq_empty(&kbd_buf)){
      console_put_str(arg);
      char byte = ioq_getchar(&kbd_buf);
      console_put_char(byte);
    }
    intr_set_status(old_status);
  }
}
void k_thread_b(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  while(1){
    enum intr_status old_status = intr_disable();
    if(!ioq_empty(&kbd_buf)){
      console_put_str(arg);
      char byte = ioq_getchar(&kbd_buf);
      console_put_char(byte);
    }
    intr_set_status(old_status);
  }
}
