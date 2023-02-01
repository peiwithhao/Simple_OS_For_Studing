#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*); 
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0, test_var_b = 0;

int main(void){
  put_str("I am Kernel\n");
  init_all();
  thread_start("k_thread_a", 31, k_thread_a, " A_");
  thread_start("k_thread_b", 31, k_thread_b, " B_");
  process_execute(u_prog_a, "user_prog_a");
  process_execute(u_prog_b, "user_prog_b");
  intr_enable();
  while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  while(1){
    console_put_str("v_a:0x");
    console_put_int(test_var_a);
    console_put_str("   ");
  }
}
void k_thread_b(void* arg){
  /* 这里传递通用参数，这里被调用函数自己需要什么类型的就自己转换 */
  char* para = arg;
  while(1){
    console_put_str("v_b:0x");
    console_put_int(test_var_b);
    console_put_str("   ");
  }
}

void u_prog_a(void){
  while(1){
    test_var_a++ ;
  }
}

void u_prog_b(void){
  while(1){
    test_var_b++ ;
  }
}

