#!/bin/bash
# build shader executables

riscv64-unknown-elf-gcc -mabi=lp64d -march=rv64imafd_zicsr_zifencei -fPIC -Wall -Wextra -O3 -ffast-math -g -static -nostartfiles -mcmodel=medany -Wl,-efragA -o fragA.rv64 ./shaders/fragA.c maths.c -lm
riscv64-unknown-elf-objdump -SDls fragA.rv64 > fragA.dis
riscv64-unknown-elf-gcc -mabi=lp64d -march=rv64imafd_zicsr_zifencei -fPIC -Wall -Wextra -O3 -ffast-math -g -static -nostartfiles -mcmodel=medany -Wl,-evertA -o vertA.rv64 ./shaders/vertA.c maths.c -lm
riscv64-unknown-elf-objdump -SDls vertA.rv64 > vertA.dis

riscv64-unknown-elf-gcc -mabi=lp64d -march=rv64imafd_zicsr_zifencei -fPIC -Wall -Wextra -O3 -ffast-math -g -static -nostartfiles -mcmodel=medany -Wl,-efragA -o fragB.rv64 ./shaders/fragB.c maths.c -lm
riscv64-unknown-elf-objdump -SDls fragB.rv64 > fragB.dis
riscv64-unknown-elf-gcc -mabi=lp64d -march=rv64imafd_zicsr_zifencei -fPIC -Wall -Wextra -O3 -ffast-math -g -static -nostartfiles -mcmodel=medany -Wl,-evertA -o vertB.rv64 ./shaders/vertB.c maths.c -lm
riscv64-unknown-elf-objdump -SDls vertB.rv64 > vertB.dis
