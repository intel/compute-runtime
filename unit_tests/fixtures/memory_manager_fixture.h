/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/options.h"

using namespace OCLRT;

class MockCommandStreamReceiver;
namespace OCLRT {
class MockMemoryManager;
}; // namespace OCLRT

class MemoryManagerWithCsrFixture {
  public:
    MockMemoryManager *memoryManager;
    ExecutionEnvironment executionEnvironment;
    MockCommandStreamReceiver *csr;
    uint32_t taskCount = 0;
    uint32_t currentGpuTag = initialHardwareTag;

    ~MemoryManagerWithCsrFixture() = default;

    void SetUp();
    void TearDown();
};
