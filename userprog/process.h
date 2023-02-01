#ifndef __USERPROG_H
#define __USERPROG_H
#include "thread.h"
#include "stdint.h"
#define USER_STACK3_VADDR (0xc0000000 - 0x1000)
#define USER_VADDR_START 0x804800
#define default_prio   31
void start_process(void* filename);
void page_dir_activate(struct task_struct* p_thread);
void process_activate(struct task_struct* pthread);
uint32_t* create_page_dir(void);
void create_user_vaddr_bitmap(struct task_struct* user_prog);
void process_execute(void* filename, char* name);
#endif
