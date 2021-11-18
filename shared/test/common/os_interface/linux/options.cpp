/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"

#include "common/gtsysinfo.h"
#include "igfxfmid.h"

namespace Os {
///////////////////////////////////////////////////////////////////////////////
// These options determine the Linux specific behavior for
// the runtime unit tests
///////////////////////////////////////////////////////////////////////////////
#if defined(__linux__)
const char *frontEndDllName = "_invalidFCL";
const char *igcDllName = "_invalidIGC";
const char *libvaDllName = nullptr;
const char *testDllName = "libtest_dynamic_lib.so";
const char *metricsLibraryDllName = "";
const char *gdiDllName = "";
const char *dxcoreDllName = "";
#endif
const char *sysFsPciPathPrefix = "./test_files/linux/devices/";
const char *pciDevicesDirectory = "./test_files/linux/by-path";
const char *sysFsProcPathPrefix = "./test_files/linux/proc/";
} // namespace Os

NEO::OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace) {
    return nullptr;
}
