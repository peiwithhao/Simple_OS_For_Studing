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

