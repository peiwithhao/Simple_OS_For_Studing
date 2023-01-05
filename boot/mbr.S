;主引导程序
;
%include "boot.inc"         ;引用本文件夹下的boot.inc文件
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
    

  mov eax,LOADER_START_SECTOR   ;起始扇区LBA地址,注意这里使用32位寄存器是可行的，虽然说实模式下咱们只能用16位地址，但不是说用不了32位寄存器
  mov bx,LOADER_BASE_ADDR       ;写入的内存地址
  mov cx,4                      ;待读入的扇区数
  call rd_disk_m_16             ;以下读取程序的其实部分（一个扇区），rd_disk_m_16表示在16位模式下读硬盘的section函数，我们在下面实现
  
  jmp LOADER_BASE_ADDR + 0x300         ;代码运行至此说明Loader已经加载完毕

;------------------------------------
;功能:读取硬盘n个扇区
rd_disk_m_16:
;-----------------------------------
                                ;eax = 扇区LBA地址
                                ;bx = 将数据写入的内存地址
                                ;cx = 读入的扇区数
  mov esi,eax                   ;备份eax
  mov di,cx                     ;备份cx

;读写硬盘：
;第1步：设置要读取的扇区数
  mov dx,0x1F2
  mov al,cl
  out dx,al         ;读取的扇区数

  mov eax,esi       ;恢复ax

;第2布：将LBA地址送入0x1F3~0x1F6
  ;LBA地址0～7位存入0x1F3
  mov dx,0x1F3
  out dx,al

  ;LBA地址8～15位写入0x1F4
  mov cl,8          
  shr eax,cl        ;将eax中数据右移8位，这样就可以接着使用al来取中间8位了
  mov dx,0x1F4
  out dx,al

  ;LBA地址16～23位写入0x1F5
  shr eax,cl
  mov dx,0x1F5
  out dx,al
  
  shr eax,cl
  and al,0x0f       ;取LBR第24～27位
  or al,0xe0        ;设置7～4位为1110,指明LBA寻址模式
  mov dx,0x1F6
  out dx,al

;第3步：向0x1F7写入读命令，即为0x20
  mov dx,0x1f7
  mov al,0x20
  out dx,al

;第4步：检测硬盘状态
  .not_ready:
  ;同一端口，写时表示写入命令字，读时表示读入硬盘状态
  nop   ;空操作，相当与sleep一下
  in al,dx      ;此时我们读取0x1F7端口，此时它充当Status寄存器
  and al,0x88   ;其中第3位为1表示硬盘控制器已经准备好,第7位为1表示硬盘忙，这里即为取对应位的值
  
  cmp al,0x08   ;判断符号位是否与顺利执行时的符号一致
  jnz .not_ready ;若未准备好，则继续回跳

;第5步：从0x1F0端口读取数据
  mov ax,di     ;di为之前备份的读入扇区数
  mov dx, 256
  mul dx
  mov cx,ax     ;这里cx来存放循环次数
;一个字为两字节，而我们需要读入一个扇区，即为512字节，每次读入一个字（这是因为data寄存器有16位），所以共需要di×512/2 = di*256次
  mov dx,0x1F0
  .go_on_read:
  in ax,dx
  mov [bx],ax   ;bx存放的是加载的内存地址
  add bx,2      ;因为每次存2字节，所以内存地址每次加2,然后继续读
  loop .go_on_read ;这里cx作为循环控制次数 
  ret

;------------------------------------
;这里不再需要悬停指针了，因为此时MBR已经成功将Loader读入内存，即将完成交接
  times 510-($-$$) db 0     ;$-$$表示该指令行距离section起始地址的偏移，这里也可表示为目前指令大小
  db 0x55,0xaa

