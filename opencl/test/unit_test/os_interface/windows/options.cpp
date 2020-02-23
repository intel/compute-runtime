/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"

#include "common/gtsysinfo.h"
#include "igfxfmid.h"

namespace Os {
///////////////////////////////////////////////////////////////////////////////
// These options determine the Windows specific behavior for
// the runtime unit tests
///////////////////////////////////////////////////////////////////////////////
const char *frontEndDllName = "";
const char *igcDllName = "";
const char *gdiDllName = "gdi32_mock.dll";
const char *testDllName = "test_dynamic_lib.dll";
const char *metricsLibraryDllName = "";
} // namespace Os

NEO::OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace) {
    NEO::OsLibrary *mockGdiDll;
    mockGdiDll = NEO::OsLibrary::load("gdi32_mock.dll");

    typedef void(__stdcall * pfSetAdapterInfo)(const void *, const void *, uint64_t);
    pfSetAdapterInfo setAdpaterInfo = reinterpret_cast<pfSetAdapterInfo>(mockGdiDll->getProcAddress("MockSetAdapterInfo"));

    setAdpaterInfo(platform, gtSystemInfo, gpuAddressSpace);

    return mockGdiDll;
}
