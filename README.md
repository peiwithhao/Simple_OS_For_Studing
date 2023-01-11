## 0x00 基础知识们
上次我们已经成功载入了内存，至此我们之后的开发就是基于内核了，而内核大部分是由C来写成，所以这里我们得先知道点高级函数的一些规约。
其中在汇编语言这一阶段我们面临的问题就有这么两个：
1. 在调用函数的过程中，谁来清理参数所占用的栈空间呢
2. 传递参数该以何种顺序压栈

这里我就直接给出几个调用规约
![](http://imgsrc.baidu.com/super/pic/item/14ce36d3d539b600ddee1572ac50352ac75cb7d7.jpg)
我们的操作系统是采用其中的cdecl规约，这就意味着：
1. 调用者将所有参数由右至左压栈
2. 由调用者清理参数所占的栈空间

## 0x01 汇编结合C语言
基础知识没了，这次就这么简单哈哈，剩下的时间咱们就来完善我们的内核
### 1.系统调用
这里系统调用我们可以理解为操作系统提供给用户程序来使用高级操作的功能接口。
这里的系统调用类似于BIOS的中断调用，但这里有个不同的点就是BIOS的中断调用每个功能有一个功能号，而系统调用的入口就只有一个，那就是0x80号中断，我之前是这样理解的，我们使用int 功能号是在实现中断，而中断的功能号分为很多种，其中0x80这个中断号就表示的是系统调用，所以在我的印象中系统调用也属于中断的一种，这个在之后学习中断的过程中再进行验证。
今天我们是奔着实现打印函数来的，所以我们这里做一个write系统调用的小demo，这里我们可以通过如下指令来查看系统调用相关知识
```
man 2 write
```

这里man 后面的2是来表示查看System Call方面的帮助
结果如下：
![](http://imgsrc.baidu.com/super/pic/item/f9dcd100baa1cd113a527fe5fc12c8fcc2ce2d0a.jpg)
而咱们调用“系统调用”有两种方法：
1. 将系统调用指令封装为C库函数，通过库函数进行系统调用
2. 直接通过int指令与操作系统通信

还有一个点需要注意，那就是我们在直接进行系统调用的过程中，linux的传参规则如下：
1. eax存储功能号
2. ebx存储第1个参数
3. ecx存储第2个参数
4. edx存储第3个参数
5. esi存储第4个参数
6. edi存储第5个参数
7. 多于5个参数存放在栈上

这里似乎跟我们印象中32位传参存放在栈中不一致，这是因为咱们之前是调用的C库，库函数掩盖了其中的具体细节，在下面的代码中我们可以从中了解到

这里直接给出demo的代码。大家可以看懂了直接去实验一把，文件名为syscall_write.S：
```
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

```

然后我们进行如下编译链接：
```
nasm -f elf -o syscall_write.o syscall_write.S
```

-f指定文件爱你格式，目的是将来要和gcc编译的elf文件格式的目标文件链接
然后使用ld将syscall_write.o链接成可执行elf文件
```
ld -m elf_i386 -o syscall_write.bin syscall_write.o
```

这里若生成后没执行权限记得使用chmod命令，这里直接网上搜索就行
下面是执行结果，可以看到分别使用两种方法进行了输出
```
dawn@dawn-virtual-machine:~/repos/OS_learning/kernel$ ./syscall_write.bin
c library says: hello world!
syscall says: hello world!
```

### 2.汇编与C语言联动
这里直接给出代码然后进行讲解：
首先是C_with_S_c.c
```
extern void asm_print(char*,int);
void c_print(char* str){
  int len=0;
  while(str[len++]);
  asm_print(str, len);
}

```

然后是C_with_S_S.S
```
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

```
这里有两点解释清楚：
1. 汇编代码导出符号用关键字global，引用外部文件使用关键字extern
2. C代码中只要将符号定义为全局即可被外部引用，引用外部符号用extern

### 3.显卡端口简介
之前还记得我们在loader时期输出文本采用的什么方法吗，这里给大家回忆下，我们之前是直接使用BIOS中断或者是往显存中写入我们的字符，但这好像没什么技术含量。
之前我们写过点显卡的知识，但是大家是否注意到我们并没有详细解释显卡的各种端口，就如同硬盘控制器那般，之前不讲解照作者的意思是说端口太多权退，所以留在这儿讲解（注意这里大伙也不要慌，这就类似于咱们对硬盘控制器的操作而已，区别就是端口不太一样），这里先给出显卡中的寄存器介绍
![](http://imgsrc.baidu.com/super/pic/item/1e30e924b899a901d27b54f358950a7b0308f5ad.jpg)
如图，寄存器好像也不是很多嘛，但其实这只是目录而已（狗头
上面的目录其实就是寄存器的分组，前四组寄存器被分成了两类寄存器，即Address Register 和 Data Register。
这里分组的目的是因为显卡中的端口太多，若一个端口占用一个寄存器的话十分昂贵且浪费，所以采用分组设计，其中Address Register存放着制定数组下标，另一个寄存器则对索引指定的下标元素进行输入输出操作。所以对这类端口的操作就是先指定Address Register的索引值，然后再对Data Register进行操作。
上图中CRT Controller Register寄存器组中的A（Address Register）和D（Data Register）端口比较特殊，它的端口地址不固定，具体取决于Miscellaneous Output Register寄存器中的Input/Output Address Select字段，如下所示
![](http://imgsrc.baidu.com/super/pic/item/e850352ac65c1038b1afc107f7119313b17e8926.jpg)
以及其中各字段的英文描述
![](http://imgsrc.baidu.com/super/pic/item/3812b31bb051f8190ae40c8f9fb44aed2f73e736.jpg)
这里的I/OAS（Input/Output Address Select）字段不仅影响CRT Controller Resisters寄存器组中的Address 和 Data寄存器，也影响Feature Control register，这里大致解释一下：
+ I/OAS 此位用来选择CRT controller寄存器组的地址
+ 此位为0时：
CRT寄存器组的端口地址被设置为0x3Bx，结合之前咱们的目录表，Address寄存器和Data寄存器实际值被设置为0x3B4-0x3B5,而Input Status #1 Register寄存器的端口地址被设置为0x3BA
+ 此位为1时：
CRT寄存器组的端口地址被设置为0x3Dx，与上述类似

默认情况下。Miscellaneous Oustput Register寄存器的值为0x67,其中可以知道I/OAS位为1,所以咱们就不用管他，照我们上述的端口值操作即可，这里给出CRT Controller Registers的各个寄存器，其他的给出来参考参考，我们主要是讨论CRT
![](http://imgsrc.baidu.com/super/pic/item/7aec54e736d12f2ebb19a0a50ac2d562843568d2.jpg)
![](http://imgsrc.baidu.com/super/pic/item/1e30e924b899a901d12a55f358950a7b0308f5dc.jpg)
![](http://imgsrc.baidu.com/super/pic/item/d53f8794a4c27d1e803f35015ed5ad6edcc438dd.jpg)
![](http://imgsrc.baidu.com/super/pic/item/bd315c6034a85edf9097f0e60c540923dc5475df.jpg)

### 4.实现单个字符打印
这里我们先再创建个目录来整理我们编写的库函数，这里定义为lib，lib目录下再定义两个目录，分别为user 和 kernel
然后我们在于lib目录下建立下面的这个stdint.h文件，这里声明了一些常用数据类型宏，十分简单
```
#ifndef_ ___LIB_STDINT_H
#define ______LIB_STDINT_H
typedef signed char int8_t;
typedef signed short int int16_t;
typedef signed int int32_t;
typedef signed long long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
#endif

```

我们要实现的打印字符函数名为put_char,这里需要使用汇编语言编写，因为我们需要和显卡打交道，我们来写一个print.S汇编文件来实现此功能,我们将该文件存放在lib/kernel目录下，在此之前先来梳理一下处理流程：
1. 备份寄存器现场
2. 获取光标坐标值，即下一个可打印字符的位置
3. 获取待打印字符串
4. 判断字符是否为控制字符，若是则进入相应处理流程，否则进入输出处理
5. 判断是否需要滚屏
6. 更新光标坐标值，使其指向下一个打印字符位置
7. 恢复寄存器现场，退出。


首先给出部分代码：：
```
TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

[bits 32]
section .text
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

```

这段代码很简单，我们首先获取了光标的高低8位，然后我们再从栈中获取参数，之后就是对应特殊字符的处理，如下：
```
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
  jl .set_cursor            ;若大于2000,则换行处理

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

```

上述代码分别实现了退格处理，换行或回车处理。
如果我们写入的字符超过2000了怎么办呢，也就是说我们这一个屏幕写满了该如何，大伙看平时咱们写个文档，一面写完了他会自动往下加一个行对吧，我们接下来就要实现这个功能，这个功能也叫做滚屏，我们先了解一些基础知识。
但是我们的显存是有32KB的，且每屏有2000个字符也就是4000字节，所以说显存可以存放32KB/4000B=8个屏的字符，但是咱们一个屏幕肯定放不下这么多，所以显卡提供了两个寄存器用来设置显存中那些在屏幕上显示的字符的起始位置，他们分别是上面CRT寄存器中索引为0xc的Start Address High Register和0xd的Start Address Low Register,这俩都是8位寄存器。
这两个寄存器的功能是屏幕自动从这俩寄存器的地址开始，向后显示2000个字符。若这里的地址过大，显示的字符将会在显存中实现回绕，这里大家应该很熟悉回绕的概念了。
这里还有第二种方法，那就是我们把屏幕固定在最初2000字节，因此我们上面这俩寄存器值只需为0即可，这会简化我们的操作，但是这种方法也有缺点那就是只能缓存2000字符。

这里采用第二种方法，若是想使用第一种方法可以参照上面对于显卡端口的操作尝试一下，下面是第二种方法的步骤：
屏幕每行80字符，共有25行
1. 将1～24行的内容整块搬到第0～23行，将第0行覆盖
2. 再将第24行用空格覆盖
3. 光标一道第24行行首

代码如下：
```
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

```

逻辑十分简单，并且我们的put_char函数也已经正式完成，但print.S文件中的put_char对其他文件来说属于外部函数，所以每个文件都得包含对此函数的引用，因此我们编写一个头文件来方便我们调用，存放在lib/kernel下：
```
#ifnde ______LIB_KERNEL_PRINT_H
#define ______LIB_KERNEL_PRINT_H
#include "stdint.h"
void put_char(uint8_t char_asci);
#endif
```

然后我们改进main.c，也就是咱们的内核，让其中使用put_char函数实现打印字符
```
#include "print.h"
void main(void){
  put_char('K');
  put_char('e');
  put_char('r');
  put_char('n');
  put_char('e');
  put_char('l');
  put_char('\n');
  put_char('h');
  put_char('e');
  put_char('l');
  put_char('l');
  put_char('o');
  put_char('1');
  put_char('\b');
  put_char('2');
  while(1);
}
```

上面看起来十分笨，这是因为我们目前只实现了打印字符，字符串还没整，我们先来试一试看是否成功。
然后进行编译链接，首先编译我们写好的print.S
```
dawn@dawn-virtual-machine:~/repos/OS_learning/lib/kernel$ nasm -f elf -o print.o print.S
```

然后重新编译main.c
```
dawn@dawn-virtual-machine:~/repos/OS_learning/kernel$ gcc -no-pie -fno-pic -m32 -I ../lib/kernel/ -c main.c -o main.o
```

照常链接
```
dawn@dawn-virtual-machine:~/repos/OS_learning/kernel$ ld -m elf_i386 -T link.script -Ttext 0xc0001500 -e main -o ./kernel.bin  main.o ../lib/kernel/print.o
```

然后就是正常打入磁盘
```
dd if=./kernel.bin of=../bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc
```

最后结果如图：
![](http://imgsrc.baidu.com/super/pic/item/5366d0160924ab18cb57eaf270fae6cd7a890bb5.jpg)
很耐思

### 5.实现打印字符串
字符咱们打印成功了，现在咱们还需要打印字符串，不然像上面一样一个一个打太奇怪了，这里实现的原理也十分简单，就是不停调用put_char就行啦。
```
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

```

当然别忘了修改头文件print.h
```
#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H
#include "stdint.h"
void put_char(uint8_t char_asci);
void put_str(char *  message);
#endif

```

之后测试我们的字符串函数
```
#include "print.h"
void main(void){
  put_str("I am Kernel\npeiwithhao");
  while(1);
}

```
之后正常编译链接，直接上效果图
![](http://imgsrc.baidu.com/super/pic/item/ac345982b2b7d0a2e1080f588eef76094a369a36.jpg)
可以看到十分成功的打印出了字符串！

### 6.实现打印整数
类似于上述知识，这里我们定义函数名put_int.
```
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
.store：
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
  jl .put_int_buffer
  popad
  ret

```

然后我们依旧去print.h中修改一下：
```
#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H
#include "stdint.h"
void put_char(uint8_t char_asci);
void put_str(char* message);
void put_int(uint32_t num);     //以十六进制打印
#endif

```

然后我们再去内核函数中测试一下，测试代码就不发了
![](http://imgsrc.baidu.com/super/pic/item/8644ebf81a4c510f11af97df2559252dd52aa588.jpg)
可以看出字符串，字符，数字，都是正常打印，大成功！

## 0x02 总结
这里C语言与汇编结合本来还有一种方法那就是内联汇编，但这对今天的程序编写意义不大，所以留做日后用到时再讲解。
对于今天的程序来说都十分简单易懂，只不过十分繁杂，大家甚至看一个就会写下一个了。
