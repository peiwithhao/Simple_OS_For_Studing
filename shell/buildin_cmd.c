#include "buildin_cmd.h"
#include "assert.h"
#include "file.h"
#include "dir.h"
#include "syscall.h"
#include "stdio.h"
#include "string.h"
#include "global.h"
#include "fs.h"
#include "shell.h"
#include "dir.h"

/* 将路径old_abs_path中的..和.转换为实际路径后存入new_abs_path */
static void wash_path(char* old_abs_path, char* new_abs_path){
  assert(old_abs_path[0] == '/');
  char name[MAX_FILE_NAME_LEN] = {0};
  char* sub_path = old_abs_path;
  sub_path = path_parse(sub_path, name);
  if(name[0] == 0){     //如果解析后发现没字符，说明路径只有'/'
    new_abs_path[0] = '/';
    new_abs_path[1] = 0;
    return;
  }
  new_abs_path[0] = 0;      //避免传给new_abs_path的缓冲区不干净
  strcat(new_abs_path, "/");    
  while(name[0]){
    /* 如果是上一级目录 */
    if(!strcmp("..", name)){
      char* slash_ptr = strrchr(new_abs_path, '/');
      /* 如果未到new_abs_path中的顶层目录，就将最右边的'/'替换为0,
       * 这样便取出了new_abs_path中最后一层路径，相当与到了上一级目录*/
      if(slash_ptr != new_abs_path){    //就比如/a/b ， 这之后就变为/a
        *slash_ptr = 0;
      }else{
        /* 如果new_abs_path中只有1个'/'，即表示已经到了顶层目录，就将下一个字符置为结束符0 */
        *(slash_ptr + 1) = 0;
      }
    }else if(strcmp(".", name)){    //如果路径不是'.'，就将name拼接到new_abs_path
      if(strcmp(new_abs_path, "/")){    //这里判断是为了避免形成"//"的情况
        strcat(new_abs_path, "/");
      }
      strcat(new_abs_path, name);
    }   //如果name为当前目录"."，则无需处理
    memset(name, 0, MAX_FILE_NAME_LEN);
    if(sub_path){
      sub_path = path_parse(sub_path, name);
    }
  }
}

void make_clear_abs_path(char* path, char* final_path){
  char abs_path[MAX_PATH_LEN] = {0};
  /* 先判断是否输入的是绝对路径 */
  if(path[0] != '/'){   //如果不是绝对路径那就拼接成绝对路径
    memset(abs_path, 0, MAX_PATH_LEN);
    if(getcwd(abs_path, MAX_PATH_LEN) != NULL){ //获取当前绝对目录
      if(!((abs_path[0] == '/') && (abs_path[1] == 0))){ //若abs_path表示的当前目录不是根目录
        strcat(abs_path, "/");
      }
    }
  }
  strcat(abs_path, path);
  wash_path(abs_path, final_path);
}

/* pwd命令的内建函数 */
void buildin_pwd(uint32_t argc, char** argv UNUSED){
  if(argc != 1){
    printf("pwd: no argument supprot!\n");
    return;
  }else{
    if(NULL != getcwd(final_path, MAX_PATH_LEN)){
      printf("%s\n", final_path);
    }else{
      printf("pwd: get current work directory failed\n");
    }
  }
  
}
/* cd命令内建函数 */
char* buildin_cd(uint32_t argc, char** argv){
  if(argc > 2){
    printf("cd: only support 1 argument!\n");
    return NULL;
  }
  /* 如果只有cd无参数，则直接返回到根目录 */
  if(argc == 1){
    final_path[0] = '/';
    final_path[1] = 0;
  }else{
    make_clear_abs_path(argv[1], final_path);
  }

  if(chdir(final_path) == -1){
    printf("cd: no such directory %s\n", final_path);
    return NULL;
  }
  return final_path;
}

