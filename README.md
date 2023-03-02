## 0x00 实现断言
今天比较特殊，直接进入正题，基础知识在写代码的时候边讲边写。首先大伙有的看过linux C库源码的可能会了解到，基本上哪儿都看的到assert()这个函数，但是每次看源码的时候我们基本就没管，因为这个跟我们的函数主体含义没啥关联，但是这里为啥要插个这样一个函数呢，原因在于在我们写程序的过程中，越往后写出错的概率可能就会越大，所以我们在这里写一个类似哨兵的函数，使用他来监督数据的正确性。

在咱们的系统中，我们总共需要实现两种断言，一种是内核系统使用的ASSERT，还有一种是用户进程使用的assert。由于我们目前还在实现内核的阶段，所以我们只实现内核断言即可。
这里我们需要知道两个前提：那就是一旦内核出现错误，那基本上都是严重错误，运行下去也没什么必要了。其二是我们在打印我们的错误的途中我们不希望有其他进程来干扰我们，所以我们还要实现一个手动开关中断的过程。
### 1.获取中断状态
这里我们继续修改上一节的interrupt.c即可。
```
#define EFLAGS_IF 0x00000200        //eflags寄存器中的if位为1
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; pop %0" : "=g" : (EFLAG_VAR))   //pushfl是指将eflags寄存器值压入栈顶


/* 开中断并返回开中断前的状态 */
enum intr_status intr_enable(){
  enum intr_status old_status;
  if (INTR_ON == intr_get_status()){
    old_status = INTR_ON;
    return old_status;
  }else{
    old_status = INTR_OFF;
    asm volatile("sti")     //开中断，sti指令将IF位置为1
    return old_status;
  }
}

/* 关中断并返回关中断前的状态 */
enum intr_status intr_disable(){
  enum intr_status old_status;
  if(INTR_ON == intr_get_status()){
    old_status = INTR_OFF;
    asm volatile("cli" : : : "memory"); //关中断，cli指令将IF位置0
    return old_status;
  }else{
    old_status = INTR_OFF;
    return old_status;
  }
}

/* 将中断状态设置为status */
enum intr_status intr_set_status(enum intr_status status){
  return status & INTR_ON ? intr_enable() : intr_disable();
}

/* 获取当前中断状态 */
enum intr_status intr_get_status(){
  uint32_t eflags = 0;
  GET_EFLAGS(eflags);
  return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF ;
}

```

上面逻辑也十分简单，也就是修改eflags的值而已，这里我们使用enum关键字来定义我们的中断状态，这是因为就两种状态，且用0或1表示不太直观，因此采用enum，大家可以自行了解该关键字，个人认为有点像键值对了。也仅仅是有点。
当然这里也需要改一下我们的头文件interrupt.h
```
#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
typedef void* intr_handler;
void idt_init(void);    //初始化idt描述表

/* 定义两种中断的状态
 * INTR_OFF值为0表示关中断
 * INTR_ON值为1表示开中断
 */
enum intr_status { //中断状态
  INTR_OFF,         //关中断
  INTR_ON           //开中断
};

enum intr_status intr_get_status(void);
enum intr_status intr_set_status(enum intr_status);
enum intr_status intr_enable(void);
enum intr_status intr_disable(void);

#endif

```

