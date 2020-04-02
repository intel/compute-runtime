/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/ult_device_factory.h"

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/variable_backup.h"
#include "shared/test/unit_test/mocks/mock_device.h"

using namespace NEO;

UltDeviceFactory::UltDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount)
    : UltDeviceFactory(rootDevicesCount, subDevicesCount, *(new ExecutionEnvironment)) {
}

UltDeviceFactory::UltDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount, ExecutionEnvironment &executionEnvironment) {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> createSingleDeviceBackup{&MockDevice::createSingleDevice, false};
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createRootDeviceFuncBackup{&DeviceFactory::createRootDeviceFunc};

    DebugManager.flags.CreateMultipleRootDevices.set(rootDevicesCount);
    DebugManager.flags.CreateMultipleSubDevices.set(subDevicesCount);
    createRootDeviceFuncBackup = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        return std::unique_ptr<Device>(MockDevice::create<MockDevice>(&executionEnvironment, rootDeviceIndex));
    };

    auto createdDevices = DeviceFactory::createDevices(executionEnvironment);

    for (auto &pCreatedDevice : createdDevices) {
        pCreatedDevice->incRefInternal();
        for (uint32_t i = 0; i < pCreatedDevice->getNumAvailableDevices(); i++) {
            this->subDevices.push_back(static_cast<SubDevice *>(pCreatedDevice->getDeviceById(i)));
        }
        this->rootDevices.push_back(static_cast<MockDevice *>(pCreatedDevice.release()));
    }
}

UltDeviceFactory::~UltDeviceFactory() {
    for (auto &pDevice : rootDevices) {
        pDevice->decRefInternal();
    }
}
