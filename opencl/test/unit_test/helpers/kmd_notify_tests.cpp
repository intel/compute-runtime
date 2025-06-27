/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_kmd_notify_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

namespace NEO {
class ExecutionEnvironment;
} // namespace NEO

using namespace NEO;

struct KmdNotifyTests : public ::testing::Test {
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
        cmdQ.reset(new MockCommandQueue(&context, device.get(), nullptr, false));
        *device->getDefaultEngine().commandStreamReceiver->getTagAddress() = taskCountToWait;
        cmdQ->getGpgpuCommandStreamReceiver().waitForFlushStamp(flushStampToWait);
        overrideKmdNotifyParams(true, 2, true, 1, false, 0, false, 0);
    }

    void overrideKmdNotifyParams(bool kmdNotifyEnable, int64_t kmdNotifyDelay,
                                 bool quickKmdSleepEnable, int64_t quickKmdSleepDelay,
                                 bool quickKmdSleepEnableForSporadicWaits, int64_t quickKmdSleepDelayForSporadicWaits,
                                 bool quickKmdSleepEnableForDirectSubmission, int64_t quickKmdSleepDelayForDirectSubmission) {
        auto &properties = hwInfo->capabilityTable.kmdNotifyProperties;
        properties.enableKmdNotify = kmdNotifyEnable;
        properties.delayKmdNotifyMicroseconds = kmdNotifyDelay;
        properties.enableQuickKmdSleep = quickKmdSleepEnable;
        properties.delayQuickKmdSleepMicroseconds = quickKmdSleepDelay;
        properties.enableQuickKmdSleepForSporadicWaits = quickKmdSleepEnableForSporadicWaits;
        properties.delayQuickKmdSleepForSporadicWaitsMicroseconds = quickKmdSleepDelayForSporadicWaits;
        properties.enableQuickKmdSleepForDirectSubmission = quickKmdSleepEnableForDirectSubmission;
        properties.delayQuickKmdSleepForDirectSubmissionMicroseconds = quickKmdSleepDelayForDirectSubmission;
    }

    template <typename Family>
    class MockKmdNotifyCsr : public UltCommandStreamReceiver<Family> {
      public:
        MockKmdNotifyCsr(const ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
            : UltCommandStreamReceiver<Family>(const_cast<ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {}
        MockKmdNotifyCsr(const ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
            : UltCommandStreamReceiver<Family>(const_cast<ExecutionEnvironment &>(executionEnvironment), rootDeviceIndex, deviceBitfield) {}

        bool waitForFlushStamp(FlushStamp &flushStampToWait) override {
            waitForFlushStampCalled++;
            waitForFlushStampParamsPassed.push_back({flushStampToWait});
            return waitForFlushStampResult;
        }

        struct WaitForFlushStampParams {
            FlushStamp flushStampToWait{};
        };

        uint32_t waitForFlushStampCalled = 0u;
        bool waitForFlushStampResult = true;
        StackVec<WaitForFlushStampParams, 1> waitForFlushStampParamsPassed{};

        WaitStatus waitForCompletionWithTimeout(const WaitParams &params, TaskCountType taskCountToWait) override {
            waitForCompletionWithTimeoutCalled++;
            waitForCompletionWithTimeoutParamsPassed.push_back({params.enableTimeout, params.waitTimeout, taskCountToWait});
            return waitForCompletionWithTimeoutResult;
        }

        struct WaitForCompletionWithTimeoutParams {
            bool enableTimeout{};
            int64_t timeoutMs{};
            TaskCountType taskCountToWait{};
        };

        uint32_t waitForCompletionWithTimeoutCalled = 0u;
        WaitStatus waitForCompletionWithTimeoutResult = WaitStatus::ready;
        StackVec<WaitForCompletionWithTimeoutParams, 2> waitForCompletionWithTimeoutParamsPassed{};
    };

    MockKmdNotifyHelper *mockKmdNotifyHelper = nullptr;
    HardwareInfo *hwInfo = nullptr;
    MockContext context;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockCommandQueue> cmdQ;
    FlushStamp flushStampToWait = 1000;
    TaskCountType taskCountToWait = 5;
};

struct KmdNotifyTestsWithMockKmdNotifyCsr : public KmdNotifyTests {
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<MockKmdNotifyCsr<FamilyType>>();
        KmdNotifyTests::SetUp();

        auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
        csr->waitForFlushStampCalled = 0;
    }

    template <typename FamilyType>
    void tearDownT() {
        KmdNotifyTests::TearDown();
    }
};

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenTaskCountWhenWaitUntilCompletionCalledThenAlwaysTryCpuPolling) {
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(2, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenTaskCountAndKmdNotifyDisabledWhenWaitUntilCompletionCalledThenTryCpuPollingWithoutTimeout) {
    overrideKmdNotifyParams(false, 0, false, 0, false, 0, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, false);
    EXPECT_EQ(0u, csr->waitForFlushStampCalled);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(false, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(0, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenNotReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndKmdWait) {
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    *csr->getTagAddress() = taskCountToWait - 1;

    csr->waitForCompletionWithTimeoutResult = WaitStatus::notReady;

    // we have unrecoverable for this case, this will throw.
    EXPECT_THROW(cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, false), std::exception);
    EXPECT_EQ(1u, csr->waitForFlushStampCalled);
    EXPECT_EQ(flushStampToWait, csr->waitForFlushStampParamsPassed[0].flushStampToWait);

    EXPECT_EQ(2u, csr->waitForCompletionWithTimeoutCalled);

    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(2, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);

    EXPECT_EQ(false, csr->waitForCompletionWithTimeoutParamsPassed[1].enableTimeout);
    EXPECT_EQ(0, csr->waitForCompletionWithTimeoutParamsPassed[1].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[1].taskCountToWait);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndDontCallKmdWait) {
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, false);
    EXPECT_EQ(0u, csr->waitForFlushStampCalled);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(2, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenDefaultArgumentWhenWaitUntilCompleteIsCalledThenDisableQuickKmdSleep) {
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(expectedTimeout, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenEnabledQuickSleepWhenWaitUntilCompleteIsCalledThenChangeDelayValue) {
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, true);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(expectedTimeout, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenDisabledQuickSleepWhenWaitUntilCompleteWithQuickSleepRequestIsCalledThenUseBaseDelayValue) {
    overrideKmdNotifyParams(true, 1, false, 0, false, 0, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr->resetKmdNotifyHelper(mockKmdNotifyHelper);
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, true);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(expectedTimeout, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_F(KmdNotifyTests, givenNotReadyTaskCountWhenPollForCompletionCalledThenTimeout) {
    *device->getDefaultEngine().commandStreamReceiver->getTagAddress() = taskCountToWait - 1;
    auto success = device->getUltCommandStreamReceiver<FamilyType>().waitForCompletionWithTimeout(true, 1, taskCountToWait);
    EXPECT_NE(NEO::WaitStatus::ready, success);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenZeroFlushStampWhenWaitIsCalledThenDisableTimeout) {
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());

    EXPECT_TRUE(device->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 0, false, QueueThrottle::MEDIUM);
    EXPECT_EQ(0u, csr->waitForFlushStampCalled);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(false, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenNonQuickSleepRequestWhenItsSporadicWaitThenOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 1, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;

    int64_t timeSinceLastWait = mockKmdNotifyHelper->properties->delayQuickKmdSleepForSporadicWaitsMicroseconds + 1;

    mockKmdNotifyHelper->lastWaitForCompletionTimestampUs = mockKmdNotifyHelper->getMicrosecondsSinceEpoch() - timeSinceLastWait;
    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, QueueThrottle::MEDIUM);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(expectedDelay, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenNonQuickSleepRequestWhenItsNotSporadicWaitThenOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 9999999, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, QueueThrottle::MEDIUM);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(expectedDelay, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenKmdNotifyDisabledWhenPowerSavingModeIsRequestedThenTimeoutIsEnabled) {
    overrideKmdNotifyParams(false, 3, false, 2, false, 9999999, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, QueueThrottle::LOW);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(1, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenKmdNotifyDisabledWhenQueueHasPowerSavingModeAndCallWaitThenTimeoutIsEnabled) {
    overrideKmdNotifyParams(false, 3, false, 2, false, 9999999, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

    cmdQ->throttle = QueueThrottle::LOW;

    cmdQ->waitUntilComplete(1, {}, 1, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(1, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenKmdNotifyDisabledWhenQueueHasPowerSavingModButThereIsNoFlushStampAndCallWaitThenTimeoutIsDisabled) {
    overrideKmdNotifyParams(false, 3, false, 2, false, 9999999, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

    cmdQ->throttle = QueueThrottle::LOW;

    cmdQ->waitUntilComplete(1, {}, 0, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(false, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(0, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenKmdNotifyDisabledWhenQueueHasPowerSavingModAndThereIsNoFlushStampButKmdWaitOnTaskCountAllowedAndCallWaitThenTimeoutIsEnabled) {
    overrideKmdNotifyParams(false, 3, false, 2, false, 9999999, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    csr->isKmdWaitOnTaskCountAllowedValue = true;

    cmdQ->throttle = QueueThrottle::LOW;

    cmdQ->waitUntilComplete(1, {}, 0, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(1, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenQuickSleepRequestWhenItsSporadicWaitOptimizationIsDisabledThenDontOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, false, 0, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, true, QueueThrottle::MEDIUM);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(expectedDelay, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenTaskCountEqualToHwTagWhenWaitCalledThenDontMultiplyTimeout) {
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    *csr->getTagAddress() = taskCountToWait;

    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, QueueThrottle::MEDIUM);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(expectedTimeout, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenTaskCountLowerThanHwTagWhenWaitCalledThenDontMultiplyTimeout) {
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    *csr->getTagAddress() = taskCountToWait + 5;

    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, QueueThrottle::MEDIUM);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(expectedTimeout, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenDefaultCommandStreamReceiverWhenWaitCalledThenUpdateWaitTimestamp) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 1, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

    EXPECT_NE(0, mockKmdNotifyHelper->lastWaitForCompletionTimestampUs.load());

    EXPECT_EQ(1u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
    csr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);
    EXPECT_EQ(2u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenDefaultCommandStreamReceiverWithDisabledSporadicWaitOptimizationWhenWaitCalledThenDontUpdateWaitTimestamp) {
    overrideKmdNotifyParams(true, 3, true, 2, false, 0, false, 0);
    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

    EXPECT_EQ(0, mockKmdNotifyHelper->lastWaitForCompletionTimestampUs.load());

    csr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);
    EXPECT_EQ(0u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
}

HWTEST_TEMPLATED_F(KmdNotifyTestsWithMockKmdNotifyCsr, givenNewHelperWhenItsSetToCsrThenUpdateAcLineStatus) {
    auto helper = new MockKmdNotifyHelper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    EXPECT_EQ(0u, helper->updateAcLineStatusCalled);

    auto csr = static_cast<MockKmdNotifyCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());
    csr->resetKmdNotifyHelper(helper);
    EXPECT_EQ(1u, helper->updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenTaskCountDiffLowerThanMinimumToCheckAcLineWhenObtainingTimeoutPropertiesThenDontCheck) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    uint32_t hwTag = 9;
    TaskCountType taskCountToWait = 10;
    EXPECT_TRUE(taskCountToWait - hwTag < KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);
    EXPECT_EQ(10u, KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);

    helper.obtainTimeoutParams(false, hwTag, taskCountToWait, 1, QueueThrottle::MEDIUM, true, false);

    EXPECT_EQ(0u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenTaskCountDiffGreaterThanMinimumToCheckAcLineAndDisabledKmdNotifyWhenObtainingTimeoutPropertiesThenCheck) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    uint32_t hwTag = 10;
    TaskCountType taskCountToWait = 21;
    EXPECT_TRUE(taskCountToWait - hwTag > KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);
    EXPECT_EQ(10u, KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);

    helper.obtainTimeoutParams(false, hwTag, taskCountToWait, 1, QueueThrottle::MEDIUM, true, false);

    EXPECT_EQ(1u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenKmdWaitModeNotActiveWhenObtainTimeoutParamsThenFalseIsReturned) {
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    auto params = helper.obtainTimeoutParams(false, 1, 1, 1, QueueThrottle::MEDIUM, false, false);

    EXPECT_FALSE(params.enableTimeout);
    EXPECT_FALSE(params.waitTimeout);
}

TEST_F(KmdNotifyTests, givenHighThrottleWhenObtainTimeoutParamsThenIndefinitelyPollSetToTrue) {
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    auto params = helper.obtainTimeoutParams(false, 1, 1, 1, QueueThrottle::HIGH, false, false);

    EXPECT_FALSE(params.enableTimeout);
    EXPECT_FALSE(params.waitTimeout);
    EXPECT_TRUE(params.indefinitelyPoll);
}

TEST_F(KmdNotifyTests, givenTaskCountDiffGreaterThanMinimumToCheckAcLineAndEnabledKmdNotifyWhenObtainingTimeoutPropertiesThenDontCheck) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    uint32_t hwTag = 10;
    TaskCountType taskCountToWait = 21;
    EXPECT_TRUE(taskCountToWait - hwTag > KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);
    EXPECT_EQ(10u, KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);

    helper.obtainTimeoutParams(false, hwTag, taskCountToWait, 1, QueueThrottle::MEDIUM, true, false);

    EXPECT_EQ(0u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismWhenAcLineIsDisconnectedThenForceEnableTimeout) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    auto params = helper.obtainTimeoutParams(false, 1, 2, 2, QueueThrottle::MEDIUM, true, false);

    EXPECT_TRUE(params.enableTimeout);
    EXPECT_EQ(KmdNotifyConstants::timeoutInMicrosecondsForDisconnectedAcLine, params.waitTimeout);
    EXPECT_EQ(10000, KmdNotifyConstants::timeoutInMicrosecondsForDisconnectedAcLine);
}

TEST_F(KmdNotifyTests, givenEnabledKmdNotifyMechanismWhenAcLineIsDisconnectedThenDontChangeTimeoutValue) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds = 5;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    auto params = helper.obtainTimeoutParams(false, 1, 2, 2, QueueThrottle::MEDIUM, true, false);

    EXPECT_TRUE(params.enableTimeout);
    EXPECT_EQ(hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds, params.waitTimeout);
}

TEST_F(KmdNotifyTests, givenEnabledKmdNotifyMechanismAndTaskCountToWaitLargerThanHwTagPlusOneAndDirectSubmissionDisabledAndQueueThrottleMediumThenDoNotApplyMultiplier) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds = 5;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    auto params = helper.obtainTimeoutParams(false, 1, 10, 2, QueueThrottle::MEDIUM, true, false);

    EXPECT_TRUE(params.enableTimeout);
    EXPECT_EQ(hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds, params.waitTimeout);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismAndFlushStampIsZeroWhenAcLineIsDisconnectedThenDontForceEnableTimeout) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    FlushStamp flushStampToWait = 0;
    auto params = helper.obtainTimeoutParams(false, 1, 2, flushStampToWait, QueueThrottle::MEDIUM, true, false);

    EXPECT_FALSE(params.enableTimeout);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismWhenPowerSavingModeIsSetThenKmdNotifyMechanismIsUsedAndReturnsShortestWaitingTimePossible) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.PowerSavingMode.set(1u);
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    FlushStamp flushStampToWait = 1;
    auto params = helper.obtainTimeoutParams(false, 1, 2, flushStampToWait, QueueThrottle::MEDIUM, true, false);
    EXPECT_TRUE(params.enableTimeout);
    EXPECT_EQ(1, params.waitTimeout);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismWhenPowerSavingModeIsRequestedThenKmdNotifyMechanismIsUsedAndReturnsShortestWaitingTimePossible) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    FlushStamp flushStampToWait = 1;
    auto params = helper.obtainTimeoutParams(false, 1, 2, flushStampToWait, QueueThrottle::LOW, true, false);
    EXPECT_TRUE(params.enableTimeout);
    EXPECT_EQ(1, params.waitTimeout);
}

TEST_F(KmdNotifyTests, givenEnabledKmdNotifyMechanismWhenPowerSavingModeIsSetAndNoFlushStampProvidedWhenParametersAreObtainedThenFalseIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.PowerSavingMode.set(1u);
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    FlushStamp flushStampToWait = 0;
    auto params = helper.obtainTimeoutParams(false, 1, 2, flushStampToWait, QueueThrottle::MEDIUM, true, false);
    EXPECT_FALSE(params.enableTimeout);
    EXPECT_EQ(0, params.waitTimeout);
}

TEST_F(KmdNotifyTests, givenEnabledKmdDirectSubmissionNotifyMechanismWhenDirectSubmissionIsEnabledThenSelectDelayTimeoutForDirectSubmission) {
    overrideKmdNotifyParams(true, 150, false, 0, false, 0, true, 20);

    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    constexpr int64_t expectedTimeout = 20;
    constexpr bool directSubmission = true;
    FlushStamp flushStampToWait = 1;
    auto params = helper.obtainTimeoutParams(false, 1, 2, flushStampToWait, QueueThrottle::MEDIUM, true, directSubmission);
    EXPECT_TRUE(params.enableTimeout);
    EXPECT_EQ(expectedTimeout, params.waitTimeout);
}

TEST_F(KmdNotifyTests, givenEnabledKmdDirectSubmissionNotifyMechanismWhenDirectSubmissionIsDisabledThenSelectBaseDelayTimeout) {
    overrideKmdNotifyParams(true, 150, false, 0, false, 0, true, 20);

    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    constexpr int64_t expectedTimeout = 150;
    constexpr bool directSubmission = false;
    FlushStamp flushStampToWait = 1;
    auto params = helper.obtainTimeoutParams(false, 1, 2, flushStampToWait, QueueThrottle::MEDIUM, true, directSubmission);
    EXPECT_TRUE(params.enableTimeout);
    EXPECT_EQ(expectedTimeout, params.waitTimeout);
}

TEST_F(KmdNotifyTests, givenDisabledKmdDirectSubmissionNotifyMechanismWhenDirectSubmissionIsEnabledThenSelectBaseDelayTimeout) {
    overrideKmdNotifyParams(true, 150, false, 0, false, 0, false, 20);

    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    constexpr int64_t expectedTimeout = 150;
    constexpr bool directSubmission = true;
    FlushStamp flushStampToWait = 1;
    auto params = helper.obtainTimeoutParams(false, 1, 2, flushStampToWait, QueueThrottle::MEDIUM, true, directSubmission);
    EXPECT_TRUE(params.enableTimeout);
    EXPECT_EQ(expectedTimeout, params.waitTimeout);
}