#include "print.h"
#include "init.h"
#include "debug.h"
#include "string.h"

int main(void){
  put_str("I am Kernel\n");
  init_all();
  char* str1 = "Hello";
  char* str2 = "World";
  char* str3 = strcat(str1,str2);
  put_str(str3);
  put_str("\n");
  ASSERT(1==2);
  while(1); 
  return 0;
}
