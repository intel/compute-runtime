/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/options.h"

namespace OCLRT {
struct MockExecutionEnvironment : ExecutionEnvironment {
    MockExecutionEnvironment() = default;
    void initAubCenter(const HardwareInfo *hwInfo, bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) override {
        initAubCenterCalled = true;
        localMemoryEnabledReceived = localMemoryEnabled;
        aubFileNameReceived = aubFileName;
        ExecutionEnvironment::initAubCenter(hwInfo, localMemoryEnabled, aubFileName, csrType);
    }
    bool initAubCenterCalled = false;
    bool localMemoryEnabledReceived = false;
    std::string aubFileNameReceived = "";
};
} // namespace OCLRT
