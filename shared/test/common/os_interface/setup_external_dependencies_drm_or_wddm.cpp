/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mock_gdi/mock_os_library.h"

#include "common/gtsysinfo.h"
#include "neo_igfxfmid.h"

namespace NEO {
extern const char *wslComputeHelperLibNameToLoad;
}

void setupExternalDependencies() {
    NEO::wslComputeHelperLibNameToLoad = "";
}

NEO::OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace) {
    NEO::OsLibrary *mockGdiDll;
    mockGdiDll = NEO::MockOsLibrary::load("");

    typedef void (*pfSetAdapterInfo)(const void *, const void *, uint64_t);
    pfSetAdapterInfo setAdapterInfo = reinterpret_cast<pfSetAdapterInfo>(mockGdiDll->getProcAddress("mockSetAdapterInfo"));

    setAdapterInfo(platform, gtSystemInfo, gpuAddressSpace);

    return mockGdiDll;
}
