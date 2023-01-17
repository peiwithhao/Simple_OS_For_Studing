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
![](http://imgsrc.baidu.com/super/pic/item/48540923dd54564e5e5b43fcf6de9c82d1584f3d.jpg)
可以看到确实实现了断言，打印了我们的文件名以及函数名还有行数等

## 0x01 实现字符串操作
上一部分我们编写main函数的时候发现，对于字符串的操作我们为0,这样实在是很不方便操作，于是本次我们来完善一下我们的字符串操作,我们在lib目录下实现我们的string.c，用来处理各种字符串操作
由于这里我们会使用很多C语言的同名函数，所以这里我们在编译的时候会添加 -fno-builtin参数，这里就是防止编译器报错。
下面直接上代码，因为这里没啥好说的，逻辑在下面注释也会说点。
```
#ifndef __LIB_STRING_H
#define __LIB_STRING_H
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
![](http://imgsrc.baidu.com/super/pic/item/d52a2834349b033b213a5f1f50ce36d3d439bd41.jpg)
可以看到在ASSERT之前我们成功拼接了hello 和 world字符串，这说明咱们的努力没有白费。

## 0x02 位图bitmap的实现
这里位图相信大家有点印象，我记得我在讲解特权级知识点的时候说过IO位图，他一般放在TSS的头顶上，这里我们实现位图是因为要进行资源管理。
而这里的位图也是一样的，他实际上就是一串二进制bit位，他的每一位都有两种状态那就是0和1,这里就拿分页来举例子，我们的位图一个bit就代表一个页，若这个页被分配出去，那我们就将该bit位置1,否额为0。
所以位图就是通过这样的方法来管理内存资源的。如下图所示:
![](http://imgsrc.baidu.com/super/pic/item/9f2f070828381f30d6872e6cec014c086f06f05d.jpg)

基础知识讲完，我们现在立即来实现，讲究的就是一个效率;
这里首先给出定义在kernel目录下的bitmap.h，这里先给出头文件是让大家先看看位图的数据结构
```
#ifndef __KERNEL_BITMAP_H
#define __KERNEL_BITMAP_H
#include "global.h"
#define BITMAP_MASK 1
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

可以看到位图就是类似于一个字符串而已，只不过他的粒度更细。
