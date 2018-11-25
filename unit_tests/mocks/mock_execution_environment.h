/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/execution_environment/execution_environment.h"
namespace OCLRT {
struct MockExecutionEnvironment : ExecutionEnvironment {
    MockExecutionEnvironment() = default;
    void initAubCenter(const HardwareInfo *hwInfo, bool localMemoryEnabled, const std::string &aubFileName) override {
        initAubCenterCalled = true;
        localMemoryEnabledReceived = localMemoryEnabled;
        aubFileNameReceived = aubFileName;
        ExecutionEnvironment::initAubCenter(hwInfo, localMemoryEnabled, aubFileName);
    }
    bool initAubCenterCalled = false;
    bool localMemoryEnabledReceived = false;
    std::string aubFileNameReceived = "";
};
} // namespace OCLRT
