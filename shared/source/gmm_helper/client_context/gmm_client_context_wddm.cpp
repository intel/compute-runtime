/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/client_context/map_gpu_va_gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/os_interface/windows/gdi_interface.h"

namespace NEO {
uint64_t GmmClientContext::mapGpuVirtualAddress(MapGpuVirtualAddressGmm *pMapGpuVa) {
    auto gmmResourceFlags = pMapGpuVa->resourceInfoHandle->getResourceFlags()->Info;
    if ((gmmResourceFlags.MediaCompressed || gmmResourceFlags.RenderCompressed) && !pMapGpuVa->resourceInfoHandle->isResourceDenyCompressionEnabled()) {
        auto gmmResourceInfo = pMapGpuVa->resourceInfoHandle->peekGmmResourceInfo();
        GMM_MAPGPUVIRTUALADDRESS gmmMapAddress = {pMapGpuVa->mapGpuVirtualAddressParams, 1, &gmmResourceInfo, pMapGpuVa->outVirtualAddress};
        return clientContext->MapGpuVirtualAddress(&gmmMapAddress);
    } else {
        return pMapGpuVa->gdi->mapGpuVirtualAddress(pMapGpuVa->mapGpuVirtualAddressParams);
    }
}
uint64_t GmmClientContext::freeGpuVirtualAddress(FreeGpuVirtualAddressGmm *pFreeGpuVa) {
    auto gmmResourceFlags = pFreeGpuVa->resourceInfoHandle->getResourceFlags()->Info;
    if ((gmmResourceFlags.MediaCompressed || gmmResourceFlags.RenderCompressed) && !pFreeGpuVa->resourceInfoHandle->isResourceDenyCompressionEnabled()) {
        auto gmmResourceInfo = pFreeGpuVa->resourceInfoHandle->peekGmmResourceInfo();
        GMM_FREEGPUVIRTUALADDRESS gmmFreeAddress = {pFreeGpuVa->hAdapter, pFreeGpuVa->baseAddress, pFreeGpuVa->size, 1, &gmmResourceInfo};
        return clientContext->FreeGpuVirtualAddress(&gmmFreeAddress);
    } else {
        return 0;
    }
}
long GmmClientContext::deallocate2(DeallocateGmm *deallocateGmm) {
    GMM_DESTROYALLOCATION2 gmmDestroyAllocation2{};
    memcpy_s(&gmmDestroyAllocation2.KmtObj, sizeof(D3DKMT_DESTROYALLOCATION2), deallocateGmm->destroyAllocation2, sizeof(D3DKMT_DESTROYALLOCATION2));

    return clientContext->DeAllocate2(&gmmDestroyAllocation2);
}

} // namespace NEO
