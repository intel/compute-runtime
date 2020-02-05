/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef _WIN32

#include "core/debug_settings/debug_settings_manager.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/os_interface/device_factory.h"
#include "core/os_interface/windows/os_interface.h"
#include "core/os_interface/windows/wddm/wddm.h"
#include "core/os_interface/windows/wddm_memory_operations_handler.h"
#include "runtime/device/device.h"

namespace NEO {

extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];

size_t DeviceFactory::numDevices = 0;

bool DeviceFactory::getDevices(size_t &numDevices, ExecutionEnvironment &executionEnvironment) {
    numDevices = 0;

    auto numRootDevices = 1u;
    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }

    executionEnvironment.prepareRootDeviceEnvironments(static_cast<uint32_t>(numRootDevices));

    auto hardwareInfo = executionEnvironment.getMutableHardwareInfo();
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        std::unique_ptr<Wddm> wddm(Wddm::createWddm(*executionEnvironment.rootDeviceEnvironments[rootDeviceIndex].get()));
        if (!wddm->init(*hardwareInfo)) {
            return false;
        }
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm.get());
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->get()->setWddm(wddm.release());
    }
    executionEnvironment.calculateMaxOsContextCount();

    numDevices = numRootDevices;
    DeviceFactory::numDevices = numDevices;

    return true;
}

void DeviceFactory::releaseDevices() {
    DeviceFactory::numDevices = 0;
}

} // namespace NEO

#endif
