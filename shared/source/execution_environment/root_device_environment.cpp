/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/aub/aub_center.h"
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/utilities/software_tags_manager.h"

namespace NEO {

RootDeviceEnvironment::RootDeviceEnvironment(ExecutionEnvironment &executionEnvironment) : executionEnvironment(executionEnvironment) {
    hwInfo = std::make_unique<HardwareInfo>();

    if (DebugManager.flags.EnableSWTags.get()) {
        tagsManager = std::make_unique<SWTagsManager>();
    }
}

RootDeviceEnvironment::~RootDeviceEnvironment() = default;

void RootDeviceEnvironment::initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (!aubCenter) {
        UNRECOVERABLE_IF(!getGmmHelper());
        aubCenter.reset(new AubCenter(getHardwareInfo(), *gmmHelper, localMemoryEnabled, aubFileName, csrType));
    }
}

void RootDeviceEnvironment::initDebugger() {
    debugger = Debugger::create(hwInfo.get());
}

void RootDeviceEnvironment::initDebuggerL0(Device *neoDevice) {
    if (this->debugger.get() != nullptr) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "Source Level Debugger cannot be used with Environment Variable enabling program debugging.\n");
        UNRECOVERABLE_IF(this->debugger.get() != nullptr);
    }

    this->getMutableHardwareInfo()->capabilityTable.fusedEuEnabled = false;
    this->getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedBuffers = false;
    this->getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedImages = false;

    this->debugger = DebuggerL0::create(neoDevice);
}

const HardwareInfo *RootDeviceEnvironment::getHardwareInfo() const {
    return hwInfo.get();
}

HardwareInfo *RootDeviceEnvironment::getMutableHardwareInfo() const {
    return hwInfo.get();
}

void RootDeviceEnvironment::setHwInfo(const HardwareInfo *hwInfo) {
    *this->hwInfo = *hwInfo;
}

bool RootDeviceEnvironment::isFullRangeSvm() const {
    return hwInfo->capabilityTable.gpuAddressSpace >= maxNBitValue(47);
}

GmmHelper *RootDeviceEnvironment::getGmmHelper() const {
    return gmmHelper.get();
}
GmmClientContext *RootDeviceEnvironment::getGmmClientContext() const {
    return gmmHelper->getClientContext();
}

void RootDeviceEnvironment::prepareForCleanup() const {
    if (osInterface && osInterface->getDriverModel()) {
        osInterface->getDriverModel()->isDriverAvaliable();
    }
}

bool RootDeviceEnvironment::initAilConfiguration() {
    auto ailConfiguration = AILConfiguration::get(hwInfo->platform.eProductFamily);

    if (ailConfiguration == nullptr) {
        return true;
    }

    auto result = ailConfiguration->initProcessExecutableName();
    if (result != true) {
        return false;
    }

    ailConfiguration->apply(hwInfo->capabilityTable);

    return true;
}

void RootDeviceEnvironment::initGmm() {
    if (!gmmHelper) {
        gmmHelper.reset(new GmmHelper(osInterface.get(), getHardwareInfo()));
    }
}

void RootDeviceEnvironment::initOsTime() {
    if (!osTime) {
        osTime = OSTime::create(osInterface.get());
    }
}

BindlessHeapsHelper *RootDeviceEnvironment::getBindlessHeapsHelper() const {
    return bindlessHeapsHelper.get();
}

void RootDeviceEnvironment::createBindlessHeapsHelper(MemoryManager *memoryManager, bool availableDevices, uint32_t rootDeviceIndex, DeviceBitfield deviceBitfield) {
    bindlessHeapsHelper = std::make_unique<BindlessHeapsHelper>(memoryManager, availableDevices, rootDeviceIndex, deviceBitfield);
}

CompilerInterface *RootDeviceEnvironment::getCompilerInterface() {
    if (this->compilerInterface.get() == nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        if (this->compilerInterface.get() == nullptr) {
            auto cache = std::make_unique<CompilerCache>(getDefaultCompilerCacheConfig());
            this->compilerInterface.reset(CompilerInterface::createInstance(std::move(cache), true));
        }
    }
    return this->compilerInterface.get();
}

BuiltIns *RootDeviceEnvironment::getBuiltIns() {
    if (this->builtins.get() == nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        if (this->builtins.get() == nullptr) {
            this->builtins = std::make_unique<BuiltIns>();
        }
    }
    return this->builtins.get();
}

void RootDeviceEnvironment::limitNumberOfCcs(uint32_t numberOfCcs) {

    hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = std::min(hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled, numberOfCcs);
    limitedNumberOfCcs = true;
}

bool RootDeviceEnvironment::isNumberOfCcsLimited() const {
    return limitedNumberOfCcs;
}
} // namespace NEO
