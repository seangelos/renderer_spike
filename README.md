This is a version of [zaunonlok/renderer](https://github.com/zauonlok/renderer) modified so that shaders are dynamically loaded and run in [Spike](https://github.com/riscv-software-src/riscv-isa-sim/tree/v1.1.0), the RISC-V ISA simulator.

## Status

This software has been written as part of a graduate thesis at the School of Informatics of the Aristotle University of Thessaloniki, under the supervision of Assistant Professor Georgios Keramidas. It is provided as a proof of concept. As such, there are many bugs and missing features from the original renderer:

- Only the basic `blinn` renderer is supported.
- Performance is very slow, due to FP instructions being emulated in software by Spike.
- Due to a possible race condition, loading larger `.obj` files is unreliable and will often cause the program's startup to fail. In such cases a different scene can be selected, or one can simply retry until it works :)

## How to run

### Prerequisites

1. [RISC-V cross-compiler](https://github.com/riscv-collab/riscv-gnu-toolchain)
2. [Spike v1.1.0](https://github.com/riscv-software-src/riscv-isa-sim/tree/v1.1.0)

Note: The GNU toolchain for RISC-V must be configured with  `--with-cmodel=medany --enable-multilib` options. The scripts assume that the `RISCV` environment variable is set and that `$RISCV/bin` is added to `PATH`.

### Build steps

1. Run the `build_shader.sh` script within the `spike` directory to build the shaders which will be loaded. 
2. Edit the `renderer/renderer/core/shaders/shader_paths.h` file to reflect the locations of the shader binaries generated in the previous step. 
3. Execute `run.sh` to build & run the project.

Mouse and keyboard input is supported, though it will be unresponsive due to the slow performance. By default, a simple "triangle" scene will be ran which includes only a single triangle. This can be changed by setting the plugin argument in `run.sh` accordingly.

## About STFB instruction

As part of the project, a simple S-type instruction `stfb` was implemented and added to the Spike simulator. This instruction essentially acts as a 32-bit store to an offset from a base address specified on a custom CSR with address `0x800`, which is intended to hold the memory address of a framebuffer. This addition is rather trivial and requires a custom build of Spike, therefore it has been disabled via a preprocessor definition in `spike/main.c`. 

If one wishes to enable this feature, an `stfb.patch` file has been provided which can be applied to the Spike v1.1.0 tree, after which Spike can be rebuilt and the relevant macro disabled.

## Acknowledgements

This work is based on the [Software renderer](https://github.com/zauonlok/renderer) written by Zhou Le and licensed under the terms of the [MIT license](renderer/LICENSE).

I would like to extend a special thanks to Nikos Stavropoulos ([nstavropoulos](https://github.com/nstavropoulos)) for his help on various aspects of this project.
