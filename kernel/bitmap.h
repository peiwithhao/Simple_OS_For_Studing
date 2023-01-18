#ifndef __KERNEL_BITMAP_H
#define __KERNEL_BITMAP_H
#include "global.h"
#define BITMAP_MASK 1

typedef int bool;
struct bitmap {
  uint32_t btmp_bytes_len;      //位图的字节长度
  /* 在遍历位图的时候，整体以字节为单位，细节上是以位为单位，因此这里的指针为单字节 */
  uint8_t* bits;                //位图的指针
};

void bitmap_init(struct bitmap* btmp);
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);

#endif
