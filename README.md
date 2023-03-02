## 0x00 文件操作相关基础函数
### 1.inode相关
这里我们首先实现获取inode具体偏移地址，以及写入inode的操作，如下我们创建fs/inode.c
```
#include "inode.h"
#include "global.h"
#include "debug.h"
#include "stdint.h"
#include "ide.h"
#include "fs.h"
#include "inode.h"
#include "super_block.h"
#include "dir.h"

/* 用来存储inode位置 */
struct inode_position{
  bool two_sec;     //inode是否跨扇区
  uint32_t sec_lba;     //inode所在的扇区号
  uint32_t off_size;    //inode在扇区内的字节便宜量
};

/* 获取inode所在的扇区和扇区内的便宜量 */
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos){
  /* inode_table在硬盘上是连续的 */
  ASSERT(inode_no < 4096);
  uint32_t inode_table_lba = part->sb->inode_table_lba;
  uint32_t inode_size = sizeof(struct inode);
  uint32_t off_size = inode_no * inode_size;    //inode_no号I结点相对于inode_table_lba的字节偏移量
  uint32_t off_sec = off_size / 512;    //inode_no号I结点相对与inode_table_lba的扇区偏移量
  uint32_t off_size_in_sec = off_size % 512;    //在所在扇区的偏移量
  /* 判断此inode是否跨扇区 */
  uint32_t left_in_sec = 512 - off_size_in_sec;
  if(left_in_sec < inode_size){
    //若此扇区剩下的空间不足一个inode大小，则说明他跨扇区了
    inode_pos->two_sec = true;
  }else{    //否则说明未跨区
    inode_pos->two_sec = false;
  }
  inode_pos->sec_lba = inode_table_lba + off_sec ;
  inode_pos->off_size = off_size_in_sec;
}

/* 将inode写入到分区part */
void inode_sync(struct partition* part, struct inode* inode, void* io_buf){
  //io_buf是用于硬盘io的缓冲区
  uint8_t inode_no = inode->i_no;
  struct inode_position inode_pos;
  inode_locate(part, inode_no, &inode_pos);     //inode的位置信息会存入inode_pos
  ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));
  /* 硬盘中的inode中的成员inode_tag和i_open_cnts是不需要的，他俩只在内存中记录链表位置和被多少进程共享 */
  struct inode pure_inode;
  memcpy(&pure_inode, inode, sizeof(struct inode));

  /* 以下inode的三个成员只存在于内存中，现在将inode同步到硬盘，清掉这三项即可 */
  pure_inode.i_open_cnts = 0;
  pure_inode.write_deny = false;    //这里置为false用来保证硬盘在读出时为可写
  pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

  char* inode_buf = (char*)io_buf;
  if(inode_pos.two_sec){
    //若是横跨两个扇区，则需要读两个扇区写两个扇区
    /* 读写硬盘是以扇区为单位的，若写入的数据少于1扇区，则需要将整个扇区读出修改，然后再写入才行 */
    ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);   //inode在format中写入硬盘时是连续写入的，所以需要读入2块扇区
    /* 开始将待写入的inode拼接到2个扇区中的相应位置 */
    memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
    /* 将拼接好的数据再写入磁盘 */
    ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
  }else{
    ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);   //只需要读1个扇区
    /* 开始将待写入的inode拼接到2个扇区中的相应位置 */
    memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
    /* 将拼接好的数据再写入磁盘 */
    ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
  }
}

/* 根据i结点号返回相应的inode */
struct inode* inode_open(struct partition* part, uint32_t inode_no){
  /* 先在已打开的inode链表当中找inode，此链表是为提速创建的缓冲区 */
  struct list_elem* elem = part->open_inodes.head.next;     //获取链表中的地一个有效结点
  struct inode* inode_found;
  while(elem != &part->open_inodes.tail){
    inode_found = elem2entry(struct inode, inode_tag, elem);
    if(inode_found->i_no == inode_no){
      inode_found->open_inodes++;   //被打开的inode节点数加一
      return inode_found;
    }
    elem = elem->next;
  }

  /* 执行到此说明open_inode链表中找不到，于是从硬盘上读入inode到内存 */
  struct inode_position inode_pos;

  /* inode位置信息会存入inode_pos */
  inode_locate(part, inode_no, &inode_pos);

  /* 为了让sys_malloc创建的新的inode被所有任务共享，需要将inode置为内核空间，此时就需要将cur->pgdir置为NULL */
  struct task_struct* cur = running_thread();
  uint32_t* cur_pagedir_bak = cur->pgdir;   //临时保存用户页目录
  cur->pgdir = NULL;
  /* 现在我们分配内存就会从内核空间分配了 */
  inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
  /* 恢复pgdir */
  cur->pgdir = cur_pagedir_bak;

  char* inode_buf;
  if(inode_pos.two_sec){
    inode_buf = (char*)sys_malloc(1024);    //两个扇区大小
    ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
  }else{
    inode_buf = (char*)sys_malloc(512);     //一个扇区足够
    ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
  }
  memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));
  /* 一会儿需要用到此inode，故将其插入到队首以便提前检索到 */
  list_push(&part->open_inodes, &inode_found->inode_tag);
  inode_found->i_open_cnts = 1;
  sys_free(inode_buf);
  return inode_found;
}

/* 关闭inode或减少inode的打开数 */
void inode_close(struct inode* inode){
  /* 如果没有进程再打开此文件，则将此inode去掉并释放空间 */
  enum intr_status old_status = intr_disable();
  if(--inode->i_open_cnts == 0){
    list_remove(&inode->inode_tag);     //从打开inode结点链表中脱链
    /* 由于我们的inode都是在内核空间中分配的，用来实现进程共享，所以这里我们需要暂时伪装成内核线程 */
    struct task_struct* cur = running_thread();
    uint32_t* cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;
    sys_free(inode);
    cur->pgdir = cur_pagedir_bak;
  }
  intr_set_status(old_status);
}

/* 初始化new_inode */
void inode_init(uint32_t inode_no, struct inode* new_inode){
  new_inode->i_no = inode_no;
  new_inode->i_size = 0;
  new_inode->i_open_cnts = 0;
  new_inode->write_deny = false;

  /* 初始化索引数组i_sector */
  uint8_t sec_idx = 0;
  while(sec_idx < 13){
    new_inode->i_sectors[sec_idx] = 0;
    sec_idx++;
  }
}

```

