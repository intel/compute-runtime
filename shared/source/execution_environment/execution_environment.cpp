/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/affinity_mask.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/os_environment.h"
#include "shared/source/utilities/wait_util.h"

namespace NEO {
ExecutionEnvironment::ExecutionEnvironment() {
    WaitUtils::init();
}

ExecutionEnvironment::~ExecutionEnvironment() {
    if (memoryManager) {
        memoryManager->commonCleanup();
        for (const auto &rootDeviceEnvironment : this->rootDeviceEnvironments) {
            SipKernel::freeSipKernels(rootDeviceEnvironment.get(), memoryManager.get());
            if (rootDeviceEnvironment->builtins.get()) {
                rootDeviceEnvironment->builtins.get()->freeSipKernels(memoryManager.get());
            }
        }
    }
    rootDeviceEnvironments.clear();
}

bool ExecutionEnvironment::initializeMemoryManager() {
    if (this->memoryManager) {
        return memoryManager->isInitialized();
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

    return memoryManager->isInitialized();
}

void ExecutionEnvironment::calculateMaxOsContextCount() {
    MemoryManager::maxOsContextCount = 0u;
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
void ExecutionEnvironment::parseAffinityMask() {
    auto affinityMaskString = DebugManager.flags.ZE_AFFINITY_MASK.get();

    if (affinityMaskString.compare("default") == 0 ||
        affinityMaskString.empty()) {
        return;
    }

    const uint32_t numRootDevices = static_cast<uint32_t>(rootDeviceEnvironments.size());

    std::vector<AffinityMaskHelper> affinityMaskHelper(numRootDevices);

    size_t pos = 0;
    while (pos < affinityMaskString.size()) {
        size_t posNextDot = affinityMaskString.find_first_of(".", pos);
        size_t posNextComma = affinityMaskString.find_first_of(",", pos);
        std::string rootDeviceString = affinityMaskString.substr(pos, std::min(posNextDot, posNextComma) - pos);
        uint32_t rootDeviceIndex = static_cast<uint32_t>(std::stoul(rootDeviceString, nullptr, 0));
        if (rootDeviceIndex < numRootDevices) {
            auto hwInfo = rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
            auto subDevicesCount = HwHelper::getSubDevicesCount(hwInfo);

            pos += rootDeviceString.size();
            if (posNextDot != std::string::npos &&
                affinityMaskString.at(pos) == '.' && posNextDot < posNextComma) {
                pos++;
                std::string subDeviceString = affinityMaskString.substr(pos, posNextComma - pos);
                uint32_t subDeviceIndex = static_cast<uint32_t>(std::stoul(subDeviceString, nullptr, 0));
                if (subDeviceIndex < subDevicesCount) {
                    affinityMaskHelper[rootDeviceIndex].enableGenericSubDevice(subDeviceIndex);
                }
            } else {
                affinityMaskHelper[rootDeviceIndex].enableAllGenericSubDevices(subDevicesCount);
            }
        }
        if (posNextComma == std::string::npos) {
            break;
        }
        pos = posNextComma + 1;
    }

    std::vector<std::unique_ptr<RootDeviceEnvironment>> filteredEnvironments;
    for (uint32_t i = 0u; i < numRootDevices; i++) {
        if (!affinityMaskHelper[i].isDeviceEnabled()) {
            continue;
        }

        rootDeviceEnvironments[i]->deviceAffinityMask = affinityMaskHelper[i];
        filteredEnvironments.emplace_back(rootDeviceEnvironments[i].release());
    }

    rootDeviceEnvironments.swap(filteredEnvironments);
}
} // namespace NEO
