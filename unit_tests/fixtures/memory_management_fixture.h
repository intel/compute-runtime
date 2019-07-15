/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/unit_tests/helpers/memory_management.h"

#include "gtest/gtest.h"

#include <functional>

struct MemoryManagementFixture {
    MemoryManagementFixture() {
        MemoryManagement::detailedAllocationLoggingActive = true;
    };
    virtual ~MemoryManagementFixture() { MemoryManagement::detailedAllocationLoggingActive = false; };

    // Typical Fixture methods
    virtual void SetUp(void);
    virtual void TearDown(void);

    // Helper methods
    void setFailingAllocation(size_t allocation);
    void clearFailingAllocation(void);

    ::testing::AssertionResult assertLeak(
        const char *leak_expr,
        size_t leakIndex);

    void checkForLeaks(void);

    typedef std::function<void(size_t)> InjectedFunction;
    void injectFailures(InjectedFunction &method, uint32_t maxIndex = 0);
    void injectFailureOnIndex(InjectedFunction &method, uint32_t Index);

    // Used to keep track of # of allocations prior at SetUp time
    // Gets compared to # at TearDown time
    size_t previousAllocations;
};