代码都比较常规，这里主要是对于inode节点的操作函数，之后我们再来实现对文件操作的函数

### 2.文件相关
我们创建新文件 fs/file.h
```
#ifndef __FS_FILE_H
#define __FS_FILE_H
#include "stdint.h"
#include "inode.c"
/* 文件结构 */
struct file {
  uint32_t  fd_pos;     //记录当前文件操作的偏移地址，以0为起始，最大为文件大小-1
  uint32_t fd_flag;     
  struct inode* fd_inode;
};

/* 标准输入输出描述符 */
enum std_fd{
  stdin_no,         //0 标准输入
  stdout_no,        //1 标准输出
  stderr_no         //2 标准错误
};

/* 位图类型 */
enum bitmap_type{
  INODE_BITMAP,     //inode位图
  BLOCK_BITMAP,     //块位图
};

#define MAX_FILE_OPEN 32    //系统可以打开的最大文件数
#endif

```
其中的fd_flag就是文件操作标识，类系O_READONLY
下面我们来构建文件表，创建文件fs/file.c
```
#include "file.h"
#include "fs.h"
#include "stdint.h"
#include "thread.h"
#include "global.h"
#include "string.h"
#include "inode.h"
#include "debug.h"
#include "memory.h"
#include "interrupt.h"
#include "stdio-kernel.h"
#include "super_block.h"

/* 文件表 */
struct file file_table[MAX_FILE_OPEN];

/* 从文件表file_table中获取一个空闲位，并返回下标，失败就返回-1 */
int32_t get_free_slot_in_global(void){
  uint32_t fd_idx = 3;
  while(fd_idx < MAX_FILE_OPEN){
    if(file_table[fd_idx].fd_inode == NULL){
      break;
    }
    fd_idx++;
  }
  if(fd_idx == MAX_FILE_OPEN){
    printk("exceed max open files\n");
    return -1;
  }
  return fd_idx;
}

/* 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中，成功则返回下标，否则返回-1 */
int32_t pcb_fd_install(int32_t global_fd_idx){
  struct task_struct* cur = running_thread();
  uint8_t local_fd_idx = 3; //跨过3个标准描述符
  while(local_fd_idx < MAX_FILES_OPEN_PER_PROC){
    if(cur->fd_table[local_fd_idx] == -1){  //-1表示可使用，我们在最开始init的时候就置-1了
      cur->fd_table[local_fd_idx] = global_fd_idx;
      break;
    }
    local_fd_idx++;
  }
  if(local_fd_idx == MAX_FILES_OPEN_PER_PROC){
    printk("exceed max open files_per_proc\n");
    return -1;
  }
  return local_fd_idx;
}

/* 分配一个inode，返回i结点号 */
int32_t inode_bitmap_alloc(struct partition part){
  int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
  if(bit_idx == -1){
    return -1;
  }
  bitmap_set(&part->inode_bitmap, bit_idx, 1);
  return bit_idx;
}

/* 分配1个扇区，返回扇区地址 */
int32_t block_bitmap_alloc(struct partition part){
  int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
  if(bit_idx == -1){
    return -1;
  }
  bitmap_scan(&part->block_bitmap, bit_idx, 1);
  return (part->sb->data_start_lba + bit_idx);
}

/* 将内存中的bitmap第bit_idx位所在的512字节同步到硬盘 */
void bitmap_sync(struct partition, uint32_t bit_idx, uint8_t btmp){
  uint32_t off_sec = bit_idx / 4096;    //该位相对与位图扇区的偏移量
  uint32_t off_size = off_sec * BLOCK_SIZE;     //偏移的字节数
  uint32_t sec_lba;                 //扇区地址
  uint8_t* bitmap_off;              //位图偏移

  /* 需要被同步到硬盘的位图只有inode_bitmap和block_bitmap */
  switch(btmp){
    case INODE_BITMAP:
      sec_lba = part->sb->inode_bitmap_lba + off_sec;
      bitmap_off = part->inode_bitmap.bits + off_size;
      break;
    case BLOCK_BITMAP:
      sec_lba = part->sb->block_bitmap_lba + off_sec;
      bitmap_off = part->block_bitmap.bits + off_size;
      break;
  }
  ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}

```

