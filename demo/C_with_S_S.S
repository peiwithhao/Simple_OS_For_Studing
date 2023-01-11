section .data
str: db "asm_print says hello world!". 0xa,0    ;这里的0是手工添加\x00结束码，这里不加0会导致一直循环
str_len equ $-str

section .text
extern c_print
global _start               ;将_start导出为全局属性，也就是对程序中的所有文件可见，这样其他文件中也可以引用被global导出的符号了
_start:
;;;;;;;;;;;; 调用c代码中的函数c_print ;;;;;;;;;;;;;
  push str
  call c_print
  add esp. 4

;;;;;;;;;;;; 退出程序 ;;;;;;;;;;;;;;;;;
  mov eax, 1
  int 0x80

global asm_print            ;相当与asm_print(str, size)
asm_print:      
  push ebp
  mov ebp, esp
  mov eax, 4                ;4号子功能，为write系统调用
  mov ebx, 1                ;文件描述符1,stdout
  mov ecx, [ebp+8]          
  mov edx, [ebp+12]
  int 0x80
  pop ebp
  ret

