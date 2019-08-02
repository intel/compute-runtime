# NEO in Linux distributions

## Arch Linux*

```
pacman -S intel-compute-runtime
```

## Centos*

```
yum install yum-plugin-copr
yum copr enable jdanecki/intel-opencl
yum install intel-opencl
```

## Clear Linux

```
swupd bundle-add computer-vision-basic
```

## Exherbo* Linux

```
cave resolve --execute intel-compute-runtime
```

## Fedora* 30 and rawhide

```
dnf install dnf-plugins-core
dnf copr enable jdanecki/intel-opencl
dnf install intel-opencl
```

## Gentoo*

```
emerge intel-neo
```

## Ubuntu* 16.04 and 18.04 ppa

```
add-apt-repository ppa:intel-opencl/intel-opencl
apt-get update
apt-get install intel-opencl
```

## Ubuntu* 19.04

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

* [Ubuntu* 16.04](https://github.com/intel/compute-runtime/blob/master/documentation/BUILD_Ubuntu.md)
* [Centos* 7](https://github.com/intel/compute-runtime/blob/master/documentation/BUILD_Centos.md)

(*) Other names and brands may be claimed as property of others.
