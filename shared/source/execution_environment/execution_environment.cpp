/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/execution_environment/root_device_environment.h"
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

    std::vector<std::vector<bool>> affinityMaskBitSet(rootDeviceEnvironments.size());
    for (uint32_t i = 0; i < affinityMaskBitSet.size(); i++) {
        auto hwInfo = rootDeviceEnvironments[i]->getHardwareInfo();
        affinityMaskBitSet[i].resize(HwHelper::getSubDevicesCount(hwInfo));
    }

    size_t pos = 0;
    while (pos < affinityMaskString.size()) {
        size_t posNextDot = affinityMaskString.find_first_of(".", pos);
        size_t posNextComma = affinityMaskString.find_first_of(",", pos);
        std::string rootDeviceString = affinityMaskString.substr(pos, std::min(posNextDot, posNextComma) - pos);
        uint32_t rootDeviceIndex = static_cast<uint32_t>(std::stoul(rootDeviceString, nullptr, 0));
        if (rootDeviceIndex < rootDeviceEnvironments.size()) {
            pos += rootDeviceString.size();
            if (posNextDot != std::string::npos &&
                affinityMaskString.at(pos) == '.' && posNextDot < posNextComma) {
                pos++;
                std::string subDeviceString = affinityMaskString.substr(pos, posNextComma - pos);
                uint32_t subDeviceIndex = static_cast<uint32_t>(std::stoul(subDeviceString, nullptr, 0));
                auto hwInfo = rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
                if (subDeviceIndex < HwHelper::getSubDevicesCount(hwInfo)) {
                    affinityMaskBitSet[rootDeviceIndex][subDeviceIndex] = true;
                }
            } else {
                std::fill(affinityMaskBitSet[rootDeviceIndex].begin(),
                          affinityMaskBitSet[rootDeviceIndex].end(),
                          true);
            }
        }
        if (posNextComma == std::string::npos) {
            break;
        }
        pos = posNextComma + 1;
    }

    uint32_t offset = 0;
    uint32_t affinityMask = 0;
    for (uint32_t i = 0; i < affinityMaskBitSet.size(); i++) {
        for (uint32_t j = 0; j < affinityMaskBitSet[i].size(); j++) {
            if (affinityMaskBitSet[i][j] == true) {
                affinityMask |= (1UL << offset);
            }
            offset++;
        }
    }

    uint32_t currentMaskOffset = 0;
    std::vector<std::unique_ptr<RootDeviceEnvironment>> filteredEnvironments;
    for (size_t i = 0u; i < this->rootDeviceEnvironments.size(); i++) {
        auto hwInfo = rootDeviceEnvironments[i]->getHardwareInfo();

        uint32_t currentDeviceMask = (affinityMask >> currentMaskOffset) & ((1UL << HwHelper::getSubDevicesCount(hwInfo)) - 1);
        bool isDeviceExposed = currentDeviceMask > 0;

        currentMaskOffset += HwHelper::getSubDevicesCount(hwInfo);
        if (!isDeviceExposed) {
            continue;
        }

        rootDeviceEnvironments[i]->deviceAffinityMask = currentDeviceMask;
        filteredEnvironments.emplace_back(rootDeviceEnvironments[i].release());
    }

    rootDeviceEnvironments.swap(filteredEnvironments);
}
} // namespace NEO
