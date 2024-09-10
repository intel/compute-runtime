/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"
using namespace NEO;

template <int devicePoolFlag, int hostPoolFlag>
struct ContextUsmPoolFlagValuesTest : public ::testing::Test {

    ContextUsmPoolFlagValuesTest() {}

    void SetUp() override {
        mockContext = std::make_unique<MockContext>();
        mockContext->usmPoolInitialized = false;
        const ClDeviceInfo &devInfo = mockContext->getDevice(0u)->getDeviceInfo();
        if (devInfo.svmCapabilities == 0) {
            GTEST_SKIP();
        }
        mockDeviceUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&mockContext->getDeviceMemAllocPool());
        mockHostUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&mockContext->getHostMemAllocPool());
        debugManager.flags.EnableDeviceUsmAllocationPool.set(devicePoolFlag);
        debugManager.flags.EnableHostUsmAllocationPool.set(hostPoolFlag);
    }

    std::unique_ptr<MockContext> mockContext;
    DebugManagerStateRestore restorer;
    MockUsmMemAllocPool *mockDeviceUsmMemAllocPool;
    MockUsmMemAllocPool *mockHostUsmMemAllocPool;
    constexpr static auto poolAllocationThreshold = 1 * MemoryConstants::megaByte;
};

using ContextUsmPoolDefaultFlagsTest = ContextUsmPoolFlagValuesTest<-1, -1>;

HWTEST2_F(ContextUsmPoolDefaultFlagsTest, givenDefaultDebugFlagsWhenCreatingContextThenPoolsAreNotInitialized, IsBeforeXeHpgCore) {
    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockDeviceUsmMemAllocPool->poolSize);
    EXPECT_EQ(nullptr, mockDeviceUsmMemAllocPool->pool);

    EXPECT_FALSE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockHostUsmMemAllocPool->poolSize);
    EXPECT_EQ(nullptr, mockHostUsmMemAllocPool->pool);
}

HWTEST2_F(ContextUsmPoolDefaultFlagsTest, givenDefaultDebugFlagsWhenCreatingContextThenPoolsAreNotInitialized, IsXeHpgCore) {
    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_FALSE(mockHostUsmMemAllocPool->isInitialized());
}

HWTEST2_F(ContextUsmPoolDefaultFlagsTest, givenDefaultDebugFlagsWhenCreatingContextThenPoolsAreNotInitialized, IsXeHpcCore) {
    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockDeviceUsmMemAllocPool->poolSize);
    EXPECT_EQ(nullptr, mockDeviceUsmMemAllocPool->pool);

    EXPECT_FALSE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockHostUsmMemAllocPool->poolSize);
    EXPECT_EQ(nullptr, mockHostUsmMemAllocPool->pool);
}

using ContextUsmPoolEnabledFlagsTest = ContextUsmPoolFlagValuesTest<1, 3>;
TEST_F(ContextUsmPoolEnabledFlagsTest, givenEnabledDebugFlagsWhenCreatingAllocaitonsThenPoolsAreInitialized) {
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

using ContextUsmPoolEnabledFlagsTestDefault = ContextUsmPoolFlagValuesTest<-1, -1>;
TEST_F(ContextUsmPoolEnabledFlagsTestDefault, givenDefaultDebugSettingsThenPoolIsInitializedWhenPlatformSupportIt) {
    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_FALSE(mockHostUsmMemAllocPool->isInitialized());

    cl_int retVal = CL_SUCCESS;
    void *pooledDeviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledDeviceAlloc);
    clMemFreeINTEL(mockContext.get(), pooledDeviceAlloc);

    void *pooledHostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledHostAlloc);
    clMemFreeINTEL(mockContext.get(), pooledHostAlloc);

    auto &productHelper = mockContext->getDevice(0u)->getProductHelper();
    bool enabledDevice = ApiSpecificConfig::isDeviceUsmPoolingEnabled() && productHelper.isUsmPoolAllocatorSupported();
    bool enabledHost = ApiSpecificConfig::isHostUsmPoolingEnabled() && productHelper.isUsmPoolAllocatorSupported();

    EXPECT_EQ(enabledDevice, mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_EQ(enabledHost, mockHostUsmMemAllocPool->isInitialized());
}