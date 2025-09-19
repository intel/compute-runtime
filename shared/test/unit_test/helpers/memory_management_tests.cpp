/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/compression_selector.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/memory_management.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "gtest/gtest.h"

using MemoryManagement::AllocationEvent;
using MemoryManagement::eventsAllocated;
using MemoryManagement::eventsDeallocated;
using MemoryManagement::failingAllocation;
using MemoryManagement::indexAllocation;
using MemoryManagement::indexDeallocation;
using MemoryManagement::numAllocations;

namespace NEO {
extern bool isStatelessCompressionSupportedForUlts;
}

TEST(allocation, GivenFailingAllocationNegativeOneWhenCreatingAllocationThenAllocationIsCreatedSuccessfully) {
    ASSERT_EQ(failingAllocation, static_cast<size_t>(-1));
    auto ptr = new (std::nothrow) char;
    EXPECT_NE(nullptr, ptr);
    delete ptr;
}

TEST(allocation, GivenFailingAllocationOneWhenCreatingAllocationsThenOnlyOneAllocationIsCreatedSuccessfully) {
    MemoryManagement::detailedAllocationLoggingActive = true;
    ASSERT_EQ(static_cast<size_t>(-1), failingAllocation);
    auto previousAllocations = numAllocations.load();
    MemoryManagement::indexAllocation = 0;

    failingAllocation = 1;
    auto ptr1 = new (std::nothrow) char;
    auto ptr2 = new (std::nothrow) char;
    delete ptr1;
    delete ptr2;
    auto currentAllocations = numAllocations.load();
    failingAllocation = -1;

    EXPECT_NE(nullptr, ptr1);
    EXPECT_EQ(nullptr, ptr2); // NOLINT(clang-analyzer-cplusplus.NewDelete)
    EXPECT_EQ(previousAllocations, currentAllocations);
    MemoryManagement::detailedAllocationLoggingActive = false;
}

TEST(CompressionSelector, WhenAllowStatelessCompressionIsCalledThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    VariableBackup<bool> backup(&NEO::isStatelessCompressionSupportedForUlts);

    NEO::isStatelessCompressionSupportedForUlts = false;
    EXPECT_FALSE(CompressionSelector::allowStatelessCompression());

    NEO::isStatelessCompressionSupportedForUlts = true;

    for (auto enable : {-1, 0, 1}) {
        debugManager.flags.EnableStatelessCompression.set(enable);

        if (enable > 0) {
            EXPECT_TRUE(CompressionSelector::allowStatelessCompression());
        } else {
            EXPECT_FALSE(CompressionSelector::allowStatelessCompression());
        }
    }
}

struct MemoryManagementTest : public MemoryManagementFixture,
                              public ::testing::Test {
    void SetUp() override {
        MemoryManagementFixture::setUp();
    }

    void TearDown() override {
        MemoryManagementFixture::tearDown();
    }
};

TEST_F(MemoryManagementTest, GivenFailingAllocationOneWhenCreatingAllocationsThenOnlyOneAllocationIsCreatedSuccessfully) {
    setFailingAllocation(1);
    auto ptr1 = new (std::nothrow) char;
    auto ptr2 = new (std::nothrow) char;
    delete ptr1;
    delete ptr2;
    clearFailingAllocation();

    EXPECT_NE(nullptr, ptr1);
    EXPECT_EQ(nullptr, ptr2); // NOLINT(clang-analyzer-cplusplus.NewDelete)
}

TEST_F(MemoryManagementTest, GivenNoFailingAllocationWhenCreatingAllocationThenMemoryIsNotLeaked) {
    auto indexAllocationTop = indexAllocation.load();
    auto indexDellocationTop = indexDeallocation.load();
    EXPECT_EQ(static_cast<size_t>(-1), MemoryManagement::enumerateLeak(indexAllocationTop, indexDellocationTop, false, false));
}

