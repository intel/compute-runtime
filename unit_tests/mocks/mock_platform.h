/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/execution_environment/execution_environment.h"
#include "runtime/platform/platform.h"
namespace NEO {

class MockPlatform : public Platform {
  public:
    using Platform::fillGlobalDispatchTable;
    using Platform::initializationLoopHelper;
    MockPlatform() : MockPlatform(*(new ExecutionEnvironment())) {}
    MockPlatform(ExecutionEnvironment &executionEnvironment) : Platform(executionEnvironment) {}
};

bool initPlatform();
} // namespace NEO
