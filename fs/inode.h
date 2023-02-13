#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "stdint.h"
#include "global.h"
#include "list.h"
#include "ide.h"

/* inode结构 */
struct inode{
  uint32_t i_no;
  /* 当此inode指向普通文件的时候，i_size是文件大小，若是目录，i_size是指该目录下所有目录项大小之和 */
  uint32_t i_size;
  uint32_t i_open_cnts;         //该文件被打开的次数
  bool write_deny;              //写文件标识
  /* i_sectors[0-11]是直接索引，i_sectors[12]用来存储一级间接索引指针 */
  uint32_t i_sectors[13];
  struct list_elem inode_tag;   //用于维护已打开的inode队列
};

struct inode* inode_open(struct partition* part, uint32_t inode_no);
void inode_sync(struct partition* part, struct inode* inode, void* io_buf);
void inode_init(uint32_t inode_no, struct inode* new_inode);
void inode_close(struct inode* inode);
#endif
