#!/bin/bash

# make object files for renderer in "objects" subdirectory

DEFS="-D_POSIX_C_SOURCE=200809L"
OPTS="-g -std=c89 -Wall -Wextra -pedantic -Og -fno-strict-aliasing -flto -ffast-math -fPIC -c"
SRCS="../renderer/main.c ../renderer/platforms/linux.c ../renderer/core/*.c ../renderer/scenes/*.c ../renderer/shaders/*.c ../renderer/tests/*.c"
LIBS="-lm -lX11"

gcc $DEFS $OPTS $SRCS $LIBS
