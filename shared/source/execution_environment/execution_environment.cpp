/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_environment.h"

#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"

namespace NEO {
ExecutionEnvironment::ExecutionEnvironment() = default;

ExecutionEnvironment::~ExecutionEnvironment() {
    if (memoryManager) {
        memoryManager->commonCleanup();
    }
    rootDeviceEnvironments.clear();
}

void ExecutionEnvironment::initializeMemoryManager() {
    if (this->memoryManager) {
        return;
    }

    int32_t setCommandStreamReceiverType = CommandStreamReceiverType::CSR_HW;
    if (DebugManager.flags.SetCommandStreamReceiver.get() >= 0) {
        setCommandStreamReceiverType = DebugManager.flags.SetCommandStreamReceiver.get();
    }

    switch (setCommandStreamReceiverType) {
    case CommandStreamReceiverType::CSR_TBX:
    case CommandStreamReceiverType::CSR_TBX_WITH_AUB:
    case CommandStreamReceiverType::CSR_AUB:
        memoryManager = std::make_unique<OsAgnosticMemoryManager>(*this);
        break;
    case CommandStreamReceiverType::CSR_HW:
    case CommandStreamReceiverType::CSR_HW_WITH_AUB:
    default:
        memoryManager = MemoryManager::createMemoryManager(*this);
        break;
    }
    DEBUG_BREAK_IF(!this->memoryManager);
}

void ExecutionEnvironment::calculateMaxOsContextCount() {
    for (const auto &rootDeviceEnvironment : this->rootDeviceEnvironments) {
        auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
        auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
        auto osContextCount = hwHelper.getGpgpuEngineInstances(*hwInfo).size();
        auto subDevicesCount = HwHelper::getSubDevicesCount(hwInfo);
        bool hasRootCsr = subDevicesCount > 1;

        MemoryManager::maxOsContextCount += static_cast<uint32_t>(osContextCount * subDevicesCount + hasRootCsr);
    }
}

void ExecutionEnvironment::prepareRootDeviceEnvironments(uint32_t numRootDevices) {
    if (rootDeviceEnvironments.size() < numRootDevices) {
        rootDeviceEnvironments.resize(numRootDevices);
    }
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        if (!rootDeviceEnvironments[rootDeviceIndex]) {
            rootDeviceEnvironments[rootDeviceIndex] = std::make_unique<RootDeviceEnvironment>(*this);
        }
    }
}
} // namespace NEO
