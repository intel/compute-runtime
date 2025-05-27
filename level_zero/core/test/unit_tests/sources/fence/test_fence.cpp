/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_fence.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>

using namespace std::chrono_literals;

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
extern volatile TagAddressType *pauseAddress;
extern TaskCountType pauseValue;
extern uint32_t pauseOffset;
extern std::function<void()> setupPauseAddress;
} // namespace CpuIntrinsicsTests

namespace L0 {
namespace ult {

using FenceTest = Test<DeviceFixture>;
TEST_F(FenceTest, whenQueryingStatusThenCsrAllocationsAreDownloaded) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    *csr->tagAddress = 0;
    Mock<CommandQueue> cmdQueue(device, csr.get());
    ze_fence_desc_t fenceDesc = {};
    auto fence = Fence::create(&cmdQueue, &fenceDesc);

    EXPECT_NE(nullptr, fence);

    EXPECT_EQ(0u, csr->downloadAllocationsCalledCount);

    auto status = fence->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, status);
    EXPECT_EQ(1u, csr->downloadAllocationsCalledCount);

    status = fence->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
}

TEST_F(FenceTest, givenFenceSignalFlagUsedWhenQueryingFenceAfterCreationThenReturnReadyStatus) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    *csr->tagAddress = 0;
    Mock<CommandQueue> cmdQueue(device, csr.get());

    ze_fence_desc_t fenceDesc = {};
    fenceDesc.stype = ZE_STRUCTURE_TYPE_FENCE_DESC;
    fenceDesc.pNext = nullptr;
    fenceDesc.flags = ZE_FENCE_FLAG_SIGNALED;

    auto fence = Fence::create(&cmdQueue, &fenceDesc);
    EXPECT_NE(nullptr, fence);

    EXPECT_EQ(0u, csr->downloadAllocationsCalledCount);

    auto status = fence->queryStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);

    status = fence->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
}

TEST_F(FenceTest, whenQueryingStatusWithoutCsrAndFenceUnsignaledThenReturnsNotReady) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    *csr->tagAddress = 0;
    Mock<CommandQueue> cmdQueue(device, csr.get());
    ze_fence_desc_t fenceDesc = {};
    auto fence = Fence::create(&cmdQueue, &fenceDesc);

    EXPECT_NE(nullptr, fence);
    auto status = fence->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, status);
    fence->destroy();
}

