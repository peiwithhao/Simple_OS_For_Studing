TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

[bits 32]
section .data
put_int_buffer dq 0         ;定义8字节缓冲区用于数字到字符的转换
section .text
;--------------------------------------------------
;put_str 通过put_char 来打印以0字符结尾的字符串
;--------------------------------------------------
;输入：栈中参数为打印的字符串
;输出：无

global put_str
put_str:
;由于本函数只用到了ebx和ecx，所以只备份这俩
  push ebx
  push ecx
  xor ecx, ecx
  mov ebx, [esp + 12]       ;获取打印字符串地址
.goon:
  mov cl, [ebx]
  cmp cl, 0                 ;若这里为0则说明到了字符串末尾
  jz .str_over
  push ecx                  ;为put_char函数传递参数
  call put_char
  add esp, 4                ;回收空间
  inc ebx
  jmp .goon

.str_over:
  pop ecx
  pop ebx
  ret

;-------------- put_char ----------------
;功能描述： 把栈中的1个字符写入光标所在处
;----------------------------------------
global put_char
put_char:
  pushad                ;备份32位寄存器,push all double,入栈先后次序是EAX->ECX->EDX->EBX->ESP->EBP->ESI->EDI
  ;需要保证gs中为正确的视频段选择子，为保险起见，每次打印时都为gs赋值
  mov ax, SELECTOR_VIDEO    ;不能直接把立即数送入段寄存器
  mov gs, ax

;;;;;;;;;;; 获取当前光标位置 ;;;;;;;;;;;;;
;先获得高8位
  mov dx, 0x03d4    ;索引寄存器
  mov al, 0x0e      ;提供光标位置的高8位
  out dx, al        ;首先得确定我们需要读的寄存器的索引
  mov dx, 0x03d5    ;通过读写数据端口0x3d5来获得或设置光标位置
  in al, dx         ;的到了光标位置的高8位
  mov ah, al
;再获取低8位
  mov dx, 0x03d4
  mov al, 0x0f
  out dx, al
  mov dx, 0x03d5
  in al, dx

;将光标存入bx
  mov bx, ax
  ;下面这一行是在栈中获取待打印字符
  mov ecx, [esp + 36]       ;这里pushad压入4×8=32字节，然后加上主掉函数的返回地址，因此esp+36字节
  cmp cl, 0xd               ;0xd代表回车键，这里实在检查是否为不可打印字符
  jz .is_carriage_return

  cmp cl, 0xa               ;0xa代表换行符
  jz .is_line_feed

  cmp cl, 0x8               ;0x8代表退格
  jz .is_backspace
  
  jmp .put_other
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.is_backspace:
;;;;;;;;;;;;;; backspace说明 ;;;;;;;;;;;;;;;
;当咱们退格的时候，本质只需要将光标移向前一个显存位置即可，后面再输入的字符自然会覆盖此处的字符
;但有可能退格后不输入新字符，这时光标和被退格的字符分离，所以我们需要添加空格或者字符0
  dec bx            ;光标值减一则我们额光标值就指向前一个字符
  shl bx,1          ;由于一个字符占2字节，所以这里得乘2
  mov byte [gs:bx], 0x20    ;将待删除字符低8字节传入0x20,指空格
  inc bx
  mov byte [gs:bx], 0x7     ;表示黑屏白字
  shr bx, 1         ;恢复光标值
  jmp .set_cursor 

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.put_other:
  shl bx,1          ;表示对应显存中的偏移字节
  mov [gs:bx], cl   ;ASCII字符本身
  inc bx
  mov byte[gs:bx], 0x07     ;字符属性
  shr bx, 1                 ;下一个光标值
  inc bx
  cmp bx, 2000              ;若小于2000,则表示没写道显存最后，因为80×25=2000
  jl .set_cursor            ;若大于2000,则换行处理,也就是进行下一行的换行处理

.is_line_feed:
.is_carriage_return:
;如果是回车，则只需把光标移动到首行
  xor dx, dx        ;dx是被除数的高16位，清0
  mov ax, bx        ;ax 是除数的低16位
  mov si, 80        ;\n和\r都表示下一行行首
  div si            ;光标值减去除以80的余数便是取整，div指令中dx存放余数
  sub bx, dx


