#include "dir.h"
#include "super_block.h"
#include "ide.h"
#include "inode.h"
#include "stdint.h"
#include "global.h"
#include "memory.h"
#include "string.h"
#include "interrupt.h"
#include "fs.h"
#include "stdint.h"
#include "stdio-kernel.h"
#include "debug.h"
#include "file.h"

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
    ide_read(part->my_disk, all_blocks[block_idx], buf, 1);
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

/* 把part分区当中pdir中编号为inode_no的目录项删除 */
bool delete_dir_entry(struct partition* part, struct dir* pdir, uint32_t inode_no, void* io_buf){
  struct inode* dir_inode = pdir->inode;
  uint32_t block_idx = 0, all_blocks[140] = {0};    //收集全部块地址
  while(block_idx < 12){
    all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    block_idx++;
  }
  if(dir_inode->i_sectors[12]){
    ide_read(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
  }

  /* 目录项存储的时候肯定不会跨区*/
  uint32_t dir_entry_size = part->sb->dir_entry_size;
  uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);     //每扇区的最大目录项数目
  struct dir_entry* dir_e = (struct dir_entry*)io_buf;
  struct dir_entry* dir_entry_found = NULL;
  uint8_t dir_entry_idx, dir_entry_cnt;
  bool is_dir_first_block = false;  //目录的第一个块
  
  /* 遍历所有块，寻找目录项 */
  block_idx = 0;
  while(block_idx < 140){
    is_dir_first_block = false;
    if(all_blocks[block_idx] == 0){
      block_idx++;
      continue;
    }
    dir_entry_idx = dir_entry_cnt = 0;
    memset(io_buf, 0, SECTOR_SIZE);
    /* 读取扇区，获得目录项 */
    ide_read(part->my_disk, all_blocks[block_idx], io_buf, 1);

    /* 遍历所有目录项，统计该扇区当中的目录项数量以及是否有待删除的目录项 */
    while(dir_entry_idx < dir_entrys_per_sec){
      if((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN){
        if(!strcmp((dir_e + dir_entry_idx)->filename, ".")){
          is_dir_first_block = true;
        }else if(strcmp((dir_e + dir_entry_idx)->filename, ".") && strcmp((dir_e + dir_entry_idx)->filename, "..")){
          dir_entry_cnt++;  //统计此扇区当中目录项的个数，用来判断删除目录项后是否回收该扇区
          if((dir_e + dir_entry_idx)->i_no == inode_no){
            //如果找到此节点，则将其记录在dir_entry_found当中
            ASSERT(dir_entry_found == NULL);        //确保目录中只有一个编号为inode_no的inode，找到一次后dir_entry_found就不是NULL
            dir_entry_found = dir_e + dir_entry_idx;
            /* 找到后继续遍历，统计总共的目录项数目 */
          }
        }
      }
      dir_entry_idx++;
    }
    /* 若此扇区未找到该目录项，则继续在下一个扇区寻找 */
    if(dir_entry_found == NULL){
      block_idx++;
      continue;
    }
    /* 走到这里说明找到了已经，目前应该清楚该目录项并判断是否收回扇区，随后推出循环直接返回 */
    ASSERT(dir_entry_cnt >= 1);
    /* 除了目录第1个扇区外，若该扇区上只有该目录项他自己，则将整个扇区回收 */
    if(dir_entry_cnt == 1 && !is_dir_first_block){
      /* a 在块位图中回收该块 */
      uint32_t block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
      bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

      /* b 将块地址从数组i_sectors或索引表中去掉 */
      if(block_idx < 12){   //在直接块当中
        dir_inode->i_sectors[block_idx] = 0;
      }else{    //在一级简介索引表中
        /* 先判断一级间接索引表中擦除该间接块的地址，如果仅有这1个间接块，则连同间接索引表所在的块一同回收 */
        uint32_t indirect_blocks = 0;
        uint32_t indirect_blocks_idx = 12;
        while(indirect_blocks_idx < 140){
          if(all_blocks[indirect_blocks_idx] != 0){
            indirect_blocks++;
          }
        }
        ASSERT(indirect_blocks >= 1);     //包括当前间接块
        if(indirect_blocks > 1){    //表示间接索引表中除了当前块还有别的块
          all_blocks[block_idx] = 0;
          ide_write(part->my_disk, dir_inode->i_sectors[12],  all_blocks + 12, 1);
        }else{          //这里表示间接索引表只有当前块，所以连着索引块一起清除
          block_bitmap_idx = dir_inode->i_sectors[12] - part->sb->data_start_lba;
          bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
          bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
          /* 将间接索引表地址清0 */
          dir_inode->i_sectors[12] = 0;
        }
      }
    }else{  //这里表示不需要清楚块，直接将目录项清空就行
      memset(dir_entry_found, 0, dir_entry_size);
      ide_write(part->my_disk, all_blocks[block_idx], io_buf, 1);
    }
    /* 更新i结点信息并同步到硬盘 */
    ASSERT(dir_inode->i_size >= dir_entry_size);
    dir_inode->i_size -= dir_entry_size;
    memset(io_buf, 0, SECTOR_SIZE * 2);     //这里传入1024字节的buf是因为inode_sync可能会使用到2个扇区大小的缓冲区
    inode_sync(part, dir_inode, io_buf);
    return true;
  }
  return false;
}
