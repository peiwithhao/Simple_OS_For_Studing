#include "file.h"
#include "fs.h"
#include "thread.h"
#include "global.h"
#include "string.h"
#include "inode.h"
#include "debug.h"
#include "memory.h"
#include "interrupt.h"
#include "stdio-kernel.h"
#include "super_block.h"

#define DEFAULT_SECS 1

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
int32_t inode_bitmap_alloc(struct partition* part){
  int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
  if(bit_idx == -1){
    return -1;
  }
  bitmap_set(&part->inode_bitmap, bit_idx, 1);
  return bit_idx;
}

/* 分配1个扇区，返回扇区地址 */
int32_t block_bitmap_alloc(struct partition* part){
  int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
  if(bit_idx == -1){
    return -1;
  }
  bitmap_set(&part->block_bitmap, bit_idx, 1);
  return (part->sb->data_start_lba + bit_idx);
}

/* 将内存中的bitmap第bit_idx位所在的512字节同步到硬盘 */
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp){
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
  if(inode_no == -1){
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

/* 把buf当中的count个字节写入file，成功则返回写入的字节数，失败则返回-1 */
int32_t file_write(struct file* file, const void* buf, uint32_t count){
  if((file->fd_inode->i_size + count) > (BLOCK_SIZE * 140)){
    //上面是支持文件的最大字节
    printk("exceed max file_size 71680 bytes, write file failed\n");
    return -1;
  }
  uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
  if(io_buf == NULL){
    printk("file_write: sys_malloc for io_buf failed\n");
    return -1;
  }
  uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);    //用来记录文件所有的块地址，也就是128间接块+12直接块
  if(all_blocks == NULL){
    printk("file_write: sys_malloc for all_blocks failed\n");
    return -1;
  }

  const uint8_t* src = buf;     //src指向buf中待写入的数据
  uint32_t bytes_written = 0;   //用来记录已经写入数据大小
  uint32_t size_left = count;   //用来记录未写入数据大小
  int32_t block_lba = -1;       //块地址
  uint32_t block_bitmap_idx = 0;    //用来记录block对应于block_bitmap中的索引，作为参数传给bitmap_sync
  uint32_t sec_idx;         //用来索引扇区
  uint32_t sec_lba;         //扇区地址
  uint32_t sec_off_bytes;   //扇区内字节偏移量
  uint32_t sec_left_bytes;  //扇区内剩余字节量
  uint32_t chunk_size;      //每次写入硬盘的数据块大小
  int32_t indirect_block_table;     //用来获取一级间接表地址
  uint32_t block_idx;       //块索引

  /* 判断文件是否是第一次写，如果是，先为其分配一个块 */
  if(file->fd_inode->i_sectors[0] == 0){
    block_lba = block_bitmap_alloc(cur_part);       //在内存中的空闲位图分配
    if(block_lba == -1){
      printk("file_write: block_bitmap_alloc failed\n");
      return -1;
    }
    file->fd_inode->i_sectors[0] = block_lba;
    /* 每分配一个块，咱们就需要将位图同步到硬盘 */
    block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
    ASSERT(block_bitmap_idx != 0);
    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
  }
  /* 写入count个字节前，该文件已经占用的块数 */
  uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
  /* 存储count字节后该文件将占用的块数 */
  uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
  ASSERT(file_will_use_blocks <= 140);

  /* 通过此增量判断是否需要分配扇区，如增量为0, 表示原扇区够用 */
  uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;
  /* 将写文件所用到的快地址收集到all_blocks，系统中块大小等于扇区大小，然后后面统一在all_blocks中获取写入地址 */
  if(add_blocks == 0){
    /* 在同一扇区内写入数据，不涉及到分配新扇区 */
    if(file_will_use_blocks <= 12){     //文件总数据量在12之类，也就是仅仅是直接块
      block_idx = file_has_used_blocks - 1;     //指向最后一个已有数据的扇区，我们将会加入数据
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
    }else{
      /* 说明我们需要往间接块写入数据 */
      ASSERT(file->fd_inode->i_sectors[12] != 0);   //咱们的十二号下标指向的是一个一级间接地址块
      indirect_block_table = file->fd_inode->i_sectors[12];
      ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);    //在这里填入all_blocks后面的所有地址数据
    }
  }else{
    /* 如果说写入数据后会增加块数量，则分为三种情况 */
    /* 第一种情况：12个直接块足够 */
    if(file_will_use_blocks <= 12){
      /* 先将有剩余空间的可继续用的扇区地址写入all_blocks */
      block_idx =file_has_used_blocks - 1;
      ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
      /* 再将未来要用的扇区分配好后写入all_blocks */
      block_idx = file_has_used_blocks;     //指向第一个需要分配的新扇区
      while(block_idx < file_will_use_blocks){
        block_lba = block_bitmap_alloc(cur_part);
        if(block_lba == -1){
          printk("file_write: block_bitmap_alloc for situation 1 failed\n");
          return -1;
        }
        /* 写文件的时候不应该存在块未使用，但已经分配扇区的情况，写文件删除的时候，应该把块地址清0 */
        ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
        file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;

        /* 每分配一个块就应该同步到硬盘 */
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
        block_idx++;    //下一个分配的新扇区
      }
    }else if(file_has_used_blocks <= 12 && file_will_use_blocks > 12){
      /* 第二种情况：旧数据在12个直接块内，新数据跨越在间接块中 */
      /* 先将有剩余空间的可继续使用的扇区地址收集到all_blocks */
      block_idx = file_has_used_blocks - 1;
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
      /* 创建一级间接块表 */
      block_lba = block_bitmap_alloc(cur_part);
      if(block_lba == -1){
        printk("file_write:block_bitmap_alloc for situation 2 failed\n");
        return -1;
      }
      /* 同步位图 */
      block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

      ASSERT(file->fd_inode->i_sectors[12] == 0);   //确保此时一级间接块未分配
      /* 分配一级间接块索引表 */
      indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;
      block_idx = file_has_used_blocks;     //这里表示第一个未使用的块
      while(block_idx < file_will_use_blocks){
        block_lba = block_bitmap_alloc(cur_part);
        if(block_lba == -1){
          printk("file_write: blok bitmap alloc for situation 2 failed\n");
          return -1;
        }
        if(block_idx < 12){
          /* 新创建的0～11块直接存入all_blocks数组 */
          ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
          file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
        }else{  //间接块只写入到all_blocks数组中，待全部分配完成后一次性同步到硬盘
          all_blocks[block_idx] = block_lba;
        }
        /* 每分配一个块，就将位图同步到硬盘 */
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
        block_idx++;    //下一个新扇区
      }
      ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);   //同步一级间接块到硬盘
    }else if(file_has_used_blocks > 12){
      /* 第三种情况：新数据占用间接块 */
      ASSERT(file->fd_inode->i_sectors[12] != 0);   //这里检测是否已经存在一级间接块
      indirect_block_table = file->fd_inode->i_sectors[12];     //获取一级间接表地址
      /* 已经使用的间接块也将被读入all_blocks */
      ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);    //获取所有间接块地址
      block_idx = file_has_used_blocks;     //第一个未使用的间接块
      while(block_idx < file_will_use_blocks){
        block_lba = block_bitmap_alloc(cur_part);
        if(block_lba == -1){
          printk("file_write:block_bitmap_alloc for situation 3 failed\n");
          return -1;
        }
        all_blocks[block_idx++] = block_lba;
        /* 每分配一个块就将位图同步到硬盘 */
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
      }
      ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);   //同步一级间接块到硬盘
    }
  }
  /* 截止目前，我们所需要用到的地址已经全部收集到all_blocks当中，下面开始写数据  */
  bool first_write_block = true;    //含有剩余空间块的标识
  file->fd_pos = file->fd_inode->i_size - 1;    //将偏移指针指向文件末尾，下面在写的时候随时更新
  while(bytes_written < count){
    memset(io_buf, 0, BLOCK_SIZE);  //清空缓冲区
    sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
    sec_lba = all_blocks[sec_idx];
    sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
    sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

    /* 判断此次写入的硬盘数据大小 */
    chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
    if(first_write_block){      //如果说是第一次写，这说明里面可能有以前的数据，所以先读出
      ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
      first_write_block = false;
    }
    memcpy(io_buf + sec_off_bytes, src, chunk_size);
    ide_write(cur_part->my_disk, sec_lba, io_buf, 1);
    printk("file write at lba 0x%x\n", sec_lba);    //调试用
    src += chunk_size;                      //将源数据指针更新
    file->fd_inode->i_size += chunk_size;   //更新文件大小
    file->fd_pos += chunk_size;
    bytes_written += chunk_size;
    size_left -= chunk_size;
  }
  inode_sync(cur_part, file->fd_inode, io_buf);
  sys_free(all_blocks);
  sys_free(io_buf);
  return bytes_written;
}

