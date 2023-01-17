#include "string.h"
#include "global.h"
#include "debug.h"
 
/* 将dst_起始的size个字节置为value */
void memset(void* dst_, uint8_t value, uint32_t size){
  ASSERT(dst_ != NULL);
  uint8_t* dst = (uint8_t*)dst_;        //定义一个指针变量，指向一个字节
  while(size-- >0)
    *dst ++ = value ;
}

/* 将src_起始的size个字节复制到dst_ */
void memcpy(void* dst_, void* src_, uint32_t size){
  ASSERT(dst_ != NULL && src_ != NULL);
  uint8_t* dst = dst_;
  const uint8_t* src = src_;
  while(size --> 0)
    *dst++ = *src++;
}

/* 连续比较以地址a_和地址b_开头的size个字节，若相等则返回0 ，若a_大于b_则返回1,否则返回-1*/
int memcmp(const void* a_, const void* b_, uint32_t size){
  const char* a = a_;
  const char* b = b_;
  ASSERT(a != NULL && b != NULL);
  while(size-- > 0){
    if(*a != *b){
      return *a > *b ? 1 : -1;
    }
    a++;
    b++;
  }
  return 0;
}

/* 将字符串从src_ 复制到 dst_ */
char* strcpy(char* dst_,const char* src_){
  ASSERT(dst_ != NULL && src_ != NULL);
  char* dst = dst_;
  while((*dst_++  =  *src_ ++));    //赋值语句，这里若赋值为\x00则会跳出循环
  return dst;
}

/* 返回字符串长度 */
uint32_t strlen(const char* str){
  ASSERT(str != NULL);
  const char* p = str;
  while(*p++);
  return (p - str - 1);         //这里地址作减法直接得出长度
}

/* 比较两个字符串，类似memcmp */
int8_t strcmp(const char* a, const char* b){
  ASSERT(a != NULL && b != NULL);
  while(*a != 0 && *a == *b){
    a++;
    b++;
  }
  return *a < *b ? -1 : *a > *b ;
}

/* 从左到右查找字符串str中首次出现字符ch的地址 */
char* strchr(const char* str, const uint8_t ch){
  ASSERT(str != NULL);
  while(*str != 0){
    if(*str == ch){
      return (char*)str;
    }
    str ++;
  }
  return NULL;
}

/* 从后往前查找字符串首次出现字符ch的地址 */
char* strrchr(const char* str, const uint8_t ch){
  ASSERT(str != NULL);
  const char* last_char = NULL;
  while(*str != 0){
    if(*str == ch){
      last_char = str;
    }
    str++ ;
  }
  return (char*)last_char;
}

/* 将字符串src_拼接dst_后，返回拼接的串地址 */
char* strcat(char* dst_, const char* src_){
  ASSERT(dst_ != NULL && src_ != NULL);
  char* str = dst_;
  while(*str++);
  --str;            //这里是因为str指针目前指向\x00，所以我们需要将指针减一
  while((*str++ = *src_++));
  return dst_;
}

/* 在字符串str中查找字符ch出现的次数 */
uint32_t strchrs(const char* str, uint8_t ch){
  ASSERT(str != NULL);
  uint32_t ch_cnt = 0;
  const char* p = str;
  while(*p != 0){
    if(*p == ch){
      ch_cnt++;
    }
    p++;
  }
  return ch_cnt;
}
