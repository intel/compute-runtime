/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"

#include "core/memory_manager/memory_operations_handler.h"
#include "runtime/aub/aub_center.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/sip.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

namespace NEO {
ExecutionEnvironment::ExecutionEnvironment() {
    hwInfo = std::make_unique<HardwareInfo>(*platformDevices[0]);
};

ExecutionEnvironment::~ExecutionEnvironment() = default;
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment);

void ExecutionEnvironment::initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (!aubCenter) {
        aubCenter.reset(new AubCenter(hwInfo.get(), localMemoryEnabled, aubFileName, csrType));
    }
}
void ExecutionEnvironment::initGmm() {
    if (!gmmHelper) {
        gmmHelper.reset(new GmmHelper(hwInfo.get()));
    }
}
void ExecutionEnvironment::setHwInfo(const HardwareInfo *hwInfo) {
    *this->hwInfo = *hwInfo;
}
bool ExecutionEnvironment::initializeCommandStreamReceiver(uint32_t deviceIndex, uint32_t deviceCsrIndex) {
    if (deviceIndex + 1 > commandStreamReceivers.size()) {
        commandStreamReceivers.resize(deviceIndex + 1);
    }

    if (deviceCsrIndex + 1 > commandStreamReceivers[deviceIndex].size()) {
        commandStreamReceivers[deviceIndex].resize(deviceCsrIndex + 1);
    }
    if (this->commandStreamReceivers[deviceIndex][deviceCsrIndex]) {
        return true;
    }
    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver(createCommandStream(*this));
    if (!commandStreamReceiver) {
        return false;
    }
    if (HwHelper::get(hwInfo->platform.eRenderCoreFamily).isPageTableManagerSupported(*hwInfo)) {
        commandStreamReceiver->createPageTableManager();
    }
    this->commandStreamReceivers[deviceIndex][deviceCsrIndex] = std::move(commandStreamReceiver);
    return true;
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
GmmHelper *ExecutionEnvironment::getGmmHelper() const {
    return gmmHelper.get();
}
CompilerInterface *ExecutionEnvironment::getCompilerInterface() {
    if (this->compilerInterface.get() == nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        if (this->compilerInterface.get() == nullptr) {
            this->compilerInterface.reset(CompilerInterface::createInstance());
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

EngineControl *ExecutionEnvironment::getEngineControlForSpecialCsr() {
    EngineControl *engine = nullptr;
    if (specialCommandStreamReceiver.get()) {
        engine = memoryManager->getRegisteredEngineForCsr(specialCommandStreamReceiver.get());
    }
    return engine;
}

bool ExecutionEnvironment::isFullRangeSvm() const {
    return hwInfo->capabilityTable.gpuAddressSpace >= maxNBitValue<47>;
}
} // namespace NEO