/* 从文件file当中读取count个字节写入buf,返回读出的字节数，若到文件尾则返回-1 */
int32_t file_read(struct file* file, void* buf, uint32_t count){
  uint8_t* buf_dst = (uint8_t*)buf;
  uint32_t size = count, size_left = size;
  /* 若要读取的字节数超过了文件可读取的剩余量，就使用剩余量作为待读取的字节数 */
  if((file->fd_pos + count) > file->fd_inode->i_size){
    size = file->fd_inode->i_size - file->fd_pos;
    size_left = size;
    if(size == 0){      //若为文件尾，则返回-1
      return -1;
    }
  }
  uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
  if(io_buf == NULL){
    printk("file_read: sys_malloc for io_buf failed\n");
    return -1;
  }
  uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);    //同sys_write类似
  if(all_blocks == NULL){
    printk("file_read: sys_malloc for io_buf failed\n");
    return -1;
  }

  uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;    //数据快所在的起始扇区地址
  uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;     //数据所在块的终止扇区地址
  uint32_t read_blocks = block_read_end_idx - block_read_start_idx;     //如果增量为0,表示数据在同一扇区
  ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);
  int32_t indirect_block_table;         //获取一级间接表地址
  uint32_t block_idx;   //获取待读取的块地址
  /* 下面开始构建all_blocks块地址数组，这里同样分情况 */
  if(read_blocks == 0){         //在同一扇区内读数据，不涉及到跨扇区读取
    ASSERT(block_read_end_idx == block_read_start_idx);
    if(block_read_start_idx < 12){
      block_idx = block_read_start_idx;
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
    }else{      //这说明是在一级间接块当中分配数据
      indirect_block_table = file->fd_inode->i_sectors[12];
      ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
    }
  }else{        //若要读取多个块，下面会有三种情况
    /* 第一种情况：起始块和终止块属于直接块 */
    if(block_read_end_idx < 12){
      block_idx = block_read_start_idx;
      while(block_idx < block_read_end_idx){
        all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        block_idx++;
      }
    }else if(block_read_start_idx < 12 && block_read_end_idx >= 12){
        /* 第二种情况：起始块和终止快一个是直接块一个是间接块 */
        /* 我们先将直接块写入all_blocks */
        block_idx = block_read_start_idx;
        while(block_idx < 12){
          all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
          block_idx++;
        }
        ASSERT(file->fd_inode->i_sectors[12] != 0);     //确保已经分配了一级间接块
        /* 这里咱们再将间接块写入all_blocks */
        indirect_block_table = file->fd_inode->i_sectors[12];
        ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);  //写入间接块的地址们
    }else{ 
      /* 第三种情况，数据均在间接块当中 */
      ASSERT(file->fd_inode->i_sectors[12] != 0);   //确保一级间接块已经分配
      indirect_block_table = file->fd_inode->i_sectors[12];
      ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
    }
  }
  /* 用到的块地址已经全部收集到all_blocks当中，下面开始读取 */
  uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
  uint32_t bytes_read = 0;
  while(bytes_read < size){
    sec_idx = file->fd_pos / BLOCK_SIZE;
    sec_lba = all_blocks[sec_idx];
    sec_off_bytes = file->fd_pos % BLOCK_SIZE;
    sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
    chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;   //这里是本轮循环待读取大小
    memset(io_buf, 0, BLOCK_SIZE);      //不清空也可以
    ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
    memcpy(buf_dst, io_buf + sec_off_bytes, chunk_size);
    buf_dst += chunk_size;
    file->fd_pos += chunk_size;
    bytes_read += chunk_size;
    size_left -= chunk_size;
  }
  sys_free(all_blocks);
  sys_free(io_buf);
  return bytes_read;
}
