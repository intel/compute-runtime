# Building Level Zero

These instructions have been tested on Ubuntu* and complement those existing for NEO in the top-level BUILD.md file.

1. Install Level Zero loader and Level Zero headers

Build Level Zero loader, as indicated in [https://github.com/oneapi-src/level-zero](https://github.com/oneapi-src/level-zero).

Build will generate ze_loader library and symlinks, as well as those for ze_validation_layer.

Optionally, two packages can be built: binary and devel.

2. Build Level Zero driver

Follow instructions in top-level BUILD.md file to build NEO. Level Zero is built by default.

When built, ze_intel_gpu library and symlinks are generated.

Optionally, you may install Level Zero loader and driver packages.

3. Build your application

Compilation needs to include the Level Zero headers and to link against the loader library:

```shell
g++ zello_world.cpp -o zello_world -lze_loader
```

If libraries not installed in system paths, include Level Zero headers and path to Level Zero loader:

```shell
g++ -I<path_to_Level_Zero_headers> zello_world.cpp -o zello_world -L<path_to_libze_loader.so> -lze_loader
```

4. Execute your application

If Level Zero loader packages have been built and installed in the system, then they will be present in system paths:

```shell
./zello_world
```

If libraries not installed in system paths, add paths to ze_loader and ze_intel_gpu libraries:

```shell
LD_LIBRARY_PATH=<path_to_libze_loader.so>:<path_to_libze_intel_gpu.so> ./zello_world
```

___(*) Other names and brands may be claimed as property of others.___