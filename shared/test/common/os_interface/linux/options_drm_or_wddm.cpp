/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mock_gdi/mock_os_library.h"

#include "common/gtsysinfo.h"
#include "igfxfmid.h"
#include "test_files_setup.h"

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
const char *sysFsPciPathPrefix = NEO_SHARED_TEST_FILES_DIR "/linux/devices/";
const char *pciDevicesDirectory = NEO_SHARED_TEST_FILES_DIR "/linux/by-path";
const char *sysFsProcPathPrefix = NEO_SHARED_TEST_FILES_DIR "/linux/proc/";
} // namespace Os

NEO::OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace) {
    NEO::OsLibrary *mockGdiDll;
    mockGdiDll = NEO::MockOsLibrary::load("");

    typedef void (*pfSetAdapterInfo)(const void *, const void *, uint64_t);
    pfSetAdapterInfo setAdapterInfo = reinterpret_cast<pfSetAdapterInfo>(mockGdiDll->getProcAddress("mockSetAdapterInfo"));

    setAdapterInfo(platform, gtSystemInfo, gpuAddressSpace);

    return mockGdiDll;
}