TEST_F(MemoryManagementTest, GivenOneFailingAllocationWhenCreatingAllocationThenMemoryIsLeaked) {
    size_t sizeBuffer = 10;
    auto ptr = new (std::nothrow) char[sizeBuffer];
    auto indexAllocationTop = indexAllocation.load();
    auto indexDeallocationTop = indexDeallocation.load();
    auto leakIndex = MemoryManagement::enumerateLeak(indexAllocationTop, indexDeallocationTop, false, false);
    ASSERT_NE(static_cast<size_t>(-1), leakIndex); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(ptr, eventsAllocated[leakIndex].address);
    EXPECT_EQ(sizeBuffer, eventsAllocated[leakIndex].size);

    // Not expecting any more failures
    EXPECT_EQ(static_cast<size_t>(-1), MemoryManagement::enumerateLeak(indexAllocationTop, indexDeallocationTop, false, false));

    delete[] ptr;
}

TEST_F(MemoryManagementTest, GivenFourEventsWhenCreatingAllocationThenMemoryIsLeakedOnce) {
    size_t sizeBuffer = 10;
    delete new (std::nothrow) char;
    auto ptr = new (std::nothrow) char[sizeBuffer];
    delete new (std::nothrow) char;
    auto indexAllocationTop = indexAllocation.load();
    auto indexDeallocationTop = indexDeallocation.load();
    auto leakIndex = MemoryManagement::enumerateLeak(indexAllocationTop, indexDeallocationTop, false, false);
    ASSERT_NE(static_cast<size_t>(-1), leakIndex); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(ptr, eventsAllocated[leakIndex].address);
    EXPECT_EQ(sizeBuffer, eventsAllocated[leakIndex].size);

    // Not expecting any more failures
    EXPECT_EQ(static_cast<size_t>(-1), MemoryManagement::enumerateLeak(indexAllocationTop, indexDeallocationTop, false, false));

    delete[] ptr;
}

TEST_F(MemoryManagementTest, GivenTwoFailingAllocationsWhenCreatingAllocationThenMemoryIsLeaked) {
    size_t sizeBuffer = 10;
    auto ptr1 = new (std::nothrow) char[sizeBuffer];
    auto ptr2 = new (std::nothrow) char[sizeBuffer];
    auto indexAllocationTop = indexAllocation.load();
    auto indexDeallocationTop = indexDeallocation.load();
    auto leakIndex1 = MemoryManagement::enumerateLeak(indexAllocationTop, indexDeallocationTop, false, false);
    auto leakIndex2 = MemoryManagement::enumerateLeak(indexAllocationTop, indexDeallocationTop, false, false);
    ASSERT_NE(static_cast<size_t>(-1), leakIndex1); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(ptr1, eventsAllocated[leakIndex1].address);
    EXPECT_EQ(sizeBuffer, eventsAllocated[leakIndex1].size);

    ASSERT_NE(static_cast<size_t>(-1), leakIndex2);
    EXPECT_EQ(ptr2, eventsAllocated[leakIndex2].address);
    EXPECT_EQ(sizeBuffer, eventsAllocated[leakIndex2].size);

    // Not expecting any more failures
    EXPECT_EQ(static_cast<size_t>(-1), MemoryManagement::enumerateLeak(indexAllocationTop, indexDeallocationTop, false, false));

    delete[] ptr1;
    delete[] ptr2;
}

TEST_F(MemoryManagementTest, WhenDeletingNullPtrThenLeakIsNotReported) {
    char *ptr = nullptr;
    delete ptr;
}

TEST_F(MemoryManagementTest, WhenPointerIsDeletedThenAllocationShouldbeVisible) {
    size_t sizeBuffer = 10;
    auto index = MemoryManagement::indexAllocation.load();
    auto ptr = new (std::nothrow) char[sizeBuffer];
    EXPECT_EQ(ptr, eventsAllocated[index].address);
    EXPECT_EQ(sizeBuffer, eventsAllocated[index].size);

    index = MemoryManagement::indexDeallocation;
    uintptr_t ptrCopy = reinterpret_cast<uintptr_t>(ptr);
    delete[] ptr;
    EXPECT_EQ(ptrCopy, reinterpret_cast<uintptr_t>(eventsDeallocated[index].address));
}

#if ENABLE_ME_FOR_LEAK_TESTING
TEST_F(MemoryManagementTest, GivenEnableForLeakTestingThenDetectLeak) {
    // Useful reference : MemoryManagement::onAllocationEvent
    MemoryManagement::breakOnAllocationEvent = 1;
    MemoryManagement::breakOnDeallocationEvent = 0;
    delete new char;
    new char;

    MemoryManagement::breakOnAllocationEvent = -1;
    MemoryManagement::breakOnDeallocationEvent = -1;
}
#endif
