/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gtest/gtest.h"
#include "unit_tests/helpers/memory_management.h"
#include <functional>

struct MemoryManagementFixture {
    MemoryManagementFixture() {
        MemoryManagement::detailedAllocationLoggingActive = true;
    };
    static const auto nonfailingAllocation = static_cast<size_t>(-1);
    static const auto invalidLeakIndex = static_cast<size_t>(-1);

    virtual ~MemoryManagementFixture() { MemoryManagement::detailedAllocationLoggingActive = false; };

    // Typical Fixture methods
    virtual void SetUp(void);
    virtual void TearDown(void);

    // Helper methods
    void setFailingAllocation(size_t allocation);
    void clearFailingAllocation(void);

    static size_t enumerateLeak(size_t indexAllocationTop, size_t indexDeallocationTop, bool lookFromEnd = false, bool requireCallStack = false, bool fastLookup = false);

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

std::string printCallStack(const MemoryManagement::AllocationEvent &event);