### 2.ASSERT
刚刚我们仅仅是实现了开关中断以及获取状态，这里我们开始实现assert，我们的assert是用来辅助程序调试的，通常用在开发阶段，我们实现的ASSERT就是一个“哨兵”，我们将程序运行到此该存在的状态传递给他，让他帮我们监督程序，若程序运行到这儿不符合我们之前构建好的状态，那么他就会报错，我们这里利用上一节写的关闭中断函数intr_disable()来实现。
首先我们在kernel目录下定义一个debug.h文件
```
#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H
void panic_spin(char* filename, int line, const char* func, const char* condition);

/***************** __VA_ARGS__  *******************
 * __VA_ARGS__是预处理器支持的专用标识符
 * 代表所有与省略号想对应的参数
 * "..."表示定义的宏参数可变 */
#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)    //表示把panic函数定义为宏，这里里面的__FILE__等宏是gcc自动解析的
/**************************************************/

#ifdef NDEBUG
  #define ASSERT(CONDITION) ((void)0)   //这里若定义了NDEBUG，则ASSERT宏会等于空值，也就是取消这个宏
#else                                   //否则就说明是调试模式，所以下面才是真正的定义断言
#define ASSERT(CONDITION) \
if(CONDITION){}else{    \               //如果条件满足，则什么也不做，直接过
  /* 符号#让编译器将宏的参数转化为字符串面量 */ \       //但是若条件不满足，则悬停程序
  PANIC(#CONDITION); \                  //调用panic_spin(),这里是可变参数
}

#endif /*__NDEBUG*/
#endif /*__KERNEL_DEBUG_H*/
```
上面的注释十分清楚，这里就是定义了一个ASSERT宏函数，然后我们继续定义debug.c,代码如下：
```
#include "debug.h"
#include "print.h"
#include "interrupt.h"

/* 打印文件名、行号、函数名、条件并使程序悬停 */
void panic_spin(char* filename, int line, const char* func, const char* condition){
  intr_disable();                   //这里先关中断，免得别的中断打扰
  put_str("\n\n\n!!! error !!!\n");
  put_str("filename:");
  put_str(filename);
  put_str("\n");
  put_str("line:0x");
  put_int(line);
  put_str("\n");
  put_str("condition:");
  put_str(condition);
  put_str("\n");
  while(1);
}
```
可以看到我们之前定义的panic_spin也就仅仅打印这几个条件以及行号而已，然后就是while循环。之后我们到main函数中使用一下，我们修改main.c如下:
```
#include "print.h"
#include "init.h"
#include "debug.h"
int main(void){
  put_str("I am Kernel\n");
  init_all();
  ASSERT(1==2);
  while(1); 
  return 0;
}

```

这里由于我们定义断言ASSERT(1==2),这样程序运行到此就会检查1是否等于2,发现不等，则他认为这里出了错误。
之后我们继续编译链接，大家是否还记得上一个文章我说过用makefile，确实以后我们的编译链接环节越来越多，这里我们必须得整理一下，不然老是一个一个编译实在是太麻烦，下面就是本次的makefile脚本文件，如下：
这里大家可以去CSDN还是哪儿都可以搜到makefile的用法，还是以前那意思，CSDN虽然说有的回答解决不了问题，但是他确实能解决大部分，所以大伙别听别人说CSDN啦跨就跟着说他啦跨，里面也是有很多精彩博文的。
下面是makefile文件：
```
BUILD_DIR = ./build
ENTRY_POINT = 0xc0001500
AS = nasm
CC = gcc
LD = ld
LIB = -I lib/ -I lib/kernel/ -I lib/user/ -I kernel/ -I device/
ASFLAGS = -f elf
CFLAGS = -Wall $(LIB) -c -fno-builtin -no-pie -fno-pic -m32 -fno-stack-protector -W -Wstrict-prototypes \
				 -Wmissing-prototypes
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o  \
			 $(BUILD_DIR)/print.o $(BUILD_DIR)/debug.o 

############### C代码编译 #################
$(BUILD_DIR)/main.o : kernel/main.c lib/kernel/print.h \
	lib/stdint.h kernel/interrupt.h kernel/init.h
	$(CC) $(CFLAGS) $< -o $@ 						#$<表示依赖的地一个文件，$@表示目标文件集合

$(BUILD_DIR)/init.o : kernel/init.c kernel/init.h lib/kernel/print.h \
	lib/stdint.h kernel/interrupt.h device/timer.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o : kernel/interrupt.c kernel/interrupt.h \
	lib/stdint.h kernel/global.h lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o : device/timer.c device/timer.h lib/stdint.h \
	lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o : kernel/debug.c kernel/debug.h \
	lib/kernel/print.h lib/stdint.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

############### 汇编代码编译 ##################
$(BUILD_DIR)/kernel.o : kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/print.o : lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@

############### 链接所有目标文件 ###############3
$(BUILD_DIR)/kernel.bin : $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@ 	#这里$^代表所有依赖文件

.PHONY : mk_dir hd clean all    			#定义伪目标

mk_dir:
	if [[ ! -d $(BUILD_DIR) ]];then mkdir $(BUILD_DIR);fi 	#若没有这个目录，则创建

hd:
	dd if=$(BUILD_DIR)/kernel.bin \
		of=/home/dawn/repos/OS_learning/bochs/hd60M.img \
		bs=512 count=200 seek=9 conv=notrunc

clean:
	cd $(BUILD) && rm -f ./*

build : $(BUILD_DIR)/kernel.bin

all : mk_dir build hd

```

