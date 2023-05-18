/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/client_context/map_gpu_va_gmm.h"

namespace NEO {
uint64_t GmmClientContext::mapGpuVirtualAddress(MapGpuVirtualAddressGmm *pMapGpuVa) {
    GMM_MAPGPUVIRTUALADDRESS gmmMapAddress = {pMapGpuVa->mapGpuVirtualAddressParams, 1, pMapGpuVa->resourceInfoHandle, pMapGpuVa->outVirtualAddress};
    return clientContext->MapGpuVirtualAddress(&gmmMapAddress);
}
uint64_t GmmClientContext::freeGpuVirtualAddress(FreeGpuVirtualAddressGmm *pFreeGpuVa) {
    GMM_FREEGPUVIRTUALADDRESS gmmFreeAddress = {pFreeGpuVa->hAdapter, pFreeGpuVa->baseAddress, pFreeGpuVa->size, 1, pFreeGpuVa->resourceInfoHandle};
    return clientContext->FreeGpuVirtualAddress(&gmmFreeAddress);
}

} // namespace NEO
