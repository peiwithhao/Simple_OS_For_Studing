#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"
typedef void* intr_handler;
void idt_init(void);    //初始化idt描述表
void register_handler(uint8_t vector_no, intr_handler function);    //中断处理程序注册入口
/* 定义两种中断的状态
 * INTR_OFF值为0表示关中断
 * INTR_ON值为1表示开中断
 */
enum intr_status { //中断状态
  INTR_OFF,         //关中断
  INTR_ON           //开中断
};

enum intr_status intr_get_status(void);     
enum intr_status intr_set_status(enum intr_status);
enum intr_status intr_enable(void);
enum intr_status intr_disable(void);

#endif
