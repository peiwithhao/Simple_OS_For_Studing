#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"
#include "stdio.h"
#include "stdio-kernel.h"
#include "fs.h"
#include "dir.h"
#include "shell.h"
#include "assert.h"
void init(void);

int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  cls_screen();
  console_put_str("[peiwithhao@localhost /]$ ");
  while(1);
  return 0;
}

/* init进程 */
void init(void){
  uint32_t ret_pid = fork();
  if(ret_pid){  //父进程
    while(1);
  }else{    //子进程
    my_shell();
  }
  panic("init: should not be here");
}
