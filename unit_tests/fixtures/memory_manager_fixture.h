/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/options.h"
#include "runtime/execution_environment/execution_environment.h"

using namespace NEO;

class MockCommandStreamReceiver;
namespace NEO {
class MockMemoryManager;
}; // namespace NEO

class MemoryManagerWithCsrFixture {
  public:
    MockMemoryManager *memoryManager;
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockCommandStreamReceiver> csr;
    uint32_t taskCount = 0;
    uint32_t currentGpuTag = initialHardwareTag;

    ~MemoryManagerWithCsrFixture() = default;

    void SetUp();
    void TearDown();
};
