<!---

Copyright (C) 2020-2021 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Level Zero in Linux distributions

## Centos* 7, Centos* 8, Centos* 8 Stream, Red Hat Enterprise Linux* 7

```
yum install yum-plugin-copr
yum copr enable jdanecki/intel-opencl
yum install level-zero intel-level-zero-gpu
```

## Fedora* 33, 34, 35, rawhide (36), Centos* 9 Stream, Red Hat Enterprise Linux* 8, Mageia* 8

```
dnf install dnf-plugins-core
dnf copr enable jdanecki/intel-opencl
dnf install level-zero intel-level-zero-gpu
```

## Mageia* Cauldron

```
dnf install dnf-plugins-core
dnf copr enable jdanecki/intel-opencl mageia-cauldron-x86_64
dnf install level-zero intel-level-zero-gpu
```

## OpenSUSE Leap 15.2

```
zypper addrepo -r https://copr.fedorainfracloud.org/coprs/jdanecki/intel-opencl/repo/opensuse-leap-15.2/jdanecki-intel-opencl-opensuse-leap-15.2.repo
zypper install level-zero intel-level-zero-gpu
```

## OpenSUSE Leap 15.3

```
zypper addrepo -r https://copr.fedorainfracloud.org/coprs/jdanecki/intel-opencl/repo/opensuse-leap-15.3/jdanecki-intel-opencl-opensuse-leap-15.3.repo
zypper install level-zero intel-level-zero-gpu
```

## OpenSUSE tumbleweed

```
zypper addrepo -r https://copr.fedorainfracloud.org/coprs/jdanecki/intel-opencl/repo/opensuse-tumbleweed/jdanecki-intel-opencl-opensuse-tumbleweed.repo
zypper install level-zero intel-level-zero-gpu
```

## Ubuntu* ppa for 18.04, 20.04, 21.04

```
add-apt-repository ppa:intel-opencl/intel-opencl
apt update
apt install libze-loader libze-intel-gpu
```

## Building and installation

* [Level Zero loader](https://github.com/oneapi-src/level-zero#building-and-installing)
* [Ubuntu*](https://github.com/intel/compute-runtime/blob/master/level_zero/doc/BUILD.md)
* Scripts to build or download rpm (copr) and deb (github and ppa) packages are available in [neo-specs](https://github.com/JacekDanecki/neo-specs) repository.

(*) Other names and brands may be claimed as property of others.
