#include "timer.h"
#include "io.h"
#include "print.h"

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
static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl, uint8_t counter_mode, uint16_t counter_value){
  /* 往控制字寄存器端口0x43写入控制字 */
  outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1)); //计数器0,rwl高低都写，方式2,二进制表示
  /* 先写入counter_value的低8位 */
  outb(counter_port, (uint8_t)counter_value);
  /* 再写入counter_value的高8位 */
  outb(counter_port, (uint8_t)counter_value >> 8);
}

/* 初始化PIT8253 */
void timer_init(){
  put_str("timer_init start\n");
  /* 设置8253的定时周期，也就是发中断的周期 */
  frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTRE0_VALUE);
  put_str("timer_init_done\n");
}
