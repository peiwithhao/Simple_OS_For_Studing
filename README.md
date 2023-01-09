## 0x00 基础知识
这里我再次强烈推荐这本书《程序员的自我修--编译、链接与库》,因为今天我们的知识有一部分是在讲解ELF文件的结构，这本书里面讲的十分的详细，如果说大伙实在不愿意看，我们也可以去ctf wiki上面看看，介绍只不过没这本书详细，这里也给出链接：
[ELF文件结构](https://ctf-wiki.org/executable/elf/structure/basic-info/)
这里我会给出一些当前步骤所需要的必要知识，如果大家想深刻理解的话还是建议我上面说的两个地方去了解。
### 1.ELF文件结构
我们拿到一个文件，我们该从哪儿知道这个文件是什么格式，有多大，什么类型等各种信息呢，可能有的同学会说我们使用检测文件的工具即可，就比如我们上一篇中所说的linux自带的工具file，但是问题是这个file工具又是怎么知道这个文件的格式然后反馈给我们用户的呢，实际上每个文件都会存在有一个文件头，这个文件头里面存放着包含这个文件的各种信息，当然ELF文件也不例外
![](http://imgsrc.baidu.com/super/pic/item/fc1f4134970a304e4536843f94c8a786c8175cda.jpg)
目标文件既会参与程序链接又会参与程序执行。出于方便性和效率考虑，根据过程的不同，目标文件格式提供了其内容的两种并行视图，如下：
![](https://ctf-wiki.org/executable/elf/structure/figure/object_file_format.png)
这里我们首先来介绍ELF header部分
![](http://imgsrc.baidu.com/super/pic/item/9c16fdfaaf51f3de8fa29da1d1eef01f3b2979e6.jpg)
上面是介绍了一些关于elf header的数据类型，下面便是具体的数据结构
```
#define EI_NIDENT   16

typedef struct {
    unsigned char   e_ident[EI_NIDENT];
    ELF32_Half      e_type;
    ELF32_Half      e_machine;
    ELF32_Word      e_version;
    ELF32_Addr      e_entry;
    ELF32_Off       e_phoff;
    ELF32_Off       e_shoff;
    ELF32_Word      e_flags;
    ELF32_Half      e_ehsize;
    ELF32_Half      e_phentsize;
    ELF32_Half      e_phnum;
    ELF32_Half      e_shentsize;
    ELF32_Half      e_shnum;
    ELF32_Half      e_shstrndx;
} Elf32_Ehdr;
```
十分直观，这里我们来简单介绍一下每个成员变量的含义：
+ 首先来介绍一下文件头中的e__ident数组，下面给出表：
![](http://imgsrc.baidu.com/super/pic/item/d009b3de9c82d1589e847d74c50a19d8bd3e42f4.jpg)
+ e_type:占2字节，指示elf目标文件类型，类型如下：
![](http://imgsrc.baidu.com/super/pic/item/09fa513d269759eee021440ef7fb43166c22df86.jpg)
![](http://imgsrc.baidu.com/super/pic/item/3ac79f3df8dcd100d7efdce4378b4710b8122f87.jpg)
+ e_machine:占2字节，指示目标文件需要在哪个机器上才能运行
![](http://imgsrc.baidu.com/super/pic/item/aec379310a55b3194e97a6b006a98226cefc178c.jpg)
+ e_version:占4字节，表示版本信息
+ e_entry:占4字节，表示程序入口地址
+ e_phoff:指明程序头表在文件中的偏移
+ e_shoff:指明文件节头表在文件中的偏移
+ e_flags:关于处理器的一些标志，这里不做具体介绍
+ e_ehsize:指明文件头大小
+ e_phentsize:指明程序头表中每个条目的大小
+ e_phnum:指明程序头表中有多少条目，也就是多少个段
+ e_shentsie:指明节头表中每个条目的大小
+ e_shnum:指明节头表中有多少个条目，也就是多少个节
+ e_shstrndx:用来指明字符串表对应条目在节头表上的索引

以上就是elf header各字段的解释，下面我们来介绍一下程序头表,注意这里严格意义上来说是介绍程序头表中的一个条目，就类似段描述表中介绍段描述符一样，一个程序头表中有着很多下面结构的元素：
```
typedef struct {
    ELF32_Word  p_type;
    ELF32_Off   p_offset;
    ELF32_Addr  p_vaddr;
    ELF32_Addr  p_paddr;
    ELF32_Word  p_filesz;
    ELF32_Word  p_memsz;
    ELF32_Word  p_flags;
    ELF32_Word  p_align;
} Elf32_Phdr;
```

我们还是采用刚刚的讲解方式，这样清楚一点：
+ p_type:表示该段的类型，类型如下
![](http://imgsrc.baidu.com/super/pic/item/1e30e924b899a9018b9f7ff358950a7b0308f549.jpg)
+ p_offset:表示本段在文件中的偏移地址
+ p_vaddr:表示本段在虚拟内存中的起始地址
+ p_paddr:仅用于与物理地址相关的系统中，因为 System V忽略用户程序中所有的物理地址，所以此项暂且保留，未设定。
+ p_filesz:表示本段在文件中的大小
+ p_memsz:表示本段子内存中的大小
+ p_flags:指明本段的标志类型，如下：
![](http://imgsrc.baidu.com/super/pic/item/d52a2834349b033ba414d81f50ce36d3d439bd53.jpg)
+ p_align:对齐方式

到这里我们所需要的elf文件结构的知识已经结束，如果想了解更多可以参考我文章开头的推荐，实际上弄懂elf文件的结构是一件十分畅快的事情，再次推荐那本《程序员的自我修养——链接、装载与库》


## 0x01 将内核载入内存
载入之前我们首先回忆一下我们现在已经用过了的空间，这个环节必不可少，因为我们不能把咱们之前写的东西给覆盖了，这样肯定会带来一些莫名其妙的错误。
咱们先来回忆磁盘，我们在0号磁盘上是打入了MBR，然后写了Loader，这个loader不想和MBR隔太近，于是我们就放在了2号磁盘。
然后回忆物理硬盘，我们在低1MB中除了可用的空间，我们在0x7c00放入的是MBR，但是这里其实可以覆盖他了，因为他没用了已经（十分功利捏），0x900开始存放的loader，然后我们在0x100000后存放的是页目录以及页表,而由于我们内核将会只存放在低端1MB，所以这里之后就不用管了，这里给出低1MB图片：
![](http://imgsrc.baidu.com/super/pic/item/7acb0a46f21fbe0984ace7d12e600c338644ad01.jpg)
上面打勾的都是可用区域。
内核被加载到内存后， loader 还要通过分析其 elf 结构将其展开到新的位置，所以说，内核在内存中有
两份拷贝，一份是 elf 格式的原文件 kernel.bin ，另一份是loader解析elf格式的 kernel.bin 后在内存中生成的
内核映像（也就是将程序中的各种段segment复制到内存后的程序体），这个映像才是真正运行的内核。

这里我给出具体存放的地方，当然大家也可以自己选块好的风水宝地，只需合理即可。
为了以后loader扩展的可能性，我们的kernel.bin放的距离他远一点，我们放在磁盘的9号扇区
```
dd if=./kernel.bin of=../bochs/hd60M.img bs=512 count=20 seek=9 conv=notrunc

```
这里选择写入20块是因为为了防止以后每次修改，这里如果少于20块的话写入会自动停止，大家不需要担心
而内存中我们的内核以后会越来越大，所以我们将内核kernel.bin文件尽量放到比较高的地址，而真正重要的内核映像就放比较前面，所以我们在0x70000这儿放内核文件，这个数字是图方便是个整而已，大家不需要深究。

所以我们接下来的工作主要有两步：
1. 加载内核：把内核文件加载到内核缓冲区
2. 初始化内核：需要在分页后，将加载进来的 elf 内核文件安置到相应的虚拟内存地址，然后跳过去
执行，从此 loader 的工作结束。

首先我们修改boot.inc里面的内容，将内核的代码地址添加上去，如下：
```
KERNEL_START_SECTOR equ 0x9         ;Kernel存放硬盘扇区
KERNEL_BIN_BASE_ADDR equ 0x70000    ;Kernel存放内存首址
KERNEL_ENTRY_POINT equ 0xc0001500   ;Kernel程序入口地址
;----------- ELF文件相关 -----------------
PT_NULL equ 0x0

```

然后我们在加载页表之前来加载内核，代码如下：
```
;------------ 加载kernel ---------------------
  mov eax, KERNEL_START_SECTOR ;kernel.bin所在的扇区号
  mov ebx, KERNEL_BIN_BASE_ADDR     ;从磁盘读出后，写入到ebx指定的地址
  mov ecx, 200                      ;读入的扇区数
  call rd_disk_m_32                 ;上述类似与传递参数,这里类似于mbr.S中的rd_disk_m_32，只需要把寄存器换成32位就行了，大部分一样我局不贴出来了

```

下面就是我们初始化内核的代码：
```
;------------  将kernel.bin中的segment拷贝到编译的地址 ----------------
kernel_init:   ;0xd45
  xor eax, eax
  xor ebx, ebx      ;ebx用来记录程序头表地址
  xor ecx, ecx      ;cx记录程序头表中的program header 数量
  xor edx, edx      ;dx记录program header的尺寸，即e_phentsize

  mov dx, [KERNEL_BIN_BASE_ADDR + 42]   ;距离文件偏移42字节的地方就是e_phentsize
  mov ebx, [KERNEL_BIN_BASE_ADDR + 28]  ;e_phoff
  add ebx, KERNEL_BIN_BASE_ADDR
  mov cx, [KERNEL_BIN_BASE_ADDR + 44]   ;e_phnum

.each_segment:
  cmp byte [ebx + 0], PT_NULL   ;若p_type等于PT_NULL,说明此program未使用
  je .PTNULL
  ;为函数memcpyu压入参数，参数从右往左依次压入
  ;函数原型类似于memcpy(dst, src, size)
  push dword [ebx + 16]     ;program header中偏移16字节的地方是p_filesz,传入size参数
  mov eax, [ebx + 4]        ;p_offset
  add eax, KERNEL_BIN_BASE_ADDR     ;此时eax就是该段的物理地址
  push eax                  ;压入memcpy的第二个参数，源地址
  push dword [ebx + 8]      ;呀如函数memcpy的第一个参数，目的地址，p_vaddr
  call mem_cpy
  add esp, 12               ;清理栈中压入的三个参数
.PTNULL:
  add ebx, edx              ;edx为program header的尺寸，这里就是跳入下一个描述符
  loop .each_segment
  ret

;----------- 逐字节拷贝 mem_cpy(dst, src, size)-------------
;输入：栈中三个参数
;输出：无
;-----------------------------------------------------------
mem_cpy:
  cld                       ;控制eflags寄存器中的方向标志位，将其置0
  push ebp
  mov ebp, esp  ;构造栈帧
  push ecx      ;rep指令用到了ecx，但ecx对于外层段的循环还有用，所以入栈备份
  mov edi, [ebp + 8]        ;dst
  mov esi, [ebp + 12]       ;src
  mov ecx, [ebp + 16]       ;size
  rep movsb                 ;逐字节拷贝,其中movs代表move string，其中源地址保存在esi，目的地址保存在edi中，其中edi和esi肯定会一直增加，而这个增加的功能由cld指令实现
  ;这里的rep指令是repeat的意思，就是重复执行movsb，循环次数保存在ecx中

  ;恢复环境
  pop ecx                   ;因为外层ecx保存的是程序段数量，这里又要用作size，所以进行恢复
  pop ebp
  ret

```
上面代码也就是逐字拷贝，逻辑比较简单，这里有意思的一点是咱们自己实现了函数调用哈哈哈，还是挺有趣的，只不过上面是一个kernel初始化代码。
所以我们此时再到loader主体里面进行调用，代码如下，注意这段代码是在开启页表后进行的，
```
;;;;;;;;;;;;;;;;;;;;;;;;; 此时可不用刷新流水线;;;;;;;;;;;;;;;;;;;;;;;;;
;这里是因为一直处于32位之下，但是为了以防万一所以还是加上一个流水线刷新
  jmp SELECTOR_CODE:enter_kernel        ;强制刷新流水线，更新gdt
enter_kernel:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  call kernel_init
  mov esp, 0xc009f000           ;这里选用0xc009f000对应物理地址为0x9f000是一个尽量靠近可用区域边界且为整的地址，并不是必须得是这个，但这个地址确实不错
  jmp KERNEL_ENTRY_POINT        ;用地址0x1500访问测试，这里相当与jmp $了

```
最后这里注意一点那就是内核函数main.c的编译，首先由于目前大多都是64位，所以gcc默认编译为64,此时我们需要指定编译版本
```
gcc -m32 -c main.c -o main.o
```
这里坑就来了，如果我们按照之前的ld方式进行链接会发现他自动生成了这样一个节
![](http://imgsrc.baidu.com/super/pic/item/203fb80e7bec54e7145c36dbfc389b504ec26a46.jpg)
这里如果我们不管的话，在kernel init部分的mem_cp会报错，所以这里我的解决方案如下：
1. 一个简单的链接脚本保存为link.script，如下：
```
ENTRY(main)

SECTIONS
{
  /DISCARD/ : {*(.note.gnu.propert)}
}
```
2. 然后进行链接
```
ld -m elf_i386 main.o -T link.script -Ttext 0xc0001500 -e main -o ./kernel.bin
```
3. 然后再次进行去节处理（虽然有链接脚本，但还不够）
```
strip --remove-section=.note.gnu.property kernel.bin
```
然后我们再用readelf就发现段成功去掉了
![](http://imgsrc.baidu.com/super/pic/item/c8177f3e6709c93d10512232da3df8dcd0005457.jpg)

之后我们直接打入9号扇区就行啦
```
dd if=./kernel.bin of=../bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc
```
下面就是咱们目前的内存示意图了：
![](http://imgsrc.baidu.com/super/pic/item/9f2f070828381f305215a26cec014c086f06f0e3.jpg)

## 0x02 特权级
计算机里面的一系列指令执行等操作都可以被认为是某个访问者来访问受访者。这里访问者和受访者都有着属于他们自己的特权级，举个简单例子就是假设我们需要开车，此时CPU相当于汽车，我们人就相当与访问者，我们现在想要进入警局获取资料，所以警局就相当与受访者，此时我们人的特权级就可以用警察和群众来表示，如果我们是警察，就刚好可以进警局拿资料，但如果我们是群众，就相当与特权级低于警察，那么我们就无法进警局拿资料了。相信这个例子能帮助你更好的理解特权级的概念。

而我们目前特权级一般有4中情况，分别是0,1,2,3,我们从计算机启动到mbr再到loader以及内核都是处于0级特权级，下面就是各特权级的使命：
![](http://imgsrc.baidu.com/super/pic/item/38dbb6fd5266d01675e6b7edd22bd40734fa3573.jpg)

对于特权级的基本概念讲解完毕，下面开始介绍一些关于他的基础知识
### 1.TSS
TSS.也就是Task State Segment,任务状态段，她是每个任务都有的结构（任务也就是进程），这里面保存了一些特权级的栈地址等，总共占104字节，下面给出具体结构：
![](http://imgsrc.baidu.com/super/pic/item/6a63f6246b600c336f97f70d5f4c510fd8f9a10d.jpg)
而在这里我们可以看到有esp0,esp1,esp2,从这个名字我们就鞥够看出这是表示的三个栈顶地址，这里为什么有这三个地址呢，是因为在我们切换特权级的时候，我们的栈那是肯定要切换到对应的特权栈的，因为若是栈不切换的话，不同特权级的一些资源都一股脑放一起了，这样不仅十分杂乱而且也需要足够大的栈。还有个问题就是为什么只有三个特权栈呢，这是因为咱们最差就是3号特权，若是咱们切换特权级那就只能切0.1.2了涩，而3号特权栈实际上也就是咱们的用户栈，他的切换是通过保存上下文来进行的。
所以TSS就是在处理器进入不同特权级的过程中，由硬件到TSS中寻找同特权级的栈，而这个寻找过程不需要咱们知道，因为这是系统级的，他就是知道。

特权级的转移分为两类，一类是由中断门、调用门等手段实现低特权级转向高特权级，另一类则是通过调用返回指令从高特权级返回低特权级，且这个方法是唯一的。
这里我还得提一嘴就是不是说每个任务都有4个栈，一个任务栈的数量取决于自身特权级，就比如说我们用户级任务，特权级为3,所以我们有4个栈，分别是用户栈，特权2,1,0栈，可是对于特权级为2的任务，他就只有3个栈，也就是特权2,1,0栈。

TSS就如同GDT一样也是个数据结构，所以为了知道怎么找到他，我们需要类似GDTR一样的东西来保存TSS的地址，这个就是TR寄存器。到此为止我们目前所需要了解的TSS知识就结束了。

### 2.CPL和DPL
这里我直接简单的叙述这两者的意义以及关系。首先PL就是Privilege Level的意思，也就是CPU若想知道谁的特权高谁的特权低，就得需要一种标识类的东西来记录那个人的特权级，不然在CPU眼里万物都是一样的。
首先我们回忆一下选择子，这里我再拿出图片让大家想起来
![](http://imgsrc.baidu.com/super/pic/item/b219ebc4b74543a96c5304085b178a82b80114ae.jpg)
这里的RPL记录的是请求特权级，也就是访问者的特权级。
但是话说回来谁是访问者呢，实际上访问者也就是咱们执行的指令，只有指令才有能力访问其他资源，所以只有他才是访问者。所以CS.RPL记录的就是当前执行指令的处理器的特权级。
然后就是CPL，Current Privilege Level,也就是当前特权级。在CPU运行的是指令，而运行的指令肯定会属于某个段，该代码段的特权级也就是代码段描述符中的DPL也就是当前CPU所处的特权级，这个特权级称为当前特权级也就是CPL，他表示处理器正在执行的代码的特权级别。
这里统一解释一下，免得大伙弄迷糊了，首先段描述符里面有个DPL，这是代表当前段的特权级，而选择子中的RPL是指当前我们访问者的特权级，而CPL也是指带的当前我们访问者的特权级，其中RPL和CPL不同的地方在于RPL是为了表示访问者请求的特权而存在的，而CPL则是一个动态的概念，他作为一种标识我们当前指令特权的存在，即使我们不访问资源他也是存在的。

说完这几者的基本概念，我们再来熟悉对于访问资源的情况，这里分为一下两种情况
1. 受访者为数据段，此时我们只能使用高特权级或平级访问
2. 受访者为代码段，此时只能平级访问

对于数据段的访问想必大家都心知肚明，我想访问一个数据那肯定要高特权级啊，不然你凭啥一个低特权访问我高特权，但是读与代码段大家可能就比较奇怪了，这里作出解释。
首先若代码段处于高特权级，我想要访问低特权级的代码，因为低特权级的代码能访问的我高特权级代码肯定也能访问，所以我这里不需要专门降级。而若代码段处于低特权级若我们想访问高特权级代码，此时又会存在一系列风险，因为我们代码处于高特权级的时候程序想干啥就干啥。所以我们这里也避免了低特权代码访问高特权代码，因此这里受访者为代码段的时候，只能平级访问。

但是这里就存在一个问题，那我们低特权级下的指令是真的需要使用高特权级指令怎么办呢，不慌，此时人家给我们也提出了一个解决方法，这个方法既保证了我们可以执行高特权级代码段上的指令，又不会提升我们的特权级
这就是一致性代码，这个概念在之前讲段描述符的时候提过一嘴。
而一致性代码还有个名字叫做依从代码段，他是指如果自己是转以后的目标段，自己的特权级一定要大于等于转以前的特权级，且在转以后的当前特权级（CPL）并不会改变，还是之前那个低特权级，这样我们就实现了在低特权下运行高特权的代码。
但是我们总不可能一直这样运行，因为有的代码他不会标识为一致性代码，所以我们就需要某种机制使得我们向高特权级转化，接下来我们就来讲述此法。

### 3.门，调用门与RPL序
门结构是使得处理器从低特权及转移到高特权级的唯一途径，那么门结构又是什么呢，他就是记录一段程序起始地址的描述符，他用来描述一段程序。只要进入这扇神奇的门，处理器就能够转移到更高的特权级上。
门描述符和段描述符类似，都是八字节大小的数据结构，下面给出几种不同的门描述符结构：
![](http://imgsrc.baidu.com/super/pic/item/c75c10385343fbf2b2148de6f57eca8065388f3e.jpg)
![](http://imgsrc.baidu.com/super/pic/item/32fa828ba61ea8d3b2195fc1d20a304e251f583f.jpg)
![](http://imgsrc.baidu.com/super/pic/item/adaf2edda3cc7cd99b181f1c7c01213fb80e9139.jpg)
![](http://imgsrc.baidu.com/super/pic/item/35a85edf8db1cb13755e17d69854564e92584b3b.jpg)

这里可以注意到任务门同其他的门有些许差别，其他三门是对应有一段函数，所以这三门函数中需要有选择子和偏移，这样才能找到对应段的某段函数了。
而任务门描述符可以直接存放在GDT、LDT、IDT（中断描述表，以后的内容）中，调用门可以位于GDT、LDT中，中断门和陷阱门仅位于IDT中
其中任务门、调用门都可以用call，jmp指令直接诶使用，原因在于这俩都直接位于描述符表中，而陷阱门和中断门之存在与IDT中，只能由中断信号触发。
任务门比较特殊，它使用TSS的描述符选择子来描述一个任务，除他之外，其他门都通过选择子和偏移来指定一段程序，虽然说他们的作用都是实现从低特权级向高特权级转移，但是他们的适用范围是不同的，下面分别来解释：
1. 调用门
call和jmp指令后接调用门选择子为参数实现系统调用，call指令使用调用门可以实现向高特权级代码转移，jmp使用调用门只能实现平级代码转移
2. 中断门
以int指令主动发中断的形式实现低到高，linux系统调用就是用其实现的
3. 陷阱门
以int3主动发中断的形式实现低到高，一般是编译器调试时使用
4. 任务门
以TSS为单位用来实现任务切换，可以借助中断或指令发起，当中断发生时若对应的中断向量号是任务门，则会发生任务切换，当然也可以像调用门那样通过call和jmp发起
![](http://imgsrc.baidu.com/super/pic/item/6a63f6246b600c3361c3f50d5f4c510fd8f9a1b9.jpg)
这个图是真的生动形象，完美解释了我们为什么能通过调用门来进入高特权级，门的特权级是一定要低于我们访问者的特权级的，这样才能保证我们能过调用门，而受访者的特权级一定得高于访问者，不然访问者何必要使用门呢。
当我们进门之后，处理器将以目标代码段DPL为当前特权级CPL，因此进门之后我们就顺利提高了特权级了。 
这里我们来介绍一下调用门的内部执行流程，先上个图：
![](http://imgsrc.baidu.com/super/pic/item/fcfaaf51f3deb48fa2728b1bb51f3a292cf578df.jpg)
结合图片来讲解，首先我们通过call 调用门选择子，这个选择子是指向GDT或者说是LDT中的某个门描述符，我们这里假设其是GDT。当我们找到了门描述符的时候，我们再次通过该门描述符里面的选择子对于GDT再次进行寻找，这里肯定会找到一个段描述符，然后我们再通过门描述符中的偏移来找到对应内核例成的地址。这里相当于是我们去表里找个地址，然后再通过这个地址找到表内另一个地址，有点类似与间接寻址了，大家对应图片仔细理解。

### 4.调用门的过程保护
我们直接来了解用户进程中通过call指令调用“调用门”的完整过程。
1. 首先假设我们要调用某个调用门需要两个参数，也就是说该门描述符中的参数值为2,（格式可以看上面发的图片），此时我们处于特权级3栈，我们想要到特权级0去，所以咱们的栈也会替换到特权级0栈，但在我们调用门前还需要传递两个参数，我们现在将这两个参数压入特权级3栈中，如下图：
![](http://imgsrc.baidu.com/super/pic/item/ae51f3deb48f8c54ba20d0ea7f292df5e1fe7f07.jpg)

2. 然后我们就要确定新栈了，这一步我们会根据门描述符中所寻找到的选择子来确定目的代码段的DPL值，这将作为我们日后的CPL值存在，同时我们会通过TSS来确定想对应DPL的栈地址，也就是栈段选择子SS和栈指针ESP，这里记做SS_NEW和ESP_NEW

3. 如果转移后代码段特权级提升，我们就需要换到新栈，此时旧段选择子我们记为SS_OLD 和 ESP_OLD，由于我们这俩值需要保存到新栈中，这是为了方便日后使用retf等指令进行返回恢复旧栈，所以此时我们需要将SS_OLD和ESP_OLD放到某个地方进行保存，例如其他的一些寄存器，然后当我们将SS_NEW 和 ESP_NEW载入到SS和ESP寄存器后，咱们再将他俩压入新栈就行了,如下图：
![](http://imgsrc.baidu.com/super/pic/item/342ac65c10385343061e92e4d613b07ecb808826.jpg)

4. 然后我们再将用户栈中保存的参数压入新栈，如图：
![](http://imgsrc.baidu.com/super/pic/item/4034970a304e251fc73ef13de286c9177e3e532d.jpg)

5. 由于调用门描述符中记录的是某个段选择子和偏移，所以此时我们的CS寄存器需要用这个选择子重新加载，所以我们需要像上次一样先将旧的CS和EIP保存到栈上，然后重新加载两个寄存器,如下:
![](http://imgsrc.baidu.com/super/pic/item/b90e7bec54e736d1341d98cdde504fc2d46269ec.jpg)

6. 之后就是按照CS:EIP指示来运行内核例程从而实现特权级从3到0啦

当我们在高特权级游玩一段时间后，我们总归是要回到我们那一亩三分地的，这里就涉及到高特权级到低特权级，这里有且仅有一种方法，那就是retf指令，下面是执行过程
1. 首先进行检查，检查之前栈中保存的旧CS选择子，判断其中的RPL，来决定是否需要进行权限变换
2. 然后弹出CS_OLD 和EIP_OLD,目前为止ESP就会指向最后压的那个参数
3. 此时我们需要跳过参数，所以得将ESP_NEW的值加上一定偏移，使得他刚好指向ESP_OLD
4. 若第一步中确定需要进行权限变换，此时再次pop两次，这样就恢复了之前的SS和ESP了

这里注意若我们在返回时需要进行权限变换，我们会检查数据段寄存器 DS ES FS GS 的内容，如果在它们之中，某个寄存器中选择子所指向的数据段描述符的 DPL 权限比返回后的 CPL (CS.RPL ）高，即数值
上返回后的 CPL＞数据段描述符的 DPL ，处理器将把数值0填充到相应的段寄存器。
由于我们进入内核态的时候肯定也要访问内核态的数据，所以这些段寄存器的选择子也会修改为对应内核态的特权级，但是有个问题就是在retf指令并没有管这些寄存器，也就是说我只管了升级没管降级。
这样的话会带来一个严重的问题，就是我们虽然返回了，处理器特权级也降下来了，但是段寄存器我们只返回了SS和CS，其他在内核中的段寄存器并没有作出改变，这就导致我们在用户态依然可以访问内核态的数据，这样是十分危险的。
这里也有可行的方法，就是将这些段寄存器都一股脑像之前CS，SS一样都保存在栈上，等retf时候再返回，或者说类似于linux一样不使用调用门，而使用中断门来进行系统调用。
而上面填充0也是一种处理器自己提供的办法，我们之前写过GDT，我们在第0个段描述符上填的是全0,若我们将段寄存器里的选择子清0会发生什么，对就是报错，从而引出处理器异常再来初始化这些段寄存器。

### 5.RPL
RPL 是谁？ RPL, Request Privilege Level ，请求特权级，这么说有点歧义，其实它代表真正请求者的特权级，我的言外之意是说 RPL 其实是代表真正资源需求者的 CPL ，大伙儿继续昕我说。以后在请求某特权级为 DPL 级别的资源时，参与特权检查的不只是 CPL ，还要加上 RPL. CPL RPL的特权必须同时大于等于受访者的特权 DPL ，即数值上：DPL>=RPL,DPL>=CPL
这里RPL是为了防止类似用户想要通过调用门来获得内核数据等危险操作，因为如果没有RPL的话，将会只检查CPL和DPL，此时我们若是通过门进入了0特权级，此时我们能干一切事，其中就包含了将内核数据写入我们用户区的任何地点（因为检查的时候没检查RPL，我们还以为是操作系统想要这样做，但其实是用户想这样，然后通过系统调用让操作系统来安排），这样当然不行。
所以就如同上面说的一样，检查的时候这三个特权级指标都得检查
特权级检查发生在什么时候呢？如何被触发？
32 位保护模式下对内存的访问要通过段描述符，段描述符中有 DPL ，这是内存的关卡，咱们现实生活中的检查也是在关卡处执行的，所以处理器的特权检查，都是只发生在往段寄存器中加载选择子访问描述符的那一瞬间，所以， RPL 放在选择子中是多么的合理。这里所说的加载选择子，是指任何访问，无论是代码，还是数据。处理器的特权检查只发生在访问前的一瞬间，这和现实生活中是一样的，通过检查之后再也不管了，直到遇到新的关卡，否则执行一步指令就要检查一次特权级，处理器啥活都甭干了。

---
大家不要把 CPL RPL 搞混了，不要误以为都是对同一个程序而言的，它们也许不都属于同 个程序RPL 位于选择子中的，所以，要看当前运行的程序在访问数据或代码时用的是谁提供的选择子，如果用的是自己提供的选择子，那肯定 CPL RPL 都出自同 个程序，如果选择子是别人提供的，那就有可能 RPL和CPL 出自两段程序。 CPL 是对当前正在运行的程序而言的，而 RPL 有可能是正在运行的程序，也可能不是在一般情况下，如果低特权级不向高特权级程序提供自己特权级下的选择子，也就是不涉及向高特权级程序“委托、代理”办事的话， CPL RPL 都来自同一程序。但凡涉及“委托、代理”，进入0特权级后， CPL是指代理人，即内核， RPL 则有可能是委托者，即用户程序，也有可能是内核自己。还是拿之前说过的调用门A举例，某用户进程运行在3特权级，它想通过调用门读取硬盘上某个文件到它自己的数据缓冲区中。它需要向该调用门提供 3个参数：文件所在的硬盘扇区号、用于存储文件的缓冲区所在的数据段选择子以及缓冲区的偏移地址。用户进程只能把与自己同一特权的数据段作为缓冲区，所以该缓冲区所在段的 DPL ，其选择子的RPL  必然为3 。进入调用门后，处理器的 CPL 由运行用户进程时的3 级变成内核态的 0级，当内核从硬盘上读取完数据后，需要将其写入用户的缓冲区中。缓冲区的选择子是由用户提供的，其 RPL 如上所述为 3,缓冲区所在段的 DPL为3 ，此时 CPL为3 ，即数值上（ CPL<=DPL&&RPL<= DPL) 成立，于是写入成功。大伙儿看到了， RPL 是用户进程提供的，而往缓冲区写数据时 CPL 指的是内核，不是同一个程序。

到这里实际上特权级的几个重要点已经结束了，为了加深理解，书的作者也提了一个比较有趣的例子，大家也可以看看：
> 不知道大伙儿学车了没有，报考驾校也要有个年龄限制，即使考 C本B本也要分年龄的。假如某个
小学生A（用户进程）特别喜欢开车，他就是想考个驾照，可驾校的门卫（调用门〉一看他年龄太小都不
让他进门，连填写报名登记表的机会都没有，怎么办？于是他就求他的长辈B（内核〉帮他去报名，长辈
的年龄肯定够了，门卫对他放行，他来到驾校招生办公室后，对招生人员说要帮别人报名。人家招生人员
对B说，好吧，帮别人代报名需要出示对方的身份证（ RPL) ，于是长辈B就把小学生A的身份证（现在
小孩子就可以申请身份证，只是年龄越小有效期越短，因为小孩子长得快嘛）拿出来了，招生人员一看，
年纪这么小啊，不到法制学车年纪呢，拒绝接收。这时候驾校招生人员的安全意识开始泛滥了，以纵容小
孩子危险驾驶为名把长辈B批评了一顿（引发异常）。

看着确实挺形象的哈哈。
### 6.IO特权级
在保护模式中，“阶级”不仅体现在数据和代码的访问之间，也体现在指令之间
一方面将指令分级是因为部分指令会对计算机产生巨大的影响所以得小心使用，其中就比如lgdt等
另一方面体现在IO读写控制上，IO读写特权是由标志寄存器eflags中的IOPL位和TSS中的IO位图决定的，他们用来指定执行IO操作的最小特权级。这里我们来看看eflags寄存器的结构，从中我们可以看到IOPL位：
![](http://imgsrc.baidu.com/super/pic/item/42166d224f4a20a495592b02d5529822730ed0b4.jpg)
IOPL,I/O Privilege Level,即IO特权级，除了限制当前任务进行IO敏感指令的最低特权级外，还用来决定任务是否允许操作所有的IO端口（也就是说如果该位打开，，便可以访问全部65536个端口）。每个任务都有自己的eflags寄存器，所以每个任务都有自己的IOPL，他表示当前任务要想执行全部IO指令的最低特权级。

而IOPL如何设置呢？
这里只有通过pushf指令将eflags整体入栈然后修改栈中的数据再弹出。另一个可利用栈的指令是iretd，用iretd从中断返回时，会将栈中相应位置的数据当作eflags的内容弹到eflags寄存器中，这就有点类似与PWN的技巧了。所以可以设置IOPL的指令有popf 和 iretd。

上面说了IOPL打开就能访问所有端口，但如果其关上的话，也就是说CPL的特权级是低于IOPL特权级的，那么我们可以通过IO位图来设置部分端口的访问权限。而这样设计的目的是使得我们在低特权级时依然能访问我们所设计需要的硬件资源，从而免去了我们进行系统调用提升权限保护上下文的消耗，说白了也就是提速而已。
上面巴巴一大堆我们可以知道其中有个东西一直出现在我们眼前，那就是位图，所以接下来我们来介绍位图的概念
## 1.IO位图
即bit map,他建立的是某种关系，这里感觉就类似表示磁盘空间的位图一样，也就是1个bit代表着一个端口，总共有65536个端口，所以我们共需要65536/8=8192个字节来表示IO位图。若某位bit为0则表示可以访问，若为1则表示禁止访问，这里相信学习过操作系统原理这门课程的同学都不需要多说了。
IO位图位于TSS中，这里TSS不包括位图的时候就只有104字节大小。至于IO位图的一些其他设置在这里我们并不需要，所以就不过多详述，这里最后给出一张TSS+位图方位的图片作为结束。
![](http://imgsrc.baidu.com/super/pic/item/a50f4bfbfbedab648ea45a05b236afc378311e24.jpg)

## 0x03 总结
这章实操不多，但是坑却很多，具体坑我上面也解释了。这里之后的特权级是重点，理解之后感觉对内核特权更加透彻。虽然十分长，但还是值得钻研的。
