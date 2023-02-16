## 0x00 遍历目录
### 1.打开和关闭目录
首先咱们要遍历目录的第一步就是要打开目录，遍历万之后需要关闭目录，因此我们先来实现这两个功能
```

/* 目录打开成功后返回目录指针，失败则返回NULL */
struct dir* sys_opendir(const char* name){
  ASSERT(strlen(name) < MAX_PATH_LEN);
  /* 如果是根目录'/'，直接返回&root_dir */
  if(name[0] = '/' && (name[1] == 0 || name[0] == '.')){
    return &root_dir;
  }
  /* 先检查待打开的目录是否存在 */
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_no = search_file(name, &searched_record);
  struct dir* dir = NULL;
  if(inode_no == -1){   //如果找不到就会提示不存在路径
    printk("In %s, sub path %s not exist\n ", name, searched_record.searched_path);
  }else{
    if(searched_record.file_type == FT_REGULAR){
      printk("%s is regular file\n", name);
    }else if(searched_record.file_type == FT_DIRECTORY){
      ret = dir_open(cur_part, inode_no);
    }
  }
  dir_close(searched_record.parent_dir);
  return ret;
}

/* 成功关闭目录p_dir返回0,失败返回-1 */
int32_t sys_closedir(struct dir* dir){
  int32_t ret = -1;
  if(dir != NULL){
    dir_close(dir);
    ret = 0;
  }
  return ret;
}

```

这里十分简单，也就是一些封装而以，我们立刻开始测试
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  printf("/dir1/subdir1 create %s!\n", sys_mkdir("/dir1/subdir1") == 0 ? "done" : "fail");
  printf("/dir1 create %s\n", sys_mkdir("/dir1") == 0 ? "done" : "fail");
  printf("now /dir1/subdir1 create %s!\n", sys_mkdir("/dir1/subdir1") == 0 ? "done" : "fail");
  struct dir* p_dir = sys_opendir("/dir1/subdir1");
  if(p_dir){
    printf("/dir1/subdir1 open done!\n");
    if(sys_closedir(p_dir) == 0){
      printf("/dir1/subdir1 close done!\n");
    }else{
      printf("/dir1/subdir1 close fail\n");
    }
  }else{
    printf("/dir1/subdir1 open fail\n");
  }
 while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}
```

测试结果如下发现确实成功执行了打开和关闭目录
![](http://imgsrc.baidu.com/super/pic/item/3812b31bb051f81916b2008d9fb44aed2f73e766.jpg)


### 2.打开一个目录项
我们遍历目录也就是打开一个个目录项，所以我们首先实现读取一个目录项，然后遍历只是循环访问这个函数就行了
```
/* 读取目录， 成功则返回1个目录项，失败则返回NULL */
struct dir_entry* dir_read(struct dir* dir){
  struct dir_entry* dir_e = (struct dir_entry*)dir->dir_buf;
  struct inode* dir_inode = dir->inode;
  uint32_t all_blocks[140] = {0}, block_cnt = 12;
  uint32_t block_idx = 0, dir_entry_idx = 0;
  while(block_idx < 12){
    all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    block_idx++;
  }
  if(dir_inode->i_sectors[12] != 0){    //如果含有一级间接块表
    ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
    block_cnt = 140;
  }
  block_idx = 0;
  uint32_t cur_dir_entry_pos = 0;   //记录当前目录项的偏移，此项用来判断是否之前已经返回过的目录项
  uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
  uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;   //1扇区可容纳的目录项个数
  /* 在目录大小内遍历 */
  while(dir->dir_pos < dir_inode->i_size){
    if(dir->dir_pos >= dir_inode->i_size){
      return NULL;
    }
    if(all_blocks[block_idx] == 0){
      //如果此块的地址为0,也就是为空，继续读出下一块
      block_idx++;
      continue;
    }
    memset(dir_e, 0, SECTOR_SIZE);
    ide_read(cur_part->my_disk, all_blocks[block_idx], dir_e, 1);   //读取该扇区里的所有目录项
    dir_entry_idx = 0;
    /* 遍历扇区内所有目录项 */
    while(dir_entry_idx < dir_entrys_per_sec){
      if((dir_e + dir_entry_idx)->f_type){  //如果f_type不等于0,也就是不等于FT_UNKNOWN
        /* 判断是不是最新的目录项，避免返回曾经已经返回过的目录项 */
        if(cur_dir_entry_pos < dir->dir_pos){
          cur_dir_entry_pos += dir_entry_size;
          dir_entry_idx++;
          continue;
        }
        ASSERT(cur_dir_entry_pos == dir->dir_pos);
        dir->dir_pos += dir_entry_size;     //更新为新位置，即下一个返回的目录项地址
        return dir_e + dir_entry_idx;
      }
      dir_entry_idx++;
    }
    block_idx++;
  }
  return NULL;
}

