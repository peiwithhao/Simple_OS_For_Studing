#include "syscall-init.h"
#include "print.h"
#include "syscall.h"
#include "thread.h"
#define syscall_nr 32
typedef void* syscall;
syscall syscall_table[syscall_nr];

/* 返回当前任务的pid */
uint32_t sys_getpid(void){
  return running_thread()->pid;
}

/* 初始化系统调用 */
void syscall_init(void){
  put_str("syscall_init_start\n");
  syscall_table[SYS_GETPID] = sys_getpid;
  put_str("syscall_init done\n");
}
