%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR  ;起始地址按照之前约定一样
LOADER_STACK_TOP equ LOADER_BASE_ADDR ;loader在实模式下的栈指针地址
jmp loader_start

;构建gdt及其内部描述符
GDT_BASE: dd 0x00000000             ;低4字节
          dd 0x00000000             ;高4字节，无效描述符，防止选择子未初始化
CODE_DESC: dd 0x0000FFFF            ;dd是伪指令，表示define double-word,也就是定义双字变量,这里的0xFFFF表示段界限
            dd DESC_CODE_HIGH4      ;代码段描述符
DATA_STACK_DESC: dd 0x0000FFFF      
                  dd DESC_DATA_HIGH4    ;栈段描述符，也就是数据段描述符,这俩共用一个段是因为方便，至于为什么这里栈的P位为什么不是1,也就是向下扩展，这是因为段描述符是由CPU检查的，而CPU并不知道这个段的作用，程序员若要实现栈向下扩展只需要使得其esp在push时减小即可
VIDEO_DESC: dd 0x80000007;limit=(0xbffff-0xb8000)/4k=0x7，这里的0xb8000~0xbffff是实模式下文本模式显存适配器的内存地址，因此段界限即为上述方程
            dd DESC_VIDEO_HIGH4 ;此时dpl为0，此乃显存段描述符
;-------- 以上共填充了3个段描述符 + 1个首段无效描述符-------------
GDT_SIZE equ $ - GDT_BASE           ;计算当前GDT已经填充的大小
GDT_LIMIT equ GDT_SIZE - 1          ;
times 60 dq 0           ;此处预留60个描述符空位，这里跟上面一致，表示define quad-word ,也就是定义60个以0填充的段描述符,这里的times是循环次数

;------ 这里定义选择子------------
SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0   ;相当于(CODE_DESC - GDT_BASE)/8 + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0   ;同上类似
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0   ;同上类似

;-------------- 以下定义gdt的指针，前2字节是gdt的界限，后4字节是gdt的起始地址 ---------
gdt_ptr dw GDT_LIMIT    ;define word
        dd GDT_BASE
loadermsg db 'I am loader in real.'

loader_start:
;----------------------------------------------
;INT 0x10   功能号：0x13    功能描述：打印字符串
;----------------------------------------------
;输入：
;AH子功能号=13H
;BH = 页号
;BL = 属性
;CX = 字符串长度
;(DH,DL) = 字符串地址
;AL = 显示输出方式
; 0 -- 字符串只含显示字符，显示属性在BL中，显示后光标位置不变
; 1 -- 字符串只含显示字符，显示属性在BL中，显示后光标位置改变
; 2 -- 字符串只含显示字符和显示属性，显示后光标位置不变
; 3 -- 字符串只含显示字符和显示属性，显示后光标位置改变
; 无返回值
  mov sp ,LOADER_BASE_ADDR
  mov bp ,loadermsg     ;ES:BP = 字符串地址
  mov cx ,20            ;CX = 字符串长度                ;注意这里使用的是BIOS中断，一旦进入保护模式就没有BIOS中断了
  mov ax ,0x1301        ;AH = 13H,AL = 01H
  mov bx ,0x001f        ;页号为0,蓝底粉红字
  mov dx ,0x1800        
  int 0x10              ;10H号中断

;---------------- 准备进入保护模式 ------------------
;1 打开A20
;2 加载gdt
;3.将CR0的PE位置0,

  ;------------=- 打开A20 --------------
  in al,0x92
  or al,0000_0010B
  out 0x92,al

  ;------------- 加载GDT --------------
  lgdt [gdt_ptr]

  ;------------- CR0第0位置1 ----------
  mov eax, cr0
  or eax, 0x00000001
  mov cr0, eax

  jmp dword SELECTOR_CODE:p_mode_start ;刷新流水线

[bits 32]
p_mode_start:
  mov ax, SELECTOR_DATA
  mov ds, ax
  mov es, ax
  mov ss, ax
  mov esp, LOADER_STACK_TOP
  mov ax, SELECTOR_VIDEO
  mov gs, ax

  mov byte [gs:160], 'p'

  jmp $
