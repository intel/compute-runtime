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

## Centos* 7, Centos* 8, Centos* 8 Stream, Red Hat Enterprise Linux* 7

```
yum install yum-plugin-copr
yum copr enable jdanecki/intel-opencl
yum install intel-opencl
```

## Exherbo Linux*

```
cave resolve --execute intel-compute-runtime
```

## Fedora* 33, 34, 35, rawhide (36), Centos* 9 Stream, Red Hat Enterprise Linux* 8, Mageia* 8

```
dnf install dnf-plugins-core
dnf copr enable jdanecki/intel-opencl
dnf install intel-opencl
```

## Mageia* Cauldron

```
dnf install dnf-plugins-core
dnf copr enable jdanecki/intel-opencl mageia-cauldron-x86_64
dnf install intel-opencl
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
## OpenSUSE Leap 15.2

```
zypper addrepo -r https://copr.fedorainfracloud.org/coprs/jdanecki/intel-opencl/repo/opensuse-leap-15.2/jdanecki-intel-opencl-opensuse-leap-15.2.repo
zypper install intel-opencl
```

## OpenSUSE Leap 15.3

```
zypper addrepo -r https://copr.fedorainfracloud.org/coprs/jdanecki/intel-opencl/repo/opensuse-leap-15.3/jdanecki-intel-opencl-opensuse-leap-15.3.repo
zypper install intel-opencl
```

## OpenSUSE tumbleweed

```
zypper addrepo -r https://copr.fedorainfracloud.org/coprs/jdanecki/intel-opencl/repo/opensuse-tumbleweed/jdanecki-intel-opencl-opensuse-tumbleweed.repo
zypper install intel-opencl
```

## PLD Linux*

```
ipoldek install intel-gmmlib intel-graphics-compiler intel-compute-runtime
```

## Ubuntu* ppa for 18.04, 20.04, 21.04

```
add-apt-repository ppa:intel-opencl/intel-opencl
apt update
apt install intel-opencl-icd
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
* Scripts to build or download rpm (copr) and deb (github and ppa) packages are available in [neo-specs](https://github.com/JacekDanecki/neo-specs) repository.

# NEO in other distributions

## FreeBSD*, DragonFly*

```
pkg install intel-compute-runtime
```

(*) Other names and brands may be claimed as property of others.
