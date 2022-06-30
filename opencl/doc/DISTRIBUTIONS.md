<!---

Copyright (C) 2020-2021 Intel Corporation

SPDX-License-Identifier: MIT

-->

# NEO OpenCL in Linux distributions

[![Packaging status](https://repology.org/badge/vertical-allrepos/intel-compute-runtime.svg)](https://repology.org/project/intel-compute-runtime/versions)

## Arch Linux*

```
pacman -S intel-compute-runtime
```

## Exherbo Linux*

```
cave resolve --execute intel-compute-runtime
```

## Gentoo*, Funtoo*

```
emerge intel-neo
```

## NixOS

```
nix-channel --add https://nixos.org/channels/nixpkgs-unstable
nix-channel --update
nix-env -i intel-compute-runtime
```

## PLD Linux*

```
ipoldek install intel-gmmlib intel-graphics-compiler intel-compute-runtime
```

## Ubuntu* 20.04, 21.04

```
apt install intel-opencl-icd
```

## Conda (Linux glibc>=2.12)

```
conda config --add channels conda-forge
conda install intel-compute-runtime
```

## Building and installation

* [Ubuntu*](https://github.com/intel/compute-runtime/blob/master/BUILD.md)
* [Centos* 8](https://github.com/intel/compute-runtime/blob/master/BUILD.md)

# NEO in other distributions

## FreeBSD*, DragonFly*

```
pkg install intel-compute-runtime
```

(*) Other names and brands may be claimed as property of others.
