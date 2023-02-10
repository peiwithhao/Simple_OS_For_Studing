## 0x00 基础知识们
### 1.inode节点
这里过多的基础知识我就不必讲了，上过操作系统课的大伙都知道在目前比较普遍的管理结构就是inode节点了，我们存放在磁盘上的一切文件都是使用inode结点来进行访问的，这里的文件同时包括普通文件与咱们的目录文件，没错在这里万物皆文件，并且所有文件都必须配备一个inode节点以供访问。
而inode如果是属于普通文件的话，那么他的具体结构如下：
![](http://imgsrc.baidu.com/super/pic/item/8ad4b31c8701a18b78d8f789db2f07082938fec2.jpg)
这里的结构十分经典，总共应该是有15个索引，前12个是直接索引，如果不够的话，那么第13个是一级间接索引，依次类推。
而如果说咱们的inode节点是属于目录文件，则其中的数据块索引指针指向的会是一系列目录项。这里目录项结构如下：
![](http://imgsrc.baidu.com/super/pic/item/f703738da9773912400660a7bd198618377ae2d1.jpg)
其中的inode编号的含义其实就是存放inode节点的数组中本inode节点的下标，这样方便存储和访问。
而inode节点中其他地方就存储一些属性相关的数据，整体可以用下面这个图来表示：
![](http://imgsrc.baidu.com/super/pic/item/8c1001e93901213fd0baab1a11e736d12e2e956f.jpg)

### 2.超级块与文件系统布局
这里我们来解释一个重要的概念，那就是超级块。
我们都知道每个文件都有一个inode，所有的inode都存放在inode数组中，但是这个inode数组都在哪儿呢？而一般每个分区都存在着自己的根目录，但是其中的地址并不唯一。所以为了正确访问这些根目录地址，我们必须要在一个固定的地址保存这些根目录的信息。而这些地址和信息被保存到的地方就是超级块，超级块是保存文件系统元信息的元信息。
咱们的文件系统是针对各个分区来管理的，inode代表文件，因为各分区都有自己的inode数组。而我们每个分区的inode数组长度是固定的，等于最大文件数。既然inode数量是有限的，必须要有一种管理inode使用情况的方法，因此我们用位图来管理inode的使用情况。
上面咱们说了一些文件系统所需要的元信息，剩下的就是空闲块了。我们为这些空闲块也准备一个空闲块位图。
这里咱们总结一下我们实现一个简单文件系统的元信息：
1. inode数组地址及大小
2. inode位图地址及大小
3. 根目录的地址和大小
4. 空闲块位图地址及大小

上述几类信息就在超级块中保存，因此一个简单的超级块结构如下：
![](http://imgsrc.baidu.com/super/pic/item/d058ccbf6c81800a8f226cc8f43533fa838b4735.jpg)
这里的魔数用来区别于其他文件系统的类型。
我们的超级块是用来存放文件系统的配置信息，所以说超级块就必须固定一个位置了，我们将他固定在个分区的第二个扇区，通常占用一个扇区的大小。
下面给出经典的ext2文件系统布局，我们也是仿照其来实现的：
![](http://imgsrc.baidu.com/super/pic/item/f7246b600c338744df81cbba140fd9f9d62aa0b4.jpg)

## 0x01 创建文件系统
### 1.创建超级块，inode，目录项
我们首先来将几个比较基础的数据结构给定义了，这几个上面我们都讲解的很清楚，首先定义超级块，创建文件fs/super_block.h:
```
#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H
#include "stdint.h"

/* 超级块 */
struct super_block{
  uint32_t magic;               //用来标识文件系统类型
  uint32_t sec_cnt;             //本分区总共的扇区数
  uint32_t inode_cnt;           //本分区的inode数量
  uint32_t part_lba_base;       //本分区的起始lba地址
  uint32_t block_bitmap_lba;    //块位图本身起始扇区地址
  uint32_t block_bitmap_sects;  //扇区位图本身占用的扇区数量
  uint32_t inode_bitmap_lba;    //i结点位图起始扇区lba地址
  uint32_t inode_bitmap_sects;  //i结点位图占用的扇区数量
  uint32_t inode_table_lba;     //i结点表起始扇区lba地址
  uint32_t inode_table_sects;   //i结点表占用的扇区数量
  uint32_t data_start_lba;      //数据区开始的地一个扇区号
  uint32_t root_inode_no;       //根目录所在的I结点号
  uint32_t dir_entry_size;      //目录项大小
  uint8_t pad[460];             //加上460字节凑够512字节，也就是1扇区的大小
} __attribute__((packed));

#endif

```

上述注释已经十分详细，具体使用我们在之后代码里面理解。
然后我们来写咱们的inode节点，创建文件fs/inode.h
```
#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "stdint.h"
#include "global.h"
#include "list.h"

/* inode结构 */
struct inode{
  uint32_t i_no;
  /* 当此inode指向普通文件的时候，i_size是文件大小，若是目录，i_size是指该目录下所有目录项大小之和 */
  uint32_t i_size;
  uint32_t i_open_cnts;         //记录文件被打开的次数
  bool write_deny;              //写文件不能并行，进程写文件的时候先检查此标识
  /* i_sectors[0-11]是直接索引，i_sectors[12]用来存储一级间接索引指针 */
  uint32_t i_sectors[13];
  struct list_elem inode_tag;
};

#endif

```

这里考虑到我们的迷你操作系统中文件不会太大，所以就12个直接索引和1个一级间接索引，而最后一个结构是链表节点，这里是为了让我们之后维护一个已打开inode队列，详细情况之后再讲，接下来实现目录项：
```
#ifndef __FS_DIR_H
#define __FS_DIR_H
#include "stdint.h"
#include "inode.h"
#define MAX_FILE_NAME_LEN   16  //最大文件名长度

/* 目录结构 */
struct dir{
  struct inode* inode;
  uint32_t dir_pos;         //记录在目录中的偏移
  uint8_t dir_buf[512];     //目录的数据缓存
};

/* 目录项结构 */
struct dir_entry{
  char filename[MAX_FILE_NAME_LEN];     //普通文件或者目录名称
  uint32_t i_no;                        //普通文件或目录对应的inode编号
  enmu file_types f_type;               //文件类型
};
#endif

```

这里的dir目录结构是只存在于内存当中的，其中的成员结构我们在使用的时候进行讲解，然后目录项中欧给你有一个文件类型，我们到fs/fs.h中定义：
```
#ifndef __FS_FS_H
#define __FS_FS_H

#define MAX_FILES_PER_PART  4096    //每个分区支持最大创建的文件数
#define BITS_PER_SEcTOR 4096    //扇区的位数
#define SECTOR_SIZE 512         //每个扇区字节大小
#define BLOCK_SIZE SECTOR_SIZE  //块字节大小

/* 文件类型 */
enum file_types{
  FT_UNKNOWN,       //不支持的文件类型
  FT_REGULAR,       //普通文件类型
  FT_DIRECTORY      //目录文件类型
};


#endif
```
这样以来咱们的准备工作已经完成了，接下来就开始正式的创建文件系统了

### 2.创建文件系统
这里创建文件系统也就是平时所说的高级格式化分区，我们创建文件fs/fs.c，而其中完成格式化分区的函数是partition_format，我会将注释写的详细一点使得大家更加容易理解
```
#include "fs.h"
#include "ide.h"
#include "stdint.h"
#include "global.h"
#include "super_block.h"
#include "inode.h"
#include "dir.h"
#include "string.h"
#include "memory.h"

/* 格式化分区，也就是初始化分区的元信息，创建文件系统 */
static void partition_format(struct disk* hd, struct partition* part){
  /* blocks_bitmap_init(为了方便实现，一个块就是一扇区) */
  uint32_t boot_sector_sects = 1;       //启动扇区数
  uint32_t super_block_sects = 1;       //超级块扇区数
  uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR); //表示inode位图所占用的扇区数，这里表示占一个扇区就行了
  uint32_t inode_table_sects = DIV_ROUND_UP((sizeof(struct inode) * MAX_FILES_PER_PART), SECTOR_SIZE); //inode表所占扇区
  uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects; //总共已经被使用的扇区
  uint32_t free_sects = part->sec_cnt - used_sects;                         //本分区剩余的扇区
  /*************************** 简单处理块位图占据的扇区数  *******************************/
  uint32_t block_bitmap_sects;
  block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);   //获取空闲扇区位图所占的扇区数
  /* block_bitmap_bit_len是位图中位的长度，也是可用块的数量 */
  uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;  //这里得出剩余空闲块扇区数
  block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);     //然后位图的扇区数就在对于位长度/每扇区的位数 向上取整来获得空闲位图扇区数
  /***************************************************************************************/

  /* 超级块初始化 */
  struct super_block sb;
  sb.magic = 0x20001109;
  sb.sec_cnt = part->sec_cnt;
  sb.inode_cnt = MAX_FILES_PER_PART;
  sb.part_lba_base = part->start_lba;
  sb.block_bitmap_lba = sb.part_lba_base + 2; //第0块是引导块，第1块是超级块，接下来就是这个空闲块位图了
  sb.block_bitmap_sects = block_bitmap_sects;

  sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects; //紧接着空闲块位图就存放咱们的i结点位图
  sb.inode_bitmap_sects = inode_bitmap_sects;

  sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects; //然后就是我们的inode数组了
  sb.inode_table_sects = inode_table_sects;

  sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;    //最后就是咱们的数据区了
  sb.root_inode_no = 0;
  sb.dir_entry_size = sizeof(struct dir_entry);

  printk("%s info:\n", part->name);
  printk("  magic:0x%x\n  part_lba_base:0x%x\n  all_sectors:0x%x\n  inode_cnt:0x%x\n  inode_bitmap_lba:0x%x\n\
        inode_bitmap_sectors:0x%x\n  inode_table_lba:0x%x\n  inode_table_sectors:0x%x\n  data_start_lba:0x%x\n",\
        sb.magic, sb.part_lba_base, sb.sec_cnt, sb.inode_cnt, sb.inode_bitmap_lba, sb.inode_bitmap_sects, sb.inode_table_lba, sb.inode_table_sects,\
        sb.data_start_lba);

  struct disk* hd = part->my_disk;
  /***************************************
   * 1.将超级块写入本分区的1扇区
   * *************************************/
  ide_write(hd, part->start_lba + 1, &sb, 1);
  printk("  super_block_lba:0x%x\n", part->start_lba + 1);

  /* 找出数据量最大的元信息，用其尺寸做存储缓冲区 */
  /* 这里是因为空闲块位图和inode数组都十分大，所以使用栈的话难以维系，所以需要申请堆来进行赋值 */
  uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects);
  buf_size = (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) * SECTOR_SIZE;

  uint8_t* buf = (uint8_t*)sys_malloc(buf_size); //申请的内存由内存管理系统清0后返回,返回一个指向1字节地址的指针

  /******************************************
   * 2.将块位图初始化并且写入sb.block_bitmap_lba
   * ****************************************/
  /* 初始化块位图 */
  buf[0] |= 0x01;   //第0个块预留给根目录，位图中先占位
  uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
  uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
  uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE); //last_size是块位图最后一扇区中剩余没用的部分

  /* 先将位图最后一字节到其所在的扇区的结束全置为1，这里置1是为了防止错误访问 */
  memset(&buf[block_bitmap_last_byte], 0xff, last_size);

  /* 再将上一步中覆盖的最后一字节内的有效位置0 */
  uint8_t bit_idx = 0;
  while(bit_idx <= block_bitmap_last_bit){
    buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
  }
  ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);   //写入空闲块位图

  /*****************************************
   * 3.将inode位图初始化并且写入sb.inode_bitmap_lba
   * ***************************************/
  /* 先清空缓冲区 */
  memset(buf, 0, buf_size);
  buf[0] |= 0x1;    //第0个inode分给根目录
  /* 由于inode_table中共有4096个inode，位图inode_bitmap刚好占1个扇区，所以位图中的位刚好全代表inode_table中的inode
   * 因此这里不需要像block_bitmap那样单独置1了*/
  ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);   //写入inode位图

  /*****************************************
   * 4.将inode数组初始化并写入sb.inode_table_lba
   *****************************************/
  /* 准备写inode_table中的第0项，也就是根目录所在的inode */
  /* 清空缓冲区 */
  memset(buf, 0, buf_size);
  struct inode* i = (struct inode*)buf;
  i->i_size = sb.dir_entry_size * 2;    //大小就是.和..两个目录项大小
  i->i_no = 0;      //这里是inode编号
  i->i_sectors[0] = sb.data_start_lba;  //由于上面的memset，i_sectors数组的其他元素都初始化为0
  ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);

  /******************************************
   * 5.将根目录写入sb.data_start_lba
   * ****************************************/
  /* 写入根目录的两个目录项.和.. */
  memset(buf, 0, buf_size);
  struct dir_entry* p_de = (struct dir_entry*)buf;
  /* 初始化当前目录“.” */
  memcpy(p_de->filename, ".", 1);
  p_de->i_no = 0;
  p_de->f_type = FT_DIRECTORY;
  p_de++;

  /* 初始化当前目录的父目录".." */
  memcpy(p_de->filename, "..", 1);
  p_de->i_no = 0;   //根目录的父目录还是其自己
  p_de->f_type = FT_DIRECTORY;

  /* sb.data_start_lba已经分配给了根目录，里面是根目录的目录项 */
  ide_write(hd, sb.data_start_lba, buf, 1);

  printk("  root_dir_lba:0x%x\n", sb.data_start_lba);
  printk("%s format done\n", part->name);
  sys_free(buf);
}

```

这个格式化的函数我在注释中已经很详细的说明了，所以这里也不过多讲，这里需要结合上面我给出的超级块的结构图进行理解，我们目前仅仅构造了根目录以及其中的两个目录项“.”和“..”，大家熟练使用linux的肯定都知道。

然后我们在其中再添加一个初始化函数就行了，在fs/fs.c当中：
```
/* 在磁盘上搜索文件系统，若没有则格式化分区创建文件系统 */
void filesys_init(){
  uint8_t channel_no = 0, dev_no, part_idx = 0;

  /* sb.buf 用来存储从硬盘上读入的超级块 */
  struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
  if(sb_buf == NULL){
    PANIC("alloc memory failed!");
  }
  printk("searching filesystem ...\n");
  while(channel_no < channel_cnt){
    dev_no = 0;
    while(dev_no < 2){
      if(dev_no == 0){  //跳过hd60M.img
        dev_no++;
        continue;
      }
      struct disk* hd = &channels[channel_no].devices[dev_no];
      struct partition* part = hd->prim_parts;
      while(part_idx < 12){   //共有4个主分区和8个逻辑分区
        if(part_idx == 4){    //当idx为4的时候处理逻辑分区
          part = hd->logic_parts;
        }
        /* channels数组是全局变量，默认值为0,disk属于嵌套结构，
        * partition是disk的嵌套结构，所以partition中成员也默认为0
        * 下面处理存在的分区 */
        if(part->sec_cnt != 0){   //如果分区存在
          memset(sb_buf, 0, SECTOR_SIZE);
          /* 读出分区的超级块，根据魔数判断是否存在文件系统 */
          ide_read(hd, part->start_lba + 1, sb_buf, 1);   //这里start_lba + 1 是超级块所在的扇区
          if(sb_buf->magic == 0x20001109){
            printk("%s has filesystem\n", part->name);
          }else{
              printk("formatting %s's partition %s.......\n", hd->name, part->name);
              partition_format(part);
          }
        }
        part_idx++;
        part++;   //下一分区
      }
      dev_no++;   //下一磁盘
    }
    channel_no++;   //下一通道
  }
  sys_free(sb_buf);
}

```
上面代码所做的工作就是简单的搜索每个分区是否存在文件系统，如果不存在就进行初始化，下面我们第一次编译运行来看看情况
![](http://imgsrc.baidu.com/super/pic/item/fcfaaf51f3deb48ffc4c4918b51f3a292cf578cb.jpg)
可以看到咱们对于hd80M,img的各个分区已经进行了初始化，理论上我们对其进行初始化过后，硬盘上就应该永远存留着我们已经格式化好的文件系统了，我们再次运行试试看
![](http://imgsrc.baidu.com/super/pic/item/6c224f4a20a4462310d54da4dd22720e0df3d7dd.jpg)
发现确实没有再次进行初始化，因为初始化程序检测到每个分区已经存在了。

### 3.挂载分区
咱们最初使用电脑的时候操作系统一般都是Windows，他的分区十分简单的已经给咱们分出来了，那就是C、D等盘，但是Linux不一样，Linux内核所在的分区是默认分区，从系统启动之后就以该分区为默认分区，如果要想使用其他分区的话需要使用mount命令手动将新分区挂在到默认分区的某个目录之下，这就使得我们的目录结构同树一样不断向下分支，当我们不想使用这个新分区的时候，就使用umount进行卸载。
但是目前的问题就是咱们的操作系统内核并不在文件系统上面，他处在一个hd60M.img的简单裸盘上面，我们根本没对其进行操作，更加谈不上分区，所以这里我们就采用一个简单的方法：
所谓的挂载，追求的就是我们能访问到磁盘上的文件，所以我们将文件系统分区的元信息加载到内存供咱们能访问就足够了，下面我们直接进行代码实现：
```
struct partition* cur_part;     //默认情况下操作的是哪个分区

/* 在分区链表中找到名为part_name的分区，并将其指针赋值给cur_part */
static bool mount_partition(struct list_elem* pelem, int arg){
  char* part_name = (char*)arg;
  struct partition* part = elem2entry(struct partition, part_tag, pelem);
  if(!strcmp(part_name, part->name)){
    cur_part = part;
    struct disk* hd = cur_part->my_disk;

    /* sb_buf用来存储从硬盘上读入的超级块 */
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

    /* 在内存中创建分区cur_part的超级块 */
    cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
    if(cur_part->sb == NULL){
      PANIC("alloc memory failed");
    }

    /* 读入超级块 */
    memset(sb_buf, 0, SECTOR_SIZE);
    ide_read(hd, cur_part->start_lba + 1, sb_buf, 1);

    /* 把缓冲区中的超级快复制到当前分区的sb中 */
    memcpy(cur_part->sb, sb_buf, sizeof(struct super_block));

    /**************** 将硬盘上的块位图读入到内存 ***************/
    cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
    if(cur_part->block_bitmap.bits == NULL){
      PANIC("alloc memory failed!");
    }
    cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;
    /* 从硬盘上面读入块位图到分区的block_bitmap.bits*/
    ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_sects);
    /***********************************************************/

    /**************** 将硬盘上的inode位图读入到内存 ***************/
    cur_part->inode_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
    if(cur_part->inode_bitmap.bits == NULL){
      PANIC("alloc memory failed!");
    }
    cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;
    /* 从硬盘上面读入块位图到分区的inode_bitmap.bits*/
    ide_read(hd, sb_buf->inode_bitmap_lba, cur_part->inode_bitmap.bits, sb_buf->inode_bitmap_sects);
    /***********************************************************/

    list_init(&cur_part->open_inodes);
    printk("mount %s done!\n", part->name);
    /* 此处返回true是为了迎合主调函数list_traversal的实现，与函数本身无关，只有返回true该函数才会停止遍历 */
    return true;
  }
  return false;         //使得list_traversal继续遍历
}

```

上述代码也就是在内存中创建一个超级块以及位图等，十分简单，接下来我们就到filesys_init函数当中注册默认装载分区是sdb1,然后来看看效果
```
  /* 确定默认操作的分区 */
  char default_part[8] = "sdb1";
  /* 挂载分区 */
  list_traversal(&partition_list, mount_partition, (int)default_part);

```

![](http://imgsrc.baidu.com/super/pic/item/79f0f736afc37931dc5e6defaec4b74542a911c6.jpg)
可以看到也是十分正常的装载了sdb1

## 0x02 文件描述符
### 1.文件描述符简介
我们的这个文件描述符与inode并不是同一个东西，inode是操作系统为自己的文件系统准备的数据结构，他用于文件存储的管理，与用户关系不大，而文件描述符才是与用户息息相关的。
咱们读写文件的时候，inode给咱们用户提供的信息仅仅是有哪些数据块，可是我们对文件的操作指针都是任意的，就拿读来说，我们读取文件都是通过多次读一小部分来进行的，在读的过程中我们必须维护一个文件偏移指针，所以为了保证下一次读的正确性，我们必须要时刻记录这个偏移指针的值，我们就将类似的倾向于用户的信息保存到文件描述符当中
在linux中，我们读写函数文件时都是操作文件描述符来完成的，而对于一个进程来说，我们打开了某个文件就是拥有了一个文件描述符，这个描述符对应了一个inode结点，然后通过inode节点来获取文件，每个进程中的PCB就保存着一个数组，用他来指向每个文件结构，也就是文件描述符，这里我给个图让大家方便理解：
![](http://imgsrc.baidu.com/super/pic/item/8326cffc1e178a8266e29466b303738da877e81f.jpg)

我们来简单梳理一下找到文件的过程：
1. 某进程把文件描述符作为参数提交给文件系统
2. 文件系统用此文件描述符在该进程的PCB当中的文件描述符数组中索引对应的元素
3. 从该元素当中获取文件的inode，最终找到inode的数据快

这里我们首先需要创建文件描述符，基本工作如下：
1. 在全局的inode队列中新建一个inode，然后返回该inode的地址
2. 在全局文件表中找到一个空位，在这里填上文件结构，使其中的fd_inode指向上一步中返回的inode地址，然后返回本文件在文件表中的下标地址
3. 在PCB中的文件描述符数组中找到一个空位，使其值指向上一布中返回的文件结构下标，并返回本文件描述符在文件描述符数组中的下标值


### 2.文件描述符实现
首先就是在我们的PCB当中添加进去元素，也就是修改thread/thread.h
```
int32_t fd_table[MAX_FILES_OPEN_PER_PROC];
```

这里是简单的在task_struct中添加了描述符数组，但是最大只支持8个。
注意这个数组也需要被初始化，所以我们在thread.c中记得修改
```
  /* 预留标准输入输出 */
  pthread->fd_table[0] = 0;
  pthread->fd_table[1] = 1;
  pthread->fd_table[2] = 2;
  /* 其余全置-1 */
  uint8_t fd_idx = 3;
  while(fd_idx < MAX_FILES_OPEN_PER_PROC){
    pthread->fd_table[fd_idx] = -1;
    fd_idx++;
  }
  pthread->stack_magic = 0xdeadbeef;    //自定义魔数
```

我们在这里先预留了三个标准输入输出错误，我们等到讲到的时候会解释。到此我们的文件描述符算是创建完毕。

## 0x03 总结
今天的部分很少，就是简单的对于一些基本数据进行解释以及初始化，在下一节我们将会进行真正的对于我们需要的文件进行编程.
