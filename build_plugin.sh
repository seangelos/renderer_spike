#!/bin/bash

# combine everything together (renderer + plugin) to make the shared object for spike
mkdir -p renderer/objects && cd renderer/objects
../build_object.sh
cd ../../framebuffer_plugin
./build_fbplugin.sh
cd ..

DEFS="-D_POSIX_C_SOURCE=200809L"
OPTS="-g -shared -Wall -Wextra -pedantic -Og -fno-strict-aliasing -flto -ffast-math -fPIC"
SRCS="framebuffer_plugin/*.o renderer/objects/*.o"
LIBS="-lm -lX11"

gcc -o plugin.so $DEFS $OPTS $SRCS $LIBS
rm -rf renderer/objects
