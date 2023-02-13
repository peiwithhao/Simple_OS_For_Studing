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
