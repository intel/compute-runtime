/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/mocks/mock_csr.h"

using namespace OCLRT;

namespace OCLRT {
class MemoryManager;
class GMockMemoryManager;
}; // namespace OCLRT

class MemoryManagerWithCsrFixture {
  public:
    MemoryManager *memoryManager;
    GMockMemoryManager *gmockMemoryManager;
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockCommandStreamReceiver> csr;
    uint32_t taskCount = 0;
    uint32_t currentGpuTag = initialHardwareTag;

    MemoryManagerWithCsrFixture() { csr = std::make_unique<MockCommandStreamReceiver>(this->executionEnvironment); }

    ~MemoryManagerWithCsrFixture() = default;

    void SetUp();
    void TearDown();
};
