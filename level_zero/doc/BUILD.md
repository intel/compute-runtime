# Building Level Zero

These instructions have been tested on Ubuntu* and complement those existing for NEO.

1. Install Level Zero loader

Build and install Level Zero loader, as indicated in [https://github.com/oneapi-src/level-zero](https://github.com/oneapi-src/level-zero).
Both packages generated from Level Zero loader are needed.

2. Build Level Zero driver with NEO

This generates `libze_intel_gpu.so`.

3. Compile your application

Compilation needs to include the Level Zero headers from the Level Zero loader and to link against the loader library.

```shell
g++ ~/zello_world.cpp -o zello_world -lze_loader
```

4. Execute your application

Set your paths to find the ze_loader and the ze_intel_gpu libraries if not present in standard paths.

```shell
./zello_world
```

___(*) Other names and brands may be claimed as property of others.___
