#include "print.h"
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
  while(1){
    put_str("Main ");
  };
  return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  while(1){
    put_str(para);
    put_str("a ");
  }
}
void k_thread_b(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  while(1){
    put_str(para);
    put_str("b ");
  }
}
