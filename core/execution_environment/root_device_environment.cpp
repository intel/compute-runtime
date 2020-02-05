/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/root_device_environment.h"

#include "core/execution_environment/execution_environment.h"
#include "core/gmm_helper/page_table_mngr.h"
#include "core/memory_manager/memory_operations_handler.h"
#include "core/os_interface/os_interface.h"
#include "runtime/aub/aub_center.h"

namespace NEO {

RootDeviceEnvironment::RootDeviceEnvironment(ExecutionEnvironment &executionEnvironment) : executionEnvironment(executionEnvironment) {}
RootDeviceEnvironment::~RootDeviceEnvironment() = default;

void RootDeviceEnvironment::initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (!aubCenter) {
        aubCenter.reset(new AubCenter(executionEnvironment.getHardwareInfo(), localMemoryEnabled, aubFileName, csrType));
    }
}
const HardwareInfo *RootDeviceEnvironment::getHardwareInfo() const {
    return executionEnvironment.getHardwareInfo();
}
} // namespace NEO