TEST_F(FenceTest, GivenGpuHangWhenHostSynchronizeIsCalledThenDeviceLostIsReturned) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = true;
    csr->testTaskCountReadyReturnValue = false;
    csr->gpuHangCheckPeriod = {};

    Mock<CommandQueue> cmdqueue(device, csr.get());
    ze_fence_desc_t desc = {};

    std::unique_ptr<WhiteBox<L0::Fence>> fence;
    fence.reset(whiteboxCast(Fence::create(&cmdqueue, &desc)));
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
    ze_fence_desc_t desc = {};

    std::unique_ptr<WhiteBox<L0::Fence>> fence;
    fence.reset(whiteboxCast(Fence::create(&cmdqueue, &desc)));
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
    ze_fence_desc_t desc = {};

    std::unique_ptr<WhiteBox<L0::Fence>> fence;
    fence.reset(whiteboxCast(Fence::create(&cmdqueue, &desc)));
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
    ze_fence_desc_t desc = {};

    std::unique_ptr<WhiteBox<L0::Fence>> fence;
    fence.reset(whiteboxCast(Fence::create(&cmdqueue, &desc)));
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
    ze_fence_desc_t fenceDesc = {};
    fence = std::unique_ptr<L0::Fence>(L0::Fence::create(&cmdQueue, &fenceDesc));
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
    ze_fence_desc_t fenceDesc = {};
    fence = std::unique_ptr<L0::Fence>(L0::Fence::create(&cmdQueue, &fenceDesc));
    EXPECT_NE(nullptr, fence);
    *csr->tagAddress = 0;
    ze_result_t result = fence->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(FenceSynchronizeTest, givenCallToFenceHostSynchronizeWithTimeoutZeroAndTaskCountEqualsTagAllocationThenHostSynchronizeReturnsSuccess) {
    std::unique_ptr<MockCommandStreamReceiver> csr = nullptr;
    csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    Mock<CommandQueue> cmdQueue(device, csr.get());
    ze_fence_desc_t fenceDesc = {};
    auto fence = std::unique_ptr<Fence>(whiteboxCast(Fence::create(&cmdQueue, &fenceDesc)));
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
    ze_fence_desc_t fenceDesc = {};
    auto fence = std::unique_ptr<Fence>(whiteboxCast(Fence::create(&cmdQueue, &fenceDesc)));
    EXPECT_NE(nullptr, fence);

    fence->taskCount = 1;
    *csr->tagAddress = 1;
    ze_result_t result = fence->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(FenceSynchronizeTest, givenInfiniteTimeoutWhenWaitingForFenceCompletionThenReturnOnlyAfterAllCsrPartitionsCompleted) {
    constexpr uint32_t activePartitions = 2;
    constexpr uint32_t postSyncOffset = 16;

    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
    VariableBackup<uint32_t> backupWaitCount(&NEO::WaitUtils::waitCount, 1);

    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    ASSERT_NE(nullptr, csr->getTagAddress());
    csr->immWritePostSyncWriteOffset = postSyncOffset;
    csr->activePartitions = activePartitions;

    Mock<CommandQueue> cmdqueue(device, csr.get());
    ze_fence_desc_t desc = {};

    std::unique_ptr<WhiteBox<L0::Fence>> fence;
    fence.reset(whiteboxCast(Fence::create(&cmdqueue, &desc)));
    ASSERT_NE(nullptr, fence);

    fence->taskCount = 1;

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, 0);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = csr->getTagAddress();

    volatile TagAddressType *hostAddr = csr->getTagAddress();
    for (uint32_t i = 0; i < activePartitions; i++) {
        *hostAddr = 0;
        hostAddr = ptrOffset(hostAddr, postSyncOffset);
    }

    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        if (CpuIntrinsicsTests::pauseCounter > 10) {
            volatile TagAddressType *nextPacket = CpuIntrinsicsTests::pauseAddress;
            for (uint32_t i = 0; i < activePartitions; i++) {
                *nextPacket = 1;
                nextPacket = ptrOffset(nextPacket, postSyncOffset);
            }
        }
    };

    constexpr uint64_t infiniteTimeout = std::numeric_limits<std::uint64_t>::max();
    auto result = fence->hostSynchronize(infiniteTimeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using FenceAubCsrTest = Test<DeviceFixture>;

HWTEST_F(FenceAubCsrTest, givenCallToFenceHostSynchronizeWithAubModeCsrReturnsSuccess) {
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;

    EnvironmentWithCsrWrapper environment;
    environment.setCsrType<MockCsrAub<FamilyType>>();

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];
    auto aubCsr = static_cast<MockCsrAub<FamilyType> *>(&neoDevice->getUltCommandStreamReceiver<FamilyType>());

    Mock<CommandQueue> cmdQueue(device, aubCsr);
    ze_fence_desc_t fenceDesc = {};
    auto fence = std::unique_ptr<L0::Fence>(Fence::create(&cmdQueue, &fenceDesc));

    ze_result_t result = fence->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeFenceDestroy, WhenDestroyingFenceThenSuccessIsReturned) {
    Mock<Fence> fence;

    auto result = zeFenceDestroy(fence.toHandle());
    EXPECT_EQ(1u, fence.destroyCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeFenceHostSynchronize, WhenSynchronizingFenceThenSuccessIsReturned) {
    Mock<Fence> fence;

    auto result = zeFenceHostSynchronize(fence.toHandle(), std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(1u, fence.hostSynchronizeCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(FenceTest, givenFenceNotEnqueuedThenStatusIsNotReady) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    *csr->tagAddress = 0;
    Mock<CommandQueue> cmdqueue(device, csr.get());
    ze_fence_desc_t desc = {};

    auto fence = whiteboxCast(Fence::create(&cmdqueue, &desc));
    ASSERT_NE(fence, nullptr);
    EXPECT_EQ(fence->queryStatus(), ZE_RESULT_NOT_READY);

    delete fence;
}

TEST_F(FenceTest, givenFenceWhenResettingThenTaskCountIsReset) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    Mock<CommandQueue> cmdqueue(device, csr.get());
    ze_fence_desc_t desc = {};

    auto fence = whiteboxCast(Fence::create(&cmdqueue, &desc));
    ASSERT_NE(fence, nullptr);
    EXPECT_EQ(NEO::CompletionStamp::notReady, fence->taskCount);

    fence->taskCount = 1;
    auto result = fence->reset(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(NEO::CompletionStamp::notReady, fence->taskCount);

    delete fence;
}

HWTEST_F(FenceTest, givenPrintfKernelWhenSynchronizingFenceThenPrintPrintfOutputIsCalled) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    std::shared_ptr<Mock<KernelImp>> kernel{new Mock<KernelImp>{}};
    TaskCountType currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.returnWaitForCompletionWithTimeout = WaitStatus::ready;
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;
    *csr.tagAddress = currentTaskCount;
    commandQueue->printfKernelContainer.push_back(std::weak_ptr<Kernel>{kernel});

    ze_fence_desc_t fenceDesc = {ZE_STRUCTURE_TYPE_FENCE_DESC,
                                 nullptr,
                                 0};
    auto fence = whiteboxCast(Fence::create(commandQueue, &fenceDesc));
    ASSERT_NE(fence, nullptr);
    fence->taskCount = currentTaskCount;
    ze_result_t result = fence->hostSynchronize(1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, commandQueue->printfKernelContainer.size());
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_FALSE(kernel->hangDetectedPassedToPrintfOutput);

    delete fence;

    commandQueue->destroy();
}

HWTEST_F(FenceTest, givenPrintfKernelAndDetectedHangWhenSynchronizingFenceThenPrintPrintfOutputAfterHangIsCalled) {
    auto driverModel = new NEO::MockDriverModel();
    driverModel->isGpuHangDetectedToReturn = true;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(driverModel));

    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    std::shared_ptr<Mock<KernelImp>> kernel{new Mock<KernelImp>{}};
    TaskCountType currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;
    csr.returnWaitForCompletionWithTimeout = WaitStatus::gpuHang;
    *csr.tagAddress = 0;
    csr.gpuHangCheckPeriod = 0us;
    commandQueue->printfKernelContainer.push_back(std::weak_ptr<Kernel>{kernel});

    ze_fence_desc_t fenceDesc = {ZE_STRUCTURE_TYPE_FENCE_DESC,
                                 nullptr,
                                 0};
    auto fence = whiteboxCast(Fence::create(commandQueue, &fenceDesc));
    ASSERT_NE(fence, nullptr);
    fence->taskCount = 1;
    ze_result_t result = fence->hostSynchronize(1);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);

    EXPECT_EQ(0u, commandQueue->printfKernelContainer.size());
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_TRUE(kernel->hangDetectedPassedToPrintfOutput);

    delete fence;
    commandQueue->destroy();
}

HWTEST_F(FenceTest, givenPrintfKernelNotCompletedWhenSynchronizingFenceWithZeroTimeoutThenPrintfOutputIsNotFlushed) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    std::shared_ptr<Mock<KernelImp>> kernel{new Mock<KernelImp>{}};
    TaskCountType currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.returnWaitForCompletionWithTimeout = WaitStatus::ready;
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;
    *csr.tagAddress = currentTaskCount - 1;

    commandQueue->printfKernelContainer.push_back(std::weak_ptr<Kernel>{kernel});

    ze_fence_desc_t fenceDesc = {ZE_STRUCTURE_TYPE_FENCE_DESC,
                                 nullptr,
                                 0};
    auto fence = whiteboxCast(Fence::create(commandQueue, &fenceDesc));
    ASSERT_NE(fence, nullptr);
    fence->taskCount = currentTaskCount;
    ze_result_t result = fence->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    EXPECT_EQ(1u, commandQueue->printfKernelContainer.size());
    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);

    delete fence;
    commandQueue->destroy();
}
} // namespace ult
} // namespace L0
