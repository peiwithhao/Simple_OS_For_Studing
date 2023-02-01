#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64

/* 环形队列 */
struct ioqueue{
  //生产者消费者
  struct lock lock;     //本缓冲区的锁
  /* 生产者，缓冲区不满的时候就继续往里面放数据
   * 否则就睡眠，此项记录哪个生产者在此缓冲区上睡眠 */
  struct task_struct* producer;

  /* 消费者，缓冲区不空就从里面拿数据，
   * 否则就睡眠，此项记录哪个消费者在此缓冲区上睡眠 */
  struct task_struct* consumer;
  char buf[bufsize];        //缓冲区大小
  int32_t head;             //头指针，写
  int32_t tail;             //尾指针，读
};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);
#endif
