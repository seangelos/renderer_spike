#!/bin/bash

# make the executable that runs within spike

riscv64-unknown-elf-gcc -mabi=lp64d -march=rv64imafd_zicsr_zifencei -fno-strict-aliasing -Wall -Wextra -I../framebuffer_plugin -O2 -g -static -nostartfiles -mcmodel=medany -Wl,-Ttext-segment,0x80000000,-T,ls.ld -o main.rv64 start.S *.c -lm
riscv64-unknown-elf-objdump -SDls main.rv64 > main.dis
