######## 应在command目录下执行 #########
if [[ ! -d "../lib" || ! -d "../build" ]];then
  echo "dependent dir do not exist!"
  cwd=$(pwd)
  cwd=${cwd##*/}
  cwd=${cwd%/}
  if [[ $cwd != "command" ]];then
    echo -e "you would better in command dir\n"
  fi
  exit
fi

BIN="cat"
CFLAGS="-m32 -fno-stack-protector -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers"
LIBS="-I ../lib/ -I ../lib/kernel/ -I ../lib/user/ -I ../kernel -I ../device -I ../thread -I ../userprog -I ../fs -I ../shell/"
OBJS="../build/string.o ../build/syscall.o ../build/stdio.o ../build/assert.o start.o"
DD_IN=$BIN
DD_OUT="/home/dawn/repos/OS_learning/bochs/hd60M.img"

nasm -f elf ./start.S -o ./start.o
ar rcs simple_crt.a $OBJS start.o
gcc $CFLAGS $LIBS -o $BIN".o" $BIN".c"
ld -m elf_i386 $BIN".o" simple_crt.a -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d", ($5+511)/512)}')

if [[ -f $BIN ]];then
  dd if=./$DD_IN of=$DD_OUT bs=512 count=$SEC_CNT seek=300 conv=notrunc
fi




