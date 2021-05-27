/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_context_win.h"

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

namespace NEO {

OsContext *OsContextWin::create(OSInterface *osInterface, uint32_t contextId, DeviceBitfield deviceBitfield,
                                EngineTypeUsage typeUsage, PreemptionMode preemptionMode, bool rootDevice) {
    if (osInterface) {
        return new OsContextWin(*osInterface->getDriverModel()->as<Wddm>(), contextId, deviceBitfield, typeUsage, preemptionMode, rootDevice);
    }
    return new OsContext(contextId, deviceBitfield, typeUsage, preemptionMode, rootDevice);
}

OsContextWin::OsContextWin(Wddm &wddm, uint32_t contextId, DeviceBitfield deviceBitfield,
                           EngineTypeUsage typeUsage, PreemptionMode preemptionMode, bool rootDevice)
    : OsContext(contextId, deviceBitfield, typeUsage, preemptionMode, rootDevice),
      wddm(wddm),
      residencyController(wddm, contextId) {}

void OsContextWin::initializeContext() {
    auto wddmInterface = wddm.getWddmInterface();
    UNRECOVERABLE_IF(!wddm.createContext(*this));

    if (wddmInterface->hwQueuesSupported()) {
        UNRECOVERABLE_IF(!wddmInterface->createHwQueue(*this));
    }
    UNRECOVERABLE_IF(!wddmInterface->createMonitoredFence(*this));

    residencyController.registerCallback();
    UNRECOVERABLE_IF(!residencyController.isInitialized());
};

OsContextWin::~OsContextWin() {
    if (contextInitialized) {
        wddm.getWddmInterface()->destroyHwQueue(hardwareQueue.handle);
        wddm.getWddmInterface()->destroyMonitorFence(residencyController.getMonitoredFence());
        wddm.destroyContext(wddmContextHandle);
    }
}

} // namespace NEO
