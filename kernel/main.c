#include "print.h"
#include "init.h"
#include "debug.h"
#include "string.h"
#include "memory.h"

int main(void){
  put_str("I am Kernel\n");
  init_all();
  void* addr = get_kernel_pages(3);
  put_str("\n get kernel pages start vaddr is:");
  put_int((uint32_t)addr);
  while(1); 
  return 0;
}
