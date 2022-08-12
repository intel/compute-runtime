/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/memory_management.h"

#include "gtest/gtest.h"

#include <functional>

struct MemoryManagementFixture {
    MemoryManagementFixture() {
        MemoryManagement::detailedAllocationLoggingActive = true;
    };
    virtual ~MemoryManagementFixture() { MemoryManagement::detailedAllocationLoggingActive = false; };

    // Typical Fixture methods
    void setUp();
    void tearDown();

    // Helper methods
    void setFailingAllocation(size_t allocation);
    void clearFailingAllocation();

    ::testing::AssertionResult assertLeak(
        const char *leakExpr,
        size_t leakIndex);

    void checkForLeaks();

    typedef std::function<void(size_t)> InjectedFunction;
    void injectFailures(InjectedFunction &method, uint32_t maxIndex = 0);
    void injectFailureOnIndex(InjectedFunction &method, uint32_t index);

    // Used to keep track of # of allocations prior at SetUp time
    // Gets compared to # at TearDown time
    size_t previousAllocations;
};
