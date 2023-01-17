#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H
void panic_spin(char* filename, int line, const char* func, const char* condition);

/***************** __VA_ARGS__  *******************
 * __VA_ARGS__是预处理器支持的专用标识符
 * 代表所有与省略号想对应的参数
 * "..."表示定义的宏参数可变 */
#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)    //表示把panic函数定义为宏
/**************************************************/

#ifdef NDEBUG
  #define ASSERT(CONDITION) ((void)0)   //这里若定义了NDEBUG，则ASSERT宏会等于空值，也就是取消这个宏
#else                                   //否则就说明是调试模式，所以下面才是真正的定义断言
#define ASSERT(CONDITION) if(CONDITION){}else{    \
  /* 符号#让编译器将宏的参数转化为字符串面量 */ \   
  PANIC(#CONDITION); \                  
}

#endif /*__NDEBUG*/
#endif /*__KERNEL_DEBUG_H*/
