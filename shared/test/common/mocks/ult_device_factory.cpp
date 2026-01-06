/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/ult_device_factory.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/tests_configuration.h"

using namespace NEO;

UltDeviceFactory::UltDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount)
    : UltDeviceFactory(rootDevicesCount, subDevicesCount, *(new ExecutionEnvironment)) {
}

UltDeviceFactory::UltDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount, ExecutionEnvironment &executionEnvironment) {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> createSingleDeviceBackup{&MockDevice::createSingleDevice, false};
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createRootDeviceFuncBackup{&DeviceFactory::createRootDeviceFunc};
    VariableBackup<decltype(DeviceFactory::createMemoryManagerFunc)> createMemoryManagerFuncBackup{&DeviceFactory::createMemoryManagerFunc};

    debugManager.flags.CreateMultipleRootDevices.set(rootDevicesCount);
    debugManager.flags.CreateMultipleSubDevices.set(subDevicesCount);
    createRootDeviceFuncBackup = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
            UnitTestSetter::setRcsExposure(*executionEnvironment.rootDeviceEnvironments[i]);
            UnitTestSetter::setCcsExposure(*executionEnvironment.rootDeviceEnvironments[i]);
        }
        executionEnvironment.calculateMaxOsContextCount();
        return std::unique_ptr<Device>(MockDevice::create<MockDevice>(&executionEnvironment, rootDeviceIndex));
    };
    createMemoryManagerFuncBackup = UltDeviceFactory::initializeMemoryManager;

    auto createdDevices = DeviceFactory::createDevices(executionEnvironment);

    for (auto &pCreatedDevice : createdDevices) {
        pCreatedDevice->incRefInternal();
        if (pCreatedDevice->getNumSubDevices() > 1) {
            for (uint32_t i = 0; i < pCreatedDevice->getNumSubDevices(); i++) {
                auto *pDevice = static_cast<SubDevice *>(pCreatedDevice->getSubDevice(i));
                this->subDevices.push_back(pDevice);
            }
        }
        if (pCreatedDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread) {
            for (auto &engine : pCreatedDevice->getAllEngines()) {
                NEO::CommandStreamReceiver *csr = engine.commandStreamReceiver;
                if (!csr->getPreemptionAllocation()) {
                    csr->createPreemptionAllocation();
                }
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

bool UltDeviceFactory::prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment, uint32_t rootDevicesCount) {
    uint32_t numRootDevices = rootDevicesCount;
    executionEnvironment.prepareRootDeviceEnvironments(numRootDevices);
    for (auto i = 0u; i < numRootDevices; i++) {
        if (executionEnvironment.rootDeviceEnvironments[i]->getHardwareInfo() == nullptr ||
            (executionEnvironment.rootDeviceEnvironments[i]->getHardwareInfo()->platform.eProductFamily == IGFX_UNKNOWN &&
             executionEnvironment.rootDeviceEnvironments[i]->getHardwareInfo()->platform.eRenderCoreFamily == IGFX_UNKNOWN_CORE)) {
            executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        }
        executionEnvironment.rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
        if (debugManager.flags.ExposeSingleDevice.get() != -1) {
            executionEnvironment.rootDeviceEnvironments[i]->setExposeSingleDeviceMode(!!debugManager.flags.ExposeSingleDevice.get());
        }
    }
    executionEnvironment.parseAffinityMask();
    auto retVal = executionEnvironment.rootDeviceEnvironments.size();
    if (retVal) {
        executionEnvironment.calculateMaxOsContextCount();
        DeviceFactory::createMemoryManagerFunc(executionEnvironment);
    }
    return retVal;
}
bool UltDeviceFactory::initializeMemoryManager(ExecutionEnvironment &executionEnvironment) {
    if (executionEnvironment.memoryManager == nullptr) {
        auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        bool enableLocalMemory = gfxCoreHelper.getEnableLocalMemory(*defaultHwInfo);
        bool aubUsage = (testMode == TestMode::aubTests) || (testMode == TestMode::aubTestsWithTbx);
        executionEnvironment.memoryManager.reset(new MockMemoryManager(false, enableLocalMemory, aubUsage, executionEnvironment));
        executionEnvironment.memoryManager->initUsmReuseLimits();
    }
    return true;
}
