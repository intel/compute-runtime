<!---

Copyright (C) 2022 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Frequently asked questions (Level Zero)

For general questions,
see the [main FAQ](https://github.com/intel/compute-runtime/blob/master/FAQ.md).

## Feature: l0_cache

Mechanism to cache binary representations of GPU kernels passed to zeModuleCreate(),
to avoid compilation overheads in subsequent calls. Please see more information in
https://github.com/intel/compute-runtime/blob/master/opencl/doc/FAQ.md#feature-cl_cache.

To enable it, please create a *l0_cache* directory in the working directory.
