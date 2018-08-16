/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
