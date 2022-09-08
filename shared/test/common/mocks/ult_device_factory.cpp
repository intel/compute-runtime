/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/ult_device_factory.h"

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

using namespace NEO;

UltDeviceFactory::UltDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount)
    : UltDeviceFactory(rootDevicesCount, subDevicesCount, *(new ExecutionEnvironment)) {
}

UltDeviceFactory::UltDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount, ExecutionEnvironment &executionEnvironment) {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> createSingleDeviceBackup{&MockDevice::createSingleDevice, false};
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createRootDeviceFuncBackup{&DeviceFactory::createRootDeviceFunc};
    VariableBackup<decltype(DeviceFactory::createMemoryManagerFunc)> createMemoryManagerFuncBackup{&DeviceFactory::createMemoryManagerFunc};

    DebugManager.flags.CreateMultipleRootDevices.set(rootDevicesCount);
    DebugManager.flags.CreateMultipleSubDevices.set(subDevicesCount);
    createRootDeviceFuncBackup = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        return std::unique_ptr<Device>(MockDevice::create<MockDevice>(&executionEnvironment, rootDeviceIndex));
    };
    createMemoryManagerFuncBackup = UltDeviceFactory::initializeMemoryManager;

    auto createdDevices = DeviceFactory::createDevices(executionEnvironment);

    for (auto &pCreatedDevice : createdDevices) {
        pCreatedDevice->incRefInternal();
        if (pCreatedDevice->getNumSubDevices() > 1) {
            for (uint32_t i = 0; i < pCreatedDevice->getNumSubDevices(); i++) {
                this->subDevices.push_back(static_cast<SubDevice *>(pCreatedDevice->getSubDevice(i)));
            }
        }
        this->rootDevices.push_back(static_cast<MockDevice *>(pCreatedDevice.release()));
    }
}

UltDeviceFactory::~UltDeviceFactory() {
    for (auto &pDevice : rootDevices) {
        pDevice->decRefInternal();
    }
}

void UltDeviceFactory::prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment, uint32_t rootDevicesCount) {
    uint32_t numRootDevices = rootDevicesCount;
    executionEnvironment.prepareRootDeviceEnvironments(numRootDevices);
    for (auto i = 0u; i < numRootDevices; i++) {
        if (executionEnvironment.rootDeviceEnvironments[i]->getHardwareInfo() == nullptr ||
            (executionEnvironment.rootDeviceEnvironments[i]->getHardwareInfo()->platform.eProductFamily == IGFX_UNKNOWN &&
             executionEnvironment.rootDeviceEnvironments[i]->getHardwareInfo()->platform.eRenderCoreFamily == IGFX_UNKNOWN_CORE)) {
            executionEnvironment.rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        }
    }
    executionEnvironment.parseAffinityMask();
    executionEnvironment.calculateMaxOsContextCount();
    DeviceFactory::createMemoryManagerFunc(executionEnvironment);
}
bool UltDeviceFactory::initializeMemoryManager(ExecutionEnvironment &executionEnvironment) {
    if (executionEnvironment.memoryManager == nullptr) {
        bool enableLocalMemory = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getEnableLocalMemory(*defaultHwInfo);
        bool aubUsage = (testMode == TestMode::AubTests) || (testMode == TestMode::AubTestsWithTbx);
        executionEnvironment.memoryManager.reset(new MockMemoryManager(false, enableLocalMemory, aubUsage, executionEnvironment));
    }
    return true;
}
