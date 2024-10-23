/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/map_gpu_va_gmm.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/test/common/mocks/mock_gmm_client_context_base.h"

namespace NEO {
uint64_t MockGmmClientContextBase::mapGpuVirtualAddress(MapGpuVirtualAddressGmm *pMapGpuVa) {
    mapGpuVirtualAddressCalled++;
    return pMapGpuVa->gdi->mapGpuVirtualAddress(pMapGpuVa->mapGpuVirtualAddressParams);
}
long MockGmmClientContextBase::deallocate2(DeallocateGmm *deallocateGmm) {
    return deallocateGmm->gdi->destroyAllocation2(deallocateGmm->destroyAllocation2);
}
} // namespace NEO
