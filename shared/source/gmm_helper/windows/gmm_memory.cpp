/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/windows/gmm_memory.h"

#include "shared/source/helpers/debug_helpers.h"

namespace NEO {
bool GmmMemory::configureDevice(GMM_ESCAPE_HANDLE hAdapter,
                                GMM_ESCAPE_HANDLE hDevice,
                                GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                GMM_GFX_SIZE_T svmSize,
                                BOOLEAN bdwL3Coherency) {
    return configureDeviceAddressSpace(hAdapter, hDevice, pfnEscape, svmSize, bdwL3Coherency);
}

bool GmmMemory::setDeviceInfo(GMM_DEVICE_INFO *deviceInfo) {
    auto status = clientContext.GmmSetDeviceInfo(deviceInfo);
    DEBUG_BREAK_IF(status != GMM_SUCCESS);
    return GMM_SUCCESS == status;
}
}; // namespace NEO
