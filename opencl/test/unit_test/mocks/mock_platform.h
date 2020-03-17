/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"

#include "opencl/source/platform/platform.h"

namespace NEO {

class MockPlatform : public Platform {
  public:
    using Platform::fillGlobalDispatchTable;
    using Platform::initializationLoopHelper;
    MockPlatform() : MockPlatform(*(new ExecutionEnvironment())) {}
    MockPlatform(ExecutionEnvironment &executionEnvironment) : Platform(executionEnvironment) {}
    bool initializeWithNewDevices();
};

Platform *platform();

Platform *constructPlatform();

bool initPlatform();
} // namespace NEO
