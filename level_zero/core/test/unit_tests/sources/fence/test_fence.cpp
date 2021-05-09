/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_command_stream_receiver.h"

#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "test.h"

#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using FenceTest = Test<DeviceFixture>;
TEST_F(FenceTest, whenQueryingStatusThenCsrAllocationsAreDownloaded) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    Mock<CommandQueue> cmdQueue(device, csr.get());
    auto fence = Fence::create(&cmdQueue, nullptr);

    EXPECT_NE(nullptr, fence);

    auto &graphicsAllocation = fence->getAllocation();

    EXPECT_EQ(neoDevice->getRootDeviceIndex(), graphicsAllocation.getRootDeviceIndex());

    EXPECT_FALSE(csr->downloadAllocationsCalled);

    auto status = fence->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, status);
    EXPECT_TRUE(csr->downloadAllocationsCalled);

    status = fence->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
}

TEST_F(FenceTest, whenQueryingStatusWithoutCsrAndFenceUnsignaledThenReturnsNotReady) {
    Mock<CommandQueue> cmdQueue(device, nullptr);
    auto fence = Fence::create(&cmdQueue, nullptr);

    EXPECT_NE(nullptr, fence);
    auto status = fence->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, status);
    fence->destroy();
}

TEST_F(FenceTest, whenQueryingStatusAndStateSignaledThenReturnSuccess) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    Mock<CommandQueue> cmdQueue(device, csr.get());
    auto fence = Fence::create(&cmdQueue, nullptr);
    EXPECT_NE(nullptr, fence);

    auto &graphicsAllocation = fence->getAllocation();
    auto hostAddr = static_cast<uint64_t *>(graphicsAllocation.getUnderlyingBuffer());
    *hostAddr = Fence::STATE_SIGNALED;
    auto status = fence->queryStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
    fence->destroy();
}

using FenceSynchronizeTest = Test<DeviceFixture>;

TEST_F(FenceSynchronizeTest, givenCallToFenceHostSynchronizeWithTimeoutZeroAndStateInitialHostSynchronizeReturnsNotReady) {
    std::unique_ptr<MockCommandStreamReceiver> csr = nullptr;
    csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    Mock<CommandQueue> cmdQueue(device, csr.get());
    std::unique_ptr<L0::Fence> fence;
    fence = std::unique_ptr<L0::Fence>(L0::Fence::create(&cmdQueue, nullptr));
    EXPECT_NE(nullptr, fence);

    ze_result_t result = fence->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(FenceSynchronizeTest, givenCallToFenceHostSynchronizeWithNonZeroTimeoutAndStateInitialHostSynchronizeReturnsNotReady) {
    std::unique_ptr<MockCommandStreamReceiver> csr = nullptr;
    csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    Mock<CommandQueue> cmdQueue(device, csr.get());
    std::unique_ptr<L0::Fence> fence;
    fence = std::unique_ptr<L0::Fence>(L0::Fence::create(&cmdQueue, nullptr));
    EXPECT_NE(nullptr, fence);
    ze_result_t result = fence->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(FenceSynchronizeTest, givenCallToFenceHostSynchronizeWithTimeoutZeroAndStateSignaledHostSynchronizeReturnsSuccess) {
    std::unique_ptr<MockCommandStreamReceiver> csr = nullptr;
    csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    Mock<CommandQueue> cmdQueue(device, csr.get());
    std::unique_ptr<L0::Fence> fence;
    fence = std::unique_ptr<L0::Fence>(L0::Fence::create(&cmdQueue, nullptr));
    EXPECT_NE(nullptr, fence);
    auto alloc = &(fence->getAllocation());
    auto hostAddr = static_cast<uint64_t *>(alloc->getUnderlyingBuffer());
    *hostAddr = Fence::STATE_SIGNALED;
    ze_result_t result = fence->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(FenceSynchronizeTest, givenCallToFenceHostSynchronizeWithTimeoutNonZeroAndStateSignaledHostSynchronizeReturnsSuccess) {
    std::unique_ptr<MockCommandStreamReceiver> csr = nullptr;
    csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    Mock<CommandQueue> cmdQueue(device, csr.get());
    std::unique_ptr<L0::Fence> fence;
    fence = std::unique_ptr<L0::Fence>(L0::Fence::create(&cmdQueue, nullptr));
    EXPECT_NE(nullptr, fence);
    auto alloc = &(fence->getAllocation());
    auto hostAddr = static_cast<uint64_t *>(alloc->getUnderlyingBuffer());
    *hostAddr = Fence::STATE_SIGNALED;
    ze_result_t result = fence->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using FenceAubCsrTest = Test<DeviceFixture>;

HWTEST_F(FenceAubCsrTest, givenCallToFenceHostSynchronizeWithAubModeCsrReturnsSuccess) {
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];
    int32_t tag;
    auto aubCsr = new MockCsrAub<FamilyType>(tag, *neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    neoDevice->resetCommandStreamReceiver(aubCsr);

    Mock<CommandQueue> cmdQueue(device, aubCsr);
    auto fence = Fence::create(&cmdQueue, nullptr);

    ze_result_t result = fence->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    fence->destroy();
}

} // namespace ult
} // namespace L0
