/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"

namespace NEO {
extern const char *wslComputeHelperLibNameToLoad;
}

void setupExternalDependencies() {
    NEO::wslComputeHelperLibNameToLoad = "";
}

void setAdapterInfo(const NEO::HardwareInfo *hwInfo) {
    mockSetAdapterInfo(&hwInfo->platform, &hwInfo->gtSystemInfo, hwInfo->capabilityTable.gpuAddressSpace);
}
