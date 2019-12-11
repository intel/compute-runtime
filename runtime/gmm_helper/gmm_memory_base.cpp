/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_memory_base.h"

#include "core/gmm_helper/gmm_helper.h"
#include "core/os_interface/windows/windows_defs.h"
#include "runtime/platform/platform.h"

#include "gmm_client_context.h"

namespace NEO {
GmmMemoryBase::GmmMemoryBase() {
    clientContext = platform()->peekGmmHelper()->getClientContext()->getHandle();
}
bool GmmMemoryBase::configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                                GMM_ESCAPE_HANDLE hDevice,
                                                GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                                GMM_GFX_SIZE_T SvmSize,
                                                BOOLEAN BDWL3Coherency) {
    return clientContext->ConfigureDeviceAddressSpace(
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
                                    GMM_GFX_PARTITIONING &gfxPartition,
                                    uintptr_t &minAddress) {
    minAddress = windowsMinAddress;
    return configureDeviceAddressSpace(hAdapter, hDevice, pfnEscape, SvmSize, BDWL3Coherency);
}
}; // namespace NEO
