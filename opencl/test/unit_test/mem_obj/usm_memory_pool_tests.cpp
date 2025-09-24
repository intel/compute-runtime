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
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

#include "gtest/gtest.h"

using namespace NEO;

struct UsmPoolTest : public ::testing::Test {
    void SetUp() override {
        deviceFactory = std::make_unique<UltClDeviceFactoryWithPlatform>(2, 0);
        device = deviceFactory->rootDevices[0];
        if (device->deviceInfo.svmCapabilities == 0) {
            GTEST_SKIP();
        }
        cl_device_id deviceId = device;

        cl_int retVal;
        mockContext.reset(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        mockDeviceUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&mockContext->getDeviceMemAllocPool());
        mockHostUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&mockContext->getDevice(0u)->getPlatform()->getHostMemAllocPool());
    }

    MockClDevice *device;
    std::unique_ptr<UltClDeviceFactoryWithPlatform> deviceFactory;
    std::unique_ptr<MockContext> mockContext;
    MockUsmMemAllocPool *mockDeviceUsmMemAllocPool;
    MockUsmMemAllocPool *mockHostUsmMemAllocPool;
    constexpr static auto poolAllocationThreshold = 1 * MemoryConstants::megaByte;
};

TEST_F(UsmPoolTest, givenCreatedContextWhenCheckingUsmPoolsThenPoolsAreNotInitialized) {
    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockDeviceUsmMemAllocPool->poolInfo.poolSize);
    EXPECT_EQ(nullptr, mockDeviceUsmMemAllocPool->pool);

    EXPECT_FALSE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockHostUsmMemAllocPool->poolInfo.poolSize);
    EXPECT_EQ(nullptr, mockHostUsmMemAllocPool->pool);
}

TEST_F(UsmPoolTest, givenEnabledDebugFlagsAndUsmPoolsNotSupportedWhenCreatingAllocationsThenPoolsAreInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeviceUsmAllocationPool.set(1);
    debugManager.flags.EnableHostUsmAllocationPool.set(3);
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = false;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = false;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = false;

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
    EXPECT_EQ(1 * MemoryConstants::megaByte, mockDeviceUsmMemAllocPool->poolInfo.poolSize);
    EXPECT_NE(nullptr, mockDeviceUsmMemAllocPool->pool);
    EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, mockDeviceUsmMemAllocPool->poolMemoryType);

    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(3 * MemoryConstants::megaByte, mockHostUsmMemAllocPool->poolInfo.poolSize);
    EXPECT_NE(nullptr, mockHostUsmMemAllocPool->pool);
    EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, mockHostUsmMemAllocPool->poolMemoryType);
}

TEST_F(UsmPoolTest, givenUsmPoolsSupportedWhenCreatingAllocationsThenPoolsAreInitialized) {
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    cl_int retVal = CL_SUCCESS;
    void *pooledDeviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledDeviceAlloc);
    clMemFreeINTEL(mockContext.get(), pooledDeviceAlloc);

    void *pooledHostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledHostAlloc);
    clMemFreeINTEL(mockContext.get(), pooledHostAlloc);

    EXPECT_EQ(UsmPoolParams::getUsmPoolSize(deviceFactory->rootDevices[0]->getGfxCoreHelper()), mockDeviceUsmMemAllocPool->poolInfo.poolSize);
    EXPECT_EQ(UsmPoolParams::getUsmPoolSize(deviceFactory->rootDevices[0]->getGfxCoreHelper()), mockHostUsmMemAllocPool->poolInfo.poolSize);
    EXPECT_TRUE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
}

TEST_F(UsmPoolTest, givenUsmPoolsSupportedAndDisabledByDebugFlagsWhenCreatingAllocationsThenPoolsAreNotInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
    debugManager.flags.EnableHostUsmAllocationPool.set(0);
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

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

TEST_F(UsmPoolTest, givenUsmPoolsSupportedAndMultiDeviceContextWhenCreatingAllocationsThenDevicePoolIsNotInitialized) {
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    mockContext->devices.push_back(deviceFactory->rootDevices[1]);
    EXPECT_FALSE(mockContext->isSingleDeviceContext());

    cl_int retVal = CL_SUCCESS;
    void *deviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), deviceAlloc);

    void *hostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), hostAlloc);

    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
    mockContext->devices.pop_back();
}

TEST_F(UsmPoolTest, givenTwoContextsWhenHostAllocationIsFreedInFirstContextThenItIsReusedInSecondContext) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostUsmAllocationPool.set(3);

    cl_int retVal = CL_SUCCESS;
    void *pooledHostAlloc1 = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledHostAlloc1);

    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(3 * MemoryConstants::megaByte, mockHostUsmMemAllocPool->poolInfo.poolSize);

    clMemFreeINTEL(mockContext.get(), pooledHostAlloc1);

    cl_device_id deviceId = device;
    auto mockContext2 = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    void *pooledHostAlloc2 = clHostMemAllocINTEL(mockContext2.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledHostAlloc2);

    EXPECT_EQ(pooledHostAlloc1, pooledHostAlloc2);

    clMemFreeINTEL(mockContext2.get(), pooledHostAlloc2);
}