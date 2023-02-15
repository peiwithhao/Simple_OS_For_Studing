#include "fs.h"
#include "ide.h"
#include "stdint.h"
#include "global.h"
#include "super_block.h"
#include "inode.h"
#include "dir.h"
#include "string.h"
#include "memory.h"
#include "debug.h"
#include "file.h"
#include "list.h"
#include "stdio-kernel.h"
#include "inode.h"
#include "console.h"
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


/* 格式化分区，也就是初始化分区的元信息，创建文件系统 */
static void partition_format(struct partition* part){
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
  /* 确定默认操作的分区 */
  char default_part[8] = "sdb1";
  /* 挂载分区 */
  list_traversal(&partition_list, mount_partition, (int)default_part);

  /* 将当前分区的根目录打开 */
  open_root_dir(cur_part);
  /* 初始化文件表 */
  uint32_t fd_idx = 0;
  while(fd_idx < MAX_FILE_OPEN){
    file_table[fd_idx++].fd_inode = NULL;
  }
}

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

/* 打开或创建文件成功后，返回文件描述符，否则返回-1 */
int32_t sys_open(const char* pathname, uint8_t flags){
  /* 对于目录需要使用dir_open，这里暂时只有open文件 */
  if(pathname[strlen(pathname) - 1] == '/'){
    printk("can not open a dirctory %s\n", pathname);
    return -1;
  }
  ASSERT(flags <= 7);
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
      break;
    default:
      /* 其余情况均为打开已存在文件 */
      fd = file_open(inode_no, flags);
  }
  /* 此fd是指任务pcb->fd_table数组中的下标 */
  return fd;
}

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

/* 将buf中连续count个字节写入文件描述符fd，成功则返回写入的字节数，失败返回-1 */
int32_t sys_write(int32_t fd, const void* buf, uint32_t count){
  if(fd < 0){
    printk("sys_write: fd error\n");
    return -1;
  }
  if(fd == stdout_no){      //如果说是标准输出，则直接在屏幕上输出
    char tmp_buf[1024] = {0};
    memcpy(tmp_buf, buf, count);
    console_put_str(tmp_buf);
    return count;
  }
  uint32_t _fd = fd_local2global(fd);
  struct file* wr_file = &file_table[_fd];
  if(wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR){
    uint32_t bytes_written = file_write(wr_file, buf, count);
    return bytes_written;
  }else{
    console_put_str("sys_write: not allowed to write file without flag O_RDWR or O_WRONLY\n");
    return -1;
  }
}

/* 从文件描述符fd指向的文件中读取count个字节到buf，若成功则返回读出的字节数，失败则返回-1 */
int32_t sys_read(int32_t fd, void* buf, uint32_t count){
  if(fd < 0){
    printk("sys_read: fd error\n");
    return -1;
  }
  ASSERT(buf != NULL);
  uint32_t _fd = fd_local2global(fd);
  return file_read(&file_table[_fd], buf, count);
}

/* 重置文件的偏移指针，成功时则返回偏移量，失败的时候返回-1 */
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence){
  if(fd < 0){
    printk("sys_lseek: fd error\n");
    return -1;
  }
  ASSERT(whence > 0 && whence < 4);
  uint32_t _fd = fd_local2global(fd);
  struct file* pf = &file_table[_fd];
  int32_t new_pos = 0;  //新的偏移量大小必须位于文件大小之内
  int32_t file_size = (int32_t)pf->fd_inode->i_size;
  switch(whence){
    case SEEK_SET:
      new_pos = offset;
      break;
    case SEEK_CUR:
      new_pos = (int32_t)pf->fd_pos + offset;
      break;
    case SEEK_END:
      new_pos = file_size + offset;
  }
  if(new_pos < 0 || new_pos > (file_size - 1)){
    return -1;
  }
  pf->fd_pos = new_pos;
  return pf->fd_pos;
}

/* 删除文件，非目录，成功返回0,失败返回-1 */
int32_t sys_unlink(const char* pathname){
  ASSERT(strlen(pathname) < MAX_PATH_LEN);
  /* 首先检查待删除的文件是否存在 */
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_no = search_file(pathname, &searched_record);
  ASSERT(inode_no != 0);
  if(inode_no == -1){
    printk("file %s not found!\n", pathname);
    dir_close(searched_record.parent_dir);
    return -1;
  }
  if(searched_record.file_type == FT_DIRECTORY){
    printk("can't delete a directory with unlink(),use rmdir() to instead\n");
    dir_close(searched_record.parent_dir);
    return -1;
  }
  /* 检查是否在已经打开的文件列表中 */
  uint32_t file_idx = 0;
  while(file_idx < MAX_FILE_OPEN){
    if(file_table[file_idx].fd_inode != NULL && (uint32_t)inode_no == file_table[file_idx].fd_inode->i_no){
      break;
    }
    file_idx++;
  }
  if(file_idx < MAX_FILE_OPEN){     //这里说明该文件正在被打开，无法删除
    dir_close(searched_record.parent_dir);
    printk("file %s is in use, not allow to delete!\n", pathname);
    return -1;
  }
  ASSERT(file_idx == MAX_FILE_OPEN);
  /* 为delete_dir_entry申请缓冲区 */
  void* io_buf = sys_malloc(SECTOR_SIZE + SECTOR_SIZE);
  if(io_buf == NULL){
    dir_close(searched_record.parent_dir);
    printk("sys_unlinkk: mallo for io_buf failed\n");
    return -1;
  }
  struct dir* parent_dir = searched_record.parent_dir;
  delete_dir_entry(cur_part, parent_dir, inode_no, io_buf);
  inode_release(cur_part, inode_no);
  sys_free(io_buf);
  dir_close(searched_record.parent_dir);
  return 0;     //成功删除文件
}

