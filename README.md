## 0x00 实现文件写入
在之前我们写系统调用的时候，我们曾经实现过一个简单的write系统调用，但那个系统调用功能是十分有限的，只能写入到显存当中，今天我们就来完善一下，让他能写入到文件当中
### 1.实现file_write
我们还是像之前那样，现在file.c中添加file_xxx核心功能，然后在fs.c当中添加外壳调用，所以我们现在添加file_write系统调用，大伙注意，这个函数十分长，我会尽可能多加点注释来帮助理解
```
/* 把buf当中的count个字节写入file，成功则返回写入的字节数，失败则返回-1 */
int32_t file_write(struct file* file, const void* buf, uint32_t count){
  if((file->fd_inode->i_size + count) > (BLOCK_SIZE * 140)){
    //上面是支持文件的最大字节
    printk("exceed max file_size 71680 bytes, write file failed\n");
    return -1;
  }
  uint8_t* io_buf = sys_malloc(512);
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

```

一口气写完，大伙注意这只是一个函数而已，这里我们分别进行了新块的分配以及写入数据，功能已经比较完善了，能应对各种不同的分配情况

### 2.改进`sys_write`和`write`系统调用
之前我们也说了前面的write系统调用只是往屏幕输出，这样下去肯定不行，我们现在就需要他可以往文件中写入数据，现在我们来修改`sys_write`
```
/* 将buf中连续count个字节写入文件描述符fd，成功则返回写入的字节数，失败返回-1 */
int32_t sys_write(int32_t fd, const void* buf, uint32_t count){
  if(fd < 0){
    printk("sys_write: fd error\n");
    return -1;
  }
  if(fd == stdout_no){      //如果说是标准输出，则直接在屏幕上输出
    char tmp_buf[1024] = 0;
    memcpy(tmp_buf, buf, count);
    console_put_str(tmp_buf);
    return count;
  }
  uint32_t _fd = fd_local2global(fd);
  struct file* wr_file = &file_table[_fd];
  if(wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & o_RDWR){
    uint32_t bytes_written = file_write(wr_file, buf, count);
    return bytes_written;
  }else{
    console_put_str("sys_write: not allowed to write file without flag O_RDWR or O_WRONLY\n");
    return -1;
  }
}

```

这就是比较完善的sys_write了，我们现在在会议一下之前写syscall系统调用的做法，首先去修改syscall.c
```
/* 系统调用write */
uint32_t write(int32_t fd, const void* buf, uint32_t count){
  return _syscall3(SYS_WRITE, fd, buf, count);
}

```

然后我们再修改一下printf函数
```
/* 格式化输出字符串format */
uint32_t printf(const char* format, ...){
  va_list args;
  va_start(args, format);
  char buf[1024] = {0};
  vsprintf(buf, format, args);
  va_end(args);
  return write(1, buf, strlen(buf));
}

```

### 3.将数据写入文件
我们来使用上面实现的操作来测试
```
int main(void){
  put_str("I am Kernel\n");
  init_all();

  intr_enable();
  //process_execute(u_prog_a, "u_prog_a");
  //process_execute(u_prog_b, "u_prog_b");
  //thread_start("k_thread_a", 31, k_thread_a, " A_");
  //thread_start("k_thread_b", 31, k_thread_b, " B_");
  uint32_t fd = -1;
  fd = sys_open("/file1", O_CREAT);
  sys_close(fd);
  fd = sys_open("/file1", O_RDWR);
  sys_write(fd, "hello, peipei\n", 14);
  sys_close(fd);
  printf("%d closed now\n", fd);
 while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}


```

