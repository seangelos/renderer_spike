#!/bin/bash

./build_plugin.sh 
cd spike && ./build_riscv.sh
cd ../renderer
spike -m1 --isa=RV64IMFDC --extlib=../plugin.so --device=framebuffer_plugin,0x10000000,triangle `pwd`/../spike/main.rv64
