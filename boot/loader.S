%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR  ;起始地址按照之前约定一样
;按照显卡输出
;输出背景色为蓝色，钱景色为红色且跳动的字符串“peiwithhao”,注意为小端序
  mov byte [gs:0x00], 'I'
  mov byte [gs:0x01], 0x94
  mov byte [gs:0x02], ' '
  mov byte [gs:0x03], 0x94
  mov byte [gs:0x04], 'a'
  mov byte [gs:0x05], 0x94
  mov byte [gs:0x06], 'm'
  mov byte [gs:0x07], 0x94
  mov byte [gs:0x08], ' '
  mov byte [gs:0x09], 0x94
  mov byte [gs:0x0a], 'l'
  mov byte [gs:0x0b], 0x94
  mov byte [gs:0x0c], 'o'
  mov byte [gs:0x0d], 0x94
  mov byte [gs:0x0e], 'a'
  mov byte [gs:0x0f], 0x94
  mov byte [gs:0x10], 'd'
  mov byte [gs:0x11], 0x94
  mov byte [gs:0x12], 'e'
  mov byte [gs:0x13], 0x94
  mov byte [gs:0x14], 'r'
  mov byte [gs:0x15], 0x94

jmp $       ;循环等待