```

以上仅仅是读取一个目录项，接下来我们来补充相关函数
### 3.实现`sys_readdir`和`sys_rewinddir`
在Linux中读取目录的函数是readdir，我们也仿照他来实现，同样当遍历目录的时候我们经常会用到目录回绕的功能，这有点像lseek函数，因此咱们在这儿也一起实现
```

/* 读取目录dir的一个目录项，成功返回目录项地址，到目录尾时或出错返回NULL */
struct dir_entry* sys_readdir(struct dir* dir){
  ASSERT(dir != NULL);
  return dir_read(dir);
}

/* 将目录dir->dir_pos置为0 */
void sys_rewinddir(struct dir* dir){
  dir->dir_pos = 0;
}

```

函数十分的简单，记得添加到fs/fs.c当中,紧接着我们立刻开始测试，
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  struct dir* p_dir = sys_opendir("/dir1/subdir1");
  if(p_dir){
    printf("/dir1/subdir1 open done!\ncontent:\n");
    char* type = NULL;
    struct dir_entry* dir_e = NULL;
    while(dir_e = sys_readdir(p_dir)){
      if(dir_e->f_type == FT_REGULAR){
        type = "regular";
      }else{
        type = "directory";
      }
      printf("  %s %s\n", type, dir_e->filename);
    }
    if(sys_closedir(p_dir) == 0){
      printf("/dir1/subdir1 close done\n");
    }else{
      printf("/dir1/subdir1 close fail\n");
    }
  }else{
    printf("/dir1/subdir1 open fail\n");
  }
  while(1);
  return 0;
}

```