然后我们可以看到上面显示没出现问题
![](http://imgsrc.baidu.com/super/pic/item/77094b36acaf2eddaf075a2ec81001e938019306.jpg)
我们可以通过调试信息来确认文件内是否真正写入数据，如下
```
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1363968 512
0014d000: 68 65 6C 6C 6F 2C 20 70 65 69 70 65 69 0A 68 65  hello, peipei.he
0014d010: 6C 6C 6F 2C 20 70 65 69 70 65 69 0A 68 65 6C 6C  llo, peipei.hell
0014d020: 6F 2C 20 70 65 69 70 65 69 0A 68 65 6C 6C 6F 2C  o, peipei.hello,
0014d030: 20 70 65 69 70 65 69 0A 68 65 6C 6C 6F 2C 20 70   peipei.hello, p
0014d040: 65 69 70 65 69 0A 68 65 6C 6C 6F 2C 20 70 65 69  eipei.hello, pei
0014d050: 70 65 69 0A 68 65 6C 6C 6F 2C 20 70 65 69 70 65  pei.hello, peipe
0014d060: 69 0A 68 65 6C 6C 6F 2C 20 70 65 69 70 65 69 0A  i.hello, peipei.
0014d070: 68 65 6C 6C 6F 2C 20 70 65 69 70 65 69 0A 68 65  hello, peipei.he
0014d080: 6C 6C 6F 2C 20 70 65 69 70 65 69 0A 68 65 6C 6C  llo, peipei.hell
0014d090: 6F 2C 20 70 65 69 70 65 69 0A 68 65 6C 6C 6F 2C  o, peipei.hello,
0014d0a0: 20 70 65 69 70 65 69 0A 68 65 6C 6C 6F 2C 20 70   peipei.hello, p
0014d0b0: 65 69 70 65 69 0A 00 00 00 00 00 00 00 00 00 00  eipei...........
0014d0c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
0014d1f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................

```

这里有这么多是因为我运行了很多次调试某个错误。

## 0x01 读取文件
紧接着我们来实现文件读取功能,同样仿照file_write的写法，其中仅仅是写和读的区别罢了
```
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

```

### 2.实现sys_read
这里我们来实现sys_read，来对file_read进行封装，我们一样还是在fs.c中完成,他非常简单
```
/* 从文件描述符fd指向的文件中读取count个字节到buf，若成功则返回读出的字节数，失败则返回-1 */
int32_t sys_read(int32_t fd, void* buf, uint32_t count){
  if(fd < 0){
    printk("sys_read: fd error\n");
    return -1;
  }
  ASSERT(but != NULL);
  uint32_t _fd = fd_local2global(fd);
  return file_read(&file_table[_fd], buf, count);
}

```

紧接着我们立刻到main函数当中测试
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  uint32_t fd = sys_open("/file1", O_CREAT);
  sys_close(fd);

  fd = sys_open("/file1", O_RDWR);
  printf("open /file1, fd:%d\n", fd);
  char buf[64] = {0};
  int read_bytes = sys_read(fd, buf, 18);
  printf("1_ read %d bytes:\n%s\n", read_bytes, buf);

  memset(buf, 0, 64);
  read_bytes = sys_read(fd, buf, 6);
  printf("2_ read %d bytes:\n%s\n", read_bytes, buf);

  memset(buf, 0, 64);
  read_bytes = sys_read(fd, buf, 6);
  printf("3_ read %d bytes:\n%s\n", read_bytes, buf);

  printf("______ close file1 and ropen ______ \n");
  sys_close(fd);
  fd = sys_open("/file1", O_RDWR);
  memset(buf, 0, 64);
  read_bytes = sys_read(fd, buf, 24);
  printf("4_ read %d bytes:\n%s\n", read_bytes, buf);
  sys_close(fd);
 while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}

```

直接运行查看结果
![](http://imgsrc.baidu.com/super/pic/item/c75c10385343fbf245f9b4e4f57eca8064388f24.jpg)
可以发现十分成功！这里有个点注意那就是文件偏移指针关闭的时候会重置为0,所以第二次打开后会从头开始读取

## 0x02 实现文件读写指针定位功能
我们读写文件那肯定不能每次都从头读从尾写，所以我们迫切需要一种能控制文件偏移指针函数，这个函数原型就是lseek， 我们先来看看lseek的用法：
lseek原型是"off_t lseek(int fd, off_t offset, int whence)", fd是文件描述符，offset是便宜量，whence是offset的参照物，该函数的共能就是设置文件读写指针fd_pos为参照物+便宜量的值，我们的whence有以下三种取值：

+ SEEK_SET，文件开始
+ SEEK_CUR, 当前读写位置
+ SEEK_END，文件尺寸大小，其实也就是文件最后一个字节的下一个字节，由于咱们的fd_pos始终指向下一个可读写的位置，它是以0为起始的便宜量，因此文件末尾是指文件大小

因此我们首先在fs.h中定义whence结构
```
/* 文件读写偏移量 */
enum whence{
  SEEK_SET = 1,
  SEEK_CUR,
  SEEK_END
};

