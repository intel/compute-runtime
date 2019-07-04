/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/unit_tests/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

class PageFaultManagerTest : public ::testing::Test {
  public:
    void SetUp() override {
        pageFaultManager = std::make_unique<MockPageFaultManager>();
    }
    void *unifiedMemoryManager = nullptr;
    std::unique_ptr<MockPageFaultManager> pageFaultManager;
};
