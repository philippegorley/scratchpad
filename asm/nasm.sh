#!/usr/bin/env bash

nasm -f elf64 $1.asm
ld -dynamic-linker /lib64/ld-linux-x86-64.so.2 $1.o $2
rm $1.o
