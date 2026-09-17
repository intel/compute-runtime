/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mock_gdi/mock_gdi.h"

#include "common/gtsysinfo.h"
#include "neo_igfxfmid.h"

namespace NEO {
extern const char *wslComputeHelperLibNameToLoad;
}

void setupExternalDependencies() {
    NEO::wslComputeHelperLibNameToLoad = "";
}

void setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace) {
    mockSetAdapterInfo(platform, gtSystemInfo, gpuAddressSpace);
}