然后我们直接使用make all 命令来进行编译，接着我们直接打开bochs看看效果：
![](http://imgsrc.baidu.com/forum/pic/item/48540923dd54564e5e5b43fcf6de9c82d1584f3d.jpg)
可以看到确实实现了断言，打印了我们的文件名以及函数名还有行数等

## 0x01 实现字符串操作
上一部分我们编写main函数的时候发现，对于字符串的操作我们为0,这样实在是很不方便操作，于是本次我们来完善一下我们的字符串操作,我们在lib目录下实现我们的string.c，用来处理各种字符串操作
由于这里我们会使用很多C语言的同名函数，所以这里我们在编译的时候会添加 -fno-builtin参数，这里就是防止编译器报错。
下面直接上代码，因为这里没啥好说的，逻辑在下面注释也会说点。
```
#ifndef __LIB_STRING_H
#define __LIB_STRING_H
#define NULL 0
#include "stdint.h"
void memset(void* dst_, uint8_t value, uint32_t size); //单字节复制，指定目的地
void memcpy(void* dst_, void* src_, uint32_t size);     //多字节复制，指定目的地
int memcmp(const void* a_, const void* b_, uint32_t size);  //比较多个字节
char* strcpy(char* dst_, const char* src_);     //复制字符串
uint32_t strlen(const char* str);               //获取字符串长度
int8_t strcmp(const char* a_, const char* b_);  //比较两个字符串
char* strchr(const char* str, const uint8_t ch);    //查找字符地址
char* strrchr(cosnt char* str, const uint8_t ch);   //反向查找字符地址
char* strcat(char* dst_, const char* src_);         //连接字符串
uint32_t strchrs(const char* str, uint8_t ch);      //计算相同字符数量

#endif

```
这里给出的是lib目录下的string.h，这里没给出string.c是因为就是普通的业务代码，写在这儿纯属占位置了没意义，大家可以按照string.h的函数声明先自己实现一下，完整源码到github上面烤吧，就在文末。
当我们编写完毕之后就去咱们的main.c里面试一试我们的函数，当然这里记得在先前的makefile里面添加依赖，如果上面makefile大伙跟着打一遍就能知道其中的含义了，当然这也是建立在你花20分钟去了解了makefile语法。
这里我选择拿strcat作为测试，我在main函数中定义两个字符传分别是hello 和 world，然后拼接，效果如下：
![](http://imgsrc.baidu.com/forum/pic/item/d52a2834349b033b213a5f1f50ce36d3d439bd41.jpg)
可以看到在ASSERT之前我们成功拼接了hello 和 world字符串，这说明咱们的努力没有白费。

## 0x02 位图bitmap的实现
这里位图相信大家有点印象，我记得我在讲解特权级知识点的时候说过IO位图，他一般放在TSS的头顶上，这里我们实现位图是因为要进行资源管理。
而这里的位图也是一样的，他实际上就是一串二进制bit位，他的每一位都有两种状态那就是0和1,这里就拿分页来举例子，我们的位图一个bit就代表一个页，若这个页被分配出去，那我们就将该bit位置1,否额为0。
所以位图就是通过这样的方法来管理内存资源的。如下图所示:
![](http://imgsrc.baidu.com/forum/pic/item/9f2f070828381f30d6872e6cec014c086f06f05d.jpg)

基础知识讲完，我们现在立即来实现，讲究的就是一个效率;
这里首先给出定义在kernel目录下的bitmap.h，这里先给出头文件是让大家先看看位图的数据结构
```
#ifndef __KERNEL_BITMAP_H
#define __KERNEL_BITMAP_H
#include "global.h"
#define BITMAP_MASK 1
typedef int bool;
struct bitmap {
  uint32_t btmp_bytes_len;      //位图的字节长度
  /* 在遍历位图的时候，整体以字节为单位，细节上是以位为单位，因此这里的指针为单字节 */
  uint8_t* bits;                //位图的指针
};

void bitmap_init(struct bitmap* btmp);
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);

#endif

```

可以看到位图就是类似于一个字符串而已，只不过他的粒度更细。下面给出具体实现代码,他同bitmap.h一样存放在kernel目录下：
```
#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

/* 将位图初始化 */
void bitmap_init(struct bitmap* btmp){
  memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

/* 判断bit_idx位是否为1,若为1,则返回true，否则返回false */
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx){
  uint32_t byte_idx = bit_idx/8;                //这里是求所在位所处的字节偏移
  uint32_t bit_odd = bit_idx%8;                //这里是求在字节中的位偏移
  return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd));     //与1进行与操作，查看是否为1
}

/* 在位图中申请连续cnt个位，成功则返回其起始位下标，否则返回-1 */
int bitmap_scan(struct bitmap* btmp, uint32_t cnt){
  uint32_t idx_byte = 0;                    //记录空闲位所在的字节
  /* 逐字节比较，蛮力法 */
  while((0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len)){
    /* 1表示已经分配，若为0xff则说明该字节内已经无空闲位，到下一字节再找 */
    idx_byte++;
  }

  ASSERT(idx_byte < btmp->btmp_bytes_len);
  if(idx_byte == btmp->btmp_bytes_len){     //这里若字节等于长度的话，那说明没有了剩余空间了
    return -1;
  }

  /* 这里若找到了空闲位，则在该字节内逐位比对，返回空闲位的索引 */
  int idx_bit = 0;
  /* 同btmp->bits[idx_byte]这个字节逐位对比 */
  while((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]){ //注意这里&是按位与，所有位都为0才返回0,跳出循环
    idx_bit++;
  }

  int bit_idx_start = idx_byte * 8 + idx_bit;       //这里就是空闲位在位图中的坐标
  if(cnt == 1){
    return bit_idx_start;           //若咱们只申请数量为1
  }

  uint32_t bit_left = (btmp->btmp_bytes_len*8 - bit_idx_start);     //记录还剩下多少个位
  uint32_t next_bit = bit_idx_start + 1;
  uint32_t count = 1;               //用来记录找到空闲位的个数

  bit_idx_start = -1;               //将其置-1,若找不到连续的位就返回
  while(bit_left-- >0){
    if(!(bitmap_scan_test(btmp,next_bit))){
      count++;
    }else{
      count = 0;
    }
    if(count == cnt){
      bit_idx_start = next_bit - cnt + 1 ;
      break;
    }
    next_bit ++ ;
  }
  return bit_idx_start;
}

/* 将位图btmp的bit_idx位设置为value */
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value){
  ASSERT((value == 0) || (value == 1));
  uint32_t byte_idx = bit_idx / 8;          //这俩同上
  uint32_t bit_odd = bit_idx % 8;
  /* 这里进行移位再进行操作 */
  if(value){                                //value为1
    btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
  }else{                                    //value为0
    btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
  }
}
```

## 0x03 内存管理系统
终于到了今天的重点，我们之前的一切实践都是为了这一项做铺垫，这里我们先介绍几点基础知识。
### 1.基础知识
今天的知识点转移阵地辣，首先我们需要做的工作首先是规划一下咱们的内存池，这个内存池大家可以看作一个资源库，什么资源呢，就是咱们的内存，好像说了点废话。
在前面咱们实现了分页机制，所以现在地址分为了虚拟地址和物理地址，为了有效分配他们，所以这里我们需要实现虚拟内存地址池和物理内存地址池。

---
首先咱们来讨论物理内存地址池。这里我们知道咱们的程序和内核都是运行在物理内存之中的，因此我们再次划分，将物理内存地址池再划分为用户物理内存池和内核物理内存池
![](http://imgsrc.baidu.com/forum/pic/item/b21c8701a18b87d6129205da420828381e30fd27.jpg)
为了图方便，我们将内存池的划分对半砍，如上图
咱们接着来说虚拟内存地址池，这里我们都知道。用户程序的地址是在链接过程就已经定下来了，而由于使用的是虚拟地址，也就是说不同进程之间的地址选择互不干涉，但程序在运行的过程中会有着动态内存的申请，比如说malloc，free等，这样我们就必须向程序返回一个虚拟内存块，但是如何知道哪些是空闲可以分配的呢，所以说这里我们也需要有虚拟内存地址池。
而虽然说内核程序完全可以自己随便找地方存，但是这样一定会存在些不可预料的错误，因此内核也需要通过内核管理系统申请内存，所以这里我们也需要有内核的虚拟地址池。当他们申请内存的时候，首先从虚拟地址池分配虚拟地址，再从物理地址池中分配物理内存，然后在内核将这两种地址建立好映射关系。
如下：
![](http://imgsrc.baidu.com/forum/pic/item/d058ccbf6c81800ad3eb20cbf43533fa838b47ed.jpg)

### 2.实现
然后这里给出对应的头文件，我们定义为kernel/memory.h
```
#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"

/* 虚拟地址池，用于虚拟地址管理 */
struct virtual_addr {
  struct bitmap vaddr_bitmap;
  uint32_t vaddr_start;
};

extern struct pool kernel_pool, user_pool;
void mem_init(void);
#endif
```

这里我们即将来实现咱们的内存管理，首先我们规划一下，还记得之前我们在loader.S中定义的栈地址吗,我们那时候将内核的栈地址定义在了虚拟地址0xc009f000,是这样的，我们这里先解释一个简单的知识点，是我们之后会遇到的PCB(程序控制块)，而我们将来所实现的PCB都必须要占用1页内存，也就是4KB大小的内存空间，因此PCB的内容首地址必须是0xXXXXX000,末尾地址必须是0xXXXXXfff.
这里我们再来简单解释一下PCB的结构（这里不做细说，因为本次的主题不是他，这里仅仅是为他预留空间），首先首地址向上是存放着一些进程或线程的信息，而高处0xXXXXXfff向下就是栈空间了，所以我们之前定义的栈首地址为0xc009f000就是为main主线程所预留的栈空间同时也是为了PCB所预留的空间，因此我们这里就将PCB的首地址定义为0xc009e000。
说完PCB，我们还需要讨论一下上面我们解释的位图，这里一个位图单位表示一页，因为我们目前分配的是512MB，可以知道我们一共分配有512MB/4KB = 128K页，所以共需要我们的位图大小为128K/8 = 16KB，所以我们需要16KB/4KB = 4个页才能完整存放咱们的位图。因此我们将位图放在咱们的PCB前面，也就是0xc009a000,这里离PCB有4个页，刚好够。
这里我们立即来实现，这里也就是一点初始化代码而已：
```
#include "memory.h"
#include "stdint.h"
#include "print.h"

#define PG_SIZE 4096    //4K
/************************ 位图地址 ****************************/
#define MEM_BITMAP_BASE 0xc009a000
/**************************************************************/

/* 0xc0000000是内核从虚拟地址3G开始
 * 而1MB指的是跨过低端1MB内存
 * */

/* 0xc0000000是内核从虚拟地址3G起
 * 0x100000是跨过低端1MB内存， 使虚拟地址在逻辑上连续*/
#define K_HEAP_START 0xc0100000         //设置堆起始地址用来进行动态分配

/* 内存池结构，生成两个实例用于管理内核内存池和用户内存池 */
struct pool{
  struct bitmap pool_bitmap;    //本内存池用到的位图结构，用于管理物理内存
  uint32_t phy_addr_start;      //本内存池的物理起始地址
  uint32_t pool_size;
};

struct pool kernel_pool, user_pool; //生成内核物理内存池和用户物理内存池
struct virtual_addr kernel_vaddr;   //此结构用来给内核分配虚拟地址

/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem){    //这里的all_mem传递的参数是总共的物理内存
  put_str("     mem_poool_init_start \n ");
  uint32_t page_table_size = PG_SIZE * 256;     //这里只计算769～1022是因为这一部分是属于内核进程
  //页表大小 = 1页的页目录表 + 第0项和第768个页目录项指向同一个页表 + 第769～1022个页目录项共指向254个页表，共256个页框
  uint32_t used_mem = page_table_size + 0x100000;   //0x100000为低端1MB内存
  uint32_t free_mem = all_mem - used_mem;
  uint16_t all_free_pages = free_mem/PG_SIZE;

  uint16_t kernel_free_pages = all_free_pages /2;
  uint16_t user_free_pages = all_free_pages - kernel_free_pages;
  /* 上面为了简化处理没有考虑余数，所以可能会丢失内存，但是这也不打紧，因为位图表示内存会小于物理内存 */
  uint32_t kbm_length = kernel_free_pages / 8;      //kernel_bitmap的长度，以字节为单位
  uint32_t ubm_length = user_free_pages / 8;        //user_bitmap的长度，以字节为单位

  uint32_t kp_start = used_mem;     //kernel_pool_start 内核物理内存池起始地址
  uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;   //user_pool_start 用户物理内存池起始地址

  kernel_pool.phy_addr_start = kp_start;
  user_pool.phy_addr_start = up_start;

  kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
  user_pool.pool_size = user_free_pages * PG_SIZE;

  kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
  user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

/******************* 内核内存池和用户内存池位图 *******************
 * 位图是全局的数据，长度不固定
 * 全局或静态的数组需要在编译时知道其长度
 * 而我们需要根据总内存大小算出需要多少字节，
 * 所以改为指定一块来生成位图
 * ****************************************************************/
//内核使用的最高地址是0xc009f000,这里是主线程的栈地址，这里咱们内核占了物理地址的低1MB，但是大概率用不了这么多
//咱们有512MB的内存，所以位图就需要4页
//所以内核内存池的位图定在MEM_BITMAP_BASE(0xc009a000)这里
  kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
  //用户内存池的位图就当跟屁虫辣
  user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

/*********************** 输出内存池信息 ***************************/
  put_str("         kernel_pool_bitmap_start:");
  put_int((int)kernel_pool.pool_bitmap.bits);
  put_str(" kernel_pool_phy_addr_start:");
  put_int((int)kernel_pool.phy_addr_start);
  put_str("\n");
  put_str("         user_pool_bitmap_start:");
  put_int((int)user_pool.pool_bitmap.bits);
  put_str(" user_pool_phy_addr_start");
  put_int((int)user_pool.phy_addr_start);
  put_str("\n");

  /* 将位图置为0 */
  bitmap_init(&kernel_pool.pool_bitmap);
  bitmap_init(&user_pool.pool_bitmap);

  /* 下面初始化内核虚拟地址的位图，按照实际物理内存大小生成数组 */
  kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;         //用于维护内核堆的虚拟地址，所以要和内核内存池大小一致

  /* 位图的数组指向一块未使用的内存
   * 目前定位在内核内存池和用户内存池之外*/
  kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
  kernel_vaddr.vaddr_start = K_HEAP_START;
  bitmap_init(&kernel_vaddr.vaddr_bitmap);
  put_str("     mem_pool_init done \n");
}

/* 内存管理部分初始化入口 */
void mem_init(){
  put_str("mem_init start\n");
  uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));     //这里是咱们之前loader.S中存放物理总内存的地址
  mem_pool_init(mem_bytes_total);
  put_str("mem_init done\n");
}


```
这里的初始化入口当然也是由咱们之前编写的kernel/init.c来进行调用啦，所以这里我们来看看执行情况
![](http://imgsrc.baidu.com/forum/pic/item/a044ad345982b2b75075a60174adcbef77099b09.jpg)
可以发现这里我们的内核物理内存池起始地址也确实是在咱们的页目录以及内核页表之后，也就是0x200000那儿，十分成功！！！
## 0x04 分配页内存
上一节咱们已经成功构建了位图，内存池且将他们初始化了，这里我们继续进行下一步工作，得使用他们了，这里我先给出memory.h改进的代码
```
#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"

/* 内存池标记，用于判断用哪个内存池，这里采用enum枚举 */
enum pool_flags{
  PF_KERNEL = 1,    //内核内存池
  PF_USER = 2       //用户内存池
};

#define PG_P_1 1    //页表项或页目录项存在属性位
#define PG_P_0 0    //页表项或页目录项存在属性位
#define PG_RW_R 0   //R/W属性位值，读/执行
#define PG_RW_R 2   //R/W属性位值，读/写/执行
#define PG_US_S 0   //U/S属性位值，系统级
#define PG_US_U 4   //U/S属性位值，用户级
/* 虚拟地址池，用于虚拟地址管理 */
struct virtual_addr {
  struct bitmap vaddr_bitmap;
  uint32_t vaddr_start;
};

extern struct pool kernel_pool, user_pool;
void mem_init(void);
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt);
void* get_kernel_pages(uint32_t pg_cnt);
#endif

```

可以看到我们是添加了一些定义和两个函数，这两个函数的具体实现我在这里简单说下，具体实现可以把源码下下来看看，里面注释十分详细，首先我们分配页表，我们需要做的事有三件：
1. 首先在虚拟内存池中申请虚拟地址，然后在虚拟内存池的位图中将他们置为1
2. 然后我们再物理内存池中申请一定量的物理页框，同样的也要在对应的物理内存池的位图中将其置为1
3. 然后我们再通过之前内存机制那篇学习到的知识访问到对应页目录项页目录表，然后修改其中的值以此来实现咱们的虚拟地址与物理地址的映射，这里我给出映射部分的代码，这里十分重要，请务必理解

```
/* 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射 */
static void page_table_add(void* _vaddr, void* _page_phyaddr){
  uint32_t vaddr = (uint32_t)_vaddr,page_phyaddr = (uint32_t)_page_phyaddr;
  uint32_t* pde = pde_ptr(vaddr);
  uint32_t* pte = pte_ptr(vaddr);

/******************************** 注意 **********************************
 * 执行*pte会访问到空的pde，所以确保pde创建完成后才能执行*pte,
 * 否则会引发page_fault。因此在*pde为0的时候，*pte只能出现在下面else语句块中的*pde后面
 * **********************************************************************/
  /* 先在页目录内判断目录项的P位，若为1则表示该表已经存在 */
  if(*pde & 0x00000001){
    //页目录项和页表项的第0位为p，这里是判断页目录项是否存在
    ASSERT(!(*pte & 0x00000001));   //这里若是说以前有已经装载的物理页框，则会报错
    if(!(*pte & 0x00000001)){
      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }else{
      PANIC("pte repeat");      //ASSERT的内置函数
      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
  }else{
    //页目录项不存在，所以需要先创建页目录再创建页表项
    /* 页表中的页框一律从内核空间分配 */
    uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
    *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    /* 分配到的物理页地址pde_phyaddr对应的物理内存清0,
     * 避免里面的旧数据变成页表项，从而让页表混乱
     * 访问到pde对应的物理地址，用pte取高20位即可
     * 因为pte基于该pde对应的物理地址内再寻址，
     * 把低12位置0便是该pde对应的物理页的起始 */
    memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
    ASSERT(!(*pte & 0x00000001));
    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
  }
}
```

这里的pte是对应的页表项地址，pde是对应的页目录项地址，我们平时说的映射其实也就是操作这个页表而已，其中若页目录项或页表项为空的时候我们必须要先创建页目录然后再创建页表来实现映射。
然后我们再到main.c里面实现
```
#include "print.h"
#include "init.h"
#include "debug.h"
#include "string.h"
#include "memory.h"

int main(void){
  put_str("I am Kernel\n");
  init_all();
  void* addr = get_kernel_pages(3);
  put_str("\n get kernel pages start vaddr is:");
  put_int((uint32_t)addr);
  put_str("\n");
  while(1);
  return 0;
}

```

这里我们是在main函数里面申请了三个虚拟页,然后我们打印一下我们的申请的虚拟页首地址，我们进入调试界面查看是否有对应的映射
![](http://imgsrc.baidu.com/forum/pic/item/a50f4bfbfbedab6402e7d605b236afc378311e59.jpg)
大家可以看到这里刚好就多了三个页大小的映射，这证明咱们已经正确的进行了映射，简直不要太激动，这里我们就已经实现了初步的内存管理了！！！


## 0x05 总结
今天基础知识很少，更多的是咱们实现代码，代码十分多但是很值得仔细看，如果大家没耐心可能看不下去，但是一旦看进去了就会荣会贯通，整体流程会理解的更加顺畅，这里的页表变换我建议大家同之前实现内存分页机制那章协同观看。


