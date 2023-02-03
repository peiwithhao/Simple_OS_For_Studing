#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H
typedef char* va_list;
#include "stdint.h"
uint32_t vsprintf(char* str, const char* format, va_list ap);
uint32_t printf(const char* format, ...);
#endif



