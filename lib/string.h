#ifndef __LIB_STRING_H
#define __LIB_STRING_H
#define NULL 0
#include "stdint.h"

void memset(void* dst_, uint8_t value, uint32_t size); //单字节复制，指定目的地
void memcpy(void* dst_, void* src_, uint32_t size);     //多字节复制，指定目的地
int memcmp(const void* a_, const void* b_, uint32_t size);  //比较多个字节
char* strcpy(char* dst_, const char* src_);     //复制字符串
uint32_t strlen(const char* str);               //获取字符串长度
int8_t strcmp(const char* a, const char* b);  //比较两个字符串
char* strchr(const char* str, const uint8_t ch);    //查找字符地址
char* strrchr(const char* str, const uint8_t ch);   //反向查找字符地址
char* strcat(char* dst_, const char* src_);         //连接字符串
uint32_t strchrs(const char* str, uint8_t ch);      //计算相同字符数量

#endif