/* ls命令内建函数 */
void buildin_ls(uint32_t argc, char** argv){
  char* pathname = NULL;
  struct stat file_stat;
  memset(&file_stat, 0, sizeof(struct stat));
  bool long_info = false;
  uint32_t arg_path_nr = 0;
  uint32_t arg_idx = 1;     //跨过argv[0], argv[0]是字符串"ls"
  while(arg_idx < argc){
    if(argv[arg_idx][0] == '-'){    //如果是选项，则首字符应该是'-'
      if(!strcmp("-l", argv[arg_idx])){     //
        long_info = true;
      }else if(!strcmp("-h", argv[arg_idx])){
        printf("usage: -l list all information about the file.\n-h for help \nlist all files in the current dirctory if no option\n");
        return;
      }else{
        printf("ls: invalid option %s\nTry `ls -h` for more information.\n", argv[arg_idx]);
      }
    }else{      //ls的路径参数
      if(arg_path_nr == 0){
        pathname = argv[arg_idx];
        arg_path_nr = 1;
      }else{
        printf("ls: only support one path\n");
        return;
      }
    }
    arg_idx++;
  }
  if(pathname == NULL){     //如果只输入了ls或ls -l, 没有输入路径则默认以当前路径作为输入绝对路径
    if(NULL != getcwd(final_path, MAX_PATH_LEN)){
      pathname = final_path;
    }else{
      printf("ls: getcwd for default path failed\n");
      return;
    }
  }else{                    //用参数路径
    make_clear_abs_path(pathname, final_path);
    pathname = final_path;
  }
  if(stat(pathname, &file_stat) == -1){
    printf("ls: cannot accsess %s: No such file or directory\n", pathname);
    return;
  }
  if(file_stat.st_filetype == FT_DIRECTORY){
    struct dir* dir = opendir(pathname);
    struct dir_entry* dir_e = NULL;
    char sub_pathname[MAX_PATH_LEN] = {0};
    uint32_t pathname_len = strlen(pathname);
    uint32_t last_char_idx = pathname_len - 1;
    memcpy(sub_pathname, pathname, pathname_len);
    if(sub_pathname[last_char_idx] != '/'){
      sub_pathname[pathname_len] = '/';
      pathname_len++;
    }
    rewinddir(dir);
    if(long_info){  //如果使用了-l 参数
      char ftype;
      printf("total: %d\n", file_stat.st_size);
      while((dir_e = readdir(dir))){        //遍历目录项
        ftype = 'd';
        if(dir_e->f_type == FT_REGULAR){
          ftype = '-';
        }
        sub_pathname[pathname_len] = 0;
        strcat(sub_pathname, dir_e->filename);
        memset(&file_stat, 0, sizeof(struct stat));
        if(stat(sub_pathname, &file_stat) == -1){
          printf("ls: cannot access %s: No such file or directory\n", dir_e->filename);
          return;
        }
        printf("%c  %d  %d  %s\n", ftype, dir_e->i_no, file_stat.st_size, dir_e->filename);
      }
    }else{      //如果没使用-l参数
      while((dir_e = readdir(dir))){
        printf("%s ", dir_e->filename);
      }
      printf("\n");
    }
    closedir(dir);
  }else{
    if(long_info){
      printf("- %d  %d  %s\n", file_stat.st_ino, file_stat.st_size, pathname);
    }else{
      printf("%s\n", pathname);
    }
  }
}

/* ps命令内建函数 */
void buildin_ps(uint32_t argc, char** argv UNUSED){
  if(argc != 1){
    printf("pc: no argument support!\n");
    return;
  }
  ps();
}

/* clear命令内建函数 */
void buildin_clear(uint32_t argc, char** argv UNUSED){
  if(argc != 1){
    printf("clear: no argument support!\n");
    return;
  }
  clear();
}

/* mkdir命令内建函数 */
int32_t buildin_mkdir(uint32_t argc, char** argv){
  int32_t ret = -1;
  if(argc != 2){
    printf("mkdir: only support 1 argument!\n");
  }else{
    make_clear_abs_path(argv[1], final_path);
    /* 若创建的不是根目录 */
    if(strcmp("/", final_path)){
      if(mkdir(final_path) == 0){
        ret = 0;
      }else{
        printf("mkdir: create directory %s failed.\n", argv[1]);
      }
    }
  }
  return ret;
}

/* rmdir命令内建函数 */
int32_t buildin_rmdir(uint32_t argc, char** argv){
  int32_t ret = -1;
  if(argc != 2){
    printf("rmdir: only support 1 argument!\n");
  }else{
    make_clear_abs_path(argv[1], final_path);
    /* 若删除的不是根目录 */
    if(strcmp("/", final_path)){
      if(rmdir(final_path) == 0){
        ret = 0;
      }else{
        printf("rmdir: remove %s failed.\n", argv[1]);
      }
    }
  }
  return ret;
}

/* rm命令内建函数 */
int32_t buildin_rm(uint32_t argc, char** argv){
  int32_t ret = -1;
  if(argc != 2){
    printf("rm: only support 1 argument!\n");
  }else{
    make_clear_abs_path(argv[1], final_path);
    /* 若删除的不是根目录 */
    if(strcmp("/", final_path)){
      if(unlink(final_path) == 0){
        ret = 0;
      }else{
        printf("rm: delete %s failed.\n", argv[1]);
      }
    }
  }
  return ret;
}
