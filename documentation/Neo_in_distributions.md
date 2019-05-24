# NEO in Linux distributions

## Arch Linux*

```
pacman -S intel-compute-runtime
```

## Clear Linux

```
swupd bundle-add computer-vision-basic
```

## Ubuntu* 19.04

```
apt-get install intel-opencl-icd
```

## Ubuntu* 18.04 and 16.04 ppa

```
add-apt-repository ppa:intel-opencl/intel-opencl
apt-get update
apt-get install intel-opencl
```

## Building and installation

* [Ubuntu* 16.04](https://github.com/intel/compute-runtime/blob/master/documentation/BUILD_Ubuntu.md)
* [Centos* 7](https://github.com/intel/compute-runtime/blob/master/documentation/BUILD_Centos.md)

(*) Other names and brands may be claimed as property of others.
