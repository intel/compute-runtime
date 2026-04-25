/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

struct WddmAllocationInternalHandleTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockDevice> device;
};

TEST_F(WddmAllocationInternalHandleTest, givenWddmAllocationWithNtSecureHandleWhenPeekInternalHandleThenHandleIsReturned) {
    MockWddmAllocation allocation(device->getGmmHelper());
    allocation.ntSecureHandle = 0x1234u;

    uint64_t handle = 0;
    void *reservedHandleData = nullptr;
    int ret = allocation.peekInternalHandle(nullptr, handle, reservedHandleData);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(handle, 0x1234u);
}

TEST_F(WddmAllocationInternalHandleTest, givenWddmAllocationWithNoNtSecureHandleWhenPeekInternalHandleThenErrorIsReturned) {
    MockWddmAllocation allocation(device->getGmmHelper());
    allocation.ntSecureHandle = 0;

    uint64_t handle = 0;
    void *reservedHandleData = nullptr;
    int ret = allocation.peekInternalHandle(nullptr, handle, reservedHandleData);

    EXPECT_NE(ret, 0);
    EXPECT_EQ(handle, 0u);
}

TEST_F(WddmAllocationInternalHandleTest, givenWddmAllocationWithParentWhenPeekInternalHandleThenParentHandleIsReturned) {
    MockWddmAllocation parentAllocation(device->getGmmHelper());
    parentAllocation.ntSecureHandle = 0x5678u;

    MockWddmAllocation childAllocation(device->getGmmHelper());
    childAllocation.parentAllocation = &parentAllocation;
    childAllocation.ntSecureHandle = 0x1234u; // Child's own handle

    uint64_t handle = 0;
    void *reservedHandleData = nullptr;
    int ret = childAllocation.peekInternalHandle(nullptr, handle, reservedHandleData);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(handle, 0x5678u); // Should return parent's handle
}

TEST_F(WddmAllocationInternalHandleTest, givenWddmAllocationWithParentAndNoHandleWhenPeekInternalHandleThenErrorIsReturned) {
    MockWddmAllocation parentAllocation(device->getGmmHelper());
    parentAllocation.ntSecureHandle = 0; // No handle

    MockWddmAllocation childAllocation(device->getGmmHelper());
    childAllocation.parentAllocation = &parentAllocation;

    uint64_t handle = 0;
    void *reservedHandleData = nullptr;
    int ret = childAllocation.peekInternalHandle(nullptr, handle, reservedHandleData);

    EXPECT_NE(ret, 0);
}

TEST_F(WddmAllocationInternalHandleTest, givenWddmAllocationWhenCreateInternalHandleWithExistingHandleThenHandleIsReturned) {
    MockWddmAllocation allocation(device->getGmmHelper());
    allocation.ntSecureHandle = 0x9ABCu;

    uint64_t handle = 0;
    void *reservedHandleData = nullptr;
    int ret = allocation.createInternalHandle(nullptr, 0, handle, reservedHandleData);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(handle, 0x9ABCu);
}

TEST_F(WddmAllocationInternalHandleTest, givenWddmAllocationWithParentWhenCreateInternalHandleThenParentHandleIsCreated) {
    MockWddmAllocation parentAllocation(device->getGmmHelper());
    parentAllocation.ntSecureHandle = 0x5678u;

    MockWddmAllocation childAllocation(device->getGmmHelper());
    childAllocation.parentAllocation = &parentAllocation;
    childAllocation.ntSecureHandle = 0;

    uint64_t handle = 0;
    void *reservedHandleData = nullptr;
    int ret = childAllocation.createInternalHandle(nullptr, 0, handle, reservedHandleData);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(handle, 0x5678u); // Should return parent's existing handle
}

} // namespace NEO
