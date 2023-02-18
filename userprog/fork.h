#ifndef __USERPROG_FORK_H
#define __USERPROG_FORK_H
#include "thread.h"
/* fork子进程，只能有用户进程通过系统调用fork来使用，内核线程不可调用是因为要从0级栈中获得esp3等 */
pid_t sys_fork(void); 
#endif
