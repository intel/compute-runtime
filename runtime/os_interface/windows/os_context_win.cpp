/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm/wddm_interface.h"
#include "runtime/os_interface/windows/os_interface.h"

namespace OCLRT {

OsContextWin::OsContextImpl(Wddm &wddm, uint32_t osContextId, EngineInstanceT engineType, PreemptionMode preemptionMode) : wddm(wddm), residencyController(wddm, osContextId) {
    UNRECOVERABLE_IF(!wddm.isInitialized());
    auto wddmInterface = wddm.getWddmInterface();
    if (!wddm.createContext(context, engineType, preemptionMode)) {
        return;
    }
    if (wddmInterface->hwQueuesSupported()) {
        if (!wddmInterface->createHwQueue(preemptionMode, *this)) {
            return;
        }
    }
    initialized = wddmInterface->createMonitoredFence(this->residencyController);
    this->residencyController.registerCallback();
};
OsContextWin::~OsContextImpl() {
    wddm.getWddmInterface()->destroyHwQueue(hwQueueHandle);
    wddm.destroyContext(context);
}

OsContext::OsContext(OSInterface *osInterface, uint32_t contextId, uint32_t numDevicesSupported, EngineInstanceT engineType, PreemptionMode preemptionMode)
    : contextId(contextId), numDevicesSupported(numDevicesSupported), engineType(engineType) {
    if (osInterface) {
        osContextImpl = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), contextId, engineType, preemptionMode);
    }
}

OsContext::~OsContext() = default;

} // namespace OCLRT