这里面我们构造了一些对于普通文件的操作，注释也十分详细就不多说了

### 3.目录相关
我们刚刚已经实现了inode以及普通文件的操作，现在我们需要再添加一些对于目录的操作，这里也就是简单的读出目录项等然后复制就行，具体代码为fs/dir.h
```
#include "dir.h"
#include "super_block.h"
#include "ide.h"
#include "inode.h"
#include "stdint.h"
#include "global.h"

struct dir root_dir;    //根目录

/* 打开根目录 */
void open_root_dir(struct partition* part){
  root_dir.inode = inode_open(part, part->sb->root_inode_no);
  root_dir.dir_pos = 0;
}


/* 在分区part上打开i结点作为inode_no的目录并返回目录指针 */
struct dir* dir_open(struct partition* part, uint32_t inode_no){
  struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
  pdir->inode = inode_open(part, inode_no);
  pdir->dir_pos = 0;
  return pdir;
}

/* 在part分区内的pdir目录内寻找名为name的文件或者目录，找到后返回true并将其目录项存入dir_e，否则返回false */
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name, struct dir_entry* dir_e){
  uint32_t block_cnt = 140;     //12个直接块，加上一级间接块，有128个，所以总共140个
  /* 12个直接块和128个间接块，共560字节 */
  uint32_t* all_blocks = (uint32_t*)sys_malloc(48 + 512);
  if(all_blocks == NULL){
    printk("search_dir_entry: sys_malloc for all_blocks failed");
    return false;
  }
  uint32_t block_idx = 0;
  while(block_idx < 12){
    all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
    block_idx++;
  }
  block_idx = 0;

  if(pdir->inode->i_sectors[12] != 0){  //如果含有1级块，则我们会直接将那一块进行读出
    ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks + 12, 1);
  }
  /* 至此，all_blocks已经存储了全部的文件扇区地址 */
  uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
  struct dir_entry* p_de = (struct dir_entry*)buf;      //p_de为指向目录项的指针
  uint32_t dir_entry_size = part->sb->dir_entry_size;
  uint32_t dir_entry_cnt = SECTOR_SIZE/dir_entry_size;  //1扇区可以容纳的目录项个数
  /* 开始在所有块中查找 */
  while(block_idx < block_cnt){
    /* 块地址为0时表示该块中无数据，继续在其他快查找 */
    if(all_blocks[block_idx] == 0){
      block_idx++;
      continue;
    }
    ide_read(part->my_disk, all_block[block_idx], buf, 1);
    uint32_t dir_entry_idx = 0;
    /* 遍历扇区中的所有目录项 */
    while(dir_entry_idx < dir_entry_cnt){
      /* 如果找到了的话就复制整个目录项 */
      if(!strcmp(p_de->filename, name)){
        memcpy(dir_e, p_de, dir_entry_size);
        sys_free(buf);
        sys_free(all_blocks);
        return true;
      }
      dir_entry_idx++;
      p_de++;
    }
    block_idx++;
    p_de = (struct dir_entry*)buf;  //这里我们的p_de要重新指向新的块，所以要更新指针为开头
    memset(buf, 0, SECTOR_SIZE);
  }
  sys_free(buf);
  sys_free(all_blocks);
  return false;
}

/* 关闭目录 */
void dir_close(struct dir* dir){
  /****************** 根目录无法关闭 *******************
   * 1 根目录自打开后就不应该关闭，否则还需要再次open_root_dir
   * 2 root_dir所在的内存是低端1MB之内，并非在堆中，free会出问题*/
  if(dir == &root_dir){
    /* 不做任何处理，直接返回 */
    return;
  }
  inode_close(dir->inode);
  sys_free(dir);
}

/* 在内存中初始化目录项p_de */
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de){
  ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);
  /* 初始化目录项 */
  memcpy(p_de->filename, filename, strlen(filename));
  p_de->i_no = inode_no;
  p_de->f_type = file_type;
}

```

