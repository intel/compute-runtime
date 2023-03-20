/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/options.h"

using namespace NEO;

class MockCommandStreamReceiver;
namespace NEO {
class MockMemoryManager;
}; // namespace NEO

class MemoryManagerWithCsrFixture {
  public:
    MockMemoryManager *memoryManager;
    ExecutionEnvironment executionEnvironment;
    MockCommandStreamReceiver *csr;
    TaskCountType taskCount = 0;
    TagAddressType currentGpuTag = initialHardwareTag;

    ~MemoryManagerWithCsrFixture() = default;

    void SetUp();
    void TearDown();
};
