/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/gtpin/gtpin_hw_helper.h"
#include "runtime/helpers/string.h"
#include "runtime/kernel/kernel.h"

namespace OCLRT {

template <typename GfxFamily>
bool GTPinHwHelperHw<GfxFamily>::addSurfaceState(Kernel *pKernel) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;

    size_t sshSize = pKernel->getSurfaceStateHeapSize();
    if ((sshSize == 0) || pKernel->isParentKernel) {
        // Kernels which do not use SSH or use Execution Model are not supported (yet)
        return false;
    }
    size_t ssSize = sizeof(RENDER_SURFACE_STATE);
    size_t btsSize = sizeof(BINDING_TABLE_STATE);
    size_t sizeToEnlarge = ssSize + btsSize;
    size_t currBTOffset = pKernel->getBindingTableOffset();
    size_t currSurfaceStateSize = currBTOffset;
    char *pSsh = reinterpret_cast<char *>(pKernel->getSurfaceStateHeap());
    char *pNewSsh = new char[sshSize + sizeToEnlarge];
    memcpy_s(pNewSsh, sshSize + sizeToEnlarge, pSsh, currSurfaceStateSize);
    RENDER_SURFACE_STATE *pSS = reinterpret_cast<RENDER_SURFACE_STATE *>(pNewSsh + currSurfaceStateSize);
    pSS->init();
    size_t newSurfaceStateSize = currSurfaceStateSize + ssSize;
    size_t currBTCount = pKernel->getNumberOfBindingTableStates();
    memcpy_s(pNewSsh + newSurfaceStateSize, sshSize + sizeToEnlarge - newSurfaceStateSize, pSsh + currBTOffset, currBTCount * btsSize);
    BINDING_TABLE_STATE *pNewBTS = reinterpret_cast<BINDING_TABLE_STATE *>(pNewSsh + newSurfaceStateSize + currBTCount * btsSize);
    BINDING_TABLE_STATE bti;
    bti.init();
    bti.setSurfaceStatePointer((uint64_t)currBTOffset);
    *pNewBTS = bti;
    pKernel->resizeSurfaceStateHeap(pNewSsh, sshSize + sizeToEnlarge, currBTCount + 1, newSurfaceStateSize);
    return true;
}

template <typename GfxFamily>
void *GTPinHwHelperHw<GfxFamily>::getSurfaceState(Kernel *pKernel, size_t bti) {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;

    if ((nullptr == pKernel->getSurfaceStateHeap()) || (bti >= pKernel->getNumberOfBindingTableStates())) {
        return nullptr;
    }
    auto *pBts = reinterpret_cast<BINDING_TABLE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(), (pKernel->getBindingTableOffset() + bti * sizeof(BINDING_TABLE_STATE))));
    auto pSurfaceState = ptrOffset(pKernel->getSurfaceStateHeap(), pBts->getSurfaceStatePointer());
    return pSurfaceState;
}

} // namespace OCLRT