上面实现了一些基本操作，我们这里还需要一点工作，那就是增加一个函数sync_dir_entry(),这个函数比较复杂，他的功能是在指定目录下添加目录项，如下：
```
/* 将目录项p_de写入父目录parent_dir中，io_buf由主调函数提供 */
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf){
  struct inode* dir_inode = parent_dir->inode;
  uint32_t dir_size = dir_inode->i_size;                    //parent_dir目录文件大小
  uint32_t dir_entry_size = cur_part->sb->dir_entry_size;   //获取目前分区的目录项大小
  ASSERT(dir_size % dir_entry_size == 0);   //dir_size应该是dir_entry_size的整数倍

  uint32_t dir_entrys_per_sec = (512 / dir_entry_size);     //每扇区最大的目录项数目
  int32_t block_lba = -1;

  /* 将该目录的所有扇区地址(12 + 128)存入all_block */
  uint8_t block_idx = 0;
  uint32_t all_blocks[140] = {0};   //all_blocks上保存目录所有的目录项的扇区地址

  /* 将12个直接块存入all_blocks */
  while(block_idx < 12){
    all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    block_idx++;
  }

  struct dir_entry* dir_e = (struct dir_entry*)io_buf;      //dir_e用来在io_buf中遍历目录项
  int32_t block_bitmap_idx = -1;
  /* 开始遍历所有块以寻找目录项空位，若已有扇区中没有空位，在不超过文件大小的情况下申请新扇区来存储新目录项 */
  block_idx = 0;
  while(block_idx < 140){
    block_bitmap_idx = -1;
    if(all_blocks[block_idx] == 0){
      block_lba = block_bitmap_alloc(cur_part);     //如果该目录项为空，则分配一个空闲块用来存放目录项
      if(block_lba == -1){
        printk("alloc block bitmap for sync_dir_entry failed\n");
        return false;
      }

      /* 每分配一次要记得同步一次block_bitmap */
      block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
      ASSERT(block_bitmap_idx != -1);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
      block_bitmap_idx = -1;
      /* 下面有三种未分配的情况： */
      if(block_idx < 12){                       //1.如果是直接块
        dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
      }else if(block_idx == 12){                //2.如果尚未分配一级间接表
        dir_inode->i_sectors[12] = block_lba;   //将上面分配的块作为1级间接块表
        block_lba = -1;
        block_lba = block_bitmap_alloc(cur_part);   //再分配一个块作为第0个间接块
        if(block_lba == -1){                //分配失败，回滚
          block_bitmap_idx = dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
          bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
          dir_inode->i_sectors[12] = 0;
          printk("alloc block bitmap for sync_dir_entry failed\n");
          return false;
        }
        /* 走到这里表示成功分配了一个空闲快，所以这里我们需要再同步一次 */
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        ASSERT(block_bitmap_idx != -1);
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

        all_blocks[12] = block_lba;
        /* 把新分配的第0个间接块地址写入一级间接块表 */
        ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
      }else{                                     //3.若是间接块未分配
        all_blocks[block_idx] = block_lba;  //把新分配的第（block_idx-12）个间接块地址写入一级间接块表
        ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
      }
      /* 再将新目录项p_de写入新分配的间接块 */
      memset(io_buf, 0, 512);
      memcpy(io_buf, p_de, dir_entry_size);
      ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
      dir_inode->i_size += dir_entry_size;
      return true;
    }
    /* 执行到这里说明block_idx不为0,此时我们将其读入内存，然后在该块中查找空目录项 */
    ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
    /* 在扇区内查找空目录项 */
    uint8_t dir_entry_idx = 0;
    while(dir_entry_idx < dir_entrys_per_sec){
      if((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN){
        //这里的FT_UNKNOWN无论是初始化或者是删除文件后，都会将f_type置为FT_UNKNOWN
        memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
        ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
        dir_inode->i_size += dir_entry_size;
        return true;
      }
      dir_entry_idx++;
    }
    block_idx++;
  }
  printk("directory is full!\n");
  return false;
}

```

我的本意是不想贴这么多代码，但是这文件系统就这样，你说这原理吧都挺容易理解，但是代码实现起来就特别繁琐，如果我这里仅仅给个思路那肯定不够，上面的函数我写一个大致过程方便理解：
1. 首先我们获取父目录的inode节点中的索引扇区地址，紧接着开始遍历看有无空闲目录项
2. 这个遍历是按照块为单位的，所以首先判断块是否为空，如果为空，就说明需要新分配一块来容纳咱们的新目录项，转到步骤3，若不为空则跳转到步骤7
3. 首先判断索引下标是否小于12,这说明他们是直接索引，我们将新分配的块分给他，然后跳转到步骤6， 否则下一步
4. 若判断索引下标等于12,则说明他连一级索引都没分配，此时我们再分配一块作为间接索引地址块，然后初始咱们的一级索引地址，然后跳转到步骤6， 否则下一步
5. 到这里肯定说明索引下标大于12,这就表示已经处于间接索引了，这里的操作同步骤3一致，就是分配块地址而已，紧接下一步
6. 我们执行到这里已经给空闲块分配的新扇区，此时我们只需要将目录项复制到相应扇区就行了
7. 这里说明最开始循环判断块不为空，此时我们需要在块内寻找空闲区域，将咱们的目录项存入即可，但若发现该块已经满了的话，接着循环判断下一块，进入步骤2

