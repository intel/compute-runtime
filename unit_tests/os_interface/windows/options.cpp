/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "common/gtsysinfo.h"
#include "runtime/os_interface/os_library.h"

#include "igfxfmid.h"

namespace Os {
///////////////////////////////////////////////////////////////////////////////
// These options determine the Windows specific behavior for
// the runtime unit tests
///////////////////////////////////////////////////////////////////////////////
const char *frontEndDllName = "";
const char *igcDllName = "";
const char *gdiDllName = "gdi32_mock.dll";
const char *gmmDllName = "mock_gmm.dll";
const char *gmmEntryName = "openMockGmm";
const char *testDllName = "test_dynamic_lib.dll";
} // namespace Os

OCLRT::OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo) {
    OCLRT::OsLibrary *mockGdiDll;
    mockGdiDll = OCLRT::OsLibrary::load("gdi32_mock.dll");

    typedef void(__stdcall * pfSetAdapterInfo)(const void *, const void *);
    pfSetAdapterInfo setAdpaterInfo = reinterpret_cast<pfSetAdapterInfo>(mockGdiDll->getProcAddress("MockSetAdapterInfo"));

    setAdpaterInfo(platform, gtSystemInfo);

    return mockGdiDll;
}
