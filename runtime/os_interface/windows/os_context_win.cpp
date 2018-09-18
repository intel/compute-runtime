/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm/wddm_interface.h"
#include "runtime/os_interface/windows/os_interface.h"

namespace OCLRT {

OsContextWin::OsContextImpl(Wddm &wddm) : wddm(wddm) {
    UNRECOVERABLE_IF(!wddm.isInitialized());
    auto wddmInterface = wddm.getWddmInterface();
    if (!wddm.createContext(context)) {
        return;
    }
    if (wddmInterface->hwQueuesSupported()) {
        if (!wddmInterface->createHwQueue(wddm.getPreemptionMode(), *this)) {
            return;
        }
    }
    initialized = wddmInterface->createMonitoredFence(*this);
};
OsContextWin::~OsContextImpl() {
    wddm.getWddmInterface()->destroyHwQueue(hwQueueHandle);
    wddm.destroyContext(context);
}

void OsContextWin::resetMonitoredFenceParams(D3DKMT_HANDLE &handle, uint64_t *cpuAddress, D3DGPU_VIRTUAL_ADDRESS &gpuAddress) {
    monitoredFence.lastSubmittedFence = 0;
    monitoredFence.currentFenceValue = 1;
    monitoredFence.fenceHandle = handle;
    monitoredFence.cpuAddress = cpuAddress;
    monitoredFence.gpuAddress = gpuAddress;
}

OsContext::OsContext(OSInterface *osInterface, uint32_t contextId) : contextId(contextId) {
    if (osInterface) {
        osContextImpl = std::make_unique<OsContextWin>(*osInterface->get()->getWddm());
    }
}
OsContext::~OsContext() = default;

} // namespace OCLRT
