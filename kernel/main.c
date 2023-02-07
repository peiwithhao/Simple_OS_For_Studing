#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"
#include "stdio.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*); 
void u_prog_a(void);
void u_prog_b(void);
int prog_a_pid = 0, prog_b_pid = 0;

int main(void){
  put_str("I am Kernel\n");
  init_all();

  intr_enable();
  process_execute(u_prog_a, "u_prog_a");
  process_execute(u_prog_b, "u_prog_b");
  thread_start("k_thread_a", 31, k_thread_a, " A_");
  thread_start("k_thread_b", 31, k_thread_b, " B_");
 while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg){
  void* addr1 = sys_malloc(256);
  void* addr2 = sys_malloc(256);
  void* addr3 = sys_malloc(256);
  
  console_put_str("thread_a malloc addr:0x");
  console_put_int((int)addr1);
  console_put_char(',');
  console_put_int((int)addr2);
  console_put_char(',');
  console_put_int((int)addr3);
  console_put_char('\n');
  int cpu_delay = 100000;
  while(cpu_delay-- > 0);
  sys_free(addr1);
  sys_free(addr2);
  sys_free(addr3);
  while(1);
}
void k_thread_b(void* arg){
  void* addr1 = sys_malloc(256);
  void* addr2 = sys_malloc(256);
  void* addr3 = sys_malloc(256);
  
  console_put_str("thread_b malloc addr:0x");
  console_put_int((int)addr1);
  console_put_char(',');
  console_put_int((int)addr2);
  console_put_char(',');
  console_put_int((int)addr3);
  console_put_char('\n');
  int cpu_delay = 100000;
  while(cpu_delay-- > 0);
  sys_free(addr1);
  sys_free(addr2);
  sys_free(addr3);
  while(1);
}

void u_prog_a(void){
  void* addr1 = malloc(256);
  void* addr2 = malloc(256);
  void* addr3 = malloc(256);
  printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);
  int cpu_delay = 100000;
  while(cpu_delay-- > 0);
  free(addr1);
  free(addr2);
  free(addr3);
  while(1);
}

void u_prog_b(void){
  void* addr1 = malloc(256);
  void* addr2 = malloc(256);
  void* addr3 = malloc(256);
  printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);
  int cpu_delay = 100000;
  while(cpu_delay-- > 0);
  free(addr1);
  free(addr2);
  free(addr3);
  while(1);
}

