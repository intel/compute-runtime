/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"

#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"
#include "opencl/source/kernel/kernel.h"

namespace NEO {

template <typename GfxFamily>
void GTPinGfxCoreHelperHw<GfxFamily>::addSurfaceState(Kernel *pKernel) const {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;

    size_t sshSize = pKernel->getSurfaceStateHeapSize();
    size_t ssSize = sizeof(RENDER_SURFACE_STATE);
    size_t btsSize = sizeof(BINDING_TABLE_STATE);
    size_t sizeToEnlarge = ssSize + btsSize;
    size_t currBTOffset = 0u;
    if (isValidOffset<SurfaceStateHeapOffset>(static_cast<SurfaceStateHeapOffset>(pKernel->getBindingTableOffset()))) {
        currBTOffset = pKernel->getBindingTableOffset();
    }
    size_t currSurfaceStateSize = currBTOffset;
    char *pSsh = static_cast<char *>(pKernel->getSurfaceStateHeap());
    char *pNewSsh = new char[sshSize + sizeToEnlarge];
    memcpy_s(pNewSsh, sshSize + sizeToEnlarge, pSsh, currSurfaceStateSize);
    RENDER_SURFACE_STATE *pSS = reinterpret_cast<RENDER_SURFACE_STATE *>(pNewSsh + currSurfaceStateSize);
    *pSS = GfxFamily::cmdInitRenderSurfaceState;
    size_t newSurfaceStateSize = currSurfaceStateSize + ssSize;
    size_t currBTCount = pKernel->getNumberOfBindingTableStates();
    memcpy_s(pNewSsh + newSurfaceStateSize, sshSize + sizeToEnlarge - newSurfaceStateSize, pSsh + currBTOffset, currBTCount * btsSize);
    BINDING_TABLE_STATE *pNewBTS = reinterpret_cast<BINDING_TABLE_STATE *>(pNewSsh + newSurfaceStateSize + currBTCount * btsSize);
    *pNewBTS = GfxFamily::cmdInitBindingTableState;
    pNewBTS->setSurfaceStatePointer((uint64_t)currBTOffset);
    pKernel->resizeSurfaceStateHeap(pNewSsh, sshSize + sizeToEnlarge, currBTCount + 1, newSurfaceStateSize);
}

template <typename GfxFamily>
void *GTPinGfxCoreHelperHw<GfxFamily>::getSurfaceState(Kernel *pKernel, size_t bti) const {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;

    if ((nullptr == pKernel->getSurfaceStateHeap()) || (bti >= pKernel->getNumberOfBindingTableStates())) {
        return nullptr;
    }
    auto *pBts = reinterpret_cast<BINDING_TABLE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(), (pKernel->getBindingTableOffset() + bti * sizeof(BINDING_TABLE_STATE))));
    auto pSurfaceState = ptrOffset(pKernel->getSurfaceStateHeap(), pBts->getSurfaceStatePointer());
    return pSurfaceState;
}
} // namespace NEO
