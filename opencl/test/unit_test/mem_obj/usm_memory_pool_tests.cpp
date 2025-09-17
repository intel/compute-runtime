/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

#include "gtest/gtest.h"

using namespace NEO;

struct UsmPoolTest : public ::testing::Test {
    void SetUp() override {
        deviceFactory = std::make_unique<UltDeviceFactory>(2, 0);
    }

    void TearDown() override {
        mockContext.reset();
        NEO::cleanupPlatform(deviceFactory->rootDevices[0]->getExecutionEnvironment());
        deviceFactory.reset();
    }

    void createPlatformWithContext() {
        platform = NEO::constructPlatform(deviceFactory->rootDevices[0]->getExecutionEnvironment());
        NEO::initPlatform(deviceFactory->rootDevices);

        device = static_cast<MockClDevice *>(platform->getClDevice(0));

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
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    std::unique_ptr<MockContext> mockContext;
    MockUsmMemAllocPool *mockDeviceUsmMemAllocPool;
    MockUsmMemAllocPool *mockHostUsmMemAllocPool;
    Platform *platform;
    constexpr static auto poolAllocationThreshold = 1 * MemoryConstants::megaByte;
};

TEST_F(UsmPoolTest, givenCreatedContextWhenCheckingUsmPoolsThenPoolsAreInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeviceUsmAllocationPool.set(1);
    debugManager.flags.EnableHostUsmAllocationPool.set(3);

    UsmPoolTest::createPlatformWithContext();
    EXPECT_TRUE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_EQ(1 * MemoryConstants::megaByte, mockDeviceUsmMemAllocPool->poolSize);
    EXPECT_NE(nullptr, mockDeviceUsmMemAllocPool->pool);

    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(3 * MemoryConstants::megaByte, mockHostUsmMemAllocPool->poolSize);
    EXPECT_NE(nullptr, mockHostUsmMemAllocPool->pool);
}

TEST_F(UsmPoolTest, givenEnabledDebugFlagsAndUsmPoolsNotSupportedWhenCheckingUsmPoolsThenPoolsAreInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeviceUsmAllocationPool.set(1);
    debugManager.flags.EnableHostUsmAllocationPool.set(3);
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = false;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = false;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = false;

    UsmPoolTest::createPlatformWithContext();

    EXPECT_TRUE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_EQ(1 * MemoryConstants::megaByte, mockDeviceUsmMemAllocPool->poolSize);
    EXPECT_NE(nullptr, mockDeviceUsmMemAllocPool->pool);
    EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, mockDeviceUsmMemAllocPool->poolMemoryType);

    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(3 * MemoryConstants::megaByte, mockHostUsmMemAllocPool->poolSize);
    EXPECT_NE(nullptr, mockHostUsmMemAllocPool->pool);
    EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, mockHostUsmMemAllocPool->poolMemoryType);
}

TEST_F(UsmPoolTest, givenUsmPoolsSupportedWhenCreatingAllocationsThenPoolsAreInitialized) {
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    UsmPoolTest::createPlatformWithContext();

    EXPECT_EQ(UsmPoolParams::getUsmPoolSize(), mockDeviceUsmMemAllocPool->poolSize);
    EXPECT_EQ(UsmPoolParams::getUsmPoolSize(), mockHostUsmMemAllocPool->poolSize);
    EXPECT_TRUE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
}

TEST_F(UsmPoolTest, givenUsmPoolsSupportedAndDisabledByDebugFlagsWhenCreatingAllocationsThenPoolsAreNotInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
    debugManager.flags.EnableHostUsmAllocationPool.set(0);
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    UsmPoolTest::createPlatformWithContext();

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
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    UsmPoolTest::createPlatformWithContext();
    std::vector<cl_device_id> devices = {device, platform->getClDevice(1)};

    cl_int retVal = CL_SUCCESS;
    mockContext.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices.data(), 2), nullptr, nullptr, retVal));
    mockDeviceUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&mockContext->getDeviceMemAllocPool());
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(mockContext->isSingleDeviceContext());

    void *deviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), deviceAlloc);

    void *hostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), hostAlloc);

    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
}

TEST_F(UsmPoolTest, givenTwoContextsWhenHostAllocationIsFreedInFirstContextThenItIsReusedInSecondContext) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostUsmAllocationPool.set(3);

    UsmPoolTest::createPlatformWithContext();

    cl_int retVal = CL_SUCCESS;
    void *pooledHostAlloc1 = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledHostAlloc1);

    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(3 * MemoryConstants::megaByte, mockHostUsmMemAllocPool->poolSize);

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

TEST_F(UsmPoolTest, GivenUsmPoolAllocatorSupportedWhenInitializingUsmPoolsThenPoolsAreInitializedOnlyWithHwCsr) {
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    UltClDeviceFactoryWithPlatform deviceFactoryWithPoolsEnabled{1, 0};
    device = deviceFactoryWithPoolsEnabled.rootDevices[0];

    UsmPoolTest::createPlatformWithContext();

    EXPECT_TRUE(platform->getHostMemAllocPool().isInitialized());
    EXPECT_TRUE(mockContext->getDeviceMemAllocPool().isInitialized());
    mockContext.reset();
    NEO::cleanupPlatform(deviceFactory->rootDevices[0]->getExecutionEnvironment());

    for (int32_t csrType = 0; csrType < static_cast<int32_t>(CommandStreamReceiverType::typesNum); ++csrType) {
        DebugManagerStateRestore restorer;
        debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(csrType));
        UsmPoolTest::createPlatformWithContext();

        EXPECT_NE(nullptr, platform);
        EXPECT_EQ(DeviceFactory::isHwModeSelected(), platform->getHostMemAllocPool().isInitialized());
        EXPECT_EQ(DeviceFactory::isHwModeSelected(), mockContext->getDeviceMemAllocPool().isInitialized());

        mockContext.reset();
        NEO::cleanupPlatform(deviceFactory->rootDevices[0]->getExecutionEnvironment());
    }
}