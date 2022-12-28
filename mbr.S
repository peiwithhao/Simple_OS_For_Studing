;主引导程序
;
;LOADER_BASE_ADDR equ 0xA000
;LOADER_START_SECTIOR equ 0x2
;------------------------------------------
SECTION MBR vstart=0x7c00   ; 向编译器表示咱们这儿起始地址应为0x7c00
  mov ax,cs
  mov dx,ax                 ; 由于BIOS通过 0:0x7c00跳转MBR，所以此时cs为0,因此借他来初始化寄存器
  mov es,ax
  mov ss,ax
  mov fs,ax
  mov sp,0x7c00
  mov ax,0xb800             ;使用文本模式，所以得把文本范围类的地址输入gs当作段基地址
  mov gs,ax 
; 清屏利用0x06号子功能，上卷全部行即可清屏
;-------------------------------------------
;INT 0x10    功能号：0x06    功能：上卷窗口
;-------------------------------------------
;输入：
;AH 功能号:0x06
;AL = 上卷行数（若为0则表示全部行，太适合我们辣）
;BH = 上卷行属性
;(CL,CH) = 窗口左上角的(X,Y)位置
;(DL,DH) = 窗口右下角的(X,Y)位置
;无返回值
  mov ax,0x600
  mov bx,0x700
  mov cx,0          ; 左上角(0,0)
  mov dx,0x184f     ; 右下角(0x4f,0x18),在VGA文本模式中一行只能容纳80字符
  int 0x10

;输出背景色为蓝色，钱景色为红色且跳动的字符串“peiwithhao”,注意为小端序
  mov byte [gs:0x00], 'p'
  mov byte [gs:0x01], 0x94
  mov byte [gs:0x02], 'e'
  mov byte [gs:0x03], 0x94
  mov byte [gs:0x04], 'i'
  mov byte [gs:0x05], 0x94
  mov byte [gs:0x06], 'w'
  mov byte [gs:0x07], 0x94
  mov byte [gs:0x08], 'i'
  mov byte [gs:0x09], 0x94
  mov byte [gs:0x0a], 't'
  mov byte [gs:0x0b], 0x94
  mov byte [gs:0x0c], 'h'
  mov byte [gs:0x0d], 0x94
  mov byte [gs:0x0e], 'h'
  mov byte [gs:0x0f], 0x94
  mov byte [gs:0x10], 'a'
  mov byte [gs:0x11], 0x94
  mov byte [gs:0x12], 'o'
  mov byte [gs:0x13], 0x94
    


;;;;;;;;;; 打印字符串结束 ;;;;;;;;;;;;;;;
  jmp $                 ;悬停指针
  times 510-($-$$) db 0     ;$-$$表示该指令行距离section起始地址的偏移，这里也可表示为目前指令大小
  db 0x55,0xaa

