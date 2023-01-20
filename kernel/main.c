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
