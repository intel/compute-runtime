/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/windows/gmm_memory_base.h"

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/windows_defs.h"

namespace NEO {
GmmMemoryBase::GmmMemoryBase(GmmClientContext *gmmClientContext) : clientContext(*gmmClientContext->getHandle()) {
}

bool GmmMemoryBase::configureDevice(GMM_ESCAPE_HANDLE hAdapter,
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
uintptr_t GmmMemoryBase::getInternalGpuVaRangeLimit() {
    return static_cast<uintptr_t>(clientContext.GetInternalGpuVaRangeLimit());
}

bool GmmMemoryBase::setDeviceInfo(GMM_DEVICE_INFO *deviceInfo) {
    auto status = clientContext.GmmSetDeviceInfo(deviceInfo);
    DEBUG_BREAK_IF(status != GMM_SUCCESS);
    return GMM_SUCCESS == status;
}
}; // namespace NEO
