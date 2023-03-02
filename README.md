## 0x00 基础知识
今天本来是准备进入内核知识，但在这之前我们先简单了解下如何获取物理内存容量，这是因为考虑到把这个讲了再加入内核分页的只是会显得很长，所以获取内存容量单独写一篇。为了在后期做好内存管理的工作，我们首先得知道自己有多少物理内存才行。
### 0.Linux获取内存基本嫩方法
在linux 2,6内核中，使用detect_memory 函数来获取内存容量的，其函数本质上是通过调用BIOS中断0x15实现的，分别是BIOS中断0x15的3个子功能，子功能号需要存放到寄存器EAX或AX中，以下是这三种模式的介绍：
+ EAX=0xE820：遍历主机上全部内存
+ AX=0xE801：分别检测低15MB和16MB～4GB内存，最大支持4GB
+ AH=0x88：最多检测64MB内存，实际内存超过此容量也按照64MB返回

这里大伙可能会奇怪，咱们不是进入保护模式了吗，我还怎么用BIOS中断呢，不是你最开始说BIOS中断只能在实模式下才能用吗，你是不是不懂装懂，你是不是根本不会啊，这个博主就是个××
![](https://cn.bing.com/images/search?q=%E6%88%91%E7%9F%A5%E9%81%93%E4%BD%A0%E5%BE%88%E6%80%A5%20%E4%BD%86%E6%98%AF%E5%85%88%E5%88%AB%E6%80%A5%E8%A1%A8%E6%83%85%E5%8C%85&FORM=IQFRBA&id=E8AB1E51005818B9451F07576DD9245205EF8A81)
这里我们并不是说要在保护模式之下进行BIOS中断操作，今天我们所讲的只是一个小demo而已，所以我们先放下上一篇的知识，咱们先在实模式下检测了物理内存再进入保护模式。这里我们将这三个方法依次介绍（如果其中一个用不了就用别的）
### 1.利用BIOS中断0x15子功能0xe820获取内存
据说这是最灵活的内存获取方式,说他比较灵活是因为他返回的信息比较丰富，而返回丰富的信息就表示我们需要用一种格式结构来组织这些数据。而内存信息的内容是用地址范围描述符来描述的，用于存储这种描述符的结构称之为地址范围描述符(Address Range Descriptor Structure,ARDS),格式如下：
![](http://imgsrc.baidu.com/forum/pic/item/9d82d158ccbf6c81fb15f32cf93eb13532fa40d0.jpg)
上述的字段从偏移也可以看出每个占4字节，其中含义大家可以有表得知，这里详细介绍其中的TYPE字段，具体意义如下：
![](http://imgsrc.baidu.com/forum/pic/item/3c6d55fbb2fb43166516a5be65a4462308f7d3ed.jpg)
而BIOS按照上述类型来返回内存信息是因为这段内存可能为一下几种情况：
+ 系统的ROM
+ ROM用到了这部分内存
+ 设备内存映射到了这部分内存
+ 由于某种原因，这段内存不适合标准设备使用
而由于我们是在32位环境下，所以我么只需要用到低32位属性属性，也就是BaseAddrLow 和 LengthLow就可以了，当然我们在调用BIOS中断不仅仅使得EAX或AX里面有相应的功能号，我们还需要通过其他寄存器传入一系列参数，下面给出具体示例：
![](http://imgsrc.baidu.com/forum/pic/item/023b5bb5c9ea15ce1eea3ecdf3003af33b87b2ea.jpg)
![](http://imgsrc.baidu.com/forum/pic/item/b64543a98226cffcce2b6176fc014a90f703eaf6.jpg)
这里值得注意的参数寄存器有ECX和ES:DI,其中ECX是指缓冲区大小，ES:DI是指缓冲区指针，被调用函数将所写内容写入该缓冲区，然后记录写入内容大小然后记录在缓冲区大小寄存器中。注意这里调用者是传入的期待BIOS写入大小，而被调用者则是往ECX写入实际大小。此中断的调用步骤如下：
1. 填写好调用前函数寄存器
2. 执行中断调用int 0x15
3. 在CF为0的情况下，“返回后输出”的寄存器便会有相应的结果

### 2.利用BIOS中断0x15子功能0xe801获取内存
这个子功能整体来说不算强，但也值得我们来学习，他虽然说最大只能识别4GB内存，但是对咱们32位地址总线足够了，但是有点特殊的就是这种方法检测到的内存是分别放到两组寄存器中的。低于15MB的内存以1KB为单位来记录，单位数量在AX和CX中记录，其中AX和CX的值是一样的，所以在15MB空间以下的实际内存容量=AX×1024.AX，CX最大值为0x3c00,即0x3c00**** 1024 =15MB.而16MB～4GB是以64KB为单位大小来记录的，单位数量在BX、DX中存储，其中这俩内容一样，跟上述AX、CX类似。下面给出输入时寄存器以及输出时寄存器的功能和作用：
![](http://imgsrc.baidu.com/forum/pic/item/8326cffc1e178a826b9ba164b303738da877e854.jpg)
这里我们大多数人都会意识到这两个问题：
1. 为什么要分“前15MB”和“16MB～4GB”。
2. 为什么要设两个内容相同的单位量寄存器，就是说AX=CX,BX=DX.

为了解释地一个问题，我们在这里给出经过测试后的结果，这里如何测试不重要，我们会在最后实战，所以这里我们只关注结果即可
![](http://imgsrc.baidu.com/forum/pic/item/c8177f3e6709c93dfe7ed033da3df8dcd0005462.jpg)
这里我们观看表头，发现实际物理内存和检测到的内存大小总是相差1MB，这是为什么呢
这里实际上是万恶的历史遗留问题，这是由于在80286版本由于有24位地址线，即可表示16MB的内存空间，其中低15MB用来正常作为内存使用，而高1MB是留给一些ISA设备作为缓冲区使用，到了现在由于为了向前兼容，所以这1MB还是被空了出来，造成一种内存空洞的现象。
所以说但我们检查内存大小大于等于16MB时，其中AX×1024必然小于等于15MB，而BX×64K必然大于0,所以我们在这种情况下是可以检查出这个历史遗留的1MB的内存空洞，但若是我们检查内存小于16MB的时候，我们所检查的内容范围就会小于实际内存1MB。
至于第二个问题，为什么要用两个内容相同的问题，我们在上面的输入寄存器的图片中可以看到，AX与CX，BX与DX这两组寄存器中，都是一个充当Extended和Configured.但这里我们暂时不去区别他们之间的不同，我怎么感觉这第二个问题我说了个寂寞。大伙别见怪，书上就这么说的。
这里再给出第二个方法的调用步骤：
1. 将AX寄存器写入0xE801
2. 执行中断调用
3. 在CF为0的情况下，“返回后输出”的寄存器便会有相应的结果

### 3. 利用BIOS中断Ox15子功能Ox88获取内存
这里就是最后一个子功能了，他使用简单，获取的东西也简单，他只能识别到最大64MB的内存，即使内存容量大于64MB，也只会显示63MB，这里为啥又少了1MB呢，这是因为此中断只能显示1MB之上的内存，所以我们在检测之后需要加上1MB，现在懂了为啥说第一种灵活了吧，这第二种第三种都有点毛病，这里像之前一样给出传递参数
![](http://imgsrc.baidu.com/forum/pic/item/dbb44aed2e738bd4532a6bf5e48b87d6267ff9e8.jpg)
调用步骤如下：
1. 将AH寄存器写入0x88
2. 执行中断调用 int 0x15
3. 在CF为0的情况下，“返回后输出”的寄存器便会有相应的结果

## 0x01 实战内存容量检测
按照我们上面介绍的基础知识，这里我们直接上代码，大伙也可以自己先尝试一下：
```
;人工对齐：total_mem_bytes4+gdt_ptr6+ards_buf244+ards_nr2,共256字节,0x100 
ards_buf times 244 db 0
ards_nr dw 0        ;用于记录ARDS结构体的数量

loader_start:
;------ int 15H eax = 0000E820,edx = 534D4150('SMAP') 获取内存布局-------
  xor ebx, ebx      ;第一次调用时，ebx置0
  mov edx, 0x534d4150 ;edx只赋值一次，循环体中不会改变
  mov di, ards_buf ;ards结构缓冲区,这里由于es我们在mbr.S中已经初始化，为0,所以这里我们不需要修改es，只需要对di赋值即可
.e820_mem_get_loop:
  mov eax, 0x0000e820   ;每次执行int 0x15之后，eax会变成0x534d4150,所以每次执行int之前都要更新为子功能号
  mov ecx, 20
  int 0x15
  jc .e820_failed_so_try_e801   ;若cf位为1则说明有错误发生，尝试下一个0xe801方法
  add di, cx            ;使di增加20字节指向缓冲区中新的ARDS结构位置
  inc word[ards_nr]     ;记录ARDS数量
  cmp ebx,0             ;若ebx为0且cf不为1,这说明ards全部返回
  jnz .e820_mem_get_loop
;在所有ards结构体中找出(base_add_low + length_low的最大值，即为内存容量
  mov cx, [ards_nr]     ;遍历每一个ards结构提，循环次数cx就是ards的数量
  mov ebx, ards_buf     ;将ebx中放入我们构造的缓冲区地址
  xor edx, edx          ;edx为最大的内存容量，在此先清0
.find_max_mem_area:     ;这里不需要判断type是否为1,最大的内存块一定是可被使用的
  mov eax, [ebx]
  add eax, [ebx+8]      ;这里ebx和ebx+8代表了BaseAddrLow 和 LengthLow
  add ebx, 20           ;ebx指向下一个ards结构体
  cmp edx, eax          ;冒泡排序，找出最大，edx寄存器始终是最大的内存容量
  jge .next_ards        ;大于或等于
  mov edx, eax          ;edx为总内存大小
.next_ards:
  loop .find_max_mem_area ;循环，以cx为循环次数
  jmp .mem_get_ok


;------ int 15H ax=E801h , 获取内存大小，最大支持4G------
;返回后，ax cx值一样，以KB为单位， bx dx 一样，以64KB为单位
;在ax和cx寄存器中为低16MB，在bx与dx寄存器中为16MB到4GB
.e820_failed_so_try_e801:
  mov ax, 0xe801
  int 0x15
  jc .e801_failed_so_try88  ;若cf位为1则说明有错误发生，尝试下一个88方法
;1 先算出低15MB的内存
; ax和cx中是以KB为单位的内存数量，因此我们将其转换为以byte为单位
  mov cx,0x400  ;这里由于cx和ax一样，所以我们将cx用作乘数，0x400即为1024
  mul cx        ;由于处于实模式，所以我们mul指令的含义是ax × cx，注意mul指令是16位乘法，生成乘数应该是32位，高16位在dx中，低16位存于ax中
  shl edx, 16   ;左移16位,这里也就是将dx保存的高16位转移到edx的高16位上
  and eax, 0x0000FFFF   ;将eax高16位清0
  or edx, eax   ;或后得出乘积，保存至edx中
  add edx, 0x100000     ;最后将差的那1MB加上
  mov esi, edx      ;这里保存一下edx的值，因为在之后的计算过程中他会被破坏

;2 再将16MB以上的内存转换为byte为单位
  xor eax, eax
  mov ax, bx
  mov ecx, 0x10000  ;0x10000为16进制的64K
  mul ecx           ;32位乘法，其高32位和低32位存放在edx和eax中
  add esi, eax      ;由于这里只能最大测出4GB，edx的值肯定为0，所以咱们只需要eax就可以了
  mov edx, esi      ;其中edx为总内存大小
  jmp .mem_get_ok

;----- int 15h ah=0x88 获取内存大小，只能获取64MB之内 -------
.e801_failed_so_try88:
  ;int 15h后，ax存入的是以KB为单位的内存容量
  mov ah, 0x88
  int 0x15
  jc .error_hlt
  and eax, 0x0000FFFF

  ;16位乘法
  mov cx, 0x400
  mul cx
  shl edx, 16
  or edx, eax
  add edx,0x100000  ;0x88子功能只会返回1MB以上的内存，所以最终我们还需要加上1MB

.error_hlt:
  jmp $

.mem_get_ok:
  mov [total_mem_bytes], edx            ;将内存换为bytes为单位然后存入total_mem_bytes中

```

可以看到我们上面有个代码是人工对齐，其实这个人工对其不是必要的，但是方便咱们调试以及查看，所以我们将其变为0x100的整数倍，我将上面代码简单介绍下，因为我们loader是存放在内存0x900的地址，然而我们在这个地址上又加上了四个段描述符和60个段描述符预留空位，此时已经用了0x200，然后我们还需要划出一点来作为ards的存放缓冲，存放最大内存，还有gdt指针，这些统统加起来为了满足0x100的整数倍，我们在此选择缓冲区大小申请244字节，这里大家认真查看代码然后计算即可，
注意还有个需要修改的地方就是mbr.S,因为我们在loader.S上去掉了jmp loader_start(占3字节)，而loader_start在loader中的偏移为我们精心准备好的0x300,所以我们在mbr跳转到loader_start时就要加上0x300,修改部分如下：
```
 jmp LOADER_BASE_ADDR + 0x300         ;代码运行至此说明Loader已经加载完毕

```
然后我们进行汇编，这里建议大家用个脚本一套完成算了，免得每次还的一行行打
```
#!/bin/sh
nasm -I include/ -o mbr.bin mbr.S
dd if=./mbr.bin of=../bochs/hd60M.img bs=512 count=1 conv=notrunc
nams -I include -o loader.bin loader.S
dd if=./loader.bin of=../bochs/hd60M.img bs=512 count=4 seek=2 conv=notrunc

```

这样在我们mbr运行完毕后就会直接跳转到loader_start开始内存检测了。这里我给出我们bochs的配置：
```
#第一步，首先设置 Bochs 在运行过程中能够使用的内存，本例为 32MB
#关键字为 me gs
megs :512
#第二步，设置对应真实机器的 BIOS VGA BIOS
#对应两个关键字为 romimage vgaromimage
romimage: file=/home/dawn/repos/OS_learning/bochs/share/bochs/BIOS-bochs-latest
vgaromimage: file=/home/dawn/repos/OS_learning/bochs/share/bochs/VGABIOS-lgpl-latest
#第三步，设置 Bochs 所使用的磁盘，软盘的关键字为 floppy
#若只有一个软盘，目IJ 使用 floppy 即可，若有多个，则为 floppya, floppyb… #floppya: 1_ 44=a.img, status=inserted
#第四步，选择启动盘符。
#boot: floppy ＃默认从软盘启动，将其注释
boot: disk #改为从硬盘启动。我们的任何代码都将直接写在硬盘上，所以不会再有读写软盘的操作。
#第五步，设置日志文件的输出。
log: bochs.out
#第六步，开启或关闭某些功能。
#下面是关闭鼠标，并打开键盘。
mouse: enabled=0
keyboard:keymap=/home/dawn/repos/OS_learning/bochs/share/bochs/keymaps/x11-pc-us.map
#硬盘设置
ata0: enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14
#下面的是增加的 bochs gdb 的支持，这样 gdb 可以远程连接到此机器的 234 口调试了
#gdbstub : enabled=l, port=l234, text_base=O, data_base=O, bss_base=O
################### 配置文件结束 #####################
ata0-master: type=disk, path="hd60M.img", mode=flat

```
可以看到内存设置为512MB，我们在开启bochs查看是否如此。
![](http://imgsrc.baidu.com/forum/pic/item/77094b36acaf2edd986c8b2dc81001e938019351.jpg)
发现果然如此，这个0x20000000大家用十进制表示会发现确实为512MB。

## 0x02 总结
本篇作为一个额外篇是为下一篇分担点篇幅，不然会显得十分冗长，今天的检测还是十分简单的，大家仔细看代码就会看懂。
