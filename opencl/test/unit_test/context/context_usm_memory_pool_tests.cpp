/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"
using namespace NEO;

template <int devicePoolFlag, int hostPoolFlag>
struct ContextUsmPoolFlagValuesTest : public ::testing::Test {

    ContextUsmPoolFlagValuesTest() {}

    void SetUp() override {
        mockContext = std::make_unique<MockContext>();
        const ClDeviceInfo &devInfo = mockContext->getDevice(0u)->getDeviceInfo();
        if (devInfo.svmCapabilities == 0) {
            GTEST_SKIP();
        }
        mockDeviceUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&mockContext->getDeviceMemAllocPool());
        mockHostUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&mockContext->getHostMemAllocPool());
        debugManager.flags.EnableDeviceUsmAllocationPool.set(devicePoolFlag);
        debugManager.flags.EnableHostUsmAllocationPool.set(hostPoolFlag);
        mockContext->initializeUsmAllocationPools();
    }

    std::unique_ptr<MockContext> mockContext;
    DebugManagerStateRestore restorer;
    MockUsmMemAllocPool *mockDeviceUsmMemAllocPool;
    MockUsmMemAllocPool *mockHostUsmMemAllocPool;
};

using ContextUsmPoolDefaultFlagsTest = ContextUsmPoolFlagValuesTest<-1, -1>;

TEST_F(ContextUsmPoolDefaultFlagsTest, givenDefaultDebugFlagsWhenCreatingContextThenPoolsAreNotInitialized) {
    EXPECT_FALSE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockDeviceUsmMemAllocPool->poolSize);
    EXPECT_EQ(nullptr, mockDeviceUsmMemAllocPool->pool);

    EXPECT_FALSE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockHostUsmMemAllocPool->poolSize);
    EXPECT_EQ(nullptr, mockHostUsmMemAllocPool->pool);
}

using ContextUsmPoolEnabledFlagsTest = ContextUsmPoolFlagValuesTest<1, 1>;
TEST_F(ContextUsmPoolEnabledFlagsTest, givenEnabledDebugFlagsWhenCreatingContextThenPoolsAreInitialized) {
    EXPECT_TRUE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_EQ(1 * MemoryConstants::megaByte, mockDeviceUsmMemAllocPool->poolSize);
    EXPECT_NE(nullptr, mockDeviceUsmMemAllocPool->pool);
    EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, mockDeviceUsmMemAllocPool->poolMemoryType);

    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(1 * MemoryConstants::megaByte, mockHostUsmMemAllocPool->poolSize);
    EXPECT_NE(nullptr, mockHostUsmMemAllocPool->pool);
    EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, mockHostUsmMemAllocPool->poolMemoryType);

    cl_int retVal = CL_SUCCESS;
    void *pooledDeviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, UsmMemAllocPool::allocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledDeviceAlloc);
    clMemFreeINTEL(mockContext.get(), pooledDeviceAlloc);

    void *pooledHostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, UsmMemAllocPool::allocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledHostAlloc);
    clMemFreeINTEL(mockContext.get(), pooledHostAlloc);
}