/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/source/aub/aub_center.h"

namespace NEO {

RootDeviceEnvironment::RootDeviceEnvironment(ExecutionEnvironment &executionEnvironment) : executionEnvironment(executionEnvironment) {
    hwInfo = std::make_unique<HardwareInfo>();
}

RootDeviceEnvironment::~RootDeviceEnvironment() = default;

void RootDeviceEnvironment::initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (!aubCenter) {
        aubCenter.reset(new AubCenter(getHardwareInfo(), localMemoryEnabled, aubFileName, csrType));
    }
}

void RootDeviceEnvironment::initDebugger() {
    debugger = Debugger::create(hwInfo.get());
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

void RootDeviceEnvironment::initGmm() {
    if (!gmmHelper) {
        gmmHelper.reset(new GmmHelper(osInterface.get(), getHardwareInfo()));
    }
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
} // namespace NEO