/* 创建目录pathname,成功就返回0,否则返回-1 */
int32_t sys_mkdir(const char* pathname){
  uint8_t rollback_step = 0;    //用于操作失败时的回滚
  void* io_buf = sys_malloc(SECTOR_SIZE * 2);
  if(io_buf == NULL){
    printk("sys_mkdir: sys_malloc for io_buf failed\n");
    return -1;
  }
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_no = -1;
  inode_no = search_file(pathname, &searched_record);
  if(inode_no != -1){   //说明找到了同名的目录或文件
    printk("sys_mkdir:file or directory %s exist\n", pathname);
    rollback_step = 1;
    goto rollback;
  }else{
    //若未找到，也要判断是最终目录没找到还是某个中间目录不存在
    uint32_t pathname_depth = path_depth_cnt((char*)pathname);
    uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
    /* 先判断是否把pathname的每个层目录都访问到了，即是否在某个中间目录就失败了 */
    if(pathname_depth != path_searched_depth){
      /* 这里说明某个中间目录不存在 */
      printk("sys_mkdir: cannot accsess %s: Not a directory,subpath %s is not exist\n",pathname, searched_record.searched_path);
      rollback_step = 1;
      goto rollback;
    }
  }
  /* 执行到这里说明没发现同名目录或文件 */
  struct dir* parent_dir = searched_record.parent_dir;
  /* 目录名称后可能会有字符'/',所以最好直接用searched_record.searched_path */
  char* dirname = strrchr(searched_record.searched_path, '/') + 1;  //这里是获取咱们想要创建的目录名称
  inode_no = inode_bitmap_alloc(cur_part);
  if(inode_no == -1){
    printk("sys_mkdir: allocate inode failed\n");
    rollback_step = 1;
    goto rollback;
  }
  struct inode new_dir_inode;
  inode_init(inode_no, &new_dir_inode);     //初始化i节点
  uint32_t block_bitmap_idx = 0;    //用来记录block对应于block_bitmap中的索引
  uint32_t block_lba = -1;
  /* 首先为目录分配一个块，用来写入目录.和.. */
  block_lba = block_bitmap_alloc(cur_part);
  if(block_lba == -1){
    printk("sys_mkdir:block bitmap alloc for create directory failed\n");
    rollback_step = 2;
    goto rollback;
  }
  new_dir_inode.i_sectors[0] = block_lba;
  /* 每分配一个块就同步到硬盘 */
  block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
  ASSERT(block_bitmap_idx != 0);
  bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

  /* 将当前目录的目录项'.'和'..'写入目录 */
  memset(io_buf, 0, SECTOR_SIZE * 2);   //清空io_buf
  struct dir_entry* p_de = (struct dir_entry*)io_buf;

  /* 初始化当前目录 "." */
  memcpy(p_de->filename, ".", 1);
  p_de->i_no = inode_no;
  p_de->f_type = FT_DIRECTORY;
  p_de++;
  /* 初始化当前目录".." */
  memcpy(p_de->filename, "..", 2);
  p_de->i_no = parent_dir->inode->i_no;
  p_de->f_type = FT_DIRECTORY;

  /* 将改好的目录文件写入 */
  ide_write(cur_part->my_disk, new_dir_inode.i_sectors[0], io_buf, 1);
  new_dir_inode.i_size = 2 * cur_part->sb->dir_entry_size;

  /* 在父目录当中添加自己的目录项 */
  struct dir_entry new_dir_entry;
  memset(&new_dir_entry, 0, sizeof(struct dir_entry));
  create_dir_entry(dirname, inode_no, FT_DIRECTORY, &new_dir_entry);
  memset(io_buf, 0, SECTOR_SIZE * 2);   ///清空io_buf
  if(!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)){
    //sync_dir_entry中将block_bitmap通过bitmap_sync同步到硬盘
    printk("sys_mkdir: sync_dir_entry to disk failed\n");
    rollback_step = 2;
    goto rollback;
  }
  /* 父目录的inode同步到硬盘 */
  memset(io_buf, 0, SECTOR_SIZE * 2);
  inode_sync(cur_part, parent_dir->inode, io_buf);
  /* 将新创建目录的inode同步到硬盘 */
  memset(io_buf, 0, SECTOR_SIZE * 2);
  inode_sync(cur_part, &new_dir_inode, io_buf);
  /* 将inode位图同步到硬盘 */
  bitmap_sync(cur_part, inode_no, INODE_BITMAP);
  sys_free(io_buf);
  /* 关闭所创建目录的父目录 */
  dir_close(searched_record.parent_dir);
  return 0;

  /* 创建文件或目录需要相关的多个资源，这里是回滚操作步骤 */
rollback:
  switch(rollback_step){
    case 2:
      bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
    case 1:
      dir_close(searched_record.parent_dir);
      break;
  }
  sys_free(io_buf);
  return -1;
}
