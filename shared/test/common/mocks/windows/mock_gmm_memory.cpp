/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/windows/mock_gmm_memory.h"

namespace NEO {

bool MockGmmMemory::configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                                GMM_ESCAPE_HANDLE hDevice,
                                                GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                                GMM_GFX_SIZE_T svmSize,
                                                BOOLEAN bdwL3Coherency) {
    configureDeviceAddressSpaceCalled++;
    configureDeviceAddressSpaceParamsPassed.push_back({hAdapter,
                                                       hDevice,
                                                       pfnEscape,
                                                       svmSize,
                                                       bdwL3Coherency});
    return configureDeviceAddressSpaceResult;
}

bool MockGmmMemory::setDeviceInfo(GMM_DEVICE_INFO *deviceInfo) {
    setDeviceInfoCalled++;
    deviceCallbacks = *deviceInfo->pDeviceCb;
    return setDeviceInfoResult;
}
} // namespace NEO
