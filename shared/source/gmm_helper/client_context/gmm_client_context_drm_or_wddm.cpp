/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/client_context/map_gpu_va_gmm.h"
#include "shared/source/os_interface/windows/gdi_interface.h"

namespace NEO {
uint64_t GmmClientContext::mapGpuVirtualAddress(MapGpuVirtualAddressGmm *pMapGpuVa) {
    return pMapGpuVa->gdi->mapGpuVirtualAddress(pMapGpuVa->mapGpuVirtualAddressParams);
}
uint64_t GmmClientContext::freeGpuVirtualAddress(FreeGpuVirtualAddressGmm *pFreeGpuVa) {
    if (!pFreeGpuVa->explicitFreeRequired) {
        return STATUS_SUCCESS;
    }
    D3DKMT_FREEGPUVIRTUALADDRESS freeGpuVirtualAddress = {pFreeGpuVa->hAdapter, pFreeGpuVa->baseAddress, pFreeGpuVa->size};
    return pFreeGpuVa->gdi->freeGpuVirtualAddress(&freeGpuVirtualAddress);
}
long GmmClientContext::deallocate2(DeallocateGmm *deallocateGmm) {
    return deallocateGmm->gdi->destroyAllocation2(deallocateGmm->destroyAllocation2);
}

} // namespace NEO
