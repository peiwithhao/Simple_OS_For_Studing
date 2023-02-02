#ifndef __USERPROG_SYSCALLINIT_H
#define __USERPROG_SYSCALLINIT_H
#include "stdint.h"
uint32_t sys_getpid(void);
void syscall_init(void);
#endif
