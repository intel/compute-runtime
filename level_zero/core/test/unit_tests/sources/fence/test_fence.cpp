/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_fence.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>

using namespace std::chrono_literals;

namespace L0 {
namespace ult {

using FenceTest = Test<DeviceFixture>;
TEST_F(FenceTest, whenQueryingStatusThenCsrAllocationsAreDownloaded) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    *csr->tagAddress = 0;
    Mock<CommandQueue> cmdQueue(device, csr.get());
    auto fence = Fence::create(&cmdQueue, nullptr);

    EXPECT_NE(nullptr, fence);

    EXPECT_FALSE(csr->downloadAllocationsCalled);

    auto status = fence->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, status);
    EXPECT_TRUE(csr->downloadAllocationsCalled);

    status = fence->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
}

TEST_F(FenceTest, whenQueryingStatusWithoutCsrAndFenceUnsignaledThenReturnsNotReady) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    *csr->tagAddress = 0;
    Mock<CommandQueue> cmdQueue(device, csr.get());
    auto fence = Fence::create(&cmdQueue, nullptr);

    EXPECT_NE(nullptr, fence);
    auto status = fence->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, status);
    fence->destroy();
}

TEST_F(FenceTest, GivenGpuHangWhenHostSynchronizeIsCalledThenDeviceLostIsReturned) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = true;
    csr->testTaskCountReadyReturnValue = false;

    Mock<CommandQueue> cmdqueue(device, csr.get());
    ze_fence_desc_t desc;

    std::unique_ptr<WhiteBox<L0::Fence>> fence;
    fence.reset(whitebox_cast(Fence::create(&cmdqueue, &desc)));
    ASSERT_NE(nullptr, fence);

    fence->taskCount = 1;
    fence->gpuHangCheckPeriod = 0ms;

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    auto result = fence->hostSynchronize(timeout);

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

TEST_F(FenceTest, GivenNoGpuHangAndOneNanosecondTimeoutWhenHostSynchronizeIsCalledThenResultNotReadyIsReturnedDueToTimeout) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = false;
    csr->testTaskCountReadyReturnValue = false;

    Mock<CommandQueue> cmdqueue(device, csr.get());
    ze_fence_desc_t desc;

    std::unique_ptr<WhiteBox<L0::Fence>> fence;
    fence.reset(whitebox_cast(Fence::create(&cmdqueue, &desc)));
    ASSERT_NE(nullptr, fence);

    fence->taskCount = 1;
    fence->gpuHangCheckPeriod = 0ms;

    constexpr uint64_t timeoutNanoseconds = 1;
    auto result = fence->hostSynchronize(timeoutNanoseconds);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(FenceTest, GivenLongPeriodOfGpuCheckAndOneNanosecondTimeoutWhenHostSynchronizeIsCalledThenResultNotReadyIsReturnedDueToTimeout) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->testTaskCountReadyReturnValue = false;

    Mock<CommandQueue> cmdqueue(device, csr.get());
    ze_fence_desc_t desc;

    std::unique_ptr<WhiteBox<L0::Fence>> fence;
    fence.reset(whitebox_cast(Fence::create(&cmdqueue, &desc)));
    ASSERT_NE(nullptr, fence);

    fence->taskCount = 1;
    fence->gpuHangCheckPeriod = 50000000ms;

    constexpr uint64_t timeoutNanoseconds = 1;
    auto result = fence->hostSynchronize(timeoutNanoseconds);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(FenceTest, GivenSuccessfulQueryResultAndNoTimeoutWhenHostSynchronizeIsCalledThenResultSuccessIsReturned) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->testTaskCountReadyReturnValue = true;

    Mock<CommandQueue> cmdqueue(device, csr.get());
    ze_fence_desc_t desc;

    std::unique_ptr<WhiteBox<L0::Fence>> fence;
    fence.reset(whitebox_cast(Fence::create(&cmdqueue, &desc)));
    ASSERT_NE(nullptr, fence);

    fence->taskCount = 1;

    constexpr uint64_t timeout = std::numeric_limits<std::uint32_t>::max();
    auto result = fence->hostSynchronize(timeout);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using FenceSynchronizeTest = Test<DeviceFixture>;

TEST_F(FenceSynchronizeTest, givenCallToFenceHostSynchronizeWithTimeoutZeroAndStateInitialThenHostSynchronizeReturnsNotReady) {
    std::unique_ptr<MockCommandStreamReceiver> csr = nullptr;
    csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    Mock<CommandQueue> cmdQueue(device, csr.get());
    std::unique_ptr<L0::Fence> fence;
    fence = std::unique_ptr<L0::Fence>(L0::Fence::create(&cmdQueue, nullptr));
    EXPECT_NE(nullptr, fence);
    *csr->tagAddress = 0;
    ze_result_t result = fence->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(FenceSynchronizeTest, givenCallToFenceHostSynchronizeWithNonZeroTimeoutAndStateInitialThenHostSynchronizeReturnsNotReady) {
    std::unique_ptr<MockCommandStreamReceiver> csr = nullptr;
    csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    Mock<CommandQueue> cmdQueue(device, csr.get());
    std::unique_ptr<L0::Fence> fence;
    fence = std::unique_ptr<L0::Fence>(L0::Fence::create(&cmdQueue, nullptr));
    EXPECT_NE(nullptr, fence);
    *csr->tagAddress = 0;
    ze_result_t result = fence->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(FenceSynchronizeTest, givenCallToFenceHostSynchronizeWithTimeoutZeroAndTaskCountEqualsTagAllocationThenHostSynchronizeReturnsSuccess) {
    std::unique_ptr<MockCommandStreamReceiver> csr = nullptr;
    csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    Mock<CommandQueue> cmdQueue(device, csr.get());
    auto fence = std::unique_ptr<Fence>(whitebox_cast(Fence::create(&cmdQueue, nullptr)));
    EXPECT_NE(nullptr, fence);

    fence->taskCount = 1;
    *csr->tagAddress = 1;
    ze_result_t result = fence->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(FenceSynchronizeTest, givenCallToFenceHostSynchronizeWithTimeoutNonZeroAndTaskCountEqualsTagAllocationThenHostSynchronizeReturnsSuccess) {
    std::unique_ptr<MockCommandStreamReceiver> csr = nullptr;
    csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    Mock<CommandQueue> cmdQueue(device, csr.get());
    auto fence = std::unique_ptr<Fence>(whitebox_cast(Fence::create(&cmdQueue, nullptr)));
    EXPECT_NE(nullptr, fence);

    fence->taskCount = 1;
    *csr->tagAddress = 1;
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
    int32_t tag = 1;
    auto aubCsr = new MockCsrAub<FamilyType>(tag, *neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    neoDevice->resetCommandStreamReceiver(aubCsr);

    Mock<CommandQueue> cmdQueue(device, aubCsr);
    auto fence = std::unique_ptr<L0::Fence>(Fence::create(&cmdQueue, nullptr));

    ze_result_t result = fence->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
