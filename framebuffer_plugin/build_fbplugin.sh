#!/bin/bash

# make (unlinked) object file for framebuffer plugin
# RICSV variable == base path of spike installation 
# should be exported before build

OPTS="-g -O2 -std=c++17 -Wall -Wextra -pedantic -fno-strict-aliasing -flto -ffast-math -fPIC -c"
SRCS="fbplugin.cc"
LIBS="-I$RISCV/include"

g++ $OPTS $SRCS $LIBS
