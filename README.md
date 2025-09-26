# TPDE ORC JIT Compiler

A command-line tool that loads LLVM IR files and JIT compiles them with [TPDE](https://github.com/tpde2/tpde) in [LLVM ORC](https://llvm.org/docs/ORCv2.html).

## Build

We need Git, CMake 3.16+, Ninja build system, a C++17 compatible compiler and LLVM release 20:

```
> git clone https://github.com/weliveindetail/tpde-orc
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
> ./build/tpde-orc 02-bfloat.ll
Loaded module: 02-bfloat.ll
[2025-09-25 10:12:44.627] [error] unsupported type: bfloat
[2025-09-25 10:12:44.627] [error] Failed to compile function main
Falling back to LLVM for module: 02-bfloat.ll
Executing main()
Program returned: 50
```

In single-threaded compilation, TPDE is ~4x faster than LLVM for this example:
```
$ ./build/tpde-orc --par 1 tpde-orc/03-csmith-tpde.ll
Load module: tpde-orc/03-csmith-tpde.ll
Compiling 100 modules on 1 threads in parallel
...
Compile-time was: 2200 ms

$ ./build/tpde-orc --par 1 tpde-orc/03-csmith-tpde.ll --llvm
Load module: tpde-orc/03-csmith-tpde.ll
Compiling 100 modules on 1 threads in parallel
...
Compile-time was: 8820 ms
```

In concurrent compilation, TPDE is only ~3x faster than LLVM in the current setup:
```
> ./build/tpde-orc --par 8 tpde-orc/03-csmith-tpde.ll
Load module: tpde-orc/03-csmith-tpde.ll
Compiling 100 modules on 8 threads in parallel
...
Compile-time was: 737 ms

> ./build/tpde-orc --par 8 tpde-orc/03-csmith-tpde.ll --llvm
Load module: tpde-orc/03-csmith-tpde.ll
Compiling 100 modules on 8 threads in parallel
...
Compile-time was: 2117 ms
```

Overhead of TPDE-to-LLVM fallback is below 5%:
```
> ./build/tpde-orc --par 8 tpde-orc/04-csmith-fallback.ll
Load module: tpde-orc/04-csmith-fallback.ll
Compiling 100 modules on 8 threads in parallel
Executing all entrypoints
...
[2025-09-26 12:53:06.800] [error] unsupported type: i112
[2025-09-26 12:53:06.800] [error] Failed to compile function 04-csmith-fallback_0
Falling back to LLVM for module: 04-csmith-fallback_0
Executing all entrypoints
Executing 04-csmith-fallback_57() checksum = 5E30C03
Executing 04-csmith-fallback_15() checksum = 5E30C03
..
Compile-time was: 1512 ms

> ./build/tpde-orc --par 8 tpde-orc/04-csmith-fallback.ll --llvm
Load module: tpde-orc/04-csmith-fallback.ll
Compiling 100 modules on 8 threads in parallel
Executing all entrypoints
Executing 04-csmith-fallback_45() checksum = 5E30C03
Executing 04-csmith-fallback_18() checksum = 5E30C03
...
Compile-time was: 1443 ms
```