也就是输出目录下的目录项信息，结果如下：
![](http://imgsrc.baidu.com/super/pic/item/37d12f2eb9389b504c08e995c035e5dde6116e05.jpg)
果然输出了正常信息

## 0x01 删除目录
### 1.删除目录与判断空目录
我们实现了创建目录和遍历，当然我们还需要能够删除他，现在我们就首先来判断目录是否为空
```
/* 判断目录是否为空 */
bool dir_is_empty(struct dir* dir){
  struct inode* dir_inode = dir->inode;
  /* 如果说目录下只有"."和".."，则说明他为空 */
  return (dir_inode->i_size == cur_part->sb->dir_entry_size * 2);
}

/* 在父目录parent_dir当中删除child_dir */
int32_t dir_remove(struct dir* parent_dir, struct dir* child_dir){
  struct inode* child_dir_inode = child_dir->inode;
  /* 空目录只在inode->i_sectors[0]中有扇区，其他扇区都为空 */
  int32_t block_idx = 1;
  while(block_idx < 13){
    ASSERT(child_dir_inode->i_sectors[block_idx] == 0);
    block_idx++;
  }
  void* io_buf = sys_malloc(SECTOR_SIZE * 2);
  if(io_buf == NULL){
    printk("dir_remove: malloc for io_buf failed\n");
    return -1;
  }
  /* 在父目录当中删除子目录对应的目录项 */
  delete_dir_entry(cur_part, parent_dir, child_dir_inode->i_no, io_buf);
  /* 回收inode中的i_sectors中所占用的扇区，并同步inode_bitmap和block_bitmap */
  inode_release(cur_part, child_dir_inode->i_no);
  sys_free(io_buf);
  return 0;
}

```

### 2.实现功能sys_rmdir以及功能验证
这里我们分别实现了判断目录是否为空，然后实现在父目录当中删除空目录，而以上都是我们需要的功能函数，真正进行删除目录的函数还是`sys_rmdir`
```
/* 删除空目录，成功就返回0,失败返回-1 */
int32_t sys_rmdir(const char* pathname){
  /* 首先检查对应文件是否存在 */
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_no = search_file(pathname, &searched_record);
  ASSERT(inode_no != 0);
  int retval = -1;      //默认返回值
  if(inode_no == -1){
    printk("In %s, sub path %s not exist\n", pathname, searched_record.searched_path);
  }else{
    if(searched_record.file_type == FT_REGULAR){
      printk("%s is regular file!\n", pathname);
    }else{
      struct dir* dir = dir_open(cur_part, inode_no);
      if(!dir_is_empty(dir)){
        printk("dir %s is not empty, it is not allowed to delete a nonempty directory!\n", pathname);
      }else{
        if(!dir_remove(searched_record.parent_dir, dir)){
          retval = 0;
        }
      }
      dir_close(dir);
    }
  }
  dir_close(searched_record.parent_dir);
  return retval;
}

```

现在咱们就来测试一下目录删除功能
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  printf("/dir1 content before delete /dir1/subdir1:\n");
  struct dir* dir = sys_opendir("/dir1/");
  char* type = NULL;
  struct dir_entry* dir_e = NULL;
  while((dir_e = sys_readdir(dir))){
    if(dir_e->f_type == FT_REGULAR){
      type = "regular";
    }else{
      type = "directory";
    }
    printf("    %s  %s\n", type, dir_e->filename);
  }
  printf("try to delete nonempty directory /dir1/subdir1\n");
  if(sys_rmdir("/dir1/subdir1") == -1){
    printf("sys_rmdir: /dir1/subdir1 delete fail\n");
  }
  printf("try to delete /dir1/subdir1/file2\n");
  if(sys_rmdir("/dir1/subdir1/file2") == -1){
    printf("sys_rmdir: /dir1/subdir1/file2 delete fail\n");
  }
  if(sys_unlink("/dir1/subdir1/file2") == 0){
    printf("sys_unlink: /dir1/subdir1/file2 delete done\n");
  }
  printf("try to delete directory /dir1/subdir1 again\n");
  if(sys_rmdir("/dir1/subdir1") == 0){
    printf("/dir1/subdir1 delete done\n");
  }
  printf("/dir1 content after delete /dir1/subdir1:\n");
  sys_rewinddir(dir);
  while((dir_e = sys_readdir(dir))){
    if(dir_e->f_type == FT_REGULAR){
      type = "regular";
    }else{
      type = "directory";
    }
    printf("    %s  %s\n", type, dir_e->filename);
  }

  while(1);
  return 0;
}

```

上面代码就是简单的测试文件删除，结果如下可见十分成功：
![](http://imgsrc.baidu.com/super/pic/item/86d6277f9e2f0708bc3620c7ac24b899a801f2c2.jpg)


## 0x02 工作目录
### 1.显示当前工作目录
我们在Linux操作的时候经常采用pwd来获取当前工作路径，我们同时也会使用cd来切换目录，而他的实现原理也十分简单，就是通过".."获取当前目录的父目录，然后依次向上遍历，这样就可以获取绝对目录了。
为了辅助这项工作，我们先在fs.c当中实现一些基础功能，由于在前面咱们大量实现了一些基础代码，所以这里的功能代码都会十分简单
```
/* 获得父目录的inode编号 */
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr, void* io_buf){
  struct inode* child_dir_inode = inode_open(cur_part, child_inode_nr);
  /* 目录中的目录项".."包括父目录的inode编号， ".."位于目录的第0块 */
  uint32_t block_lba = child_dir_inode->i_sectors[0];
  ASSERT(block_lba >= cur_part->sb->data_start_lba);
  inode_close(child_dir_inode);
  ide_read(cur_part->my_disk, block_lba, io_buf, 1);
  struct dir_entry* dir_e = (struct dir_entry*)io_buf;
  /* 第0个目录项是"."，第1个目录项是".." */
  ASSERT(dir_e[1].i_no < 4096 && dir_e[1].f_type == FT_DIRECTORY);
  return dir_e[1].i_no;     //返回..也就是父目录的inode编号
}

/* 在inode编号为p_inode_nr的目录中查找inode编号为c_inode_nr的子目录的名字，将名字存入缓冲区path
 * 成功则返回0,失败返回-1 */
