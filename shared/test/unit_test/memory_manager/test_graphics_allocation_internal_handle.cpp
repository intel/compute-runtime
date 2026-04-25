/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

struct GraphicsAllocationInternalHandleTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockDevice> device;
};

TEST_F(GraphicsAllocationInternalHandleTest, givenBaseGraphicsAllocationWhenPeekInternalHandleThenDefaultZeroIsReturned) {
    MockGraphicsAllocation allocation(nullptr, 0);

    uint64_t handle = 0xFFFFu; // Set to non-zero initially
    void *reservedHandleData = nullptr;
    int ret = allocation.peekInternalHandle(nullptr, handle, reservedHandleData);

    EXPECT_EQ(ret, 0);
    // Default implementation returns 0 without modifying handle
}

TEST_F(GraphicsAllocationInternalHandleTest, givenBaseGraphicsAllocationWhenCreateInternalHandleThenDefaultZeroIsReturned) {
    MockGraphicsAllocation allocation(nullptr, 0);

    uint64_t handle = 0xFFFFu; // Set to non-zero initially
    void *reservedHandleData = nullptr;
    int ret = allocation.createInternalHandle(nullptr, 0, handle, reservedHandleData);

    EXPECT_EQ(ret, 0);
    // Default implementation returns 0 without modifying handle
}

TEST_F(GraphicsAllocationInternalHandleTest, givenBaseGraphicsAllocationWhenClearInternalHandleThenNoOpReturns) {
    MockGraphicsAllocation allocation(nullptr, 0);

    // clearInternalHandle should be a no-op for base class
    allocation.clearInternalHandle(0);
    // No assertion needed - just verify it doesn't crash
}

TEST_F(GraphicsAllocationInternalHandleTest, givenBaseGraphicsAllocationWhenGetNumHandlesThenDefaultZeroIsReturned) {
    MockGraphicsAllocation allocation(nullptr, 0);

    uint32_t numHandles = allocation.getNumHandles();
    EXPECT_EQ(numHandles, 0u);
}

TEST_F(GraphicsAllocationInternalHandleTest, givenBaseGraphicsAllocationWhenSetNumHandlesThenNoOpReturns) {
    MockGraphicsAllocation allocation(nullptr, 0);

    // setNumHandles should be a no-op for base class
    allocation.setNumHandles(5u);

    // Verify it's still 0 (default implementation doesn't store the value)
    uint32_t numHandles = allocation.getNumHandles();
    EXPECT_EQ(numHandles, 0u);
}

TEST_F(GraphicsAllocationInternalHandleTest, givenNullReservedHandleDataWhenPeekInternalHandleThenSucceeds) {
    MockGraphicsAllocation allocation(nullptr, 0);

    uint64_t handle = 0;
    int ret = allocation.peekInternalHandle(nullptr, handle, nullptr);

    EXPECT_EQ(ret, 0);
}

TEST_F(GraphicsAllocationInternalHandleTest, givenNullReservedHandleDataWhenCreateInternalHandleThenSucceeds) {
    MockGraphicsAllocation allocation(nullptr, 0);

    uint64_t handle = 0;
    int ret = allocation.createInternalHandle(nullptr, 0, handle, nullptr);

    EXPECT_EQ(ret, 0);
}

} // namespace NEO
