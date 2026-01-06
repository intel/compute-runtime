/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {

namespace PagaFaultManagerTestConfig {
extern bool disabled;
}

class PageFaultManagerConfigFixture : public ::testing::Test {
  public:
    void SetUp() override {
        if (PagaFaultManagerTestConfig::disabled) {
            GTEST_SKIP();
        }
    }
};

class PageFaultManagerTest : public ::testing::Test {
  public:
    void SetUp() override {
        memoryManager = std::make_unique<MockMemoryManager>(executionEnvironment);
        unifiedMemoryManager = std::make_unique<SVMAllocsManager>(memoryManager.get());
        pageFaultManager = std::make_unique<MockPageFaultManager>();
    }

    MemoryProperties memoryProperties;

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockMemoryManager> memoryManager;
    std::unique_ptr<SVMAllocsManager> unifiedMemoryManager;
    std::unique_ptr<MockPageFaultManager> pageFaultManager;
};
} // namespace NEO
