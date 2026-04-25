/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include <limits>

namespace NEO {

struct MemoryAllocationInternalHandleTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockDevice> device;
};

TEST_F(MemoryAllocationInternalHandleTest, givenMemoryAllocationWithValidInternalHandleWhenPeekInternalHandleThenHandleIsReturned) {
    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, 0, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    allocation.internalHandle = 0x1234u;

    uint64_t handle = 0;
    void *reservedHandleData = nullptr;
    int ret = allocation.peekInternalHandle(nullptr, handle, reservedHandleData);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(handle, 0x1234u);
}

TEST_F(MemoryAllocationInternalHandleTest, givenMemoryAllocationWithInvalidInternalHandleWhenPeekInternalHandleThenErrorIsReturned) {
    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, 0, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    allocation.internalHandle = std::numeric_limits<uint64_t>::max();

    uint64_t handle = 0;
    void *reservedHandleData = nullptr;
    int ret = allocation.peekInternalHandle(nullptr, handle, reservedHandleData);

    EXPECT_EQ(ret, -1);
    EXPECT_EQ(handle, 0u);
}

TEST_F(MemoryAllocationInternalHandleTest, givenMemoryAllocationWithZeroHandleWhenPeekInternalHandleThenHandleIsReturned) {
    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, 0, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    allocation.internalHandle = 0;

    uint64_t handle = 0xFFFFu; // Set to non-zero initially
    void *reservedHandleData = nullptr;
    int ret = allocation.peekInternalHandle(nullptr, handle, reservedHandleData);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(handle, 0u);
}

TEST_F(MemoryAllocationInternalHandleTest, givenMemoryAllocationWithMaxMinusOneHandleWhenPeekInternalHandleThenHandleIsReturned) {
    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, 0, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    allocation.internalHandle = std::numeric_limits<uint64_t>::max() - 1;

    uint64_t handle = 0;
    void *reservedHandleData = nullptr;
    int ret = allocation.peekInternalHandle(nullptr, handle, reservedHandleData);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(handle, std::numeric_limits<uint64_t>::max() - 1);
}

} // namespace NEO
