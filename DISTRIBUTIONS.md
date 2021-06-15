<!---

Copyright (C) 2019-2021 Intel Corporation

SPDX-License-Identifier: MIT

-->

# NEO in Linux distributions

## Level Zero specific

* [distributions](https://github.com/intel/compute-runtime/blob/master/level_zero/doc/DISTRIBUTIONS.md)

## OpenCL specific

* [distributions](https://github.com/intel/compute-runtime/blob/master/opencl/doc/DISTRIBUTIONS.md)

## Intel software for General Purpose GPU capabilities

* Documentation and instructions for installing, deploying, and updating Intel software to enable general purpose GPU (GPGPU) capabilities for Linux*-based operating system distributions can be found on:
    [https://dgpu-docs.intel.com](https://dgpu-docs.intel.com)

* An Intel brand integrated and global search accessible Linux graphics landing page [https://intel.com/linux-graphics-drivers](https://intel.com/linux-graphics-drivers)

## Packages mirror

Starting with [release 19.43.14583](https://github.com/intel/compute-runtime/releases/tag/19.43.14583) all packages are mirrored on
[SourceForge](https://sourceforge.net/projects/intel-compute-runtime) as older packages are automatically deleted on
[launchpad](https://launchpad.net/~intel-opencl/+archive/ubuntu/intel-opencl) and [copr](https://copr.fedorainfracloud.org/coprs/jdanecki/intel-opencl).

## Neo in docker containers

Docker images are provided in [intel-opencl](https://hub.docker.com/r/intelopencl/intel-opencl) repository.

Example for Fedora* 33

```
docker run -it --device /dev/dri:/dev/dri --rm docker.io/intelopencl/intel-opencl:fedora-33-copr clinfo
```

