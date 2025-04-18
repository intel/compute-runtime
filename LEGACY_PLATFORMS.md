<!---

Copyright (C) 2024-2025 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Support for legacy platforms

Starting from release [24.35.30872.22](https://github.com/intel/compute-runtime/releases/tag/24.35.30872.22) regular packages support Gen12 and later devices.

Support for Gen8, Gen9 and Gen11 devices will be delivered via packages with _legacy1_ suffix:
- intel-opencl-icd-legacy1_24.35.30872.22_amd64.deb
- intel-level-zero-gpu-legacy1_1.3.30872.22_amd64.deb

## Supported legacy platforms

|Platform|OpenCL|Level Zero|WSL
|--------|:----:|:--------:|:----:|
[Broadwell](https://ark.intel.com/content/www/us/en/ark/products/codename/38530/broadwell.html) | 3.0 | -- | --
[Skylake](https://ark.intel.com/content/www/us/en/ark/products/codename/37572/skylake.html) | 3.0 | 1.5 | --
[Kaby Lake](https://ark.intel.com/content/www/us/en/ark/products/codename/82879/kaby-lake.html) |  3.0 | 1.5 | --
[Coffee Lake](https://ark.intel.com/content/www/us/en/ark/products/codename/97787/coffee-lake.html) |  3.0 | 1.5 | Yes
[Apollo Lake](https://ark.intel.com/content/www/us/en/ark/products/codename/80644/apollo-lake.html) | 3.0 | -- | --
[Gemini Lake](https://ark.intel.com/content/www/us/en/ark/products/codename/83915/gemini-lake.html) | 3.0 | -- | --
[Ice Lake](https://ark.intel.com/content/www/us/en/ark/products/codename/74979/ice-lake.html) |  3.0 | 1.5 | Yes
[Elkhart Lake](https://ark.intel.com/content/www/us/en/ark/products/codename/128825/elkhart-lake.html) | 3.0 | -- | Yes

## Maintanance expectations

No new features will be added to legacy packages.

In case of critical fixes we plan to deliver them through a new release from [24.35 release branch](https://github.com/intel/compute-runtime/tree/releases/24.35).

## Build instructions

To build legacy1 packages follow instructions [here](BUILD.md#optional---building-neo-with-support-for-gen8-gen9-and-gen11-devices).
