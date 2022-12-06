/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mock_gdi/mock_os_library.h"

#include "common/gtsysinfo.h"
#include "igfxfmid.h"

namespace Os {
///////////////////////////////////////////////////////////////////////////////
// These options determine the Windows specific behavior for
// the runtime unit tests
///////////////////////////////////////////////////////////////////////////////
const char *frontEndDllName = "";
const char *igcDllName = "";
const char *gdiDllName = "";
const char *dxcoreDllName = "";
const char *testDllName = "test_dynamic_lib.dll";
const char *metricsLibraryDllName = "";
} // namespace Os

NEO::OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace) {
    NEO::OsLibrary *mockGdiDll;
    mockGdiDll = NEO::MockOsLibrary::load("");

    typedef void (*pfSetAdapterInfo)(const void *, const void *, uint64_t);
    pfSetAdapterInfo setAdapterInfo = reinterpret_cast<pfSetAdapterInfo>(mockGdiDll->getProcAddress("mockSetAdapterInfo"));

    setAdapterInfo(platform, gtSystemInfo, gpuAddressSpace);

    return mockGdiDll;
}
