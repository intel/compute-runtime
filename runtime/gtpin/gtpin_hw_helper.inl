/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
