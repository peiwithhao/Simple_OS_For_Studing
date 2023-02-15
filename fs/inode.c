#include "inode.h"
#include "global.h"
#include "debug.h"
#include "stdint.h"
#include "ide.h"
#include "fs.h"
#include "super_block.h"
#include "dir.h"
#include "thread.h"
#include "interrupt.h"
#include "file.h"

/* 用来存储inode位置 */
struct inode_position{
  bool two_sec;     //inode是否跨扇区
  uint32_t sec_lba;     //inode所在的扇区号
  uint32_t off_size;    //inode在扇区内的字节偏移量
};

/* 获取inode所在的扇区和扇区内的偏移量 */
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
      inode_found->i_open_cnts++;   //被打开的inode节点数加一
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

/* 将硬盘分区的inode清空 */
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf){
  ASSERT(inode_no < 4096);
  struct inode_position inode_pos;  //存放该inode_no的硬盘位置信息
  inode_locate(part, inode_no, &inode_pos);
  ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));
  char* inode_buf = (char*)io_buf;
  if(inode_pos.two_sec){    //若跨一个扇区
    /* 首先将原硬盘上的内容先读出来 */
    ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    /* 这里将inode_buf对应的位置清0 */
    memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
    /* 将修改过的buf写入硬盘 */
    ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
  }else{        //这里说明存在于一个扇区之中
    /* 首先将原硬盘上的内容先读出来 */
    ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    /* 这里将inode_buf对应的位置清0 */
    memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
    /* 将修改过的buf写入硬盘 */
    ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
  }
}

/* 回收inode的数据块和inode本身 */
void inode_release(struct partition* part, uint32_t inode_no){
  struct inode* inode_to_del = inode_open(part, inode_no);
  ASSERT(inode_to_del->i_no == inode_no);
  /* 1.先回收inode占用的块 */
  uint8_t block_idx = 0, block_cnt = 12;
  uint32_t block_bitmap_idx;
  uint32_t all_blocks[140] = {0};       //12个直接块和128个间接块
  /* a 先将12个直接块存入all_blocks */
  while(block_idx < 12){
    all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
    block_idx++;
  }
  /* b 如果一级间接表存在，将其128个间接块读到all_blocks[12~]当中，并且释放以及间接块表所占的扇区 */
  if(inode_to_del->i_sectors[12] != 0){
    ide_read(part->my_disk, inode_to_del->i_sectors[12], all_blocks + 12, 1);
    block_cnt = 140;
    /* 回收一级间接块表占用的扇区，这里是因为没必要用了，扇区地址已经收集完了 */
    block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
    ASSERT(block_bitmap_idx > 0);
    bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
  }
  /* c inode所有的块地址已经收集到all_blocks中，下面逐个回收 */
  block_idx = 0;
  while(block_idx < block_cnt){
    if(all_blocks[block_idx] != 0){
      block_bitmap_idx = 0;
      block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
      ASSERT(block_bitmap_idx > 0);
      bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    }
    block_idx++;
  }
  /* 2.回收该inode所占用的inode空间 */
  bitmap_set(&part->inode_bitmap, inode_no, 0);
  bitmap_sync(cur_part, inode_no, INODE_BITMAP);

  /********** 以下inode_delete是调试用的  *********
   * 此函数会在inode_table中将此inode清0,
   * 但实际上是不需要的，inode分配是由inode位图控制的，
   * 硬盘上的数据不需要清0, 可以直接覆盖 */
  void* io_buf = sys_malloc(1024);
  inode_delete(part, inode_no, io_buf);
  sys_free(io_buf);
  /************************************************/
  inode_close(inode_to_del);
}
