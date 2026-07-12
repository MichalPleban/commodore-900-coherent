# Commodore 900 Coherent

This repository contains the source code for the Commodore 900 Z8001 version of Coherent. The sources are in ongoing development, i.e. bugs are being fixed and new features are being added. If you are interested in the original unmodified cources, check the [original source repository](https://github.com/MichalPleban/commodore-900-dump)

## Building

The code builds on a modern machine (Windows/Linux/MacOS) using GNU make. You need the following development tools:

* [Portable C Compiler](https://github.com/MichalPleban/pcc-z8001). Check out the `z8001` branch and type `./configure --target=z8001-coherent` followed by `make` and `make install`. This will build `z8001-coherent-cc`, `z8001-coherent-cpp` and `z8001-coherent-ccom`.
* [Cross-development tools](https://github.com/MichalPleban/commodore-900-crossdev). The `make` command will build `z8001-coherent-as` and `z8001-coherent-ld`.

Make sure that all these utilities are in your `PATH`, then type `make`. A hard drive image will be built in `build/dist/hdd.bin`, which can be used in an emulator or tansferred to a hard drive in a real machine.

## Limitations

Several binaries (notably the original C compiler) have no known sources. These binaries are placed into `src/dist` along with configuration files and non-compilable data, and copied verbatim by the `make` command into `dist/root`.
