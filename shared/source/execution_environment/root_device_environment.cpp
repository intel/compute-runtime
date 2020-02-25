/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/source/aub/aub_center.h"

namespace NEO {

RootDeviceEnvironment::RootDeviceEnvironment(ExecutionEnvironment &executionEnvironment) : executionEnvironment(executionEnvironment) {}
RootDeviceEnvironment::~RootDeviceEnvironment() = default;

void RootDeviceEnvironment::initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (!aubCenter) {
        aubCenter.reset(new AubCenter(getHardwareInfo(), localMemoryEnabled, aubFileName, csrType));
    }
}
const HardwareInfo *RootDeviceEnvironment::getHardwareInfo() const {
    return executionEnvironment.getHardwareInfo();
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

} // namespace NEO
