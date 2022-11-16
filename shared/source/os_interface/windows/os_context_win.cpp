/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_context_win.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

namespace NEO {

OsContext *OsContextWin::create(OSInterface *osInterface, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor) {
    if (osInterface) {
        return new OsContextWin(*osInterface->getDriverModel()->as<Wddm>(), rootDeviceIndex, contextId, engineDescriptor);
    }
    return new OsContext(rootDeviceIndex, contextId, engineDescriptor);
}

OsContextWin::OsContextWin(Wddm &wddm, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor)
    : OsContext(rootDeviceIndex, contextId, engineDescriptor),
      residencyController(wddm, contextId),
      wddm(wddm) {
}

void OsContextWin::initializeContext() {

    if (wddm.getRootDeviceEnvironment().executionEnvironment.isDebuggingEnabled()) {
        debuggableContext = wddm.getRootDeviceEnvironment().osInterface->isDebugAttachAvailable() && !isInternalEngine();
    }
    auto wddmInterface = wddm.getWddmInterface();
    UNRECOVERABLE_IF(!wddm.createContext(*this));

    if (wddmInterface->hwQueuesSupported()) {
        UNRECOVERABLE_IF(!wddmInterface->createHwQueue(*this));
    }
    UNRECOVERABLE_IF(!wddmInterface->createMonitoredFence(*this));

    residencyController.registerCallback();
    UNRECOVERABLE_IF(!residencyController.isInitialized());
};

void OsContextWin::reInitializeContext() {
    if (contextInitialized && (false == this->wddm.skipResourceCleanup())) {
        wddm.destroyContext(wddmContextHandle);
    }
    UNRECOVERABLE_IF(!wddm.createContext(*this));
};

void OsContextWin::getDeviceLuidArray(std::vector<uint8_t> &luidData, size_t arraySize) {
    auto *wddm = this->getWddm();
    auto *hwDeviceID = wddm->getHwDeviceId();
    auto luid = hwDeviceID->getAdapterLuid();
    luidData.reserve(arraySize);
    for (size_t i = 0; i < arraySize; i++) {
        char *luidArray = nullptr;
        if (i < 4) {
            luidArray = (char *)&luid.LowPart;
            luidData.emplace(luidData.end(), luidArray[i]);
        } else {
            luidArray = (char *)&luid.HighPart;
            luidData.emplace(luidData.end(), luidArray[i - 4]);
        }
    }
};

OsContextWin::~OsContextWin() {
    if (contextInitialized && (false == this->wddm.skipResourceCleanup())) {
        wddm.getWddmInterface()->destroyHwQueue(hardwareQueue.handle);
        wddm.getWddmInterface()->destroyMonitorFence(residencyController.getMonitoredFence());
        wddm.destroyContext(wddmContextHandle);
    }
}

} // namespace NEO
