/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "core/execution_environment/execution_environment.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/helpers/hw_info.h"
#include "core/os_interface/device_factory.h"
#include "core/os_interface/hw_info_config.h"
#include "core/os_interface/linux/drm_memory_operations_handler.h"
#include "core/os_interface/linux/drm_neo.h"
#include "core/os_interface/linux/os_interface.h"

#include "drm/i915_drm.h"

namespace NEO {
size_t DeviceFactory::numDevices = 0;

bool DeviceFactory::getDevices(size_t &totalNumRootDevices, ExecutionEnvironment &executionEnvironment) {

    HwDeviceIds hwDeviceIds;

    size_t numRootDevices = 1u;
    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }
    for (size_t i = 0; i < numRootDevices; i++) {
        auto hwDeviceId = Drm::discoverDevices();
        hwDeviceIds.push_back(std::move(hwDeviceId));
    }
    totalNumRootDevices = numRootDevices;

    executionEnvironment.prepareRootDeviceEnvironments(static_cast<uint32_t>(numRootDevices));

    uint32_t rootDeviceIndex = 0u;

    for (auto &hwDeviceId : hwDeviceIds) {
        Drm *drm = Drm::create(std::move(hwDeviceId), *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
        if (!drm) {
            return false;
        }

        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandler>();
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(new OSInterface());
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->get()->setDrm(drm);
        rootDeviceIndex++;
    }

    auto hardwareInfo = executionEnvironment.getMutableHardwareInfo();
    HwInfoConfig *hwConfig = HwInfoConfig::get(hardwareInfo->platform.eProductFamily);
    if (hwConfig->configureHwInfo(hardwareInfo, hardwareInfo, executionEnvironment.rootDeviceEnvironments[0]->osInterface.get())) {
        return false;
    }
    executionEnvironment.calculateMaxOsContextCount();
    DeviceFactory::numDevices = numRootDevices;

    return true;
}

void DeviceFactory::releaseDevices() {
}
} // namespace NEO