static int get_child_dir_name(uint32_t p_inode_nr, uint32_t c_inode_nr, char* path, void* io_buf){
  struct inode* parent_dir_inode = inode_open(cur_part, p_inode_nr);
  /* 填充all_blocks */
  uint8_t block_idx = 0;
  uint32_t all_blocks[140] = {0}, block_cnt = 12;
  while(block_idx < 12){
    all_blocks[block_idx] = parent_dir_inode->i_sectors[block_idx];
    block_idx++;
  }
  if(parent_dir_inode->i_sectors[12]){  //若包含了一级间接块表，就将其读入all_blocks 
    ide_read(cur_part->my_disk, parent_dir_inode->i_sectors[12], all_blocks + 12, 1);
    block_cnt = 140;
  }
  inode_close(parent_dir_inode);
  struct dir_entry* dir_e = (struct dir_entry*)io_buf;
  uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
  uint32_t dir_entrys_per_sec = (512 / dir_entry_size);
  block_idx = 0;
  /* 遍历所有块 */
  while(block_idx < block_cnt){
    if(all_blocks[block_idx]){
      ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
      uint8_t dir_e_idx = 0;
      /* 遍历每个目录项 */
      while(dir_e_idx < dir_entrys_per_sec){
        if((dir_e + dir_e_idx)->i_no == c_inode_nr){
          strcat(path, "/");
          strcat(path, (dir_e + dir_e_idx)->filename);
          return 0;
        }
        dir_e_idx++;
      }
    }
    block_idx++;
  }
  return -1;
}
```

这里我们分别实现了获取父目录i节点号和获取对应子目录名，这里只是功能函数，我们之后会使用到。
### 2.实现sys_getcwd
正如标题，我们这个函数是用来获取当前目录的绝对路径的，这里的当前目录涉及到了用户进程，所以我们需要到thread.h中修改PCB和初始化信息
```
uint32_t cwd_inode_nr
```

然后我们别忘了到thread.c中初始化这个项就行也就是`pthread->cwd_inode_nr = 0;` 
我们的默认工作目录是根目录'/'
然后我们来实现`sys_getcwd`函数
```
/* 把当前工作目录绝对路径写入buf，size是buf的大小
 * 当buf为NULL的时候，由操作系统分配存储工作路径的空间并返回地址，失败则返回NULL */
char* sys_getcwd(char* buf, uint32_t size){
  /* 确保buf不为空，若用户进程提供的buf为NULL，系统调用getcwd中要为用户进程通过mallo分配内存 */
  ASSERT(buf != NULL);
  void* io_buf = sys_malloc(SECTOR_SIZE);
  if(io_buf == NULL){
    return NULL;
  }

  struct task_struct* cur_thread = running_thread();
  int32_t parent_inode_nr = 0;
  int32_t child_inode_nr = cur_thread->cwd_inode_nr;
  ASSERT(child_inode_nr >= 0 && child_inode_nr < 4096);
  if(child_inode_nr == 0){  //如果说是根目录，直接返回'/'
    buf[0] = '/';
    buf[1] = 0;
    return buf;
  }
  memset(buf, 0, size);
  char full_path_reverse[MAX_PATH_LEN] = {0};   //用来存放全路径缓冲区
  /* 从下往上逐层找父目录，直到找到根目录为止，当child_inode_nr为根目录的inode编号0停止 */
  while((child_inode_nr)){
    parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr, io_buf);
    if(get_child_dir_name(parent_inode_nr, child_inode_nr, full_path_reverse, io_buf) == -1){   //若未找到名字，失败退出
      sys_free(io_buf);
      return NULL;
    }
    child_inode_nr = parent_inode_nr;
  }
  ASSERT(strlen(full_path_reverse) >= size);
  /* 至此full_path_reverse中的路径是反过来的，
   * 现在我们将其反置*/
  char* last_slash;     //用于记录字符串最后一个斜杠地址
  while((last_slash = strrchr(full_path_reverse, '/'))){
    uint16_t len = strlen(buf);     //由于咱们最开始清0,所以这里len第一次应该是0,然后依次增加
    strcpy(buf + len, last_slash);
    /* 在full_path_reverse中添加结束字符，作为下一次执行strcpy中的last_slash的边界 */
    *last_slash = 0;
  }
  sys_free(io_buf);
  return buf;
}

