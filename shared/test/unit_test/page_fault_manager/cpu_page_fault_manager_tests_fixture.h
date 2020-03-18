/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/unit_test/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "gtest/gtest.h"

namespace NEO {

namespace PagaFaultManagerTestConfig {
extern bool disabled;
}

class PageFaultManagerConfigFixture : public ::testing::Test {
  public:
    void SetUp() {
        if (PagaFaultManagerTestConfig::disabled) {
            GTEST_SKIP();
        }
    }
};

class PageFaultManagerTest : public ::testing::Test {
  public:
    void SetUp() override {
        pageFaultManager = std::make_unique<MockPageFaultManager>();
    }
    void *unifiedMemoryManager = nullptr;
    std::unique_ptr<MockPageFaultManager> pageFaultManager;
};
} // namespace NEO
