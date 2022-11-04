/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/windows/gmm_memory_base.h"
#include "shared/source/os_interface/windows/windows_defs.h"

namespace NEO {

bool GmmMemoryBase::configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                                GMM_ESCAPE_HANDLE hDevice,
                                                GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                                GMM_GFX_SIZE_T svmSize,
                                                BOOLEAN bdwL3Coherency) {
    return clientContext.ConfigureDeviceAddressSpace(
               {hAdapter},
               {hDevice},
               {pfnEscape},
               svmSize,
               0,
               0,
               bdwL3Coherency,
               0,
               0) != 0;
}

}; // namespace NEO
