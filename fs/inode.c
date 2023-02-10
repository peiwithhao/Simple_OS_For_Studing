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