### 4.路径解析相关
我们要使用文件，就必须得定位，而对于咱们用户访问文件都是通过文件名，更加准确的是得通过文件路径，所以当我们输入一长串路径的时候，我们需要解析其中所包含的文件或者说是目录了，因此我们到fs/fs.c中实现咱们的路径解析函数，如下：
```
/* 将最上层路径名称解析出来 */
static char* path_parse(char* pathname, char* name_store){
  if(pathname[0] == '/'){
    /* 路径中出现1个或多个连续的字符'/'，将这些'/'跳过，如"///a/b" */
    while(*(++pathname) == '/');
  }

  /* 开始一般的路径解析 */
  while(*pathname != '/' && *pathname != 0){
    *name_store++ = *pathname++;
  }
  if(pathname[0] == 0){     //如果路径字符串为空，则返回NULL
    return NULL;
  }
  return pathname;
}

/* 返回路径深度，比如说/a/b/c，深度为3 */
int32_t path_depth_cnt(char* pathname){
  ASSERT(pathname != NULL);
  char* p = pathname;
  char name[MAX_FILE_NAME_LEN];     //用于path_parse的参数做路径解析
  uint32_t depth = 0;

  /* 解析路径，从中拆分出各级名称 */
  p = path_parse(p, name);
  while(name[0]){
    depth++;
    memset(name, 0, MAX_FILE_NAME_LEN);
    if(p){  //如果p不是NULL，则继续分析路径
      p = path_parse(p, name);
    }
  }
  return depth;
}

```

这里仅仅涉及了字符串的处理，十分简单

### 5.实现文件检索功能
在我们打开文件之前，我们需要确认文件是否在磁盘上存在，同样在创建文件的时候我们也需要确认目录中是否也有同名文件，而这些共能都需要一个最基本的操作，那就是文件检索，因此我们接下来来实现他,在此之前我们需要先定义一些结构体，不得不说在文件系统里面这些结构是真的多，但是气人的是他缺一不可，仅仅从这里我也体会到了真正设计操作系统的不容易，同时更加佩服象书的原作者了：
```

/* 打开文件时的选项 */
enum oflags{
  O_RDONLY,     //只读
  O_WRONLY,     //只写
  O_RDWR,       //读写
  O_CREAT = 4   //创建
};

extern struct partition* cur_part;

/* 用来记录查找文件过程中已经找到的上级路径 */
struct path_search_record{
  char searched_path[MAX_PATH_LEN];     //查找过程中的父路径
  struct dir* parent_dir;               //文件或目录所在的直接父目录
  enum file_types file_type;    //标明我们找到的是普通文件还是目录
};

```

