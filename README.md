## 0x00 基础知识们
今天我们来介绍关于中断的基础知识，理论上经常将中断分为两大类。
### 1.外部中断
一般指CPU外部硬件传来的中断，由于中断源必定为硬件，所以也被称为硬件中断，而我们CPU为了知道外面有人说哎呀我不行了我要中断辣，所以必须得有个传话筒，这里就存在着两个“传话筒”，也就是两根信号线，他们分别是INTR(INTeRrupt)和NMI(Non Maskable Interrupt)
![](http://imgsrc.baidu.com/forum/pic/item/2e2eb9389b504fc224d7cac0a0dde71191ef6d6f.jpg)
上图给出了两种信号线的区别，其中INTR收到的信号都是不影响系统运行的，可以随时处理，而NMI收到的信号是必须要立刻处理，NMI收到信号后，一切其他的工作都失去了意义，先把这个中断处理了才是重中之重。
首先我们介绍可屏蔽中断，可屏蔽中断是指的由INTR线传递的中断信号，外部设备如硬盘、网卡等发出的中断都是可屏蔽中断。可屏蔽的意思是此外部设备发出的中断， CPU 可以不理会，因为它不会让系统右机，所以可以通过 eflags寄存器的 IF 位将所有这些外部设备的中断屏蔽。另外，这些设备都是接在某个中断代理设备的，通过该中断代理也可以单独屏蔽某个设备的中断，这是后话，后面会有详细介绍。
然后我们介绍不可屏蔽中断，他是由NMI线传递的中断信号，只要这里传递了中断，计算机就说明遭到了严重的问题，必须立刻处理。此时上述eflags寄存器的IF位对他也毫无影响。

CPU 收到中断后，得知道发生了什么事情才能执行相应的处理办法。这是通过中断向量表或中断描述符表（中断向量表是实模式下的中断处理程序数组，在保护模式下已经被中断描述符表代替，在后面章节中会细说〉来实现的，首先为每一种中断分配一个中断向量号，中断向量号就是一个整数，它就是中断向量表或中断描述符表中的索引下标，用来索引中断项。中断发起时，相应的中断向量号通过 NMI INTR引脚被传入 CPU ，中断向量号是中断向量表或中断描述符表里中断项的下标， CPU 根据此中断向量号在中断向量表或中断描述符表中检索对应的中断处理程序井去执行。

### 2.内部中断
介绍玩外部中断，这里我们来介绍内部的，内部中断又可以继续划分，分为软中断和异常
软中断，就是由软件主动发起的中断，因为它来自于软件，所以称之为软中断。由于该中断是软件运行中主动发起的，所以它是主观上的，井不是客观上的某种内部错误。
下面就是咱们可以发起中断的指令：
+ int (8位数据)：8位可表示256种中断，这条指令在之后我们会经常使用
+ int3 ：调试断点指令，它所出发的中断向量号是3,当我们使用gdb或bochs进行调试时，实际上就是调试器fork了一个子进程。调试器中设置断点实际上就是父进程修改子进程，将其用int3指令替换，从而实现中断使得咱们可以停在断点，
+ into ：这是中断溢出指令，它所触发的中断向量号是4。不过，能否引发4号中断是要看 eflags志寄存器中的 OF 位是否为1，如果是1才会引发中断，否则该指令悄悄地什么都不做，低调得很
+ bound ：这是检查数组索引越界指令，它可以触发5号中断，用于检查数组的索引下标是否在上下边界之内。该指令格式是“bound 16/32 位寄存器， 16/32 位内存飞目的操作数是用寄存器来存储的，其内容是待检测的数组下标值。源操作数是内存，其内容是数组下标的下边界和上边界 当执行 bound 指令时，若下标处于数组索引的范围之外，则会触发5号中断。
+ ud2 ：触发6号中断，无实际用途

而异常是我们程序运行时出现的错误，他同样不受eflags寄存器中的标志位的影响，也就是说不可屏蔽，因为都出现错误了计算机说想看不见也不行，异常可以分为以下3种：
1. Fault，故障，可修复，例如缺页异常
2. Trap，陷阱，自己想陷入中断，所以中断返回后执行下一条指令。
3. Abort, 终止，无法修复，操作系统为求自保，只能把该程序从进程表移除

这里给出我们异常与中断的汇总表供大家参考：
![](http://imgsrc.baidu.com/forum/pic/item/a044ad345982b2b70acc5c0174adcbef77099ba0.jpg)

其中我们的中断向量号的作用类似于选择子，都是某个表的下标。

### 3.中断描述符表
得，又来一个表，中断描述符表（Interrupt Descriptor Table，IDT）是保护模式下存放中断处理程序入口的一个表，这里注意同实模式下的中断表区分开来。
而中断描述符表中不止有中断描述符，还有任务门描述符和陷阱门描述符。而由于所有的描述符都指向了一段程序，这里就体现出来他与GDT的不同，所以在这里的描述符有个另外的名字，那就是“门”。
所有的门都是8字节，这里我们来回忆一下描述符中的字段类型，type字段指明了该描述符的类型，其中的S位若为1则说明该段为数据段，为0则表示为系统段，咱们这里的门就属于系统段
这里给出之前门的描述符结构：
![](http://imgsrc.baidu.com/forum/pic/item/c75c10385343fbf2b2148de6f57eca8065388f3e.jpg)
![](http://imgsrc.baidu.com/forum/pic/item/32fa828ba61ea8d3b2195fc1d20a304e251f583f.jpg)
![](http://imgsrc.baidu.com/forum/pic/item/adaf2edda3cc7cd99b181f1c7c01213fb80e9139.jpg)
![](http://imgsrc.baidu.com/forum/pic/item/35a85edf8db1cb13755e17d69854564e92584b3b.jpg)
当然这里为了避免忘记，同时给出之前段描述符的概念结构：
![](http://imgsrc.baidu.com/forum/pic/item/5882b2b7d0a20cf453bb011b33094b36adaf99cf.jpg)
可以看出这里结构都是类似，只不过有的不同位的功能发生了变化，这也导致了我们的门可以正常放在段描述表或者说中断描述表中了。
这里提一嘴各门的区别：
1. 任务门：配合TSS使用实现特权级切换，可存放在GDT，LDT，IDT中,描述符中任务门的type字段二进制为0101
2. 中断门：包含中断处理程序所在段的段选择子以及偏移，当通过此方式进入中断之后，eflags寄存器的IF位自动置0,也就是关中断，防止中断嵌套。Linux就是使用中断门实现系统调用，也就是著名的int 0x80。中断门只允许存放在IDT中。描述符中中断门的type字段二进制为1110
3. 陷阱门：类似于中断门，区别就是IF位不会置0,只允许存放在IDT中，陷阱门的type字段二进制为1111
4. 调用门：提供用户进程进入特权0级，其DPL为3，只能用call或jmp指令调用，可以安装在GDT和LDT中，type值为1100

既然内存中应该存在一个IDT供我们使用，所以我们寻找他的方式类似与GDT，也就是说有一个寄存器来存放IDT的物理地址，这个寄存器就是IDTR，下面是IDTR的结构：
![](http://imgsrc.baidu.com/forum/pic/item/d53f8794a4c27d1eeada5b015ed5ad6edcc43872.jpg)
其中低16位代表段界限，高32位代表IDT物理地址，16位的表界限可以表示2^16B，也就是64KB，而一个门占8字节，所以一共可以存放64KB/8B = 8192个，这里注意虽然GDT第0表项为全0不可用，但IDT却无此限制，中断向量为0表示除法错。
同加载GDTR一样，加载IDTR也有个专门的指令--lidt，其用法是
lidt 48位内存数据

### 4.中断处理过程及保护
中断过程分为CPU外和CPU内两部分
CPU外： 外部设备的1中断由中断代理芯片接收，处理后将该中断的中断向量号发送到CPU
CPU内： CPU执行该中断向量号对应的中断处理程序
这里我们先讨论处理器内部的内容
1. 处理器根据中断向量号定位中断门描述符。
2. 处理器进行特权级检查
由于中断是通过中断向量号通知到处理器的，中断向量号只是个整数，其中并不包含RPL，所以在对由中断引起的特权级转移做特权级检查中不会涉及RPL。中断门的特权级检查同调用门类似，对于软件主动发起的软中断，当前特权级CPL必须在门描述符和目标代码段DPL之间，下面分情况解释
+ 若是由软中断int n，int3，into引发的中断，这些是由用户自主发起的，所以处理器要检查当前特权级和门描述符DPL，这是检查进门的特权下限，若是检查通过，也就是CPL特权级是高于门DPL的，那么将进入下一步“门框”的检查，否则处理器将会报出异常
+ 这一步检查特权级的上限“门框”，处理器要检查当前特权级CPL和门描述符中所记录选择子对应的目标代码段的DPL，若CPL特权级小于目标代码段的DPL，则检查通过，否则处理器引发异常。
+ 若中断是由外部设备和异常引起的，则只检查CPL和目标代码段的DPL，若CPL小于目标代码段特权，则检查通过，否则处理器引发异常。
3. 执行中断处理程序
特权级检查通过后，将门描述符目标代码段选择子加载到代码段寄存器cs中，把门描述符中的偏移地址加载至EIP中，然后执行中断处理程序。
过程如下图所示：
![](http://imgsrc.baidu.com/forum/pic/item/55e736d12f2eb9387366193790628535e4dd6fe8.jpg)

中断发生后，eflags中的NT位和TF位会被置0,若中断对应的是中断门，则在进入中断门后eflags的IF位会自动置0以此来防止中断嵌套，但是我们依然可以在中断处理中将IF位打开，我们先前说过修改eflags寄存器的内容只能通过pushf压栈然后恢复栈来修改，但此关系到内存访问了，效率想必是十分低效的，所以处理器专门提供了一个修改IF位的指令（开小灶是吧）。那就是cli和sti，其中cli指令使得IF位为0,sti指令使得IF为1,分别称之为关中断和开中断。
IF位只能限制外部设备中断，而对其他影响系统正常运行的中断都无效。
这里我们再来解释TF，即Trap Flag，也就是陷阱标志位，这用在调试环境中，当TF为0表示禁止单步执行。
而NT表示Nest Task Flag，即任务嵌套位，用来标记任务嵌套调用的情况。任务嵌套调用就是指CPU挂起当前的任务转而去执行另外一个任务，待到该任务执行完再转回去执行之前的任务，而CPU能如此是因为他会执行以下操作：
1. 将旧任务的TSS段选择子写到新任务TSS中的“上一个任务TSS的指针”字段中。
2. 将新任务eflags寄存器中NT置为1,表示新任务之所以呢能够执行是因为有别的任务调用了他

而CPU从新任务返回到旧任务是通过iret指令，他有两个功能，一个是中断返回，一个是返回旧任务，所以这里就需要用到NT位，因为执行iret的时候会去检查NT位的值，若为1则说明当前任务是嵌套执行的，若为0则说明是在中断处理环境下，于是执行正常的中断退出流程

### 5.中断压栈
中断发生时，处理器收到一个中断向量，根据该中断向量号在IDT中的偏移，然后找到对应的门然后通过其中的选择子，然后将该选择子移入CS中，再将门描述符中的偏移字段移入EIP。这时由于CS和EIP会被刷新，所以处理器会将被中断的程序的CS和EIP保存到当前中断处理程序使用的栈当中，至于说中断处理用的哪个栈，这里不好说，因为中断在任何特权级下都有可能发生，所以我们除了保存CS，EIP外还需要保存EFLAGS，如果涉及到特权级变化还要压入SS和ESP寄存器，下面来介绍寄存器入栈情况以及顺序：
1. 当处理器通过中断向量找到对应的中断描述符后，比较CPL和中断门描述符中选择子对应目标代码段的DPL对比，若发现向高特权级转移，则需要切换到高特权级的栈，这也意味着当我们执行完中断处理程序后需要恢复旧栈才行。因此处理器先临时保存旧SS和ESP的值，记做SS_old,和ESP_old,然后在TSS中寻找到对应目标代码段同特权级的栈加载到寄存器SS和ESP中，记作SS_new,和ESP_new，再将临时保存的SS_old和ESP_old压栈备份，如图所示：
![](http://imgsrc.baidu.com/forum/pic/item/1c950a7b02087bf48e04382cb7d3572c10dfcf0f.jpg)
2. 然后压入EFLAGS寄存器，如下图
![](http://imgsrc.baidu.com/forum/pic/item/f2deb48f8c5494ee05cc60dc68f5e0fe98257e0d.jpg)
3. 然后因为需要切换代码段，所以也要将CS和EIP保存到栈中进行备份，用以在中断结束后恢复被中断的进程，如下图：
![](http://imgsrc.baidu.com/forum/pic/item/5882b2b7d0a20cf4e358911a33094b36adaf992f.jpg)
4. 某些异常会爆出错误码，这个错误码是用于报告一场是在哪个段上发生的，也就是发生异常的位置，所以错误码中包含选择子等信息。他一般紧跟EIP后入栈，记为ERROR_CODE.如下图：
![](http://imgsrc.baidu.com/forum/pic/item/83025aafa40f4bfba811f191464f78f0f6361835.jpg)

处理器执行完中断处理程序后需要返回到被中断进程，也就是使用iret指令进行弹站，这里需要保证上述顺序。如果说有中断错误码，处理器并不知晓，所以这需要我们手动将其跳过，也就是说当我们准备用iret指令返回时当前栈指针必须得指向栈中备份的EIP_old所在的位置，这样才能依次对号入座。

### 6.中断错误码
错误码用来指明中断发生在哪个段上，所以说错误码最主要的部分是选择子，这里给出错误码的结构：
![](http://imgsrc.baidu.com/forum/pic/item/9345d688d43f879494a326eb971b0ef41ad53a92.jpg)
可以看出和选择子有9分的相似（ ，其中我们可以看到他的低2位有所不同，他代表的不是RPL，而是IDT和EXT，这里依次解释
+ EXT表示EXTernal event,即外部事件，用来指明中断源是否来自处理器内部，如果中断源是不可屏蔽中断NMI或外部设备，EXT为1,否则为0。
+ IDT表示选择子是否指向中断描述符表IDT，IDT位为1则表示此选择子指向中断描述符表，否则指向GDT或者是IDT

其中TI和选择子TI是一致的，为0知指选择子是从GDT中检索描述符，为1是从LDT中检索，而这个位起效的前提是IDT为0。
通常能够压入错误码的中断属于中断向量号在0～32之内的异常，而外部33～255之间和int软中断通常不会产生错误码，因此也不会产生错误码。

## 0x01 可编程中断控制器8259A
这里我们来介绍一个可编程中断控制器，我们以后要通过他来完成进程的调度，而8259A的功能是负责所有来自外设的中断。
### 1.简介
上节我们说到CPU为了接受到外部的可屏蔽中断，需要一根INTR引线，而这个“传话筒”只有一根却要接受多种外设传来的信号，例如打印机，声卡等，我们的CPU只能串行的执行任务，那么如果一群设备同时发送中断信号该怎么办呢，这时我们必须要进行一个中断仲裁，以此来决定CPU先接受哪个中断信号。所以我们需要一个专业的人来做专业的事，这个专业的人就是我们的中断代理，由他来负责对所有中断仲裁。

咱们介绍的8259A就是中断代理的一种
Intel处理器共有256个中断，可是8259A只可以管理8个中断，所以他们将多个8259A控制器组合，这里被成为级联。这里n个8259A进行级联可支持7n+1个中断源，级联时之恩那个有一片是主片master，其余均为从片。来自从片的中断只能传递给主片，再由主片传递给CPU。
每个独立外设所发出的中断只有接在中断请求（IRQ：Interrupt ReQuest）信号线上才会被CPU知晓。
这里我们来解释什么是级联，由于我们单个8259A芯片只有8个终端请求信号线：IRQ0～IRQ7,这肯定是不够的，所以我们用一种组合的方式来进行扩展，这就类似于我们平时的交换机，如果一个交换机无法支持数量众多的主机相连的话，我们会在交换机上再连个交换机，然后现在就相当与多了很多端口供主机相连，这也就解释了为啥n个控制器可以支持7n+1个中断而不是8n了。如下图：
![](http://imgsrc.baidu.com/forum/pic/item/f9dcd100baa1cd1153a218e5fc12c8fcc2ce2dda.jpg)
而我们个人的电脑中一般只有两片8259A芯片，我们同样会将其进行级联，所以共可用中断15个，这里给出家用计算机中的级联结构：
![](http://imgsrc.baidu.com/forum/pic/item/94cad1c8a786c917687938fc8c3d70cf3ac757f8.jpg)
每个外设发出中断信号他都会以为是发送到了INTR信号线上，但其实他是发给了中断代理芯片，然后再由代理传送信号到CPU的INTR信号线。然后我来介绍8259的内部结构，先给出示意图：
![](http://imgsrc.baidu.com/forum/pic/item/838ba61ea8d3fd1f4e58c8ff754e251f94ca5fa5.jpg)
+ INT:这是在中断控制器仲裁出中断信号后由此传递给CPU
+ INTA：INT Acknoledge，中断响应信号，位于8259A中，用来接受来此CPU的中断响应信号。
+ IMR：Interrupt Mask Register，中断屏蔽寄存器，宽度为8位，用来屏蔽某个外设中断
+ IRR：Interrupt Request Register，中断请求寄存器，宽度为8位，保存经过IMR筛选过后的中断信号形成一个中断待响应队列进行锁存。
+ PR：Rriority Register，优先级仲裁器，当多个中断发生，他将会找出优先级更高的中断。
+ IST：In-Service Register，中断服务寄存器，宽度是8位，当某个中断正在被处理时，保存在此寄存器中。

上面介绍的寄存器是8位的原因是我们一个8259A芯片支持8个中断，这里一个bit位就代表着一个中断。我们接着来介绍中断响的过程：
1. 外设发出一个中断信号，然后外设首先会将其发送至对应的8259A芯片的IRQ接口
2. 8259A首先检查IMR寄存器中对应中断的位，若为1则说明该中断被屏蔽，直接丢弃，若为0则说明可以通过，因此进行下一步
3. 将中断信号送入IRR，当某个恰当时机，将IRR的中断对应的bit位置1,这就表示他存在于队列之中
4. 然后PR会从IRR中选出一个优先级最大的中断，这里注意IRQx后的x越小代表优先级越大，例如IRQ0就是最大优先级
5. 之后通过INT线路传递信号给CPU的INTR线
6. 此时CPU知道有了新的中断信号，于是CPU执行完手上的活后通过INTA接口向8259A发送一个中断响应信号，此时8259A就知道CPU已经准备完毕
7. 将刚才芯片选出的优先级最大的中断在ISR寄存器中对应的位置为1，同时要将IRR中对应bit位置0,表示从队列中取出
8. 然后CPU再次通过INTA线向8259A索要中断向量号，这里的中断向量号是一个起始中断向量号再加上IRQ接口号，也就是IRQ后面跟着的一个数字
9. 之后8259A通过数据系统总线传递给CPU，CPU拿到中断向量号后去找到对应门描述符然后进行程序处理
10. 如果中断处理器的EOI通知（End of Interrupt）被设置为手动模式，则中断处理程序结束之后必须向8259A发送EOI代码，然后将ISR寄存器中正在处理中断对应的位置0。
11. 如果终端处理器的EOI通知被设置为自动模式，则刚才8259A接收到第二个INTA信号后，8259A会自动将此中断在ISR中对应的bit位置0

### 2.8259A的编程
这里对他编程的逻辑十分简单，就是对它进行初始化，设置主片和从片的级联方式，指定起始中断向量号以及设置各种工作模式。
在实模式的时候，BIOS就使用过8259A芯片，其中的IRQ0～IRQ7已经被BIOS分配了0x8～0xf的中断向量号。而在我们的保护模式之下，中断向量号为0x8～0xf的范围已经被CPU占了，分配给了各种异常，大家可以看上面介绍8259A的图片
其中我们的中断向量号是由起始中断向量号加上IRQ接口号，而接口号是固定的，所以我们通过设置起始号来映射对应中断向量号，在8259A中有两组寄存器，一组是初始化命令寄存器组，用来保存初始化命令字(ICW,Initialization Command Words)，ICW共4个，ICW1~ICW4，另一组寄存器是操作命令字寄存器组，用来保存操作命令序（OCW，Operation Command Word）共3个，OCW1~OCW3。所以说我们对8259A编程也可分为两部分：
+ 使用ICW做初始化，也就是向端口发送一系列ICW，这里我们之前磁盘和显卡已经熟悉了，这里一般是设置是否级联，设置起始中断向量号，设置中断结束模式。这里注意要严格按照ICW1～4的顺序写入
+ 使用OCW来控制8259A，前面所说的中断屏蔽和中断结束都是通过往8259A端口发送OCW来实现的，而OCW的顺序没有严格要求

之后是个大工程，我们依次来介绍ICW和OCW。

1. ICW1：用来初始化连接方式和中断信号的触发方式。连接方式就是单片或多片，触发方式是指中断请求信号是电平触发还是边沿触发，这里注意ICW1需要写入主片的0x20端口和从片的0xA0端口，结构如下：
![](http://imgsrc.baidu.com/forum/pic/item/71cf3bc79f3df8dccbc0d9548811728b46102871.jpg)
其中IC4表示是否要写入ICW4,IC4为1表示需要在后面系融入ICW4,为0则不需要，这里注意x86系统的IC4必须为1，而SNGL表示single，若SNGL为1表示单片，为0表示级联。ADI表示call address inteval，用来设置8085的调用时间间隔，x86不需要设置。LTIM表示level/edge triggered mod，用来设置检测方式，LTIM为0表示边沿触发，为1表示电平触发。
2. ICW2：用来设置起始中断向量号，注意ICW2需要写入主片0x21端口和从片0xA1端口，结构如下图：
![](http://imgsrc.baidu.com/forum/pic/item/f636afc379310a55e1fe8831f24543a983261018.jpg)
这里我们只需要设置IRQ0的中断向量号，之后都是依次排开，所以我们只需要写高5位T3～T7,ID0～ID3这低3位不用管，由于我们只填写5位是因为我们IRQ0后面到IRQ7一共有八个中断，所以我们每次给IRQ0分配只需要分配8的倍数就行
3. ICW3：只有在级联方式下才需要，也就是ICW1中SNGL位为0的情况，结构如下,这里注意ICW3需要写入主片的0x21端口以及从片的0xA1端口：
![](http://imgsrc.baidu.com/forum/pic/item/86d6277f9e2f070858c47cc5ac24b899a801f236.jpg)
![](http://imgsrc.baidu.com/forum/pic/item/3ac79f3df8dcd100a15faee4378b4710b8122f37.jpg)
对于主片，ICW3中设置1的那一位对应的那一位对应的IRQ接口用于连接从片，若为0则表示外部设备，而对于从片，要设置与主片的连接方式，只需在从片上制定主片用于链接自己的那个IRQ接口就行了。在中断响应的时候，主片会发送与从片做级联的IRQ接口号，所有从片用自己的ICW3低3位和他对比，若一致则认为是发给自己的，这低3位刚好可以表示0～7,就比如说主片与某个从片做级联的是IRQ7,则对应从片的低三位用二进制表示应该为111。从片高5位为0.
4. ICW4：用于设置8259A的工作模式，当ICW1中IC4为1时才需要设置它，ICW4需要写入主片的0x21端口和从片的0xA1端口，结构如下图所示：
![](http://imgsrc.baidu.com/forum/pic/item/b17eca8065380cd7516bf328e444ad34588281fa.jpg)
其中SFNM表示特殊全嵌套模式，若其为0表示全嵌套模式，为1表示特殊全嵌套模式。BUF表示8259A是否工作在缓冲模式下，为0表示非缓冲，为1相反。当多个8259A级联时，如果工作在缓冲模式下M/S用来规定谁是主片谁是从片，为1表示主片，为0表示从片，若工作在非缓冲模式下，则该位无效。AEOI表示自动结束中断，为0表示非自动，为1表示自动结束中断。缪PM表示微处理器类型，为1表示x86，为0表示8085或8080处理器。

之后我们来介绍OCW的格式。
1. OCW1：用来屏蔽连接在8259A上的外部设备的中断信号，实际上就是把OCW1写入了IWR寄存器，当然这里的屏蔽还不是最后一道关卡，可能你在这儿允许通过，但是eflags寄存器的IF位为0,则可屏蔽中断会全部关闭，仍然不会将该信号发往CPU，这里注意OCW1要写入主片的0x21或从片的0xA1号端口，结构如下图所示
![](http://imgsrc.baidu.com/forum/pic/item/a8ec8a13632762d074b4f57ae5ec08fa503dc669.jpg)
2. OCW2：用来设置中断结束方式和优先级模式，注意OCW2要写入到主片的0x20及从片的0xA0端口。下面是其对应的结构:
![](http://imgsrc.baidu.com/forum/pic/item/902397dda144ad349d45d64295a20cf430ad8579.jpg)
在OCW2中比较灵活的是有个开关位SL,可以针对某个特定优先级的中断进行操作。OCW2其中的一个作用就是发EOI信号结束中断。如果使SL为1,可以用OCW2的低3位来制定位于ISR寄存器中的哪一个中断被中止，如果SL为0则低三位不起作用，8259A会自动将正在处理的中断结束，也就是把ISR寄存器中优先级最高的位清0。OCW2另外一个作用就是设置优先级控制方式，使用R来设置，若R为0,则表示固定优先级，即接口号越低优先级越高，若R为1则使用循环优先级。如下图：
![](http://imgsrc.baidu.com/forum/pic/item/7af40ad162d9f2d32ac8752aecec8a136227cc1e.jpg)
此时如果SL为0,初始的优先级次序为IR0>1>2>3>4>>5>6>7,当某级别的中断被处理完成后，他的优先级将会变为最低，将最高优先级传给之前较之第一级的中断需求。另外还可以打开SL开关使得SL为1,再通过低3为设置最低优先级是哪个IRQ接口。然后我们依次来介绍相应位，首先是R，刚刚介绍过，为1表示循环优先级，为0表示固定优先级。SL，Specific Level，表示是否指定优先级，此处SL只是开启低三位开关，若为1则低三位有效，为0则无效。EOI，End of Interrupt，为中断结束命令位，EOI为1则会另ISR相应位清0,这里如果我们手动发送EOI的话表示8259A采用手动结束中断，此时ICW4中的AEOI位为0,下面再给出表格让大家梳理一遍：
![](http://imgsrc.baidu.com/forum/pic/item/7a899e510fb30f247f80bcc68d95d143ac4b03d8.jpg)
3. OCW3：设定特殊屏蔽方式以及查询方式，注意OCW3要写入主片的0x20端口或从片的0xA0端口，结构如下图所示：
![](http://imgsrc.baidu.com/forum/pic/item/b8389b504fc2d562597e8028a21190ef77c66c85.jpg)

介绍完上述字段，我们这里最后解释一下8259A是如何识别发过来的字段到底是ICW1～4还是OCW1～3呢，每个芯片就俩端口这是该如何识别呢（主片的两个端口为0x20和0x21,从片两个端口为0xA0和0xA1）。我们先总结一下：
+ ICW1和OCW2,OCW3用0x20和0xA0写入
+ ICW2～ICW4和OCW1用0x21和0xA1写入

由于咱们的ICW必须保证一定次序写入，所以8259A就知道写入端口的数据是什么了。
而OCW的写入顺序无关，且ICW1和OCW2,OCW3的写入端口相同，所以8259A采用控制字段的第3～4位来唯一确定，如下图：
![](http://imgsrc.baidu.com/forum/pic/item/1e30e924b899a9013f6e0bf358950a7b0308f598.jpg)
这样就确保了我们的8259A知道每次发送过来的控制字段是属于哪一类了。

## 0x02 编写中断处理程序
这里我们分几块来分别处理，首先就是init_all(),这函数咱们用来初始化所有设备和数据结构，这里他会首先调用idt_init(),他用来初始化中断相关内容，这一步咱们也分一下，那就是先执行pic_init()，他用来初始化可编程中断控制器8259A，然后idt_desc_init()来初始化中断描述符表IDT。
这里我们首先定义kernel.S,存放在kernel目录下:

```
;------ kernel/kernel.S ----------
[bits 32]
%define ERROR_CODE nop      ;若在相关的异常中CPU已经自动压入了，这里表示错误码，为保持栈中格式统一，这里不做操作
%define ZERO push 0         ;若在相关异常中CPU没有压入错误码，这里为了统一格式，手工压入一个0

extern put_str              ;声明外部函数，表明咱们要用到

section .data
intr_str db "interrupt occur!", 0xa, 0
global intr_entry_table
intr_entry_table:

%macro VECTOR 2             ;这里定义一个多行宏，后面的2是指传递两个参数,里面的%1,%2代表参数，类似linux shell脚本
section .text
intr%1entry：               ;每个中断处理程序都要压入中断向量号，所以一个中断类型一个中断处理程序，自己知道自己的中断向量号是多少
  %2                        ;手工压入32位数据或者空操作，保证最后都会需要跨过一个4字节来进行iret
  push intr_str 
  call put_str
  add esp, 4                ;清理栈空间
  
  ;如果是从片上进入的中断，除了往片上发送EOI外，还要往主片上发送EOI
  mov al, 0x20              ;中断结束命令EOI，这里R为0,SL为0,EOI为1
  out 0xa0, al              ;向从片发送，这里端口号可以使用dx或者立即数
  out 0x20, al              ;向主片发送

  add esp, 4                ;跨过error_code
  iret                      ;从中断返回

section .data
  dd intr%1entry            ;存储各个中断入口程序的地址，形成intr_entry_table数组,这里是因为编译后会将相同的节合并成一个段，所以这里会生成一个数组
%endmacro                   ;多行宏结束标志

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
VECTOR 0x20,ZERO

```

这里我们定义了一个中断处理程序的宏,也就是VECTOR 参数1,参数2，我们先来测试测试，所以所有的中断处理程序的功能就一个，打印一段字符串。然后我们需要将这些中断处理程序安装到中断描述表当中。
于是我们再编写一个interrupt.c，存放在kernel目录下,interrupt.c的功能就是初始化IDT描述表和IDT描述符，简单来讲就是准备中断相关数据而已。这里由于代码太多就不贴这儿了,详细代码以及注释我都放github
了，文末会说，这里给出描述符的数据结构作为参考：
```
/*中断门描述符结构体*/
struct gate_desc {
  uint16_t func_offset_low_word;
  uint16_t selector;
  uint8_t dcount;                   //不用考虑，这里为固定值
  uint8_t attribute;
  uint16_t func_offset_high_word;
};
```

之后我们同样类似于之前定义一些我们需要的宏，之前是在boot.inc中定义的段描述符，现在我们在kernel目录下创建一个global.h用来定义一些门描述符的类型。
```
//  "kernel/global.h"
#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H
#include "stdint.h"

#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

#define TL_GDT 0
#define TL_LDT 1

#define SELECTOR_K_CODE ((1<<3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA ((2<<3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_STACK SELECTOR_K_DATA
#define SELECTOR_K_GS ((3<<3) + (TI_GDT << 2) + RPL0)

/* ------------ IDT描述符属性 -------------- */
#define IDT_DESC_P  1
#define IDT_DESC_DPL0 0
#define IDT_DESC_DPL3 3
#define IDT_DESC_32_TYPE    0xE     //32位的门
#define IDT_DESC_16_TYPE    0x6     //16位的门，不会用到
#define IDT_DESC_ATTR_DPL0 ((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)
#define IDT_DESC_ATTR_DPL3 ((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)

#endif

```

目前我们的代码实现了一些初始化中断描述符以及表，所以我们还需要设置好中断代理8259A，这里我们使用内联汇编来写，这里内联汇编的语法上网搜索一下即可：
```
// lib/kernel/io.h
/*************** 机器模式 ******************/
#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "stdint.h"

/* 向端口port写入一个字节 */
static inline void outb(uint16_t port, uint8_t data){
/***************************************************
 对端口指定N表示0～255,d表示用dx存储端口号，%b0表示对应al，%wl表示对应dx 
***************************************************/
  asm volatile("outb %b0, %w1" :: "a"(data)."Nd"(port));    //input为data，port，约束分别为al寄存器和dx
/**************************************************/
}

/* 将addr处起始的word_cnt个字写入端口port */
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt){
/***************************************************
 + 表示限制既做输入，又做输出，outsw是吧ds:esi处的16位内容写入port端口，我们设置段描述符的时候已经将ds，es，ss段选择子都设置为相同的值了
***************************************************/
  asm volatile("cld; rep outsw" : "+S"(addr), "+cx"(word_cnt) : "d"(port)); //output为addr和word_cnt,约束分别为si和cx，input包含前两位和后面的port，约束为dx
/**************************************************/
}

/* 将从端口port读入一个字节返回 */
static inline uint8_t inb(uint16_t port){
  uint8_t data;
  asm volatile("inb %w1, %b0" : "=a"(data) : "Nd"(port));
  return data;
}

/* 将从端口port读入的word_cnt个字写入addr */
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt){
/**************************************************
 insw是指将从端口port处读入16字节到es:edi指向的内存
***************************************************/
  asm volatile("cld; rep insw" : "+D"(addr),"+c"(word_cnt) : "d"(port) : "memory");
/**************************************************/
}

#endif

```

上面的内容很简单，也就是简单的定义了几个端口输入输出函数而已，之后我们就要正式开始对8259A进行编程了.这里说是编程，其实就是往对应端口写入控制指令字而已，如果忘记了可以看看前面的介绍，一共有ICW1～4,OCW1～3七种控制字。
这里由于我们硬盘是接在了从片的引脚，所以我们需要采用级联的形式，这里再次给出控制字和端口的对应，如果大家忘记了可以往上翻阅：
+ ICW1和OCW2,OCW3用0x20（主片）或0xA0（从片）写入
+ ICW2~4和OCW1是用0x21（主片）或0xA1（从片）写入

我这里给出初始化8259A的部分代码：
```
/* 初始化可编程中断控制器 */
static void pic_init(void){
  /* 初始化主片 */
  outb(PIC_M_CTRL, 0x11);           //ICW1:边沿触发，级联8259,需要ICW4
  outb(PIC_M_DATA, 0x20);           //ICW2:起始中断向量号为0x20,也就是IRQ0的中断向量号为0x20
  outb(PIC_M_DATA, 0x04);           //ICW3:设置IR2接从片
  outb(PIC_M_DATA, 0x01);           //ICW4:8086模式，正常EOI，非缓冲模式，手动结束中断

  /* 初始化从片 */
  outb(PIC_S_CTRL, 0x11);           //ICW1:边沿触发，级联8259,需要ICW4
  outb(PIC_S_DATA, 0x28);           //ICW2：起始中断向量号为0x28
  outb(PIC_S_DATA, 0x02);           //ICW3:设置从片连接到主片的IR2引脚
  outb(PIC_S_DATA, 0x01);           //ICW4:同上

  /* 打开主片上的IR0,也就是目前只接受时钟产生的中断 */
  outb(PIC_M_DATA, 0xfe);           //OCW1:IRQ0外全部屏蔽
  outb(PIC_S_DATA, 0xff);           //OCW1:IRQ8~15全部屏蔽

  put_str(" pic init done!\n");
}

```

上面解释的也十分清楚，这是包含在interrupt.c中的
这里来总结一下，我们目前所完成的工作有：
1. 编写kernel.S,里面包含一个中断处理程序宏，且目前所有中断程序都一样，都是输出一个字符串
2. 编写interrupt.c，里面我们初始化了IDT，以及门描述符
3. 编写io.h,里面包含端口写数据的一些函数，使用了内联汇编
4. 完善inaterrupt.c,里面我们初始化了8259A，且目前只支持时钟中断

所以现在继续我们的工作，我们需要加载IDT，跟我们之前加载GDT类似，也就是将IDT的地址存入IDTR而已，这里我们需要使用指令lidt，为了避免麻烦我们继续采用内联汇编实现
```
  /* 加载idt */
  UINT64_t idt_operand = (sizeof(idt)-1) | ((uint64_t)((uint32_t)idt << 16));   //这里(sizeof(idt)-1)是表示段界限，占16位，然后我们的idt地址左移16位表示高32位，表示idt首地址
  asm volatile("lidt %0" : : "m" (idt_operand));
```
这里也是我们修改的interrupt.c。

到此为止，我们的一切准备工作已经就绪，现在我们再将其全部封装起来，在kernel目录下创建一个init.c文件
```
//kernel/init.c
#include "init.h"
#include "print.h"
#include "interrupt.h"

/* 负责初始化所有模块 */
void init_all(){
  put_str("init_all\n");
  idt_init();       //初始化中断
}

```

之后我们修改我们的main.c函数就可以了
```
#include "print.h"
#include "init.h"
void main(void){
  put_str("I am Kernel\n");
  init_all();
  asm volatile("sti");  //为演示中断处理，在此临时开中断
  while(1);
}
```

为了避免文件杂乱，这里我们生成一个build目录来存放咱们所有的目标文件以及二进制文件，然后我们依次进行编译链接：
```
gcc -no-pie -fno-pic -fno-stack-protector -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/main.o kernel/main.c \
nasm -f elf -o build/print.o kernel/print.S \
nasm -f elf -o build/kernel.o kernel/kernel.S \
gcc -no-pie -fno-pic -m32 -fno-stack-protector -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/interrupt.o kernel/interrupt.c \
gcc -no-pie -fno-pic -m32 -fno-stack-protector -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/init.o kernel/init.c \
ld -m elf_i386 -T kernel/link.script -Ttext 0xc0001500 -e main -o build/kernel.bin  build/main.o build/init.o build/interrupt.o build/print.o build/kernel.o \
dd if=./build/kernel.bin of=./bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc
```

这里是不是感觉很麻烦，每次都要重新编译？不要急，下一篇我们就会用makefile来进行管理了。
然后我给出目录树结构防止大家迷糊，所以给出目录树
```
lib/
├── kernel/
│   ├── io.h
│   ├── print.h
│   └── print.S
├── stdint.h
└── user/
kernel/
├── global.h
├── init.c
├── init.h
├── interrupt.c
├── interrupt.h
├── kernel.S
├── link.script
└── main.c
boot/
├── assemble.sh*
├── include/
│   └── boot.inc
├── loader.bin
├── loader.S
├── mbr.bin
└── mbr.S
build/
├── init.o
├── interrupt.o
├── kernel.bin*
├── kernel.o
├── main.o
└── print.o
demo/
├── C_with_S_c.c
├── C_with_S_S.S
├── syscall_write.bin*
├── syscall_write.o
└── syscall_write.S

```

然后我们到bochs里面运行一下试试看，效果如下图：
![](http://imgsrc.baidu.com/forum/pic/item/0ff41bd5ad6eddc44ea4f23e7cdbb6fd536633b7.jpg)
可以看到时钟每次发送中断信号，我们的中断处理程序都会输出一个字符串，我们也可以在bochs中调试命令info idt来查看idt的信息，如下图：
![](http://imgsrc.baidu.com/forum/pic/item/bd315c6034a85edf6832b8e60c540923dc5475bd.jpg)

## 0x03 改进中断处理程序
上面我们都知道，中断处理程序简单的不能再简单，所以这里我们是来写处理程序的，这里我们采用C语言编写然后汇编代码调用C语言函数即可，这样咱们写的也简单点。
我们首先需要在C函数中声明出一个数组idt_table，这里面存放我们所需要的中断程序地址，因此一个元素占有4字节，而我们中断程序是按照中断向量号区分的。那么该如何找到对应的程序地址呢，这里只需要进行一个简单的计算，由于中断处理程序的地址在idt_table中占4字节，所以中断程序地址 = idt_table地址+中断向量号×4,简单的计算。
本次修改我们需要修改两个文件，那就是interrupt.c和kernel.S,
这里贴出关键代码，首先是修改的interrupt.c部分
```
/* 通用的中断处理函数，一般用在出现异常的时候处理 */
static void general_intr_handler(uint64_t vec_nr){
  /* IRQ7和IRQ15会产生伪中断，IRQ15是从片上最后一个引脚，保留项，这俩都不需要处理 */
  if(vec_nr == 0x27 || vec_nr == 0x2f){
    return;
  }
  put_str("int vector : 0x");       //这里我们仅实现一个打印中断数的功能
  put_int(vec_nr);
  put_char('\n');
}

/* 完成一般中断处理函数注册以及异常名称注册 */
static void exception_init(void){
  int i;
  for(i  = 0; i < IDT_DESC_CNT; i++){
/* idt_table数组中的函数是在进入中断后根据中断向量号调用的
 * */
    idt_table[i] = general_intr_handler;    //这里初始化为最初的普遍处理函数
    intr_name[i] = "unknown";               //先统一赋值为unknown
  }
  intr_name[0] = "#DE Divide Error";
  intr_name[1] = "#DB Debug Exception";
  intr_name[2] = "NMI Interrupt";
  intr_name[3] = "#BP Breakpoint Exception";
  intr_name[4] = "#OF Overflow Exception";
  intr_name[5] = "#BR BOUND Range Exceeded Exception";
  intr_name[6] = "#UD Invalid Opcode Exception";
  intr_name[7] = "#NM Device Not Available Exception";
  intr_name[8] = "#DF Double Fault Exception";
  intr_name[9] = "Coprocessor Segment Overrun";
  intr_name[10] = "#TS Invalid TSS Exception";
  intr_name[11] = "#NP Segment Not Present";
  intr_name[12] = "#SS Stack Fault Exception";
  intr_name[13] = "#GP General Protection Exception";
  intr_name[14] = "#PF Page-Fault Exception";
  //intr_name[15]是保留项，未使用
  intr_name[16] = "#MF x87 FPU Floating-Point Error";
  intr_name[17] = "#AC Alignment Check Exception";
  intr_name[18] = "#MC Machine-Check Exception";
  intr_name[19] = "#XF SIMD Floating-Point Exception";
}

```

然后修改kernel.S部分
```
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

```

这里再给出压栈情况，注意这里无特权变换
![](http://imgsrc.baidu.com/forum/pic/item/279759ee3d6d55fbca903be328224f4a21a4dda5.jpg)

然后我们像上次一样继续编译运行查看结果：
![](http://imgsrc.baidu.com/forum/pic/item/77094b36acaf2edd1959042cc81001e93801934f.jpg)
简直不要太成功，这里可以发现我们确实一直在触发0x20中断向量号的时钟中断

## 0x04 定时器来力！！
上面我们已经成功实现了中断，但是本节还没完，我们要知道上面是我们实现的中断处理程序，但是这个中断是谁发出的呢，没错就是咱们的标题定时器，他也有个名字叫做计数器。
他的存在是为了缓和内部时钟和外部时钟，解决处理器和外部设备同步数据时的时序配合问题，其大致思路就是计时器定时发信号，用该信号向处理器发送中断，这样处理器就会去执行相应的中断处理程序。
这里我们介绍一个叫做8253的定时器。而硬件计时器一般有两种计时的方式：
1. 正计时：每一次时钟脉冲发生，将当前计数值加一，直到与设定值相等，类似与闹钟。
2. 倒计时：先设定好计数器的值，每次时钟脉冲发生就将计数减一，直到为0,类似于秒表定时

而我们介绍的8253是使用倒计时的，这里我们对他编程主要是设置这个值。
闲话到此为止，我们先来看看8253内部结构：
![](http://imgsrc.baidu.com/forum/pic/item/a2cc7cd98d1001e9846358cafd0e7bec55e7979c.jpg)
8253中有3个独立的计数器，分别0～3,他们对应的端口号分别是0x40~0x42，且都是16位大小。下图是独立一个计数器的结构图：
![](http://imgsrc.baidu.com/forum/pic/item/c9fcc3cec3fdfc039c34af7d913f8794a5c226a2.jpg)
计数器本质上是一个减法器，因为咱们是倒计时嘛。每个计数器有三个引脚：
+ CLK，时钟输入信号,这里是指计数器自己的工作节拍，每收到一个这样的信号，计数器的值就减1
+ GATE，门控制输入信号，后面介绍
+ OUT，计数器输出信号，定时工作结束之后，会从该引脚发出信号来说明定时完成，这样处理器或外部设备就可以执行相应的动作

然后我们依次来介绍计数器中的寄存器们，
+ 计数初值寄存器，占16位，根据名字即可理解，他是存放的我们对8253初始化时写入的计数初始值。
+ 计数器执行部件，实际上为16位减法器，它将初值寄存器的值拿下来不断原地减1。
+ 输出锁存寄存器，用于把当前减法计数器中的计数值保存下来

而我们8283中的三个计数器都有自己独特的使命，如下：
![](http://imgsrc.baidu.com/forum/pic/item/35a85edf8db1cb13a8e370d69854564e93584b69.jpg)
我们这里只需要了解计数器0，他的主要作用是产生时钟信号，这个时钟连接到主片IRQ0引脚上，也就是说计数器0决定了时钟中断信号的频率。

这里我们想要对他进行编程，这就类似与咱们上面对8259A编程，我们首先需要了解一点控制字的格式，他一般是写入操作端口0x43,它是8位寄存器。如下图
![](http://imgsrc.baidu.com/forum/pic/item/a9d3fd1f4134970ac7895cead0cad1c8a6865d79.jpg)
这里依次介绍相关字段
+ SC1和SC0：如图，表示计数器0～3
+ RW1和RW2：如图
+ M2～M0：之后详解
+ BCD：也就是BCD码，用4位二进制来表示十进制，这一位为0表示使用2进制计数，若为1表示使用BCD

上面的M2～M0是表示的工作方式，这里来集中解释一下：
![](http://imgsrc.baidu.com/forum/pic/item/6a63f6246b600c330d45910d5f4c510fd9f9a133.jpg)
之前说了一大通，所以计数器什么时候开始记时呢，有可能会认为是我们写入计数器初始值之后，但实际上不是如此，开始时机与工作方式相关，他需要两个条件:
1. GATE为高电平，即GATE为1,由硬件控制
2. 计数器初值写入了减法器，这是由软件out指令来控制的

上面的条件根据哪个未完成来划分，分为软件启动和硬件启动：
+ 条件1达成，现在只需写入计数初值。工作方式0,2,3,4都是用软件启动计数过程
+ 条件2达成，现在只需要坐等GATE由0变成1的上升沿出现时，计数器才会开始计数，工作方式1,5都是用硬件启动计数过程

计数器是按上述满足两个条件启动的，而对于终止的话，根据不同的工作方式，分为强制终止和自动终止：
+ 强制终止，计时到（为0）后，减法计数器会将计数初始值重新载入，继续下一轮计数，工作方式2和3都是采用这种技术方式，对于这种采用循环计数的方式，我们只能施加外力来结束他，不然他一直运行下去了，这里我们只需要将GATE置0,破坏其运行条件即可。
+ 自动终止，工作方式0,1,4,5都是单次计数，也就是一轮之后就终止，此时也可以在过程中将GATE置0强制终止。

然后我么介绍6种工作方式：
+ 方式0：计数结束输出正跳变信号，在方式0下，会将该计数器通道的OUT变为低电平，这直到我们的计数值为0.技术工作由软件启动，也就是处理器用out指令将计数初值写入计数器，然后到计数器就开始减1,注意这里可能会有一个时钟脉冲延迟，因为计数器的工作是按照自己的时钟CLK来进行的。在之后CLK每收到一次脉冲信号，减法计数器就将计数值减一。
+ 方式1：硬件可重触发单稳方式，触发信号是GATE，在方式1下，计数初值写入计数器后，OUT引脚变为高电平，但是注意这里GATE不论是高电平还是低电平他都不会启动计数，而是要等到GATE从低电平到高电平那个上升沿才会启动计数，此后在一个CLK下降沿开始计数，OUT变为低电平，之后CLK每收到一个时钟脉冲，减法器工作，直到计数值为0,此时OUT由低到高电平，产生正跳变
+ 方式2：比率发生器：处理器把控制字写入到计数器当中，OUT变为高电平，GATE为高电平的前提下，处理器将计数值初值写入后，在下一个CLK时钟脉冲下沿，计数器开始计数，属于软件启动。当计数值为1时，OUT变为低电平，然后知道计数值为0,OUT又变为高电平，同时计数初值又会被载入减法器，这一过程不断重复。

---
这里我们即将开始实验，首先介绍一下8253的初始化步骤，这里很简单就是写入控制字就行：
1. 往控制字寄存器端口0x43中写入控制字;
2. 计数器端口中写入计数初值;

## 0x05 收下吧，这是我最后的升级辣，JOJO！！！
时间要开始加速了，MADE IN HAVEN ! 
我们学习8253的目的是为了给IRQ0引脚上的时钟中断信号提速，这里他的默认频率是18.206HZ，也就是大约1秒发出18次中断信号。本次我们一次搞多点，100HZ如何？

首先我们梳理一下过程：
+ IRQ0引脚的时钟中断信号频率是由8253计数器0设置的，所以我们要使用计数器0
+ 时钟中断信号肯定要循环，所以这里我们采用方式2
+ 计数器初值我们采用11932，这是因为计数器0的工作频率是1.19318MHz，这里我们为了实现100Hz的中断信号，所以我们需要1.19318M/100,这样就得出了我们需要的初始值了。

本次我们构建一个device目录，然后目录下创建timer.c程序
```
#include "timer.h"
#include "io.h"
#include "print.h"

#define IRQ0_FREQUENCY 100                      //咱们所期待的频率
#define INPUT_FREQUENCY 1193180                 //计数器平均CLK频率
#define COUNTRE0_VALUE  INPUT_FREQUENCY / IRQ0_FREQUENCY    //计数器初值
#define CONTRER0_PORT   0x40                    //计数器0的端口号
#define COUNTER0_NO     0                       //计数器0
#define COUNTER_MODE    2                       //方式2
#define READ_WRITE_LATCH 3                      //高低均写
#define PIT_CONTROL_PORT 0x43                   //控制字端口号

/* 把操作的计数器counter_no,读写锁属性rwl,计数器模式counter_mode
 * 写入模式控制寄存器并赋予初值 counter_value
 *  */
static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl, uint8_t counter_mode, uint16_t counter_value){
  /* 往控制字寄存器端口0x43写入控制字 */
  outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1)); //计数器0,rwl高低都写，方式2,二进制表示
  /* 先写入counter_value的低8位 */
  outb(CONTRER0_PORT, (uint8_t)counter_value);
  /* 再写入counter_value的高8位 */
  outb(COUNTER_MODE, (uint8_t)counter_mode >> 8);
}

/* 初始化PIT8253 */
void timer_init(){
  put_str("timer_init start\n");
  /* 设置8253的定时周期，也就是发中断的周期 */
  frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTRE0_VALUE);
  put_str("timer_init_done\n");
}
```

感觉这个比之前都简单得多，之后我们照常编译链接，最后执行效果如下图：
![](http://imgsrc.baidu.com/forum/pic/item/8b13632762d0f703fcaadb194dfa513d2797c5df.jpg)
有没有发现没啥变化，但其实这里图片上看不出来，但确实他的频率被重新设置了，肯定是比以前快的多。

## 0x06 总结
这章终于解释了一下操作系统中比较重要的中断，学习之后也可以自己去bochs上面调试一下看看执行中断程序之前都干了啥，然后之后的设置8258A以及8253都对中断有了另外的见解，其次上一篇的特权级也是十分重要的，这在以后会慢慢体现。
