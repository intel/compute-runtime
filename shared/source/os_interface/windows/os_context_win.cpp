/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_context_win.h"

#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

namespace NEO {

OsContext *OsContext::create(OSInterface *osInterface, uint32_t contextId, DeviceBitfield deviceBitfield,
                             aub_stream::EngineType engineType, PreemptionMode preemptionMode,
                             bool lowPriority, bool internalEngine, bool rootDevice) {
    if (osInterface) {
        return new OsContextWin(*osInterface->get()->getWddm(), contextId, deviceBitfield, engineType, preemptionMode,
                                lowPriority, internalEngine, rootDevice);
    }
    return new OsContext(contextId, deviceBitfield, engineType, preemptionMode, lowPriority, internalEngine, rootDevice);
}

OsContextWin::OsContextWin(Wddm &wddm, uint32_t contextId, DeviceBitfield deviceBitfield,
                           aub_stream::EngineType engineType, PreemptionMode preemptionMode,
                           bool lowPriority, bool internalEngine, bool rootDevice)
    : OsContext(contextId, deviceBitfield, engineType, preemptionMode, lowPriority, internalEngine, rootDevice),
      wddm(wddm),
      residencyController(wddm, contextId) {

    auto wddmInterface = wddm.getWddmInterface();
    if (!wddm.createContext(*this)) {
        return;
    }
    if (wddmInterface->hwQueuesSupported()) {
        if (!wddmInterface->createHwQueue(*this)) {
            return;
        }
    }
    initialized = wddmInterface->createMonitoredFence(*this);
    residencyController.registerCallback();
};

bool OsContextWin::isInitialized() const {
    return (initialized && residencyController.isInitialized());
}

OsContextWin::~OsContextWin() {
    wddm.getWddmInterface()->destroyHwQueue(hardwareQueue.handle);
    wddm.getWddmInterface()->destroyMonitorFence(residencyController.getMonitoredFence());
    wddm.destroyContext(wddmContextHandle);
}

} // namespace NEO
