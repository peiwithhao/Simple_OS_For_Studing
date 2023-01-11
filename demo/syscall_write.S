section .data
str_c_lib: db "c library says: hello world!", 0xa   ;0xa为ASCII中的‘\n’，也就是换行符
str_c_lib_len equ $-str_c_lib 

str_syscall: db "syscall says: hello world!", 0xa
str_syscall_len equ $-str_syscall 

section .text
global _start
_start:
;;;;;;;;;; 方式1：模拟C语言中系统调用库函数write ;;;;;;;;;;;;;;;;
  push str_c_lib_len    ;按照之前的规约来压栈传参
  push str_c_lib
  push 1

  call simu_write
  add esp, 12       ;回收栈空间

;;;;;;;;;; 方式2: 直接进行系统调用 ;;;;;;;;;;;;;;
  mov eax, 4
  mov ebx, 1
  mov ecx, str_syscall
  mov edx, str_syscall_len
  int 0x80          ;发起中断

;;;;;;;;;; 退出程序 ;;;;;;;;;;;;;;
  mov eax, 1        ;1号子功能exit
  int 0x80          ;发起中断，通知linux完成请求的功能

;;;;;;;;;; 自定义simu_write用来模拟c库中系统调用函数write
simu_write:
  push ebp
  mov ebp,esp
  mov eax, 4
  mov ebx, [ebp+8]
  mov ecx, [ebp+12]
  mov edx, [ebp+16]
  int 0x80
  pop ebp
  ret 
