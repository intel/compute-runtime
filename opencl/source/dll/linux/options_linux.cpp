/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"

#include "igc.opencl.h"

namespace Os {
// Compiler library names
const char *frontEndDllName = FCL_LIBRARY_NAME;
const char *igcDllName = IGC_LIBRARY_NAME;
const char *libvaDllName = "libva.so.2";
const char *gdiDllName = "/usr/lib/wsl/lib/libdxcore.so";
const char *dxcoreDllName = "/usr/lib/wsl/lib/libdxcore.so";

const char *sysFsPciPathPrefix = "/sys/bus/pci/devices/";
const char *pciDevicesDirectory = "/dev/dri/by-path";
const char *sysFsProcPathPrefix = "/proc";

// Metrics Library name
const char *metricsLibraryDllName = "libigdml.so.1";
} // namespace Os
