#include "timer.h"
#include "io.h"
#include "print.h"
#include "thread.h"
#include "debug.h"

#define IRQ0_FREQUENCY 100                      //咱们所期待的频率
#define INPUT_FREQUENCY 1193180                 //计数器平均CLK频率
#define COUNTRE0_VALUE  INPUT_FREQUENCY / IRQ0_FREQUENCY    //计数器初值
#define CONTRER0_PORT   0x40                    //计数器0的端口号
#define COUNTER0_NO     0                       //计数器0
#define COUNTER_MODE    2                       //方式2
#define READ_WRITE_LATCH 3                      //高低均写
#define PIT_CONTROL_PORT 0x43                   //控制字端口号

/* 把操作的计数器counter_no,读写锁属性rwl,计数器模式counter_mode
 * 写入模式控制寄存器并赋予初值 counter_value
 *  */
uint32_t ticks;         //ticks是内核自开中断开启以来总共的滴答数

static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl, uint8_t counter_mode, uint16_t counter_value){
  /* 往控制字寄存器端口0x43写入控制字 */
  outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1)); //计数器0,rwl高低都写，方式2,二进制表示
  /* 先写入counter_value的低8位 */
  outb(CONTRER0_PORT, (uint8_t)counter_value);
  /* 再写入counter_value的高8位 */
  outb(COUNTER_MODE, (uint8_t)counter_mode >> 8);
  return;
}

/* 时钟的中断处理函数 */
static void intr_timer_handler(void){
  put_str("i am in intr_timer\n");
  struct task_struct* cur_thread = running_thread();
  put_str("the cur_thread addr is :0x");
  put_int(cur_thread);
  put_str("\n");
  ASSERT(cur_thread->stack_magic == 0xdeadbeef);    //检查栈是否溢出
  cur_thread->elapsed_ticks++;          //记录此线程占用的CPU时间
  put_int(cur_thread->ticks);
  put_str("\n");
  put_int(cur_thread->elapsed_ticks);
  ticks++;          //从内核第一次处理时间中断后开始至今的滴答数，内核态和用户态总共的滴答数
  /*if(cur_thread->ticks == 0){   //查看时间片是否用完
    schedule();
  }else{
    put_str("\n");
    put_int(cur_thread->ticks);
    put_str("\n");
    cur_thread->ticks--;
    
  }
  */
}

/* 初始化PIT8253 */
void timer_init(){
  put_str("timer_init start\n");
  /* 设置8253的定时周期，也就是发中断的周期 */
  frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTRE0_VALUE);
  register_handler(0x20, intr_timer_handler);
  put_str("timer_init_done\n");
}


