/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef _WIN32

#include "core/debug_settings/debug_settings_manager.h"
#include "core/device/device.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/os_interface/device_factory.h"
#include "core/os_interface/windows/os_interface.h"
#include "core/os_interface/windows/wddm/wddm.h"
#include "core/os_interface/windows/wddm_memory_operations_handler.h"

namespace NEO {

extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];

bool DeviceFactory::getDevices(size_t &totalNumRootDevices, ExecutionEnvironment &executionEnvironment) {
    HwDeviceIds hwDeviceIds;
    totalNumRootDevices = 0;

    size_t numRootDevices = 1u;
    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }
    for (size_t i = 0; i < numRootDevices; i++) {
        auto hwDeviceId = Wddm::discoverDevices();
        if (hwDeviceId) {
            hwDeviceIds.push_back(std::move(hwDeviceId));
        }
    }
    if (hwDeviceIds.empty()) {
        return false;
    }
    totalNumRootDevices = numRootDevices;

    executionEnvironment.prepareRootDeviceEnvironments(static_cast<uint32_t>(totalNumRootDevices));

    uint32_t rootDeviceIndex = 0u;

    for (auto &hwDeviceId : hwDeviceIds) {
        std::unique_ptr<Wddm> wddm(Wddm::createWddm(std::move(hwDeviceId), *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex].get()));
        if (!wddm->init()) {
            return false;
        }
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm.get());
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->get()->setWddm(wddm.release());
        rootDeviceIndex++;
    }

    executionEnvironment.calculateMaxOsContextCount();
    return true;
}

void DeviceFactory::releaseDevices() {
}
} // namespace NEO

#endif
