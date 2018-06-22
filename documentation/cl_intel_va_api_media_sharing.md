# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Enabling [cl_intel_va_api_media_sharing](https://www.khronos.org/registry/OpenCL/extensions/intel/cl_intel_va_api_media_sharing.txt) extension

To enable cl_intel_va_api_media_sharing extension Neo needs to be compiled on system with libva 2.x installed.
Before compilation additional packages have to be installed.

1. Download sources:

* libdrm             https://anongit.freedesktop.org/git/mesa/drm.git
* libva              https://github.com/intel/libva.git

Example:

```shell
git clone https://anongit.freedesktop.org/git/mesa/drm.git libdrm
git clone https://github.com/intel/libva.git libva
```

2. Compile and install libdrm

Example:

```shell
cd libdrm
./autogen.sh
make -j `nproc`
sudo make install
```

3. Compile and install libva

Example:

```shell
cd libva
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
./autogen.sh
make -j `nproc`
sudo make install
```

4. During Neo compilation verify libva was discovered

```shell
-- Checking for module 'libva>=1.0.0'
--   Found libva, version 1.1.0
-- Looking for vaGetLibFunc in va
-- Looking for vaGetLibFunc in va - found
-- Using libva
```
