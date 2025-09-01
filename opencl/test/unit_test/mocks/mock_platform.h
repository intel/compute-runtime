/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"

#include "opencl/source/platform/platform.h"

namespace NEO {
class MockClDevice;
class MockDevice;
class MockPlatform : public Platform {
  public:
    using Platform::fillGlobalDispatchTable;
    using Platform::usmPoolInitialized;
    MockPlatform() : MockPlatform(*(new ExecutionEnvironment())) {}
    MockPlatform(ExecutionEnvironment &executionEnvironment) : Platform(executionEnvironment) {}
    bool initializeWithNewDevices();
    bool initializeAsMockClDevices(std::vector<std::unique_ptr<Device>> devices);
};

Platform *platform();

Platform *platform(ExecutionEnvironment *executionEnvironment);

void cleanupPlatform(ExecutionEnvironment *executionEnvironment);

Platform *constructPlatform();

Platform *constructPlatform(ExecutionEnvironment *executionEnvironment);

bool initPlatform(std::vector<MockDevice *> deviceVector);

bool initPlatform();
} // namespace NEO
