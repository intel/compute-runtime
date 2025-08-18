/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/shared_pool_allocation.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

struct SharedPoolAllocationTest : public ::testing::Test {
    void SetUp() override {
        buffer = reinterpret_cast<void *>(gpuAddress);
        mockAllocation = std::make_unique<MockGraphicsAllocation>(buffer, gpuAddress, allocationSize);
    }

    const uint64_t gpuAddress = 0x1200;
    const size_t allocationSize = 4096;
    void *buffer = nullptr;
    std::unique_ptr<MockGraphicsAllocation> mockAllocation;
};

TEST_F(SharedPoolAllocationTest, givenSharedPoolAllocationWithOffsetWhenAccessingGettersThenReturnsCorrectValues) {
    constexpr size_t chunkOffset = 64;
    constexpr size_t chunkSize = 256;
    SharedPoolAllocation sharedPoolAllocation(mockAllocation.get(), chunkOffset, chunkSize, nullptr);

    EXPECT_EQ(chunkOffset, sharedPoolAllocation.getOffset());
    EXPECT_EQ(chunkSize, sharedPoolAllocation.getSize());
    EXPECT_EQ(mockAllocation.get(), sharedPoolAllocation.getGraphicsAllocation());
    EXPECT_EQ(mockAllocation->getGpuAddress() + chunkOffset, sharedPoolAllocation.getGpuAddress());
    EXPECT_EQ(mockAllocation->getGpuAddressToPatch() + chunkOffset, sharedPoolAllocation.getGpuAddressToPatch());
    EXPECT_EQ(ptrOffset(mockAllocation->getUnderlyingBuffer(), chunkOffset), sharedPoolAllocation.getUnderlyingBuffer());
}

TEST_F(SharedPoolAllocationTest, givenSharedPoolAllocationWithNonPooledGraphicsAllocationWhenAccessingGettersThenReturnsZeroOffsetAndBaseValues) {
    SharedPoolAllocation sharedPoolAllocation(mockAllocation.get());

    EXPECT_EQ(0u, sharedPoolAllocation.getOffset());
    EXPECT_EQ(mockAllocation->getUnderlyingBufferSize(), sharedPoolAllocation.getSize());
    EXPECT_EQ(mockAllocation.get(), sharedPoolAllocation.getGraphicsAllocation());
    EXPECT_EQ(mockAllocation->getGpuAddress(), sharedPoolAllocation.getGpuAddress());
    EXPECT_EQ(mockAllocation->getGpuAddressToPatch(), sharedPoolAllocation.getGpuAddressToPatch());
    EXPECT_EQ(mockAllocation->getUnderlyingBuffer(), sharedPoolAllocation.getUnderlyingBuffer());
}

TEST_F(SharedPoolAllocationTest, givenSharedPoolAllocationWithMutexWhenObtainingLockThenMutexIsProperlyLocked) {
    constexpr size_t chunkOffset = 64;
    constexpr size_t chunkSize = 256;
    std::mutex mtx;
    SharedPoolAllocation sharedPoolAllocation(mockAllocation.get(), chunkOffset, chunkSize, &mtx);

    {
        auto lock = sharedPoolAllocation.obtainSharedAllocationLock();
        EXPECT_TRUE(lock.owns_lock());
        EXPECT_EQ(&mtx, lock.mutex());
        EXPECT_FALSE(mtx.try_lock());
    }

    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

TEST_F(SharedPoolAllocationTest, givenSharedPoolAllocationWithoutMutexWhenObtainingLockThenReturnsEmptyLock) {
    SharedPoolAllocation sharedPoolAllocation(mockAllocation.get());

    auto lock = sharedPoolAllocation.obtainSharedAllocationLock();

    EXPECT_FALSE(lock.owns_lock());
    EXPECT_EQ(nullptr, lock.mutex());
}