.is_carriage_return_end:
  add bx, 80        ;由于换了一行，所以这里加上80
  cmp bx, 2000
.is_line_feed_end:
  jl .set_cursor 

;将1～24行搬运到0～23,然后24行填充空格
.roll_screen:       ;开始滚屏
  cld               ;设置方向标志位为增
  mov ecx, 960      ;2000-80=1920,共1920×2=3840个字符，而movsd一次复制4字节，所以需要3840/4=960次
  mov esi, 0xc00b80a0   ;第1行行首
  mov edi, 0xc00b8000   ;第0行行首
  rep movsd         ;循环复制

;;;;;;;;; 将最后一行填充为空白
  mov ebx, 3840     ;最后一行首字符的地一个字节偏移 = 1920×2
  mov ecx, 80
.cls:
  mov word [gs:ebx], 0x0720     ;0x0720是黑底白字空格
  add ebx, 2
  loop .cls
  mov bx, 1920      ;移动光标至最后一行开头

.set_cursor:
;将光标设为bx值，这里和之前获取类似
;;;;;;;; 先设置高8位 ;;;;;;;;
  mov dx, 0x03d4
  mov al, 0x0e
  out dx, al
  mov dx, 0x03d5
  mov al, bh
  out dx, al

;;;;;;;; 再设置低8位 ;;;;;;;;
  mov dx, 0x03d4
  mov al, 0x0f
  out dx, al
  mov dx, 0x03d5
  mov al, bl
  out dx, al

.put_char_done:
  popad
  ret

;----------- 将小端序的数字变成对应的ASCII后，倒置 ------------------
;输入：栈中参数为待打印的数字
;输出：在屏幕上打印十六进制数字，并不会打印前缀0x
;--------------------------------------------------------------------
global put_int
put_int:
  pushad
  mov ebp, esp
  mov eax, [ebp + 4*9]
  mov edx, eax
  mov edi, 7        ;指定在put_int_buffer中初始的便宜量
  mov ecx, 8        ;32位数据中16位数据占8位
  mov ebx, put_int_buffer

;将32位数字按照十六进制的形式从低位到高位逐个处理，共处理8个十六进制
.16based_4bits:     ;每4位二进制是十六进制的1位
  and edx, 0x0000000F   ;解析十六进制的每一位
  cmp edx, 9        ;数字0～9和a~f分别处理成对应字符
  jg .is_A2F
  add edx, '0'      ;ASCII码是8位大小
  jmp .store
.is_A2F:
  sub edx, 10       ;A~F减去10所得的差再加上字符A的ASCII码就是其对应的ASCII码
  add edx, 'A'

;将每一位数字转换成对应的字符后，按照类似“大端”的顺序存储到缓冲区put_int_buffer,这里就相当与我们把它当做了字符处理
.store:
;此时dl中应该是数字对应的ASCII码
  mov [ebx+edi], dl
  dec edi
  shr eax, 4
  mov edx, eax
  loop .16based_4bits 

;现在put_int_buffer中全是字符，咱们打印前把高位连续的字符去掉，比如000123变成123
.ready_to_print:
  inc edi           ;此时edi为-1（0xFFFFFFFF），加一变为0
.skip_prefix_0:
  cmp edi, 8        ;若已经比较到第9个字符了则表示待打印字符串全为0
  je .full0
;找出连续的0字符，edi作为非0的最高位字符的偏移
.go_on_skip:
  mov cl, [put_int_buffer+edi]
  inc edi
  cmp cl, '0'
  je .skip_prefix_0 ;继续判断下一个字符 
  dec edi           ;由于edi指向的是下一个字符，所以这里得减一是的edi指向当前不为0字符
  jmp .put_each_num

.full0:
  mov cl, '0'       ;输入全为0,则只打因0
.put_each_num:
  push ecx          ;此时cl中为可打印字符
  call put_char
  add esp, 4
  inc edi
  mov cl, [put_int_buffer+edi]
  cmp edi, 8
  jl .put_each_num
  popad
  ret
