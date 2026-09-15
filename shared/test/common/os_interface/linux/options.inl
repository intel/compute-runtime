/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "common/gtsysinfo.h"
#include "neo_igfxfmid.h"

#include <cstdint>

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
const char *sysFsPciPathPrefix = "/linux/devices/";
const char *pciDevicesDirectory = "/linux/by-path";
const char *sysFsProcPathPrefix = "/linux/proc/";
const char *sysFsSystemCpuPathPrefix = "/linux/devices/system/cpu";
} // namespace Os

namespace ContextGroup {
uint32_t maxContextCount = 8;
}
