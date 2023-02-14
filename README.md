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
可以发现十分成功！

## 0x02 实现文件读写指针定位功能

