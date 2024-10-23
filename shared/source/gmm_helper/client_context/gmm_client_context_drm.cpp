/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"

namespace NEO {
uint64_t GmmClientContext::mapGpuVirtualAddress(MapGpuVirtualAddressGmm *pMapGpuVa) {
    return 0;
}
uint64_t GmmClientContext::freeGpuVirtualAddress(FreeGpuVirtualAddressGmm *pFreeGpuVa) {
    return 0;
}
long GmmClientContext::deallocate2(DeallocateGmm *deallocateGmm) {
    return 0;
}

} // namespace NEO
