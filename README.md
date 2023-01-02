## 0x00 基础知识们
### 1.保护模式
咱们前面说过一个模式，那就是实模式，有看过我之前文章的同学可能知道，实模式就是咱们的程序编址都是在物理地址下进行，但是这对于咱们如今程序员实现软件编程和程序共享是十分困难的，因为在不同的机器上使用物理地址很有可能出现程序之间的冲突，并且实模式的寻址也只能支持1MB内存，这对于我们现在的程序肯定是完全不够的，所以这里就需要用到另外一种模式，也就是保护模式。
现在咱们的操作系统的步骤我在这里总结一下，首先是BIOS加载位于磁盘0扇区的MBR，然后MBR加载位于磁盘2扇区的Loader，且到现在为止，他们都是位于实模式下，现在我们继续介绍保护模式。
![](https://ts1.cn.mm.bing.net/th/id/R-C.42b8b4f8c9bce60b3e1c9ca5f27ebbc7?rik=4qpHwCFR9oIuxQ&riu=http%3a%2f%2fpic.616pic.com%2fys_bnew_img%2f00%2f22%2f14%2fpoCVtSyqkb.jpg&ehk=Dzz%2femSXBTDRkncBWPDft9wkyLK%2bhMi1qsRKUXPWnsY%3d&risl=&pid=ImgRaw&r=0)

保护模式为了解决上述实模式遇到的困难，首先那当然是寄存器不能就只有16位了，由于我们实现的是32位操作系统（这里实现32位是因为他相较之于64位会简单些，我实现这个操作系统是为了学习而已，请各位师傅轻喷），这里注意我们在实模式下虽然说寄存器只用到了16位，但并不代表这个寄存器只有16位，实际上寄存器位数取决与你的CPU是多少位的，这里我们实现的是32位系统，也就是32位CPU，所以我们的寄存器实际上是有32位的，实模式只不过只用了他的低16位表示而已。

这里给出扩展了的寄存器，就打比方ax来说，ax代表低16位，eax则代表整个32位寄存器，当然现在有了64位寄存器，那么rax代表的是64位
![](http://imgsrc.baidu.com/super/pic/item/71cf3bc79f3df8dc68887e558811728b461028b9.jpg)
由于咱们的寻址位数扩展的32位，按字节编地址，则咱们的寻址范围大小就有2^32，即为4GB。但这里有个例外，那就是段基地址寄存器他仍然是16位，就是上述S结尾的寄存器们。我们都知道段寄存器是存储某个段的开头地址，所以说这里咱们内存扩展到4GB之后段基址应该不至于不变呐，这里我首先给个结论，那就是此时的段基址寄存器里面所保存的已经不再是基地址了，而是一个被称作选择子的东西，这个选择子我们之后再详细介绍，这里我们只需要知道段基址还是16位寄存器，而且寻址方式还是段基址加上偏移就够了。

但是为啥不能直接给段基址寄存器变为32位然后直接按照实模式那样寻址呢，那是因为在保护模式下，为了突出保护模式这个名称的意思，我们在访问段的时候就必须添加一些适当的约束，比如说访问控制等。所以这些控制条件在一个寄存器下是放不下的，因此就专门设计了一个数据结构————全局描述表。这个表里面没一个表项称为段描述符，其大小为64字节，这个描述符就用来描述自己所对应的那个内存段的起始地址、大小、权限等信息。这个全局描述表由于表示内存所有段信息，所以十分大，因此存在一个叫做GDTR寄存器专门指向表地址。
![](http://imgsrc.baidu.com/super/pic/item/960a304e251f95ca7e0f63728c177f3e66095278.jpg)
这样之后，咱们的段基址寄存器所保存的就不是段基址了，他保存的是所寻址段在全集描述表下的某一个段描述符的索引，我们可以将全局描述表看作一个数组，然后每个段描述符是其中的元素，我们此时要寻找那个段描述符只需要给出下标索引即可，咱们的段基址寄存器现在就保存的是这个下标，他还有个名字那就是选择子。
这里还有亮点需要说出：
1. 段描述符位于内存当中，这对CPU来说十分慢（相较之于访问寄存器来说）
2. 段描述符格式奇怪，一个数据需要分三个地方存储，这对于CPU来说无疑更加麻烦
   
所以针对上述两个问题，80286的保护模式给出了解决方案，那就是采用缓存技术，将段信息用一个寄存器来保存，这就是段描述符寄存器（Descriptor Cache Registers）。对程序员不可见，如同Cache一样。
以下给出各CPU版本下的段描述符寄存器结构：
![](http://imgsrc.baidu.com/super/pic/item/b03533fa828ba61e0fc138eb0434970a314e59c3.jpg)

### 2,保护模式寻址变化
在实模式之前，寻址方式有相对寻址，基址寻址、变址寻址、直接寻址、间接寻址等（这里对于学习过计算机组成原理的同学应该不在话下，如果这里不熟也可以网上翻阅，百度、csdn都可以，CSDN虽然差评很多，但是对于一些基础知识也是有很多好的博客的），具体形式参考以下代码：
```
mov ax, [si] 
mov ax, [di] 
mov ax, [bx] 
mov ax, [bx+si] 
mov ax, [bx+si+Oxl234] 
mov ax, [bx+di] 
mov ax, [bx+di +Oxl2 34] 
```
实模式下对于内存寻址来说，其中的基址寻址、变址寻址、基址变址寻址，这三种形式中的基址寄存器只能是 bx ，bp，变址寄存器只能是si 、 di ，也就是说，只能用这 个寄存器。其中 bx 默认的段寄存器是由，它经常用于访问数据段， bp默认的段寄存器是 SS ，它经常用于访问栈。

总之在实模式下每个寄存器有其独特的使命，寄存器不能随便瞎用，否则会报错。

但是在保护模式下这写寄存器对于内存寻址就不会这么刻板，而是所有32位寄存器都可以参与内存寻址。

### 3. 模式反转
由于我们的CPU运行模式有实模式和保护模式两种，为了兼顾他们，所以设计CPU十分困难。CPU处于实模式下时，虽然一切都是16位寄存器，但这并不代表寄存器只有16位，他2依然可以使用32位的资源，也就是说他们的资源是互通的，无论在哪种模式下都可以使用他们。但是我们如何知道同一个汇编语句是在哪种模式之下呢。
首先我们来看看指令格式，如下图：
![](http://imgsrc.baidu.com/super/pic/item/267f9e2f0708283810962cd0fd99a9014d08f15f.jpg)
这里给出一个简单的例子，比如在表示bx寄存器的时候，实模式是使用010来表示，但是在保护模式的时候010就代表了ebx，但是相同的指令格式，CPU并不知道你到底是实模式还是保护模式下，所以这就得交给我们的编译器来决定。
因此编译器提供了伪指令[bits]。现在我们的Loader还是在实模式下运行，但是他要实现从实模式下到保护模式的转化，所以在Loader这个程序中需要同时存在实模式和保护模式的代码。这里给出[bits]伪指令的功能：
+ bits的指令格式为[bits 16]或[bits 32]
+ [bits 16]是告诉编译器，下面的代码请给出编译成16位的机器码
+ [bits 32]是告诉编译器，下面额代码请给出编译成32位的机器码

“下面的代码”就是从这个bits标签到下一个bits标签所包含的范围。
说完模式反转的基础知识，这里我再给出一般进入保护模式的方法：
1. 打开A20
2. 加载gdt
3. 将cr0的pe位置1

这里的几个东西大家看不懂没关系，后面我会细讲。这三个步骤可以不顺序也可以不连续。这里再给出几个模式反转例子供大家参考。首先请看以下代码：
```
[bits 16] 
mov ax, Oxl234 
mov dx, Oxl234  
[bits 32] 
mov eax , Oxl234 
mov edx, Oxl234 
```

这里再给出机器编译之后的指令：
![](http://imgsrc.baidu.com/super/pic/item/960a304e251f95ca7b866e728c177f3e66095287.jpg)
可以看出在使用[bits 32]前后，咱们的机器指令是有所改变的，之前操作的是ax，之后操作的是eax，但是其中的操作码没有变化。因此为了让CPU第一时间知道指令操作的是ax还是eax，我们需要在操作码前加上一个前缀字段，在上面给出的指令格式中也可以看到。这里我们重点来介绍0x66反转和0x67反转。

0x66反转的含义是不管当前模式是什么，总是转变相反的模式运行
比如，在指令中添加了0x66反转前缀后：
假设当前是16位实模式，操作数大小将变为32位
假设当前运行模式是32位保护模式，操作数大小将变为16位
这里给出例子，代码如下：
```
[bits 16) 
mov ax, Oxl234 
mov eax, Oxl234 
[bits 32 ] 
mov ax, Oxl234 
mov eax , Oxl234
```

然后就是编译后的机器指令
![](http://imgsrc.baidu.com/super/pic/item/a8ec8a13632762d0e9e7587be5ec08fa503dc61d.jpg)
这里我们可以看到虽然在bits16下，但是我们增加了0x66前缀，使得编译的机器指令是以32位来进行编译的，bits32类似。

---

接下来我们介绍0x67前缀，0x66前缀使得在当前模式可以使用其他模式的操作数，而0x67则可以在当前模式使用其他模式的寻址方式。我们同样用一个例子来进行演示：
```
[bits 16]
mov word [bx], Ox1234
mov word [eax], Ox1234
mov dword [eax], Ox1234
[bits 32]
mov dword [eax], Ox1234
mov word [eax], Ox1234
mov dword [bx], Ox1234
```

之后便是对应的机器指令
![](http://imgsrc.baidu.com/super/pic/item/4034970a304e251f3c3e183ce286c9177e3e532d.jpg)

其中可以看到第二行使用0x67号前缀，这就使得咱们可以在16位实模式的情况下使用eax，同样第8行也使得在当前保护模式下可以使用16位寄存器bx。
以上，关于模式反转介绍完成。

### 4.全局描述符表（Global Descriptor Table，GDT）
这个表在前面咱们小小的提出来过，这里给出他的详细解释
首先我们知道他类似于段描述符为元素的数组，所以我们首先给出段描述符的结构(共占用8字节)：
![](http://imgsrc.baidu.com/super/pic/item/5882b2b7d0a20cf453bb011b33094b36adaf99cf.jpg)
上图是因为方便观看所以分为了两部分，实际上他俩是连续的。这里我来解释下各字段的含义：
+ 由于保护模式下地址总线宽度为32位，所以段基址需要用32位来表示。
+ 段界限表示段扩展边界的最值，即最大扩展多少（代码段、数据段等）或最小扩展多少（栈），段界限用20位来表示，所以能表示最大范围大小为2^20,注意这个段界限值为一个单位量，他的单位要么是字节，要么是4KB，这是由描述符中的G位来决定的，因此段的大小要么是2^20字节，即1MB，要么是2^20**** 4KB=4GB.
+ G位若为0,则表示段界限粒度为1字节，若为0则表示段界限粒度为4KB
+ S字段，一个段描述符，在CPU里面分为两大类，一个是描述系统段，一个是描述数据段。在CPU眼里，凡是硬件运行所需要的东西都可称之为系统，凡是软件运行所需要的东西都可以称之为数据，无论是代码、还是数据，包括栈都是作为硬件的输入，都只是给硬件提供数据而已，所以代码段在段描述符中也属于数据段（非系统段）。S为0表示系统段、S为1表示数据段。
+ 8～11位为TYPE字段，用来指定本描述符的类型。这个TYPE字段需要由上述S位来决定具体意义，具体对应结构如下：
![](http://imgsrc.baidu.com/super/pic/item/14ce36d3d539b6007688f073ac50352ac75cb7b4.jpg)
其中我们关注的是非系统段，系统段在以后解释。
其中熟悉linux的同学可能知道X(执行),R(读),W(写)位分别所代表的含义，A表示Accessed位，这是由CPU来设置的，每当CPU访问过后，会将此为置1，所以创建一个新段描述符时应将此位置0.C表示一致性代码，Conforming，是指如果字节是转移的目标段，并且是一致性代码段，自己的特权值一定要高于当前特权值，转移后的特权级不与自己的DPL为主，而是与转以前的低特权级一致，也就是听从、依从转移前的低特权级。C为1表示该段是一致性代码，为0表示非一致性代码。E是用来表示段扩展方向，E为0表示向上扩展，为1表示向下扩展。
+ 13～14位为DPL字段，Descriptor Privilege Level，即描述符特权级，指本内存段的特权级，这两位能表示4种特权级，即0,1,2,3级，数字越小，特权级越大。CPU由实模式进入保护模式后，特权级自动为0。用户程序通常处于3级
+ P字段，Present，表示段是否存在。若段存在与内存中，则P为1,否则为0。P位是由CPU来检查的，若为0,则CPU会抛出异常。
+ AVL字段，Available，即为“可用”，这里的可用是针对用户来说的，也就是说操作系统可以任意使用。
+ L字段，用来设置是否为64位代码段，L为1则表示64位代码段，否则表示32位代码段。
+ D/B字段，用来指示有效地址（段内偏移地址）及操作数大小。对于代码段来说，此位为D位，若D为0,则表示有效地址和操作数为16,指令有效地址用IP寄存器。若D为1,表示指令有效地址及操作数为32位，指令有效地址用EIP寄存器。对于栈段来说，此位为B位，用来制定操作数大小，若B为0则使用个SP寄存器，若B为1使用esp寄存器。

--- 
### 5.段描述符寄存器（GDTR）
我们知道，内存中存在着一个全局描述符表GDT，里面存放着一个个段描述符，所以我们需要一个寄存器来指向这个描述符表，这个寄存器就叫做GDTR（这个寄存器比较特殊，有48位），这里给出他的结构：
![](http://imgsrc.baidu.com/super/pic/item/3c6d55fbb2fb43164a1d84be65a4462308f7d3f0.jpg)
低16位表示GDT以字节为单位的界限值，相当于GDT字节大小-1.后32位表示GDT的起始地址，而每个段描述符占用8字节，则最大的段描述表可以存放2^16/8 = 8192 = 2^13个段或门（即为系统段）。不过对于此寄存器的访问无法用mov gdtr, xxxx这种指令为gdtr初始化,存在有专门的指令实现此功能，这就是lgdt指令。虽然说我们是为了进入保护模式才使用这个指令，看似此指令只能在实模式下执行，但实际上他也可以在保护模式下执行。
lgdt指令格式为：
lgdt 48位内存数据（这48位内存数据在上面讲的很清楚）
咱们知道了全局描述符表、段描述符、选择子（即为段描述符在描述表中的索引）的概念之后，这里讲如何使用他们。
有于咱们的段基址寄存器CS、DS、ES、FS、GS、SS，为16位，所以他们存储的选择子也为16位，其中低2位（也就是第0、1位）用来存储RPL，即请求特权级，可以表示4种特权级。在选择子的第2位是TI位，即Table Indicator，用来指示选择子实在GDT中还是LDT中。TI为0表示在GDT中，为1表示在LDT中。剩余13位即表示索引值，以下给出选择子结构：
![](http://imgsrc.baidu.com/super/pic/item/b219ebc4b74543a96c5304085b178a82b80114ae.jpg)

### 6. 打开A20地址线
我们首先要知道什么是地址回绕，在处于实模式下时，只有20位地址线，即A0～A19,20位地址线能表示2^20字节，即为1MB大小，0x0～0xFFFFF，若内存超过1MB，则需要21条地址线支持。因此若地址进位到1MB以上，如0x100000,由于没有21位地址线，则会丢弃多余位数，变成0x00000,这种就叫做回绕
![](http://imgsrc.baidu.com/super/pic/item/3801213fb80e7bec3b4cfd256a2eb9389a506b64.jpg)
而当CPU发展到80286后，虽然地址线从20位发展到24位从而能访问16MB，但任何时候兼容总得放第一位。80286是第一款具有保护模式的CPU，他在实模式下也应该和之前的8086一模一样。也就是仍然只使用20条地址线。但是80286有24条地址线，也就是A20地址线是存在的，若访问0x100000~0x10FFEF之间的内存，系统将直接访问这块物理内存，而不会像之前那样回绕到0.

为了解决上述问题，IBM在键盘控制器上的一些输出线来控制第21根地址线（A20）的有效性，故被称为A20Gate。
+ 若A20Gate打开，则不会回绕
+ 若A20Gate关闭，则回绕。
这样就完美兼容了实模式和保护模式。所以我们在进入保护模式前需要打开A20地址线，操作就如同读取硬盘控制器类似，将端口0x92的第一位置1就可以了，如下：
```
in al，0x92
or al,0000_0010B
out 0x92,al
```

### 7.开启保护模式
这是进入保护模式的最后一步，这里我们简单介绍一下控制寄存器CR0,下面是他的结构：
![](http://imgsrc.baidu.com/super/pic/item/0df3d7ca7bcb0a46c1e375fd2e63f6246a60af35.jpg)
![](http://imgsrc.baidu.com/super/pic/item/ae51f3deb48f8c5417193beb7f292df5e1fe7f28.jpg)
我们这里只需要关注其中的PE段即可，此位用于开启保护模式，是保护模式的开关，只有当打开此位后，CPU才会真正进入u保护模式。代码如下：
```
mov eax, cr0
or eax, 0x00000001
mov cr0, eax
```
## 0x01 进入保护模式实现
这里我们需要修改上次的代码，由于loader.bin是用来进入保护模式的，由于其会超过512字节，所以我们需要吧mbr.S中加载loader.bin的读入扇区增大，目前它是1扇区，为了避免以后再次修改，我们直接改为读入4扇区，修改代码如下：
```
mov cx,4  ;待读入扇区数
call rd_disk_m_16
```

还有一个需要修改的代码是include/boot.inc,这里增加了一些配置信息，loader.S中用到的配置都是定义在boot.inc中的符号，代码如下：
```
;---------- loader 和 kernel ------------
LOADER_BASE_ADDR equ 0x900     ;内存首址
LOADER_START_SECTOR equ 0x2     ;硬盘扇区
;---------- gdt描述符属性 ---------------
DESC_G_4K equ 1_000000000000000000000000b     ;G位，表示粒度
DESC_D_32 equ  1_00000000000000000000000b     ;D位，表示为32位
DESC_L    equ   0_0000000000000000000000b     ;64位代码标记，此处为0即可
DESC_AVL  equ    0_000000000000000000000b     ;CPU不用此位，此位为0
DESC_LIMIT_CODE2 equ 1111_0000000000000000b   ;表示代码段的段界限值第二段
DESC_LIMIT_DATA2 equ DESC_LIMIT_CODE2         ;表示数据段的段界限值第二段
DESC_LIMIT_VIDEO2 equ 0000_000000000000000b
DESC_P    equ 1_000000000000000b              ;表示该段存在
DESC_DPL_0 equ 00_0000000000000b              ;描述该段特权值
DESC_DPL_1 equ 01_0000000000000b
DESC_DPL_2 equ 10_0000000000000b
DESC_DPL_3 equ 11_0000000000000b
DESC_S_CODE equ 1_000000000000b               ;代码段为非系统段
DESC_S_DATA equ DESC_S_CODE                   ;数据段为非系统段
DESC_S_sys equ 0_000000000000b
DESC_TYPE_CODE equ 1000_00000000b             ;x=1,c=0,r=0,a=0,代码段可执行，非一致性，不可读，已访问位a清0

DESC_TYPE_DATA equ 0010_00000000b             ;x=0,e=0,w=1,a=0,数据段不可执行，向上扩展,可写,已访问位a清0

DESC_CODE_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + \        ;定义代码段的高四字节
DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + \
DESC_P + DESC_S_CODE + \
DESC_TYPE_CODE + 0x00

DESC_DATA_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + \        ;定义数据段的高四字节
DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + \
DESC_P + DESC_S_DATA + \
DESC_TYPE_DATA + 0x00

DESC_VIDEO_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + \
DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x00

;----------- 选择子属性 --------------
RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
RPL3 equ 11b
TI_GDT equ 000b
TI_LDT equ 100b

```

在定义完一些结构文件之后，我们现在来改写今天的重中之重，Loader.S,其中添加的部分主要是进入保护模式的部分

