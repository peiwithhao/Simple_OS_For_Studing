## 0x00 基础知识们
咱们目前已经实现了各种系统调用以及用户进程，内核线程等更不必说，但是咱们现在的用户进程仅仅是一个伪造的，为什么说伪造呢，那是因为咱们的用户进程也都是在内核部分，这里仅仅是将他的特权级给改为了3而已，而我们真正实现用户进程那还得咱们的文件系统的支持，要实现文件系统是个大工程，这里我们得先实现满足文件系统的环境，那就是硬盘。
之前咱们已经有了一个虚拟磁盘hd60M.img，但是它只充当了启动盘的作用，仅仅用来存储内核，是个没有文件系统的裸盘。这里我们为了避免冲突，就另外创建一个磁盘专门来存放咱们的文件系统
### 1.创建从盘
回忆一下以前的步骤，我们使用bin/bximage 来创建一个80MB的磁盘作为从盘，如下：
![](http://imgsrc.baidu.com/super/pic/item/738b4710b912c8fc13c7770bb9039245d78821c4.jpg)
操作同之前一致，只不过这里我们将大小定为80M，接下来我们的操作就是将所圈的那行添加到bochsrc.disk中，但这里我们先别添加，咱们得先证明他确确实实被装上了，在物理地址0x475处存储着主机上安装的硬盘的数量，它由BIOS进行检测并写入的。这里我们在安装新硬盘之前先来检测一下：
![](http://imgsrc.baidu.com/super/pic/item/3bf33a87e950352aa07ba4ce1643fbf2b3118bda.jpg)
这里可以看到目前仍然是1个磁盘，然后我们将刚刚创建的磁盘的那行配置语句填写在bochrc.disk下方
```
ata0-slave: type=disk, path="hd80M.img", mode=flat
```
这里我们需要将master改为slave让他作为从盘。添加后我们再来到bochs当中检测:
![](http://imgsrc.baidu.com/super/pic/item/472309f7905298223738b80592ca7bcb0b46d4e9.jpg)
这里注意我们再添加一些参数，这里我们修改一下主盘与从盘也就是最后两排的参数：
```
ata0-master: type=disk, path="hd60M.img", mode=flat, cylinders=121, heads=16, spt=63
ata0-slave: type=disk, path="hd80M.img", mode=flat, cylinders=162, heads=16, spt=63

```

至于其中参数的含义，我们在下面讲解。
可以看到确实变成了2,说明咱们添加磁盘成功！

### 2.创建磁盘分区表
为了让咱们的文件系统更好的扎根在本磁盘上，我们首先要对其进行分区操作，本次我们使用的工具就是fdisk，这里我们先从物理结构上理解磁盘，当然是机械磁盘，固态硬盘咱们不考虑。
+ 盘片：布满磁性介质的小光盘
+ 扇区：硬盘读写的基本单位，在磁道上均匀分布。通常一个扇区为512字节大小
+ 磁道：盘片上的一个个同心园环就是一个个磁道
+ 磁头：一个读取磁盘的机械臂，每个盘面都存在一个磁头，用来读取该盘面上的磁盘信息
+ 柱面：由不同盘面的相同磁道构成的一个圆柱面就叫住面，这里也是为了提升咱们的IO读写速度
+ 分区：由多个编号连续的柱面组成的，这里注意一个柱面不能包含多个分区

上面介绍完了基础知识，得出以下公式：
1. 硬盘容量 = 单片容量×磁头数
2. 单片容量 = 每磁道扇区数×磁道数×512字节

所以说磁盘容量 = 每磁道扇区数×磁道数×512字节×磁头数
上面的磁道数也可以被叫做柱面数我们之前的配置是柱面数是162,磁头数是16,spt也就是每个磁道的扇区数是63。而柱面数和磁头数是取决于具体配置的，而其他诸如扇区数或扇区大小一般都是通用规约，也就是说在硬盘容量已知的情况下，我们需要凑出适合的柱面数×磁头数 = 硬盘容量/63/512。
咱们可以来稍微计算一下，现在咱们的硬盘容量是80MB，所以柱面数×磁头数 = 83607552/63/512=2592,所以咱们的162×16刚好得出这个数，因此咱们采用上述的配置手法。
一般我们的硬盘只支持4个分区，但是随着技术的发展硬盘容量越来越大，我们需要支持的分区数也尽量越多越好，但这里仍保持着4个分区的传统，这是因为我们仍然需要兼容一些以往的配置，所以这里有一个解决方案那就是4个分区中我们将一个分区再进行划分为若干个子分区，所以这样在理论上我们就可以支持任意数量个分区了。
接下来我们立刻开始进行分区：
1. 我们首先使用
```
fdisk -l ./hd80M.img
```
来查看硬盘信息
![](http://imgsrc.baidu.com/super/pic/item/b3b7d0a20cf431adb8b2ccff0e36acaf2fdd9890.jpg)
2. 然后正式开始分区：
![](http://imgsrc.baidu.com/super/pic/item/dbb44aed2e738bd4a17f3df7e48b87d6267ff99e.jpg)
这里有个command输入，我们先输入m来查看一下帮助手册
3. 看到我标亮的区域，新建一个分区，这里我们接着输入n试试
![](http://imgsrc.baidu.com/super/pic/item/b3119313b07eca800d54b621d42397dda04483a7.jpg)
这里会发现并没有设置柱面等信息，所以我们暂且ctrl+c退出设置，我们再输入x来进入专业模式
![](http://imgsrc.baidu.com/super/pic/item/adaf2edda3cc7cd9f76abb1f7c01213fb90e91b5.jpg)
然后我们看到了可以设置磁头数和柱面数，这里我们依次选择然后输入咱们配置中的数字就行了
![](http://imgsrc.baidu.com/super/pic/item/5fdf8db1cb134954a85d67a2134e9258d0094ab1.jpg)
4. 然后我们使用n来创建分区
![](http://imgsrc.baidu.com/super/pic/item/77094b36acaf2edddb55c62fc81001e938019354.jpg)
这里我们使用n分别创建了两个分区，一个1号主分区，还有一个4号扩展分区，大伙配置看我的步骤就行，然后我们使用p指令来查看硬盘分区信息。接下来我们再键入n指令来创建分区的话他就会默认创建扩展分区中的子分区了，配置如下
![](http://imgsrc.baidu.com/super/pic/item/0dd7912397dda144c09de374f7b7d0a20df48664.jpg)
5. 我们再使用w指令来保存设置，然后再使用fdisk来查看硬盘信息
![](http://imgsrc.baidu.com/super/pic/item/d1a20cf431adcbef02a6f1c0e9af2edda2cc9f62.jpg)

### 3.分区表
这里的分区表也就是Disk Partition Table,简称DPT，是由多个分区元信息汇成的表，表中每一个表项都对应一个分区。最初的磁盘分区表位于MBR当中，我们最开始讲MBR的时候就已经说过他的结构，这里我们再来重新回忆一下：
1. 主引导记录MBR，位于0～0x1BD，共计446字节
2. 磁盘分区表DPT，位于0x1BE～0x1FD，共计64字节
3. 结束魔数55AA，表示此扇区为主引导扇区，里面包含控制程序

本来一个硬盘是只有1个分区表的，但是随着我们需要的分区越来越多，咱们这个固定的分区表就很难满足咱们的需求了，但为了向上兼容，我们无法改变他的一些结构，所以这里我们采用了下面的解决方法，那就是扩展分区中的子分区被咱们视作一个个小硬盘，而每个硬盘上存在着一个分区表，这样一来，咱们的硬盘上就可以存在许多个分区表了，也就支持了咱们多个分区的需求了。
上面说了每个子分区也是当作一个硬盘来看待的，所以子分区同上面的结构也是一致的，首先就是1块EBR（真实硬盘叫做MBR）所占的一个块，然后后面跟一些空闲快（这里空闲是因为同属于EBR的磁道不能跨柱面存在），其中MBR和EBR的结构是一致的，MBR只有一个，EBR理论上可以有无数个。
由于扩展分区采用链式分区表，所以EBR中分区表地一个分区表项用来描述所包含的逻辑分区的元信息，第二分区表项用来描述下一个子扩展分区的地址，第三、第四表项暂未用到。位于EBR中的分区表相当于链表中的节点，地一个分区表项存放的是分区数据，第二个分区表项存放的是后继分区的指针。
这里注意我们的前两个分区表项都是指向一个分区的起始地址，第一个表项是指向的是该逻辑分区最开始的山区，这里被称作操作系统引导扇区，即OBR引导扇区。第二个分区表项指向下一个子扩展分区的EBR引导扇区。下面给出单个分区表项的结构
![](http://imgsrc.baidu.com/super/pic/item/d8f9d72a6059252d9bd195c2719b033b5ab5b936.jpg)
其中OBR在咱们这儿简单来说就是引导程序所处的分区，里面一般都存放着咱们的内核加载程序Loader。
这里我们还需要解释一下“分区起始偏移地址”和“分区容量扇区数”
1. “分区起始偏移地址”是指相对于本分区所依赖的上层对象（也就是说将该子分区包含在内的总扩展分区的起始扇区LBA地址），当然如果本分区就是主分区或者总扩展分区的话，那么该值为0。
2. “分区容量扇区数”表示分区的容量扇区数，说了等于白说。

然后我们来实际操作一下，先使用xxd命令来查看一下咱们硬盘的起始512字节，这里我们将其封装在一个脚本之下：
```
#usage: sh xxd.sh 文件 起始地址 长度
xxd -u -a -g 1 -s $2 -l $3 $1

```

结果如下
```
dawn@dawn-virtual-machine:~/repos/OS_learning$ ./xxd.sh bochs/hd80M.img 0 512
00000000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
000001b0: 00 00 00 00 00 00 00 00 F4 3F C8 5C 00 00 00 00  .........?.\....
000001c0: 21 02 83 00 20 12 00 08 00 00 00 3F 00 00 00 00  !... ......?....
000001d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
000001e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 04  ................
000001f0: 25 12 05 0F 3F A1 00 48 00 00 E0 35 02 00 55 AA  %...?..H...5..U.

```

从中可以看到0～0x1b0之间全是0,后面的这一部分从0x1be开始才是咱们的分区表，直到最后的魔数0x55和0xAA。这里由于我们只创建了1分区和4分区，所以中间的部分也都是0x00,因此我们来查看一下重点的这两个分区类型：
|分区|分区类型|偏移扇区|扇区数|
|--|--|--|--|
|主分区hd80M.img1|0x83|0x00000800|0x00003F00|
|总扩展分区hd80M.img4|0x05|0x00004800|0x000235E0|

这里我将用画图来给大家演示整个分区的步骤：
首先我们通过上面的表来给大家简单介绍一下目前的硬盘结构：
![](http://imgsrc.baidu.com/super/pic/item/c8177f3e6709c93dcb6f8531da3df8dcd0005453.jpg)

这里只是大致给出来主分区和扩展分区，这里我们再来查看一下扩展分区中的子分区，首先我们将0x4800乘512得到0x900000，然后查看512字节如下
```
dawn@dawn-virtual-machine:~/repos/OS_learning$ ./xxd.sh bochs/hd80M.img 0x900000 512
00900000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
009001b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 05  ................
009001c0: 06 14 83 05 05 1D 00 08 00 00 70 23 00 00 00 07  ..........p#....
009001d0: 28 1E 05 00 08 2D 00 30 00 00 38 39 00 00 00 00  (....-.0..89....
009001e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
009001f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 55 AA  ..............U.

```

这里我们可以看到钱两个表项是存在值的，我们将关键信息存放如下:
|分区编号|分区类型|偏移扇区|扇区数|
|--|--|--|--|
|逻辑分区hd80M.img5|0x83|0x00000800|0x00002370|
|下一个子扩展分区|0x05|0x00003000|0x00003938|

子扩展分区是在总扩展分区中创建的，所以子扩展分区的绝对扇区LBA地址=总扩展分区的绝对扇区LBA地址+子扩展分区的偏移扇区LBA地址
而逻辑分区是在子扩展分区中创建的，逻辑分区的绝对LBA地址=子扩展分区绝对扇区LBA地址+逻辑分区的偏移扇区LBA地址
![](http://imgsrc.baidu.com/super/pic/item/203fb80e7bec54e74b3891d8fc389b504ec26a24.jpg)
后面的子扩展分区都是依次类推，这里我就不多讲了。

## 0x01 编写硬盘驱动程序
为了支持硬盘操作，我们还需要做几件事。硬盘上有两个ata通道，也被称作IDE通道。第1个ata通道上的两个硬盘(主和从)的中断信号挂在8259A的IRQ14上面，而为了区分是对主盘还是从盘进行操作，是在硬盘控制器的device寄存器中第4位的dev位指定的。而第2个ata通道是链接在8259A的从片上的IRQ15上，而咱们的8259A的主片是用IRQ2来级联从片的，所以来自从片的中断需要通过IRQ2来传到主片，因此还需要打开IRQ2接口。因此我们修改interrupt.c文件如下：
```
  /* 开启时钟中断，键盘中断，IRQ2,硬盘接口 */
  outb(PIC_M_DATA, 0xf8);           //OCW1
  outb(PIC_S_DATA, 0xbf);           //OCW1

```

这里为了避免麻烦，我们在内核态下也实现了格式化输出，跟用户态是一样的，如下创建lib/kernel/stdio-kernel.c：
```
#include "stdio-kernel.h"
#include "stdio.h"
#include "console.h"
#include "global.h"
#include "print.h"

#define va_start(args, first_fix) args=(va_list)&first_fix
#define va_end(args) args = NULL

/* 供内核使用的格式化输出函数 */
void printk(const char* format, ...){
  va_list args;
  va_start(args, format);
  char buf[1024] = {0};
  vsprintf(buf, format, args);
  va_end(args);
  console_put_str(buf);
}

```

此后我们正式开始编写硬盘相关结构,首先定义和硬盘相关的数据结构，构建文件device/ide.h
```
#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H
#include "stdint.h"
#include "list.h"
#include "bitmap.h"
#include "sync.h"

/* 分区结构 */
struct partition {
  uint32_t start_lba;           //起始扇区
  uint32_t sec_cnt;             //扇区数
  struct disk* my_disk;         //分区所属硬盘
  struct list_elem part_tag;    //用于队列的标记
  char name[8];                 //分区名称
  struct super_block* sb;       //本分区的超级块
  struct bitmap block_bitmap;   //块位图
  struct bitmap inode_bitmap;   //i节点位图
  struct list open_inodes;      //本分区打开的i结点队列
};

/* 硬盘结构 */
struct disk{
  char name[8];         //本硬盘的名称
  struct ide_channel* my_channel;   //此块硬盘归属于哪个ide通道
  uint8_t dev_no;       //本硬盘是主0,还是从1
  struct partition prim_parts[4];
  struct partition logic_parts[8];  //逻辑分区数量无限，但我们这里限制了上限8
};

/* ata通道结构 */
struct ide_channel{
  char name[8];                 //本ata通道的名称
  uint16_t port_base;           //本通道的起始端口号
  uint8_t irq_no;               //本通道所用的中断号
  struct lock lock;             //通道锁
  bool expection_intr;          //表示等待硬盘的中断
  struct semaphore disk_done;   //用于阻塞、唤醒驱动程序
  struct disk devices[2];       //一个通道上的主从两个硬盘
};
#endif

```

上面我的注释已经十分清楚了，有部分的结构我们目前还没有定义，我们之后再进行讲解,然后我们来初始化咱们的通道：
```
#include "ide.h"
#include "stdint.h"
#include "debug.h"
#include "stdio-kernel.h"
#include "stdio.h"
#include "sync.h"

/* 定义硬盘各寄存器的端口号 */
#define reg_data(channel)   (channel->port_base + 0)
#define reg_error(channel)  (channel->port_base + 1)
#define reg_sect_cnt(channel)  (channel->port_base + 2)
#define reg_lba_l(channel)  (channel->port_base + 3)
#define reg_lba_m(channel)  (channel->port_base + 4)
#define reg_lba_h(channel)  (channel->port_base + 5)
#define reg_dev(channel)  (channel->port_base + 6)
#define reg_status(channel)  (channel->port_base + 7)
#define reg_cmd(channle)    (reg_status(channel))
#define reg_alt_status(channel)     (channel->port_base + 0x206)
#define reg_ctl(channel)    reg_alt_status(channel)

/* reg_alt_status寄存器的一些关键位 */
#define BIT_ALT_STAT_BSY    0x80    //硬盘忙
#define BIT_ALT_STAT_DRDY   0x40    //驱动器准备好
#define BIT_ALT_STAT_DRQ    0x8     //数据传输准备好了

/* device寄存器的一些关键位 */
#define BIT_DEV_MBS     0xa0
#define BIT_DEV_LBA     0x40
#define BIT_DEV_DEV     0x10        //表示主盘

/* 一些硬盘操作指令 */
#define CMD_IDENTIFY    0xec    //identify指令
#define CMD_READ_SECTOR 0x20    //读扇区指令
#define CMD_WRITE_SECTOR 0x30   //写扇区指令

/* 定义可读写的最大扇区数 */
#define max_lba ((80*1024*1024/512) - 1)    //支持80MB

uint8_t channel_cnt;        //按硬盘数计算的通道数
struct ide_channel channels[2];     //有两个ide通道

/* 硬盘数据结构初始化 */
void ide_init(){
  printk("ide_init start\n");
  uint8_t hd_cnt = *((uint8_t*)(0x475));    //获取硬盘数量，这里的地址是固定的
  ASSERT(hd_cnt > 0);
  channel_cnt = DIV_ROUND_UP(hd_cnt, 2);    //一个通道有两个硬盘，这里我们通过硬盘数量反推通道数
  struct ide_channel* channel;
  uint8_t channel_no = 0;

  /* 处理每个通道上的硬盘 */
  while(channel_no < channel_cnt){
    channel = &channels[channel_no];
    sprintf(channel->name, "ide%d", channel_no);
    /* 为每个ide通道初始化端口基地址以及中断向量 */
    switch(channel_no){
      case 0:
        channel->port_base = 0x1f0;     //ide0通道的起始端口号是0x1f0
        channel->irq_no = 0x20 + 14;    //8259A中的IRQ14
        break;
      case 1:
        channel->port_base = 0x170;
        channel->irq_no = 0x20 + 15;     //最后一个引脚，用来相应ide1通道上的中断
        break;
    }
    channel->expection_intr = false;    //未向硬盘写入指令时不期待硬盘的中断
    lock_init(&channel->lock);
    /* 初始化为0,目的是向硬盘控制器请求数据后，
     * 因盘驱动sema_down此信号会阻塞线程，
     * 直到硬盘完成后通过发中断，由中断处理程序将此信号sema_up，唤醒线程*/
    sema_init(&channel->disk_done, 0);
    channel_no++;
  }
  printk("ide_init_done\n");
}

```

这里如果大家忘了可以翻看我之前写的硬盘那一章

### 2.实现thread_yield和idle线程
thread_yield函数的功能是主动把CPU使用权给让出来，他与thread_block的区别是，thread_yield执行后的任务状态是TASK_READY,知道这一点我们就到thread.c中实现
```
struct task_struct* idle_thread;

/* 实现任务调度 */
void schedule(){
  ASSERT(intr_get_status() == INTR_OFF);
  struct task_struct* cur = running_thread();
  if(cur->status == TASK_RUNNING){
    //这里若是从运行态调度，则是其时间片到了的正常切换，因此将其改变为就绪态
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag);
    cur->ticks = cur->priority;
    //重新将当前线程的ticks再重置为其priority
    cur->status = TASK_READY;
  }else{
    /* 说明可能是阻塞自己 */
  }

  thread_tag = NULL;    //将thread_tag清空
  /* 如果就绪队列当中没有可以运行的任务，就唤醒idle */
  if(list_empty(&thread_ready_list)){
    thread_unblock(idle_thread);
  }
  /* 将thread_ready_list队列中的地一个就绪线程弹出，准备将他调入CPU运行*/
  thread_tag = list_pop(&thread_ready_list);
  struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
  next->status = TASK_RUNNING;

  /* 激活任务页表等 */
  process_activate(next);

  switch_to(cur, next);
}

/* 主动让出cpu，换其他线程运行 */
void thread_yield(void){
  struct task_struct* cur_thread = running_thread();
  enum intr_status old_status = intr_disable();
  ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
  list_append(&thread_ready_list, &cur->general_tag);
  cur->status = TASK_READY;
  schedule();
  intr_set_status(old_status);
}

/* 初始化线程环境 */
void thread_init(void){
  put_str("thread_init start\n");
  list_init(&thread_ready_list);
  list_init(&thread_all_list);
  lock_init(&pid_lock);
  /* 将当前main函数创建为线程 */
  make_main_thread();

  /* 创建idle线程 */
  idle_thread = thread_start("idle", 10, idle, NULL);
  put_str("thread_init done\n");
}


```

这里的idle线程就是一个闲逛线程，也就是当此时没有需要运行的线程的时候，会使用hlt指令让处理器停止执行指令，真正让CPU休息.

### 3.实现休眠函数
当硬盘处理CPU请求的时候往往会消耗大量时间，此时为了避免浪费CPU资源，我们可以在等待硬盘操作的情况下把CPU主动让出来，所以我们在timer.c中定义休眠函数。
```
#define mil_seconds_per_intr (1000/IRQ0_FREQUENCY)  //10毫秒1次时钟中断
/* 以tick为单位的sleep，任何时间形式的sleep都会转换此ticks形式 */
static void ticks_to_sleep(uint32_t sleep_ticks){
  uint32_t start_tick = ticks;
  /* 若间隔的ticks数不够便让出CPU */
  while(ticks - start_tick < sleep_ticks){
    thread_yield();
  }
}

/* 以毫秒为单位的sleep */
void mtime_sleep(uint32_t m_seconds){
  uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
  ASSERT(sleep_ticks > 0);
  ticks_to_sleep(sleep_ticks);
}

```

这里也就是简单的计算出时钟中断周期，然后按照这个周期时间进行休眠了

### 4.完善硬盘驱动程序
我们首先来写一些比较常规的端口读取代码，这里详情请参考之前写硬盘的代码，只不过从当时的汇编到现在的c了，如下添加到device/ide.c当中
```
/* 选择读写的硬盘 */
static void select_disk(struct disk* hd){
  uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
  if(hd->dev_no == 1){      //若是从盘就置DEV位为1
    reg_device |= BIT_DEV_DEV;
  }
  outb(reg_dev(hd->my_channel), reg_device);
}

/* 向硬盘控制器写入起始扇区地址及要读写的扇区数 */
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt){
  ASSERT(lba <= max_lba);
  struct ide_channel* channel = hd->my_channel;
  /* 写入要读写的扇区数 */
  outb(reg_sect_cnt(channel), sec_cnt);     //如果sec_cnt为0,则表示写入256个扇区

  /* 写入lba地址，也就是扇区号 */
  outb(reg_lba_l(channel), lba);
  outb(reg_lba_m(channel), lba >> 8);
  outb(reg_lba_h(channel), lba >> 16);

  /* 这里还有4位是写到device寄存器的 */
  outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24 );
}

/* 向通道发出命令cmd */
static void cmd_out(struct ide_channel* channel, uint8_t cmd){
  /* 只要向硬盘发出了命令便将此标记为true，硬盘中断处理程序需要根据他来判断 */
  channel->expection_intr = true;
  outb(reg_cmd(channel), cmd);
}

/* 硬盘读入sec_cnt个扇区的数据到buf */
static void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt){
  uint32_t size_in_byte;
  if(sec_cnt == 0){
    /* 为0表示256 */
    size_in_byte = 256 * 512;
  }else{
    size_in_byte = sec_cnt * 512;
  }
  insw(reg_data(hd->my_channel), buf, size_in_byte/2);  //这里是写入字，所以除2
}

/* 将buf中sec_cnt扇区的数据写入磁盘 */
static void write2sector(struct disk* hd, void* buig, uint8_t sec_cnt){
  uint32_t size_in_byte;
  if(sec_cnt == 0){
    /* 为0表示256 */
    size_in_byte = 256 * 512;
  }else{
    size_in_byte = sec_cnt * 512;
  }
  outsw(reg_data(hd->my_channel), buf, size_in_byte/2);
}

/* 等待30秒 */
static bool busy_wait(struct disk* hd){
  struct ide_channel* channel = hd->my_channel;
  uint16_t time_limit = 30 * 1000;
  while(time_limit -= 10 >= 0){
    if(!(inb(reg_status(channel))& BIT_STAT_BSY)){      //如果bsy为0就表示不忙
      return (inb(reg_status(channel)) & BIT_STAT_DRQ);     //DRQ为1表示硬盘已经准备好了数据
    }else{
      mtime_sleep(10);
    }
  }
  return false;
}

```

上面都是一些功能性的函数，但他们还并没有进行使用，接下来我们就开始继续实现汇总的读写硬盘，并且实现硬盘中断函数且将其注册到中断程序表当中
```
/* 从硬盘读取sec_cnt个山区到buf */
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt){
  ASSERT(lba <= max_lba);
  ASSERT(sec_cnt > 0);
  lock_acquire(&hd->my_channel->lock);

  /* 1. 先选择操作的硬盘 */
  select_disk(hd);
  uint32_t secs_op;             //每次操作的扇区数
  uint32_t secs_done = 0;       //已完成的扇区数
  while(secs_done < sec_cnt){
    if((secs_done + 256) <= sec_cnt){       //由于读取端口是8位寄存器，所以最大一次读取256扇区
      secs_op = 256;
    }else{
      secs_op = sec_cnt - secs_done;
    }

    /* 2.写入待读入的扇区数和起始扇区号 */
    select_sector(hd, lba + secs_done, secs_op);

    /* 3.执行的命令写入reg_cmd寄存器 */
    cmd_out(hd->my_channel, CMD_READ_SECTOR);   //准备开始读数据

    /***************** 阻塞自己的时机 ****************
     * 在硬盘已经开始工作后才能阻塞自己，
     * 现在已经开始工作了，所以我们将自己阻塞，等待硬盘完成读操作后通过
     * 中断处理程序将自己唤醒 */
    sema_down(&hd->my_channel->disk_done);
    /*************************************************/
    /* 4.检测硬盘状态是否可读,醒来后执行下面代码 */
    if(!busy_wait(hd)){
      char error[64];
      sprintf(error, "%s read sector %d failed !!!!\n", hd->name, lba);
      PANIC(error);
    }

    /* 5.把数据从硬盘的缓冲区但中读出 */
    read_from_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);

    secs_done += secs_op;
  }
  lock_release(&hd->my_channel->lock);
}

/* 将buf中sec_cnt扇区数据写入硬盘 */
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt){
  ASSERT(lba <= max_lba);
  ASSERT(sec_cnt > 0);
  lock_acquire(&hd->my_channel->lock);

  /* 1. 先选择操作的硬盘 */
  select_disk(hd);
  uint32_t secs_op;             //每次操作的扇区数
  uint32_t secs_done = 0;       //已完成的扇区数
  while(secs_done < sec_cnt){
    if((secs_done + 256) <= sec_cnt){
      secs_op = 256;
    }else{
      secs_op = sec_cnt - secs_done;
    }

    /* 2.写入待读入的扇区数和起始扇区号 */
    select_sector(hd, lba + secs_done, secs_op);

    /* 3.执行的命令写入reg_cmd寄存器 */
    cmd_out(hd->my_channel, CMD_WRITE_SECTOR);   //准备开始读数据

    /* 4.检测硬盘状态是否可读,醒来后执行下面代码 */
    if(!busy_wait(hd)){
      char error[64];
      sprintf(error, "%s write sector %d failed !!!!\n", hd->name, lba);
      PANIC(error);
    }

    /* 5.把数据从硬盘的缓冲区但中读出 */
    write2sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);

    /* 在硬盘响应期间阻塞自己 */
    sema_down(&hd->my_channel->disk_done);
    secs_done += secs_op;
  }
  lock_release(&hd->my_channel->lock);

}

/* 硬盘中断程序 */
void intr_hd_handler(uint8_t irq_no){
  ASSERT(irq_no == 0x2e || irq_no == 0x2f);
  uint8_t ch_no = irq_no - 0x2e;    //查看是哪个通道
  struct ide_channel* channel = &channels[ch_no];
  ASSERT(channel->irq_no == irq_no);
  if(channel->expection_intr){      //这里若判断为true，则说明是我们自己设置的，是需要处理的中断
    channel->expection_intr = false;
    sema_up(&channel->disk_done);
    /* 读取状态寄存器使得硬盘控制器认为此次的中断已被处理，从而硬盘可以继续执行新的读写 */
    inb(reg_status(channel));
  }
}


/* 硬盘数据结构初始化 */
void ide_init(){
  printk("ide_init start\n");
  uint8_t hd_cnt = *((uint8_t*)(0x475));    //获取硬盘数量，这里的地址是固定的
  ASSERT(hd_cnt > 0);
  channel_cnt = DIV_ROUND_UP(hd_cnt, 2);    //一个通道有两个硬盘，这里我们通过硬盘数量反推通道数
  struct ide_channel* channel;
  uint8_t channel_no = 0;

  /* 处理每个通道上的硬盘 */
  while(channel_no < channel_cnt){
    channel = &channels[channel_no];
    sprintf(channel->name, "ide%d", channel_no);
    /* 为每个ide通道初始化端口基地址以及中断向量 */
    switch(channel_no){
      case 0:
        channel->port_base = 0x1f0;     //ide0通道的起始端口号是0x1f0
        channel->irq_no = 0x20 + 14;    //8259A中的IRQ14
        break;
      case 1:
        channel->port_base = 0x170;
        channel->irq_no = 0x20 + 15;     //最后一个引脚，用来相应ide1通道上的中断
        break;
    }
    channel->expection_intr = false;    //未向硬盘写入指令时不期待硬盘的中断
    lock_init(&channel->lock);
    /* 初始化为0,目的是向硬盘控制器请求数据后，
     * 因盘驱动sema_down此信号会阻塞线程，
     * 直到硬盘完成后通过发中断，由中断处理程序将此信号sema_up，唤醒线程*/
    sema_init(&channel->disk_done, 0);
    register_handler(channel->irq_no, intr_hd_handler);
    channel_no++;
  }

  printk("ide_init_done\n");
}

```

### 5.获取硬盘信息，扫描分区表
这里我们使用两个方案来验证咱们的硬盘驱动程序，第一是向硬盘发出identify命令获取硬盘信息，第二是扫描分区表
identify命令是0xec，用于获取硬盘参数，下面列出我们需要的参数
![](http://imgsrc.baidu.com/super/pic/item/0e2442a7d933c89533846ebd941373f08302007a.jpg)
由于涉及到分区的管理，所以咱们得给每个分区命个名，这里我们采用linux的方案,也就是[x]d[y][n],中括号中的值是可选的，如下：
+ x：表示硬盘分类，h代表IDE磁盘，s代表SCSI磁盘
+ d：表示disk
+ y：表示设备号，a是第一个硬盘，b是第2个硬盘，依次类推
+ n：表示分区号

这里我们先定义一些数据，也是在ide.c中定义，如下：
```

/* 用于记录总扩展分区的起始lba，初始为0,partition_scan时以此为标记 */
int32_t ext_lba_base = 0;
uint8_t p_no = 0, l_no = 0;     //用来记录硬盘主分区和逻辑分区的下标
struct list partition_list;     //分区队列

/* 构建一个16字节大小的结构体，用来存分区表项 */
struct partition_table_entry{
  uint8_t bootable;         //是否可引导
  uint8_t start_head;       //起始磁头
  uint8_t start_sec;        //起始扇区
  uint8_t start_chs;        //起始柱面
  uint8_t fs_type;          //分区类型
  uint8_t end_head;         //结束磁头
  uint8_t end_sec;          //结束扇区
  uint8_t end_chs;          //结束柱面
  /* 重点是下面两个，我们之前画图也是着重这里 */
  uint32_t start_lba;       //本分区起始扇区lba地址
  uint32_t sec_cnt;         //本分区的扇区数目
} __attribute__((packed));  //保证此结构是16字节大小

/* 引导扇区，mbr或者ebr所在的扇区 */
struct boot_sector{
  uint8_t other[446];       //引导代码
  struct partition_table_entry partition_table[4];  //分区表共4项，64字节
  uint16_t signature;       //魔数0x55,0xaa
} __attribute__((packed));

/* 将dst中len个相邻字节交换位置后存入buf */
static void swap_pairs_bytes(const char* dst, char* buf, uint32_t len){
  uint8_t idx;
  for(idx = 0; idx < len; idx += 2){
    buf[idx + 1] = *dst++;
    buf[idx] = *dst++;
  }
  buf[idx] = '\0';
}

/* 获得硬盘参数信息 */
static void identify_disk(struct disk* hd){
  char id_info[512];
  select_disk(hd);
  cmd_out(hd->my_channel, CMD_IDENTIFY);
  /* 阻塞自己，等待硬盘准备好数据再唤醒 */
  sema_down(&hd->my_channel->disk_done);

  /* 醒来后执行以下代码 */
  if(!busy_wait(hd)){   //若失败
    char error[64];
    sprintf(error, "%s identify failed !!!!\n", hd->name);
    PANIC(error);
  }
  read_from_sector(hd, id_info, 1);
  char buf[64];
  uint8_t sn_start = 10*2, sn_len = 20, md_start = 27*2, md_len = 40;
  swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
  printk("  disk %s info:\n     SN: %s\n",hd->name, buf);
  memset(buf, 0, sizeof(buf));
  swap_pairs_bytes(&id_info[md_start], buf, md_len);
  printk("     MODULE: %s\n", buf);
  uint32_t sectors = *(uint32_t*)&id_info[60*2];
  printk("     SECTORS: %d\n", sectors);
  printk("     CAPACITY: %dMB\n",sectors * 512 /1024/1024);
}

/* 扫描硬盘hd中地址为ext_lba的扇区中的所有分区 */
static void partition_scan(struct disk* hd, uint32_t ext_lba){
  struct boot_sector* bs = sys_malloc(sizeof(struct boot_sector));
  ide_read(hd, ext_lba, bs, 1);
  uint8_t part_idx = 0;
  struct partition_table_entry* p = bs->partition_table;

  /* 遍历分区表4个分区表项 */
  while(part_idx++ < 4){
    if(p->fs_type == 0x5){  //若为扩展分区
      if(ext_lba_base != 0){
        /* 子扩展分区的start_lba是相对于主引导扇区中的总扩展分区地址 */
        partition_scan(hd, p->start_lba + ext_lba_base);
      }else{
        /* ext_lba_base为0表示第一次读取引导块，也就是主引导记录所在的扇区 */
        /* 记录下扩展分区的起始lba地址，后面所有的扩展发扽去都相对于此 */
        ext_lba_base = p->start_lba;
        partition_scan(hd, p->start_lba);
      }
    }else if(p->fs_type != 0){  //如果是有效的分区类型
      if(ext_lba == 0){         //主分区
        hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
        hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
        hd->prim_parts[p_no].my_disk = hd;
        list_append(&partition_list, &hd->prim_parts[p_no].part_tag);
        sprintf(hd->prim_parts[p_no].name, "%s%d", hd->name, p_no + 1);
        p_no++;
        ASSERT(p_no < 4);
      }else{
        hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
        hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
        hd->logic_parts[l_no].my_disk = hd;
        list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
        sprintf(hd->logic_parts[l_no].name, "%s%d", hd->name, l_no + 5);
        l_no++;
        if(l_no >= 8){  //咱们这里限制了只支持8个
          return;
        }
      }
    }
    p++;
  }
  sys_free(bs);
}

/* 打印分区信息 */
static bool partition_info(struct list_elem* pelem, int arg UNUSED){
  struct partition* part = elem2entry(struct partition, part_tag, pelem);
  printk("      %s start_lba:0x%x, sec_cnt:0x%x\n",part->name, part->start_lba, part->sec_cnt);
  /* 此处的返回与函数本身无关，只是为了让主调函数继续向下遍历 */
  return false;
}

/* 硬盘数据结构初始化 */
void ide_init(){
  printk("ide_init start\n");
  uint8_t hd_cnt = *((uint8_t*)(0x475));    //获取硬盘数量，这里的地址是固定的
  ASSERT(hd_cnt > 0);
  list_init(&partition_list);
  channel_cnt = DIV_ROUND_UP(hd_cnt, 2);    //一个通道有两个硬盘，这里我们通过硬盘数量反推通道数
  struct ide_channel* channel;
  uint8_t channel_no, dev_no = 0;

  /* 处理每个通道上的硬盘 */
  while(channel_no < channel_cnt){
    channel = &channels[channel_no];
    sprintf(channel->name, "ide%d", channel_no);
    /* 为每个ide通道初始化端口基地址以及中断向量 */
    switch(channel_no){
      case 0:
        channel->port_base = 0x1f0;     //ide0通道的起始端口号是0x1f0
        channel->irq_no = 0x20 + 14;    //8259A中的IRQ14
        break;
      case 1:
        channel->port_base = 0x170;
        channel->irq_no = 0x20 + 15;     //最后一个引脚，用来相应ide1通道上的中断
        break;
    }
    channel->expection_intr = false;    //未向硬盘写入指令时不期待硬盘的中断
    lock_init(&channel->lock);
    /* 初始化为0,目的是向硬盘控制器请求数据后，
     * 因盘驱动sema_down此信号会阻塞线程，
     * 直到硬盘完成后通过发中断，由中断处理程序将此信号sema_up，唤醒线程*/
    sema_init(&channel->disk_done, 0);
    register_handler(channel->irq_no, intr_hd_handler);
    
    /* 分别获取两个个硬盘的参数及分区信息 */
    while(dev_no < 2){
      struct disk* hd = &channel->devices[dev_no];
      hd->my_channel = channel;
      hd->dev_no = dev_no;
      sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
      identify_disk(hd);    //获取硬盘参数
      if(dev_no != 0){      //内核本身的裸盘hd60M.img不做处理
        partition_scan(hd, 0);     //扫描该硬盘的分区
      }
      p_no = 0, l_no = 0;
      dev_no++;
    }
    dev_no = 0;         //为初始化下一个channel做准备
    channel_no++;
  }
  printk("\n    all partition info\n");
  /* 打印所有分区信息 */
  list_traversal(&partition_list, partition_info, (int)NULL);
  printk("ide_init_done\n");
}

```

代码比较长，但是注释十分详细，这里我们只有一个通道，且该通道上面的主盘是咱们的hd60M.img是个裸盘，只用来存放内核程序，不用实现文件系统，所已并不需要像我们这样分区，因此只需要修改咱们的从盘hd80M.img即可。这里我们初始化后看看效果
![](http://imgsrc.baidu.com/super/pic/item/0824ab18972bd407e201233b3e899e510eb30936.jpg)
我们发现十分完美的打印出来了咱们目前的磁盘信息。

## 0x02 总结
本次实现我们都是为了下一章的文件系统做铺垫，其中涉及到很多硬盘的相关知识，这里要是看着有点吃力可能是前面磁盘的部分忘记了，建议大家再去看看前面讲解磁盘的文章再继续观看。
