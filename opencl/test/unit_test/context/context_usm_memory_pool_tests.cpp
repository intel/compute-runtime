/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

struct ContextUsmPoolTest : public ::testing::Test {
    void SetUp() override {
        device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), 0u)});
        if (device->deviceInfo.svmCapabilities == 0) {
            GTEST_SKIP();
        }
        cl_device_id deviceId = device.get();

        cl_int retVal;
        mockContext.reset(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        mockDeviceUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&mockContext->getDeviceMemAllocPool());
        mockHostUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&mockContext->getHostMemAllocPool());
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> mockContext;
    MockUsmMemAllocPool *mockDeviceUsmMemAllocPool;
    MockUsmMemAllocPool *mockHostUsmMemAllocPool;
    constexpr static auto poolAllocationThreshold = 1 * MemoryConstants::megaByte;
};

TEST_F(ContextUsmPoolTest, givenCreatedContextWhenCheckingUsmPoolsThenPoolsAreNotInitialized) {
    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockDeviceUsmMemAllocPool->poolSize);
    EXPECT_EQ(nullptr, mockDeviceUsmMemAllocPool->pool);

    EXPECT_FALSE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockHostUsmMemAllocPool->poolSize);
    EXPECT_EQ(nullptr, mockHostUsmMemAllocPool->pool);
}

TEST_F(ContextUsmPoolTest, givenEnabledDebugFlagsAndUsmPoolsNotSupportedWhenCreatingAllocationsThenPoolsAreInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeviceUsmAllocationPool.set(1);
    debugManager.flags.EnableHostUsmAllocationPool.set(3);
    RAIIProductHelperFactory<MockProductHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = false;
    raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = false;
    cl_int retVal = CL_SUCCESS;
    void *pooledDeviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledDeviceAlloc);
    clMemFreeINTEL(mockContext.get(), pooledDeviceAlloc);

    void *pooledHostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledHostAlloc);
    clMemFreeINTEL(mockContext.get(), pooledHostAlloc);

    EXPECT_TRUE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_EQ(1 * MemoryConstants::megaByte, mockDeviceUsmMemAllocPool->poolSize);
    EXPECT_NE(nullptr, mockDeviceUsmMemAllocPool->pool);
    EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, mockDeviceUsmMemAllocPool->poolMemoryType);

    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(3 * MemoryConstants::megaByte, mockHostUsmMemAllocPool->poolSize);
    EXPECT_NE(nullptr, mockHostUsmMemAllocPool->pool);
    EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, mockHostUsmMemAllocPool->poolMemoryType);
}

TEST_F(ContextUsmPoolTest, givenUsmPoolsSupportedWhenCreatingAllocationsThenPoolsAreInitialized) {
    RAIIProductHelperFactory<MockProductHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    cl_int retVal = CL_SUCCESS;
    void *pooledDeviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledDeviceAlloc);
    clMemFreeINTEL(mockContext.get(), pooledDeviceAlloc);

    void *pooledHostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledHostAlloc);
    clMemFreeINTEL(mockContext.get(), pooledHostAlloc);

    EXPECT_EQ(2 * MemoryConstants::megaByte, mockDeviceUsmMemAllocPool->poolSize);
    EXPECT_EQ(2 * MemoryConstants::megaByte, mockHostUsmMemAllocPool->poolSize);
    EXPECT_TRUE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
}

TEST_F(ContextUsmPoolTest, givenUsmPoolsSupportedAndDisabledByDebugFlagsWhenCreatingAllocationsThenPoolsAreNotInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
    debugManager.flags.EnableHostUsmAllocationPool.set(0);
    RAIIProductHelperFactory<MockProductHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    cl_int retVal = CL_SUCCESS;
    void *deviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), deviceAlloc);

    void *hostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), hostAlloc);

    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_FALSE(mockHostUsmMemAllocPool->isInitialized());
}

TEST_F(ContextUsmPoolTest, givenUsmPoolsSupportedAndMultiDeviceContextWhenCreatingAllocationsThenPoolsAreNotInitialized) {
    RAIIProductHelperFactory<MockProductHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    mockContext->devices.push_back(nullptr);
    EXPECT_FALSE(mockContext->isSingleDeviceContext());

    cl_int retVal = CL_SUCCESS;
    void *deviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), deviceAlloc);

    void *hostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), hostAlloc);

    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_FALSE(mockHostUsmMemAllocPool->isInitialized());
    mockContext->devices.pop_back();
}