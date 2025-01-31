/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_context_win.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

namespace NEO {

OsContext *OsContextWin::create(OSInterface *osInterface, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor) {
    if (osInterface && osInterface->getDriverModel()->getDriverModelType() == DriverModelType::wddm) {
        return new OsContextWin(*osInterface->getDriverModel()->as<Wddm>(), rootDeviceIndex, contextId, engineDescriptor);
    }
    return new OsContext(rootDeviceIndex, contextId, engineDescriptor);
}

OsContextWin::OsContextWin(Wddm &wddm, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor)
    : OsContext(rootDeviceIndex, contextId, engineDescriptor),
      residencyController(wddm, contextId),
      wddm(wddm) {
}

bool OsContextWin::initializeContext(bool allocateInterrupt) {

    NEO::EnvironmentVariableReader envReader;
    bool disableContextCreationFlag = envReader.getSetting("NEO_L0_SYSMAN_NO_CONTEXT_MODE", false);
    if (!disableContextCreationFlag) {
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
    }

    return true;
};

void OsContextWin::reInitializeContext() {
    NEO::EnvironmentVariableReader envReader;
    bool disableContextCreationFlag = envReader.getSetting("NEO_L0_SYSMAN_NO_CONTEXT_MODE", false);
    if (!disableContextCreationFlag) {
        if (contextInitialized && (false == this->wddm.skipResourceCleanup())) {
            wddm.getWddmInterface()->destroyHwQueue(hardwareQueue.handle);
            wddm.destroyContext(wddmContextHandle);
        }
        UNRECOVERABLE_IF(!wddm.createContext(*this));
        auto wddmInterface = wddm.getWddmInterface();
        if (wddmInterface->hwQueuesSupported()) {
            UNRECOVERABLE_IF(!wddmInterface->createHwQueue(*this));
            UNRECOVERABLE_IF(!wddmInterface->createMonitoredFence(*this));
        }
    }
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

uint32_t OsContextWin::getDeviceNodeMask() {
    auto *wddm = this->getWddm();
    auto *hwDeviceID = wddm->getHwDeviceId();
    return hwDeviceID->getAdapterNodeMask();
}

uint64_t OsContextWin::getOfflineDumpContextId(uint32_t deviceIndex) const {
    return 0;
}

bool OsContextWin::isDirectSubmissionSupported() const {
    auto &rootDeviceEnvironment = wddm.getRootDeviceEnvironment();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto isWSL = rootDeviceEnvironment.isWddmOnLinux();

    return !isWSL && productHelper.isDirectSubmissionSupported(rootDeviceEnvironment.getReleaseHelper());
}

OsContextWin::~OsContextWin() {
    if (contextInitialized && (false == this->wddm.skipResourceCleanup())) {
        wddm.getWddmInterface()->destroyHwQueue(hardwareQueue.handle);
        if (residencyController.getMonitoredFence().fenceHandle != hardwareQueue.progressFenceHandle) {
            wddm.getWddmInterface()->destroyMonitorFence(residencyController.getMonitoredFence().fenceHandle);
        }
        wddm.destroyContext(wddmContextHandle);
    }
}

} // namespace NEO
