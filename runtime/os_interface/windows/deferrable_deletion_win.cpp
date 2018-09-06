/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/deferrable_deletion_win.h"

namespace OCLRT {

template <typename... Args>
DeferrableDeletion *DeferrableDeletion::create(Args... args) {
    return new DeferrableDeletionImpl(std::forward<Args>(args)...);
}
template DeferrableDeletion *DeferrableDeletion::create(Wddm *wddm, D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle);

DeferrableDeletionImpl::DeferrableDeletionImpl(Wddm *wddm, D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle) {
    this->wddm = wddm;
    if (handles) {
        this->handles = new D3DKMT_HANDLE[allocationCount];
        for (uint32_t i = 0; i < allocationCount; i++) {
            this->handles[i] = handles[i];
        }
    }
    this->allocationCount = allocationCount;
    this->resourceHandle = resourceHandle;
}
void DeferrableDeletionImpl::apply() {
    bool destroyStatus = wddm->destroyAllocations(handles, allocationCount, resourceHandle);
    DEBUG_BREAK_IF(!destroyStatus);
}
DeferrableDeletionImpl::~DeferrableDeletionImpl() {
    if (handles) {
        delete[] handles;
    }
}
} // namespace OCLRT
