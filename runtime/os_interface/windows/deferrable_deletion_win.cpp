/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/deferrable_deletion_win.h"

#include "runtime/os_interface/windows/wddm/wddm.h"

namespace OCLRT {

template <typename... Args>
DeferrableDeletion *DeferrableDeletion::create(Args... args) {
    return new DeferrableDeletionImpl(std::forward<Args>(args)...);
}
template DeferrableDeletion *DeferrableDeletion::create(Wddm *wddm, const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle);

DeferrableDeletionImpl::DeferrableDeletionImpl(Wddm *wddm, const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle) {
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
bool DeferrableDeletionImpl::apply() {
    bool destroyStatus = wddm->destroyAllocations(handles, allocationCount, resourceHandle);
    DEBUG_BREAK_IF(!destroyStatus);
    return true;
}
DeferrableDeletionImpl::~DeferrableDeletionImpl() {
    if (handles) {
        delete[] handles;
    }
}
} // namespace OCLRT
