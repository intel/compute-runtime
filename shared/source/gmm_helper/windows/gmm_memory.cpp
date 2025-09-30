/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/windows/gmm_memory.h"

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/windows_defs.h"

namespace NEO {
GmmMemory::GmmMemory(GmmClientContext *gmmClientContext) : clientContext(*gmmClientContext->getHandle()) {
}

bool GmmMemory::configureDevice(GMM_ESCAPE_HANDLE hAdapter,
                                GMM_ESCAPE_HANDLE hDevice,
                                GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                GMM_GFX_SIZE_T svmSize,
                                BOOLEAN bdwL3Coherency,
                                uintptr_t &minAddress,
                                bool obtainMinAddress) {
    minAddress = windowsMinAddress;
    auto retVal = configureDeviceAddressSpace(hAdapter, hDevice, pfnEscape, svmSize, bdwL3Coherency);
    if (obtainMinAddress) {
        minAddress = getInternalGpuVaRangeLimit();
    }
    return retVal;
}
uintptr_t GmmMemory::getInternalGpuVaRangeLimit() {
    return static_cast<uintptr_t>(clientContext.GetInternalGpuVaRangeLimit());
}

bool GmmMemory::setDeviceInfo(GMM_DEVICE_INFO *deviceInfo) {
    auto status = clientContext.GmmSetDeviceInfo(deviceInfo);
    DEBUG_BREAK_IF(status != GMM_SUCCESS);
    return GMM_SUCCESS == status;
}
}; // namespace NEO
