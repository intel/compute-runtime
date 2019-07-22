# NEO in Linux distributions

## Arch Linux*

```
pacman -S intel-compute-runtime
```

## Clear Linux

```
swupd bundle-add computer-vision-basic
```

## Exherbo Linux

```
cave resolve --execute intel-compute-runtime
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
