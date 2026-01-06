/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/windows/gmm_memory.h"

namespace NEO {

bool GmmMemory::configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                            GMM_ESCAPE_HANDLE hDevice,
                                            GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                            GMM_GFX_SIZE_T svmSize,
                                            BOOLEAN bdwL3Coherency) {
    return true;
}

}; // namespace NEO
