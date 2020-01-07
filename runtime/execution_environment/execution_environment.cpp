/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"

#include "core/compiler_interface/compiler_interface.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/gmm_helper/gmm_helper.h"
#include "core/helpers/hw_helper.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/compiler_interface/default_cl_cache_config.h"
#include "runtime/helpers/device_helpers.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

namespace NEO {
ExecutionEnvironment::ExecutionEnvironment() {
    hwInfo = std::make_unique<HardwareInfo>(*platformDevices[0]);
};

ExecutionEnvironment::~ExecutionEnvironment() {
    sourceLevelDebugger.reset();
    compilerInterface.reset();
    builtins.reset();
    rootDeviceEnvironments.clear();
}

void ExecutionEnvironment::initGmm() {
    if (!gmmHelper) {
        gmmHelper.reset(new GmmHelper(rootDeviceEnvironments[0]->osInterface.get(), hwInfo.get()));
    }
}

void ExecutionEnvironment::setHwInfo(const HardwareInfo *hwInfo) {
    *this->hwInfo = *hwInfo;
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
        memoryManager = std::make_unique<TbxMemoryManager>(*this);
        break;
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

void ExecutionEnvironment::initSourceLevelDebugger() {
    if (hwInfo->capabilityTable.sourceLevelDebuggerSupported) {
        sourceLevelDebugger.reset(SourceLevelDebugger::create());
    }
    if (sourceLevelDebugger) {
        bool localMemorySipAvailable = (SipKernelType::DbgCsrLocal == SipKernel::getSipKernelType(hwInfo->platform.eRenderCoreFamily, true));
        sourceLevelDebugger->initialize(localMemorySipAvailable);
    }
}

void ExecutionEnvironment::calculateMaxOsContextCount() {
    auto &hwHelper = HwHelper::get(this->hwInfo->platform.eRenderCoreFamily);
    auto osContextCount = hwHelper.getGpgpuEngineInstances().size();
    auto subDevicesCount = DeviceHelper::getSubDevicesCount(this->getHardwareInfo());
    bool hasRootCsr = subDevicesCount > 1;

    MemoryManager::maxOsContextCount = static_cast<uint32_t>(osContextCount * subDevicesCount * this->rootDeviceEnvironments.size() + hasRootCsr);
}

GmmHelper *ExecutionEnvironment::getGmmHelper() const {
    return gmmHelper.get();
}
GmmClientContext *ExecutionEnvironment::getGmmClientContext() const {
    return gmmHelper->getClientContext();
}
CompilerInterface *ExecutionEnvironment::getCompilerInterface() {
    if (this->compilerInterface.get() == nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        if (this->compilerInterface.get() == nullptr) {
            auto cache = std::make_unique<CompilerCache>(getDefaultClCompilerCacheConfig());
            this->compilerInterface.reset(CompilerInterface::createInstance(std::move(cache), true));
        }
    }
    return this->compilerInterface.get();
}

BuiltIns *ExecutionEnvironment::getBuiltIns() {
    if (this->builtins.get() == nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        if (this->builtins.get() == nullptr) {
            this->builtins = std::make_unique<BuiltIns>();
        }
    }
    return this->builtins.get();
}

bool ExecutionEnvironment::isFullRangeSvm() const {
    return hwInfo->capabilityTable.gpuAddressSpace >= maxNBitValue(47);
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