这里我们记录父目录是因为，当我们在解析某个文件路径的时候发现某个地方不存在出现了断链，这时候我们就可以通过查看他的父目录来知道是从哪儿解析错误的。
下面我们再到fs.c中添加函数：
```
/* 搜索文件pathname, 若找到则返回其inode号，否则返回-1 */
static int search_file(const char* pathname, struct path_search_record* searched_record){
  /* 如果待查找的是根目录，为避免下面无用的查找，则直接返回已知根目录信息 */
  if(!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")){
    searched_record->parent_dir = &root_dir;        //我们的根目录是处于内核低1MB的，常驻内存
    searched_record->file_type = FT_DIRECTORY;
    searched_record->searched_path[0] = 0;      //搜索路径置空
    return 0;
  }

  uint32_t path_len = strlen(pathname);
  /* 保证pathname至少是这样的路径/x，且小于最大长度 */
  ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
  char* sub_path = (char*)pathname;
  struct dir* parent_dir = &root_dir;
  struct dir_entry dir_e;

  /* 记录路径解析出来的各级名称， 如路径“/a/b/c”，数组name每次提出来的值是"a", "b", "c" */
  char name[MAX_FILE_NAME_LEN] = {0};
  searched_record->parent_dir = parent_dir;
  searched_record->file_type = FT_UNKNOWN;
  uint32_t parent_inode_no = 0;     //父目录的inode号
  /* 上面仅仅是初始化,下面开始查找 */
  sub_path = path_parse(sub_path, name);    //name里面包含的是分割出来的目录名
  while(name[0]){   //若第一个字符就是结束符，则结束循环
    /* 记录查找过的路径，但不能超过search_path的长度512字节 */
    ASSERT(strlen(searched_record->searched_path) < 512);
    /* 记录已经存在的父目录 */
    strcat(searched_record->searched_path, "/");
    strcat(searched_record->searched_path, name);
    /* 在所给的目录中查找文件 */
    if(search_dir_entry(cur_part, parent_dir, name, &dir_e)){
      memset(name, 0, MAX_FILE_NAME_LEN);
      /* 若sub_path不等于NULL，也就是未结束的时候继续拆分路径 */
      if(sub_path){
        sub_path = path_parse(sub_path, name);
      }

      if(FT_DIRECTORY == dir_e.f_type){     //如果说被打开的是目录
        parent_inode_no = parent_dir->inode->i_no;  //更新父目录，然后接着向下循环
        dir_close(parent_dir);          //关掉之前的父目录
        parent_dir = dir_open(cur_part, dir_e.i_no);
        searched_record->parent_dir = parent_dir;
        continue;
      }else if(FT_REGULAR == dir_e.f_type){
        searched_record->file_type = FT_REGULAR;
        return dir_e.i_no;
      }
    }else{
        /* 这里说明查找目录项失败，返回-1 */
        return -1;
    }
  }
  /* 执行到这里说明肯定遍历了完整路径，并且查找的文件或目录只有同名目录存在 */
  dir_close(searched_record->parent_dir);

  /* 保存被查找目录的直接父目录 */
  searched_record->parent_dir = dir_open(cur_part, parent_inode_no);
  searched_record->file_type = FT_DIRECTORY;
  return dir_e.i_no;
}

```

上面的过程就是咱们通过路径来搜索文件的过程，逻辑需要仔细理解。

## 0x01 创建文件
这里我们创建的是普通文件，至于创建目录我们留做日后讲解
### 1.实现file_create函数
我们要创建文件，需要分配位图，空闲块等，下面我直接来介绍步骤：
1. 创建文件的inode，所以我们需要向inode位图来申请位图来获取inode号，然后更新inode位图和inodetable数组
2. inode->i_sectors需要想block位图来获取可用块，所以block位图需要更新，data_start_lba以后的某个扇区会被分配
3. 新增的文件必须属于某个目录，所以该目录的i_size会增加一个目录项大小，然后将其目录项写入该目录的i_sectors[]的某个扇区
4. 若有步骤失败，则回滚
5. inode位图等都需要同步回硬盘

大致步骤已经给大家摆在这里了，剩下的就是实践：
```
/* 创建文件，如果成功就返回文件描述符，否则返回-1 */
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag){
  /* 先创建一个公共的操作缓冲区 */
  void* io_buf = sys_malloc(1024);
  if(io_buf == NULL){
    printk("in file_create: sys_malloc for io_buf failed\n");
    return -1;
  }
  uint8_t rollback_step = 0;   //用于操作失败的时候回滚各资源状态
  /* 为新文件分配inode */
  int32_t inode_no = inode_bitmap_alloc(cur_part);
  if(inode_no = -1){
    printk("in file_cretate: allocte inode failed\n");
    return -1;
  }
  /* 此inode要从堆中申请内存，不可生成局部变量，因为file_table数组的文件描述符的inode指针需要指向他 */
  struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
  if(new_file_inode == NULL){
    printk("first create: sys_malloc for inode failed\n");
    rollback_step = 1;
    goto rollback;
  }
  inode_init(inode_no, new_file_inode);     //初始化i结点
  /* 返回的是file_table数组空闲元素的下标 */
  int fd_idx = get_free_slot_in_global(); 
  if(fd_idx == -1){
    printk("exceed max open files\n");
    rollback_step = 2;
    goto rollback;
  }
  /* 这里说明成功找到了文件打开表空位，下面赋值进去 */
  file_table[fd_idx].fd_inode = new_file_inode;
  file_table[fd_idx].fd_pos = 0;
  file_table[fd_idx].fd_flag = flag;
  file_table[fd_idx].fd_inode->write_deny = false; 

  struct dir_entry new_dir_entry;
  memset(&new_dir_entry, 0, sizeof(struct dir_entry));

  create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);     //创建普通文件的目录项
  /* 同步内存数据到硬盘 */
  /* a 在目录parent_dir中安装目录项new_dir_entry,写入硬盘后返回true，否则返回false */
  if(!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)){
    printk("sync dir_entry to disk failed\n");
    rollback_step = 3;
    goto rollback;
  }
  memset(io_buf, 0, 1024);
  /* b 将父目录i结点内容同步到硬盘 */
  inode_sync(cur_part, parent_dir->inode, io_buf);
  memset(io_buf, 0, 1024);

  /* c 将新创建的inode内容同步到硬盘 */
  inode_sync(cur_part, new_file_inode, io_buf);
  
  /* d 将inode_bitmap位图同步到硬盘 */
  bitmap_sync(cur_part, inode_no, INODE_BITMAP);
  
  /* e 将创建的文件i结点添加到open_inodes链表 */
  list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
  new_file_inode->i_open_cnts = 1;
  sys_free(io_buf);
  return pcb_fd_install(fd_idx);

  /* 下面是回滚步骤 */
  rollback:
    switch(rollback_step){
      case 3:
        /* 失败时，将file_table中的相应位清空 */
        memset(&file_table[fd_idx], 0, sizeof(struct file));
      case 2:
        sys_free(new_file_inode);
      case 1:
        /* 如果新文件inode创建失败，则分配的inode_no也要恢复*/
        bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
        break;
    }
    sys_free(io_buf);
    return -1;
}

```

