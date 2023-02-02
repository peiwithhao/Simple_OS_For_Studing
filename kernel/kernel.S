[bits 32]
%define ERROR_CODE nop      ;若在相关的异常中CPU已经自动压入了，这里表示错误码，为保持栈中格式统一，这里不做操作
%define ZERO push 0         ;若在相关异常中CPU没有压入错误码，这里为了统一格式，手工压入一个0

extern put_str              ;声明外部函数，表明咱们要用到
extern idt_table            ;idt_table是interrupt.c中注册的中断处理函数数组

section .data
global intr_entry_table
intr_entry_table:

%macro VECTOR 2             ;这里定义一个多行宏，后面的2是指传递两个参数,里面的%1,%2代表参数，类似linux shell脚本
section .text
intr%1entry:               ;每个中断处理程序都要压入中断向量号，所以一个中断类型一个中断处理程序，自己知道自己的中断向量号是多少
  %2                        ;手工压入32位数据或者空操作，保证最后都会需要跨过一个4字节来进行iret
  push ds
  push es
  push fs
  push gs
  pushad                    ;压入EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
  
  ;如果是从片上进入的中断，除了往片上发送EOI外，还要往主片上发送EOI
  mov al, 0x20              ;中断结束命令EOI，这里R为0,SL为0,EOI为1
  out 0xa0, al              ;向从片发送，这里端口号可以使用dx或者立即数,OCW2
  out 0x20, al              ;向主片发送

  push %1                   ;不管idt_table中的目标函数是否需要参数，我们这里一律都压入中断向量号

  call [idt_table + %1*4]   ;调用idt_table中的C版本中断处理函数
  jmp intr_exit

section .data
  dd intr%1entry            ;存储各个中断入口程序的地址，形成intr_entry_table数组,这里是因为编译后会将相同的节合并成一个段，所以这里会生成一个数组
%endmacro                   ;多行宏结束标志

section .text
global intr_exit
intr_exit:
;下面是恢复上下文环境
  add esp,4                 ;跳过中断号
  popad
  pop gs
  pop fs
  pop es
  pop ds
  add esp, 4                ;跳过error_code或者说我们自己压入的0
  iretd

VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO
VECTOR 0x08,ZERO
VECTOR 0x09,ZERO
VECTOR 0x0a,ZERO
VECTOR 0x0b,ZERO
VECTOR 0x0c,ZERO
VECTOR 0x0d,ZERO
VECTOR 0x0e,ZERO
VECTOR 0x0f,ZERO
VECTOR 0x10,ZERO
VECTOR 0x11,ZERO
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO
VECTOR 0x18,ZERO
VECTOR 0x19,ZERO
VECTOR 0x1a,ZERO
VECTOR 0x1b,ZERO
VECTOR 0x1c,ZERO
VECTOR 0x1d,ZERO
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO
VECTOR 0x20,ZERO            ;时钟中断
VECTOR 0x21,ZERO            ;键盘中断
VECTOR 0x22,ZERO            ;级联从片
VECTOR 0x23,ZERO            ;串口2对应入口
VECTOR 0x24,ZERO            ;串口1对应入口
VECTOR 0x25,ZERO            ;并口2对应入口
VECTOR 0x26,ZERO            ;软盘对应入口
VECTOR 0x27,ZERO            ;并口1对应入口
VECTOR 0x28,ZERO            ;实时时钟中断对应入口
VECTOR 0x29,ZERO            ;重定向
VECTOR 0x2a,ZERO            ;保留
VECTOR 0x2b,ZERO            ;保留
VECTOR 0x2c,ZERO            ;ps/2鼠标
VECTOR 0x2d,ZERO            ;fpu浮点单元异常
VECTOR 0x2e,ZERO            ;硬盘
VECTOR 0x2f,ZERO            ;保留

;;;;;;;;;;;;;;; 0x80号中断 ;;;;;;;;;;;;;;;;;
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
;1 保存上下文环境
  push 0    ;压入0,使得栈中格式统一
  push ds
  push es
  push fs
  push gs
  pushad    ;PUSHAD压入8个寄存器，类似上面的中断函数例程
  push 0x80     ;同样为了保持统一的栈格式

;2 为系统调用子功能传入参数
  push edx  ;第3个参数，下面依次递减
  push ecx  
  push ebx

;3 调用子功能处理函数
  call [syscall_table + eax*4]
  add esp, 12   ;跨过上面的三个参数

;4 将call调用后的返回值存入当前内核栈中的eax的位置
  mov [esp + 8*4], eax
  jmp intr_exit     ;恢复上下文
  

