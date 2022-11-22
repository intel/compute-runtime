/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/options.h"

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
    TaskCountType taskCount = 0;
    TagAddressType currentGpuTag = initialHardwareTag;

    ~MemoryManagerWithCsrFixture() = default;

    void setUp();
    void tearDown();
};