上面基础的创建文件方法我们已经实现，接下来就是如何使用，所以我们来实现open函数，其爱fs/fs.c中实现
```
/* 打开或创建文件成功后，返回文件描述符，否则返回-1 */
int32_t sys_open(const char* pathname, uint8_t flags){
  /* 对于目录需要使用dir_open，这里暂时只有open文件 */
  if(pathname[strlen[pathname] - 1] == '/'){
    printk("can not open a dirctory %s\n", pathname);
    return -1;
  }
  ASSERT(flag <= 7);
  int32_t fd = -1;  //默认为找不到
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));

  /* 记录目录深度，帮助判断中间某个目录不存在的情况 */
  uint32_t pathname_depth = path_depth_cnt((char*)pathname);
  
  /* 先检查文件是否存在 */
  int inode_no = search_file(pathname, &searched_record);
  bool found = inode_no != -1 ? true : false ;
  if(searched_record.file_type == FT_DIRECTORY){
    printk("can't open a directory with open(),use opendir to instead\n");
    dir_close(searched_record.parent_dir);
    return -1;
  }
  uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
  /* 先判断是否把pathname的各层目录都访问到了，即是否在某个中间目录就失败了 */
  if(pathname_depth != path_searched_depth){
    printk("cannot access %s: Not a directory, subpath %s is not exist\n", pathname, searched_record.searched_path);
    dir_close(searched_record.parent_dir);
    return -1;
  }
  /* 若是在最后一个路径上没有找到，并且不是要创建文件，直接返回-1 */
  if(!found && !(flags & O_CREAT)){
    printk("in path %s, file %s is't exist\n", searched_record.searched_path, (strrchr(searched_record.searched_path, '/') + 1));
    dir_close(searched_record.parent_dir);
    return -1;
  }else if(found && flags & O_CREAT){
    printk("%s has already exist\n", pathname);
    dir_close(searched_record.parent_dir);
    return -1;
  }
  switch(flags & O_CREAT){
    case O_CREAT:
      printk("creating file\n");
      fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1), flags);
      dir_close(searched_record.parent_dir);
      //其余为打开文件
  }
  /* 此fd是指任务pcb->fd_table数组中的下标 */
  return fd;
}

```

最后我们还需要到初始化函数filesys_init中打开我们的root_dir和初始化咱们的全局文件表了
```
  /* 将当前分区的根目录打开 */
  open_root_dir(cur_part);
  /* 初始化文件表 */
  uint32_t fd_idx = 0;
  while(fd_idx < MAX_FILE_OPEN){
    file_table[fd_idx++].fd_inode = NULL;
  }

```

### 3.创建咱们的第一个文件
咱们修改main函数，调用sys_open
```
int main(void){
  put_str("I am Kernel\n");
  init_all();

  intr_enable();
  //process_execute(u_prog_a, "u_prog_a");
  //process_execute(u_prog_b, "u_prog_b");
  //thread_start("k_thread_a", 31, k_thread_a, " A_");
  //thread_start("k_thread_b", 31, k_thread_b, " B_");
  sys_open("/file1", O_CREAT);
 while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}

```

