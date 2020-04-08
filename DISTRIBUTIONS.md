# NEO in Linux distributions

## Level Zero specific

* [distributions](https://github.com/intel/compute-runtime/blob/master/level_zero/doc/DISTRIBUTIONS.md)

## OpenCL specific

* [distributions](https://github.com/intel/compute-runtime/blob/master/opencl/doc/DISTRIBUTIONS.md)

## Packages mirror

Starting with [release 19.43.14583](https://github.com/intel/compute-runtime/releases/tag/19.43.14583) all packages are mirrored on
[SourceForge](https://sourceforge.net/projects/intel-compute-runtime) as older packages are automatically deleted on
[launchpad](https://launchpad.net/~intel-opencl/+archive/ubuntu/intel-opencl) and [copr](https://copr.fedorainfracloud.org/coprs/jdanecki/intel-opencl).

## Neo in docker containers

Docker images are provided in [intel-opencl](https://hub.docker.com/r/intelopencl/intel-opencl) repository.

Example for Fedora* 30

```
docker run -it --device /dev/dri:/dev/dri --rm docker.io/intelopencl/intel-opencl:fedora-30-copr clinfo
```

