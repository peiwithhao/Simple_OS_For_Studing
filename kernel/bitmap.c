#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

/* 将位图初始化 */
void bitmap_init(struct bitmap* btmp){
  memset(btmp, 0, btmp->btmp_bytes_len);
}

/* 判断bit_idx位是否为1,若为1,则返回true，否则返回false */
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx){
  uint32_t byte_idx = bit_idx/8;                //这里是求所在位所处的字节偏移
  uint32_t byte_odd = bit_idx%8;                //这里是求在字节中的位偏移
  return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd));     //与1进行与操作，查看是否为1
}

/* 在位图中申请连续cnt个位，成功则返回其起始位下标，否则返回-1 */
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);

void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);