```

然后我们到fs.c中实现函数sys_lseek
```
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

```

紧接着立刻到main函数中实现
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  uint32_t fd = sys_open("/file1", O_CREAT);
  sys_close(fd);

  fd = sys_open("/file1", O_RDWR);
  printf("open /file1, fd:%d\n", fd);
  char buf[64] = {0};
  int read_bytes = sys_read(fd, buf, 18);
  printf("1_ read %d bytes:\n%s\n", read_bytes, buf);

  memset(buf, 0, 64);
  read_bytes = sys_read(fd, buf, 6);
  printf("2_ read %d bytes:\n%s\n", read_bytes, buf);

  memset(buf, 0, 64);
  read_bytes = sys_read(fd, buf, 6);
  printf("3_ read %d bytes:\n%s\n", read_bytes, buf);

  printf("______ SEEK_SET 0 ______ \n");
  sys_lseek(fd, 0, SEEK_SET)
  memset(buf, 0, 64);
  read_bytes = sys_read(fd, buf, 24);
  printf("4_ read %d bytes:\n%s\n", read_bytes, buf);
  sys_close(fd);
 while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}

```

这里我们在连续读30字节后又从头开始读24字节
![](http://imgsrc.baidu.com/super/pic/item/58ee3d6d55fbb2fb805243d50a4a20a44723dcf8.jpg)
这里我们并没有关闭文件，但是指针确实指向了开头，我们通过查看输出就知道了。

## 0x03 实现文件删除
### 1.回收inode
这里我们删除文件涉及比较底层的inode，block等，相关函数是unlink，这些删除其实就是创建的逆过程，代码实现如下，当然先从底层开始，在inode.c中实现
```

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

```

这里我提一句就是最后那个调试作用的inode_delete，象书的作者认为不太需要，但是我个人觉得为了避免可能出现UAF漏洞，这里还是尽量清0比较好，实现清0的结果就是会延长我们的程序处理时间，但是我觉得为了安全也是值得的。

### 2.删除目录项
我们删除了inode与其相关的数据，那肯定要在当前目录把目录项也给删除，我在下面简单叙述一下相关步骤：
1. 在文件所在的目录中擦除目录项，使其为0
2. 根目录无法清空，如果目录项独占一个块，并且该块不是根目录最后一个块的话，将其回收
3. 目录inode的i_size减去一个目录项大小
4. 将目录inode同步到硬盘

下面我们到dir.c中添加函数完成工作
```
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
          bitmap_set(&part->block_bitmap,, block_bitmap_idx, 0);
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

```

函数不是很困难，这个函数目的就是删掉文件对应的目录项，这离咱们实现文件删除已经不远了

### 3.实现`sys_unlink`
在Linux下删除文件使用unlink系统调用，所以咱们仿照其先实现内核部分`sys_unlink`
```
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

```

其中用到了咱们之前编写的删除`inode`和`dir_entry`,所以咱们不用担心同步的问题
接下来我们到main函数当中测试一下
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  uint32_t fd = sys_open("/file1", O_CREAT);
  sys_close(fd);
  printf("/file1 delete %s!\n", sys_unlink("/file1") == 0 ? "done" : "fail");

 while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}