```

### 3.实现sys_chdir改变工作目录
这里其实很简单，也就是修改PCB就行了
```
/* 更改当前工作目录为绝对路径path，成功则返回0,失败返回-1 */
int32_t sys_chdir(const char* path){
  int32_t ret = -1;
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_no = search_file(path, &searched_record);
  if(inode_no != -1){
    if(searched_record.file_type == FT_DIRECTORY){
      running_thread()->cwd_inode_nr = inode_no;
      ret = 0;
    }else{
      printk("sys_chdir: %s is regular file or other\n", path);
    }
  }
  dir_close(searched_record.parent_dir);
  return ret;
}
```

然后我们就开始测试,同样的修改main函数：
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  char cwd_buf[32] = {0};
  sys_getcwd(cwd_buf, 32);
  printf("cwd:%s\n", cwd_buf);
  sys_chdir("/dir1");
  printf("change cwd now\n");
  sys_getcwd(cwd_buf, 32);
  printf("cwd:%s\n", cwd_buf);
  while(1);
  return 0;
}
```

我们先打印当前工作目录然后切换目录，十分简单，结果如下
![](http://imgsrc.baidu.com/super/pic/item/960a304e251f95cae296e7718c177f3e66095291.jpg)

## 0x03 获得文件属性
有了咱们之前的基础，这一部分简单的不能再简单，我们只需要再实现一个系统调用内核实现就行，首先咱们定义一个文件属性结构体，在fs/fs.h中定义
```
/* 文件属性结构体 */
struct stat{
  uint32_t st_ino;                  //inode编号
  uint32_t st_size;                 //尺寸
  enum file_types st_filetype;      //文件类型
};

```

接下来我们实现sys_stat函数来获取信息
```
/* 在buf中填充文件结构相关信息，成功则返回0，失败返回-1 */
int32_t sys_stat(const char* path, struct stat* buf){
  /* 若直接查看根目录'/', */
  if(!strcmp(path, "/") || !strcmp(path, "/.") || !strcmp(path, "/..")){
    buf->st_filetype = FT_DIRECTORY;
    buf->st_ino = 0;
    buf->st_size = root_dir.inode->i_size;
    return 0;
  }
  int32_t ret = -1;     //默认返回值
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));   //初始化记录
  int inode_no = search_file(path, &searched_record);
  if(inode_no != -1){
    struct inode* obj_inode = inode_open(cur_part, inode_no);
    buf->st_size = obj_inode->i_size;
    inode_close(obj_inode);
    buf->st_filetype = searched_record.file_type;
    buf->st_ino = inode_no;
    ret = 0;
  }else{
    printk("sys_stat: %s not found\n", path);
  }
  dir_close(searched_record.parent_dir);
  return ret;
}

```

函数十分简单，我们继续到main函数中检测
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  struct stat obj_stat;
  sys_stat("/", &obj_stat);
  printf("/'s info\n    i_no:%d\n   size:%d\n   filetype:%s\n", \
      obj_stat.st_ino, obj_stat.st_size, \
      obj_stat.st_filetype == 2 ? "directory" : "regular");
  sys_stat("/dir1", &obj_stat);
  printf("/dir1's info\n    i_no:%d\n   size:%d\n   filetype:%s\n", \
      obj_stat.st_ino, obj_stat.st_size, \
      obj_stat.st_filetype == 2 ? "directory" : "regular");
  while(1);
  return 0;
}
```

紧接着结果如下:
![](http://imgsrc.baidu.com/super/pic/item/adaf2edda3cc7cd979b7411e7c01213fb90e915b.jpg)
可以看出一切都还蛮正常的，截至目前，文件系统已经全部结束

## 0x04 总结
繁杂的文件系统终于全部结束，同之前的篇章不同，文件系统更多的是代码实际操作，原理反而简单几句就可以说清楚，这里我建议大家去观看原书《操作系统真象还原》，这里讲解的更加清楚，我文件系统基本上全写代码和一点思路，原理之类的讲解不是很清楚。总之我们已经离一个简单但功能还算全的操作系统不远了，期待ing！
