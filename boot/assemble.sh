#!/bin/sh
nasm -I include/ -o mbr.bin mbr.S
dd if=./mbr.bin of=../bochs/hd60M.img bs=512 count=1 conv=notrunc
nams -I include -o loader.bin loader.S
dd if=./loader.bin of=../bochs/hd60M.img bs=512 count=2 seek=2 conv=notrunc
