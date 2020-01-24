/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_memory_base.h"

#include "core/helpers/debug_helpers.h"
#include "core/os_interface/windows/windows_defs.h"

#include "gmm_client_context.h"

namespace NEO {
GmmMemoryBase::GmmMemoryBase(GmmClientContext *gmmClientContext) : clientContext(*gmmClientContext->getHandle()) {
}
bool GmmMemoryBase::configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                                GMM_ESCAPE_HANDLE hDevice,
                                                GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                                GMM_GFX_SIZE_T SvmSize,
                                                BOOLEAN BDWL3Coherency) {
    return clientContext.ConfigureDeviceAddressSpace(
               {hAdapter},
               {hDevice},
               {pfnEscape},
               SvmSize,
               0,
               0,
               BDWL3Coherency,
               0,
               0) != 0;
}

bool GmmMemoryBase::configureDevice(GMM_ESCAPE_HANDLE hAdapter,
                                    GMM_ESCAPE_HANDLE hDevice,
                                    GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                    GMM_GFX_SIZE_T SvmSize,
                                    BOOLEAN BDWL3Coherency,
                                    uintptr_t &minAddress,
                                    bool obtainMinAddress) {
    minAddress = windowsMinAddress;
    auto retVal = configureDeviceAddressSpace(hAdapter, hDevice, pfnEscape, SvmSize, BDWL3Coherency);
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
