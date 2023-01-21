[bits 32]
section .text
global switch_to
switch_to:
  ;栈中此处是返回地址
  push esi
  push edi
  push edx
  push ebp
  mov eax, [esp + 20]   ;得到栈中的参数cur, cur = [esp + 20]
  mov [eax], esp        ;保存栈顶指针esp,task_struct的self_kstack字段
                        ;self_kstack在task_struct中的偏移为0
                        ;所以直接往thread开头处存4字节即可

  ;----------- 以上是备份当前线程的环境，下面是恢复下一个线程的环境 -------------
  mov eax, [esp + 24]   ;的到栈中的参数next
  mov esp, [eax]        ;恢复栈顶
  pop ebp
  pop ebx
  pop edi
  pop esi
  ret                   ;返回到上面switch_to下面的那句注释的返回地址
                        ;如果未由中断进入，第一次执行的时候会返回到kernel_thread
  
