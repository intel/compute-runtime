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
    void initAubCenter(const HardwareInfo *hwInfo, bool localMemoryEnabled) override {
        initAubCenterCalled = true;
        ExecutionEnvironment::initAubCenter(hwInfo, localMemoryEnabled);
    }
    bool initAubCenterCalled = false;
};
} // namespace OCLRT