``` 

这里代码十分简单，也就是删除掉说done，失败说fail，下面为了进行对比，我们在运行前先查看一下具体硬盘情况：
以下第一行是块位图的表，第二行是inode位图的表，第三行是inode_table的表，第四行是根目录的内容


```
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1049600 512
00100400: 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00100410: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
001005f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1051648 512
00100c00: 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00100c10: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00100df0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1052160 512
00100e00: 00 00 00 00 48 00 00 00 00 00 00 00 00 00 00 00  ....H...........
00100e10: 67 0A 00 00 00 00 00 00 00 00 00 00 00 00 00 00  g...............
00100e20: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00100e30: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00100e40: 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00  ................
00100e50: 54 00 00 00 00 00 00 00 00 00 00 00 68 0A 00 00  T...........h...
00100e60: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00100ff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1363456 512
0014ce00: 2E 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0014ce10: 00 00 00 00 02 00 00 00 2E 00 00 00 00 00 00 00  ................
0014ce20: 00 00 00 00 00 00 00 00 00 00 00 00 02 00 00 00  ................
0014ce30: 66 69 6C 65 31 00 00 00 00 00 00 00 00 00 00 00  file1...........
0014ce40: 01 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00  ................
0014ce50: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
0014cff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................

```

下面我们运行一次来查看效果:
![](http://imgsrc.baidu.com/super/pic/item/d4628535e5dde71182c04431e2efce1b9c1661e2.jpg)
可以看到我们运行是没问题的，接下来我们查看硬盘
```
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1049600 512
00100400: 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00100410: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
001005f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1051648 512
00100c00: 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00100c10: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00100df0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1052160 512
00100e00: 00 00 00 00 30 00 00 00 00 00 00 00 00 00 00 00  ....0...........
00100e10: 67 0A 00 00 00 00 00 00 00 00 00 00 00 00 00 00  g...............
00100e20: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00100ff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1363456 512
0014ce00: 2E 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0014ce10: 00 00 00 00 02 00 00 00 2E 00 00 00 00 00 00 00  ................
0014ce20: 00 00 00 00 00 00 00 00 00 00 00 00 02 00 00 00  ................
0014ce30: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
0014cff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

可以看到确实改变了，对应的位大家可以自行比对

## 0x04 创建目录
### 1.实现sys_mkdir创建目录
我们对于文件实际上已经大部分完成了，但是目前只能在根目录上玩耍，为了更贴近实际，我们再继续来实现创建目录的功能
由于比较繁琐，所以先给出具体思路：
1. 检查新建的目录在文件系统上是否存在
2. 为新目录创建inode
3. 为新目录分配1个块存储该目录中的目录项
4. 创建目录项"."和".."
5. 在新目录的父目录当中添加新目录的目录项
6. 同步资源到硬盘

以上功能我们在mkdir的内核部分`sys_mkdir`中实现，如下：
```
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

```

### 2.功能验证
之前咱们是已经把file1给删除了，现在我们就修改main函数来测试上面编写的文件目录功能
```
int main(void){
  put_str("I am Kernel\n");
  init_all();
  intr_enable();
  printf("/dir1/subdir1 create %s!\n", sys_mkdir("/dir1/subdir1") == 0 ? "done" : "fail");
  printf("/dir1 create %s\n", sys_mkdir("/dir1") == 0 ? "done" : "fail");
  printf("now /dir1/subdir1 create %s!\n", sys_mkdir("/dir1/subdir1") == 0 ? "done" : "fail");
  int fd = sys_open("/dir1/subdir1/file2", O_CREAT | O_RDWR);
  if(fd != -1){
    printf("/dir1/subdir1/file2 creat done!\n");
    sys_write(fd, "Catch me if you can!\n", 21);
    sys_lseek(fd, 0, SEEK_SET);
    char buf[32] = {0};
    sys_read(fd, buf, 21);
    printf("/dir1/subdir1/file2 says:\n%s\n", buf);
    sys_close(fd);
  }
 while(1);//{
    //console_put_str("Main ");
  //};
  return 0;
}

```

