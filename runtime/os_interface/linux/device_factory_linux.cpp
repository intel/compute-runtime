/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/helpers/hw_info.h"
#include "runtime/device/device.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/os_interface/linux/drm_memory_operations_handler.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/os_interface.h"

#include "drm/i915_drm.h"

namespace NEO {
size_t DeviceFactory::numDevices = 0;

bool DeviceFactory::getDevices(size_t &numDevices, ExecutionEnvironment &executionEnvironment) {
    unsigned int devNum = 0;
    size_t numRootDevices = 1;

    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }

    executionEnvironment.prepareRootDeviceEnvironments(static_cast<uint32_t>(numRootDevices));

    for (auto rootDeviceIndex = 0u; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        Drm *drm = Drm::create(rootDeviceIndex);
        if (!drm) {
            return false;
        }

        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandler>();
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(new OSInterface());
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->get()->setDrm(drm);
    }

    auto hardwareInfo = executionEnvironment.getMutableHardwareInfo();
    const HardwareInfo *pCurrDevice = platformDevices[devNum];
    HwInfoConfig *hwConfig = HwInfoConfig::get(pCurrDevice->platform.eProductFamily);
    if (hwConfig->configureHwInfo(pCurrDevice, hardwareInfo, executionEnvironment.rootDeviceEnvironments[0]->osInterface.get())) {
        return false;
    }
    executionEnvironment.calculateMaxOsContextCount();

    numDevices = numRootDevices;
    DeviceFactory::numDevices = numDevices;

    return true;
}

void DeviceFactory::releaseDevices() {
    if (DeviceFactory::numDevices > 0) {
        for (unsigned int i = 0; i < DeviceFactory::numDevices; ++i) {
            Drm::closeDevice(i);
        }
    }
    DeviceFactory::numDevices = 0;
}

void ClDevice::initializeCaps() {
}
} // namespace NEO
