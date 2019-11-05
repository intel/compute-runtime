# NEO in Linux distributions

## Arch Linux*

```
pacman -S intel-compute-runtime
```

## Centos* 7, 8, Red Hat Enterprise Linux* 7

```
yum install yum-plugin-copr
yum copr enable jdanecki/intel-opencl
yum install intel-opencl
```

## Clear Linux

```
swupd bundle-add computer-vision-basic
```

## Exherbo Linux*

```
cave resolve --execute intel-compute-runtime
```

## Fedora* 30, 31, rawhide, Red Hat Enterprise Linux* 8 Beta, Mageia* 7

```
dnf install dnf-plugins-core
dnf copr enable jdanecki/intel-opencl
dnf install intel-opencl
```

## Gentoo*

```
emerge intel-neo
```

## NixOS

```
nix-channel --add https://nixos.org/channels/nixpkgs-unstable
nix-channel --update
nix-env -i intel-compute-runtime
```

## OpenSUSE Leap 15.1

```
zypper addrepo -r https://copr.fedorainfracloud.org/coprs/jdanecki/intel-opencl/repo/opensuse-leap-15.1/jdanecki-intel-opencl-opensuse-leap-15.1.repo
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

## Ubuntu* ppa for 16.04, 18.04, 19.04, 19.10

```
add-apt-repository ppa:intel-opencl/intel-opencl
apt-get update
apt-get install intel-opencl-icd
```

## Ubuntu* 19.04, 19.10

```
apt-get install intel-opencl-icd
```

## Neo in docker containers

Docker images are provided in [intel-opencl](https://hub.docker.com/r/intelopencl/intel-opencl) repository.

Example for Fedora* 30

```
docker run -it --device /dev/dri:/dev/dri --rm docker.io/intelopencl/intel-opencl:fedora-30-copr clinfo
```

## Additional configuration

To allow Neo accessing GPU device make sure user has permissions to files in /dev/dri directory.
In first step /dev/dri/renderD* files are opened, if it fails, /dev/dri/card* files are used.

Under Ubuntu* or Centos* user must be in video group.
In Fedora* all users by default have access to /dev/dri/renderD* files,
but have to be in video group to access /dev/dri/card* files.
 
## Building and installation

* [Ubuntu*](https://github.com/intel/compute-runtime/blob/master/documentation/BUILD_Ubuntu.md)
* [Centos* 8](https://github.com/intel/compute-runtime/blob/master/documentation/BUILD_Centos.md)
* [spec files](https://github.com/JacekDanecki/neo-specs) for rpm-based Linux distributions

(*) Other names and brands may be claimed as property of others.