我们首先运行一下试一试：
![](https://img-blog.csdnimg.cn/e12586616bb347899e2de215e7494aea.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM3NTAwNTE2,size_16,color_FFFFFF,t_70)
这里我本人的运行第一次忘记截图了，所以用的别的师傅的图，但是我们基本的信息都是一致的。
然后当我们再次运行main线程的时候，他会提示文件已经存在了，如下：
![](http://imgsrc.baidu.com/forum/pic/item/6f061d950a7b0208a91b012627d9f2d3562cc823.jpg)
这个图就是我第二次运行内核所跑出的结果，他提示咱们已经正确创建过/file1了，我们再到磁盘上面检查一下看是否确实正确创建了。
为了到磁盘上面查找信息，所以咱们拿之前初始化的时候打印的信息来确定
![](https://img-blog.csdnimg.cn/b60a5d55a0a54a8ea7bac09b73038118.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM3NTAwNTE2,size_16,color_FFFFFF,t_70)

这里可以看到咱们的data扇区最开始是在0xA67，且咱们的根目录最开始就会放入磁盘数据扇区最开始的地方，所以这里应该是存放的咱们的根目录，其中存放着一些目录项，因此我们通过xxd脚本进行查看
0xA67用十进制来表示就是2663扇区，所以便宜量用字节来表示就是2663×512 = 1363456
![](http://imgsrc.baidu.com/forum/pic/item/5d6034a85edf8db1864545a34c23dd54574e7489.jpg)
这里我们看到了我们自己创建的文件，十分成功,除了file1,其实咱们也可以看到"."和".."，他的ASCII码为0x2E和0x2E2E,也就恰好表示了当前目录与父目录


## 0x02 文件的打开与关闭
上一节我们成功实现了创建文件的功能，这一节我们来补充一下关闭文件的部分
### 1.打开文件
我们目前已经在根目录下创建了文件file1,现在咱们要再进行打开操作了，直接写代码，原理没啥好讲的，在file.c中定义函数file_open
```
/* 打开编号为inode_no的inode对应得文件 */
int32_t file_open(uint32_t inode_no, uint8_t flag){
  int fd_idx = get_free_slot_in_global();
  if(fd_idx == -1){
    printk("exceed max open files\n");
    return -1;
  }
  file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no);
  file_table[fd_idx].fd_pos = 0;        //每次打开文件需要将偏移指针置0,也就是开头
  file_table[fd_idx].fd_flag = flag;
  bool* write_deny = &file_table[fd_idx].fd_inode->write_deny;
  if(flag & O_WRONLY || flag & O_RDWR){
    //只要是关于写文件，判断是否有其他进程正写此文件
    //若是读文件，不考虑write_deny
    /* 以下进入临界区前先关中断 */
    enum intr_status old_status = intr_disable();
    if(!(*write_deny)){     //这里若通过则说明没有别的进程正在写
      *write_deny = true;   //这里置为空避免多个进程同时写文件
      intr_set_status(old_status);
    }else{
      intr_set_status(old_status);
      printk("file can not be write now, try again later\n");
      return -1;
    }
  } //若是读文件或者创建文件，则不用理会write_deny，保持默认
  return pcb_fd_install(fd_idx);
}

```

这里记得修改一下sys_open
```
  switch(flags & O_CREAT){
    case O_CREAT:
      printk("creating file\n");
      fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1), flags);
      dir_close(searched_record.parent_dir);
      break;
    default:
      /* 其余情况均为打开已存在文件 */
      fd = file_open(inode_no, flags);
  }

```


### 2.文件的关闭
关闭文件比较简单，如下：
```
/* 关闭文件 */
int32_t file_close(struct file* file){
  if(file == NULL){
    return -1;
  }
  file->fd_inode->write_deny = false;
  inode_close(file->fd_inode);
  file->fd_inode = NULL;
  return 0;
}

```

这里就是一些释放空间和置初值了，然后我们到fs/fs.c中还需要实现两个函数：
```
/* 将文件描述符转化为文件表的下标 */
static uint32_t fd_local2global(uint32_t local_fd){
  struct task_struct* cur = running_thread();
  int32_t global_fd = cur->fd_table[local_fd];
  ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);
  return (uint32_t)global_fd;
}

/* 关闭文件描述符所指向的文件.成功返回0,否则返回-1 */
int32_t sys_close(int32_t fd){
  int32_t ret = -1;
  if(fd > 2){
    uint32_t _fd = fd_local2global(fd);
    ret = file_close(&file_table[_fd]);
    running_thread()->fd_table[fd] = -1;    //使该进程的描述符可用
  }
  return ret;
}

```

这一部分十分简单，我们立马到main函数中测试：
```
int main(void){
  put_str("I am Kernel\n");
  init_all();

  intr_enable();
  //process_execute(u_prog_a, "u_prog_a");
  //process_execute(u_prog_b, "u_prog_b");
  //thread_start("k_thread_a", 31, k_thread_a, " A_");
  //thread_start("k_thread_b", 31, k_thread_b, " B_");
  uint32_t fd = sys_open("/file1", O_RDONLY);
  printf("fd:%d\n", fd);
  sys_close(fd);
  printf("%d closed now\n", fd);
 while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}

```

![](http://imgsrc.baidu.com/forum/pic/item/8718367adab44aed93f78623f61c8701a08bfbb8.jpg)

## 0x03 总结
就目前来说咱们对于文件系统还远远没有完成，可见他的代码量之多，现在仅仅实现了创建文件以及打开和关闭文件这几个基本功能，还有比较重要的将在之后讲解。
本章代码多确实没办法，因为原理始终是那么简单的，说也说不了很多，但是他代码长也是没办法。