这个测试函数就是创建多级目录然后在文件内顺便测试一下之前所有的write和read、lseek功能
![](http://imgsrc.baidu.com/super/pic/item/7af40ad162d9f2d3fa162528ecec8a136227ccf6.jpg)
这里可以看到符合咱们所愿，因此咱们立刻到硬盘中测试一下
```
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1364992 512
0014d400: 43 61 74 63 68 20 6D 65 20 69 66 20 79 6F 75 20  Catch me if you
0014d410: 63 61 6E 21 0A 00 00 00 00 00 00 00 00 00 00 00  can!............
0014d420: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
0014d5f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

可以看到确实写入了一段文字，我们再来看看新建目录的情况，首先到根目录下查看
```
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1363456 512
0014ce00: 2E 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0014ce10: 00 00 00 00 02 00 00 00 2E 00 00 00 00 00 00 00  ................
0014ce20: 00 00 00 00 00 00 00 00 00 00 00 00 02 00 00 00  ................
0014ce30: 64 69 72 31 00 00 00 00 00 00 00 00 00 00 00 00  dir1............
0014ce40: 01 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00  ................
0014ce50: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
0014cff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

发现确实存在名为dir1的子目录，然后我们分析里面的结构
![](http://imgsrc.baidu.com/super/pic/item/b8014a90f603738d24240de5f61bb051f919ec8f.jpg)
这里01表示的是inode节点号，然后02表示的是file_type，这个2表示FT_DIRECTORY,我们找到这个线索之后就去看看这个inode节点
这里我们查看`inode_table`来查看数据
```
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1052160 512
00100e00: 00 00 00 00 48 00 00 00 00 00 00 00 00 00 00 00  ....H...........
00100e10: 67 0A 00 00 00 00 00 00 00 00 00 00 00 00 00 00  g...............
00100e20: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00100e30: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00100e40: 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00  ................
00100e50: 48 00 00 00 00 00 00 00 00 00 00 00 68 0A 00 00  H...........h...
00100e60: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00100e90: 00 00 00 00 00 00 00 00 02 00 00 00 48 00 00 00  ............H...
00100ea0: 00 00 00 00 00 00 00 00 69 0A 00 00 00 00 00 00  ........i.......
00100eb0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00100ee0: 00 00 00 00 03 00 00 00 15 00 00 00 00 00 00 00  ................
00100ef0: 00 00 00 00 6A 0A 00 00 00 00 00 00 00 00 00 00  ....j...........
00100f00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00100ff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

我们可以通过计算得出一个inode节点占76字节，所以我们在这里我们可以得到dir1的inode节点数据
![](http://imgsrc.baidu.com/super/pic/item/f603918fa0ec08fae6b912601cee3d6d54fbdab8.jpg)
然后我们可以得知其扇区首地址是0xA68,因此我们继续查看
```
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1363968 512
0014d000: 2E 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0014d010: 01 00 00 00 02 00 00 00 2E 2E 00 00 00 00 00 00  ................
0014d020: 00 00 00 00 00 00 00 00 00 00 00 00 02 00 00 00  ................
0014d030: 73 75 62 64 69 72 31 00 00 00 00 00 00 00 00 00  subdir1.........
0014d040: 02 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00  ................
0014d050: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
0014d1f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................

```

可以看到我们成功找到了subdir1这个目录项，然后我们又可以通过他来找到file2，这里我们获取他的inode节点号为02,因此我们同样的到inode_table中寻找，
![](http://imgsrc.baidu.com/super/pic/item/0df431adcbef7609b0a798586bdda3cc7dd99e4d.jpg)
这里就是subdir的inode节点了，因此我们同样访问他来查看内容：
```
dawn@dawn-virtual-machine:~/repos/OS_learning$ sh ./xxd.sh bochs/hd80M.img 1364480 512
0014d200: 2E 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0014d210: 02 00 00 00 02 00 00 00 2E 2E 00 00 00 00 00 00  ................
0014d220: 00 00 00 00 00 00 00 00 01 00 00 00 02 00 00 00  ................
0014d230: 66 69 6C 65 32 00 00 00 00 00 00 00 00 00 00 00  file2...........
0014d240: 03 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00  ................
0014d250: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
0014d3f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................

```

可以看到成功获取了file2的目录项，本节完毕

## 0x05 总结
到目前为止，我们的文件系统已经完成了大部分，剩下的还有一些功能未完善，但是比较硬的部分已经啃完了，这两天时间比较紧，争取明天把文件系统搞完然后开始用户交互。
