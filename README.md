# TPDE ORC JIT Compiler

A command-line tool that loads LLVM IR files and JIT compiles them using LLVM ORC.

## Build

We need Git, CMake 3.16+, Ninja build system, a C++17 compatible compiler and LLVM release 20:

```
> git clone <repository-url>
> cd tpde-orc
> git submodule update --init --recursive
> cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=On
> ninja -C build tpde-orc
> ./build/tpde-orc 01-basic.ll
Loaded module: 01-basic.ll
Executing main()
Program returned: 0
> ./build/tpde-orc 01-basic.ll --entrypoint custom_entry
Loaded module: 01-basic.ll
Executing custom_entry()
Program returned: 123
```
