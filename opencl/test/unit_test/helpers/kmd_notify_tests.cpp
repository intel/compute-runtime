/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gmock/gmock.h"

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

    class MockKmdNotifyHelper : public KmdNotifyHelper {
      public:
        using KmdNotifyHelper::acLineConnected;
        using KmdNotifyHelper::getMicrosecondsSinceEpoch;
        using KmdNotifyHelper::lastWaitForCompletionTimestampUs;
        using KmdNotifyHelper::properties;

        MockKmdNotifyHelper() = delete;
        MockKmdNotifyHelper(const KmdNotifyProperties *newProperties) : KmdNotifyHelper(newProperties){};

        void updateLastWaitForCompletionTimestamp() override {
            KmdNotifyHelper::updateLastWaitForCompletionTimestamp();
            updateLastWaitForCompletionTimestampCalled++;
        }

        void updateAcLineStatus() override {
            KmdNotifyHelper::updateAcLineStatus();
            updateAcLineStatusCalled++;
        }

        uint32_t updateLastWaitForCompletionTimestampCalled = 0u;
        uint32_t updateAcLineStatusCalled = 0u;
    };

    template <typename Family>
    class MockKmdNotifyCsr : public UltCommandStreamReceiver<Family> {
      public:
        MockKmdNotifyCsr(const ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
            : UltCommandStreamReceiver<Family>(const_cast<ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {}

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

        WaitStatus waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait) override {
            waitForCompletionWithTimeoutCalled++;
            waitForCompletionWithTimeoutParamsPassed.push_back({enableTimeout, timeoutMs, taskCountToWait});
            return waitForCompletionWithTimeoutResult;
        }

        struct WaitForCompletionWithTimeoutParams {
            bool enableTimeout{};
            int64_t timeoutMs{};
            uint32_t taskCountToWait{};
        };

        uint32_t waitForCompletionWithTimeoutCalled = 0u;
        WaitStatus waitForCompletionWithTimeoutResult = WaitStatus::Ready;
        StackVec<WaitForCompletionWithTimeoutParams, 2> waitForCompletionWithTimeoutParamsPassed{};
    };

    template <typename Family>
    MockKmdNotifyCsr<Family> *createMockCsr() {
        auto csr = new MockKmdNotifyCsr<Family>(*device->executionEnvironment, device->getDeviceBitfield());
        device->resetCommandStreamReceiver(csr);

        mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
        csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

        return csr;
    }

    MockKmdNotifyHelper *mockKmdNotifyHelper = nullptr;
    HardwareInfo *hwInfo = nullptr;
    MockContext context;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockCommandQueue> cmdQ;
    FlushStamp flushStampToWait = 1000;
    uint32_t taskCountToWait = 5;
};

HWTEST_F(KmdNotifyTests, givenTaskCountWhenWaitUntilCompletionCalledThenAlwaysTryCpuPolling) {
    auto csr = createMockCsr<FamilyType>();

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(2, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_F(KmdNotifyTests, givenTaskCountAndKmdNotifyDisabledWhenWaitUntilCompletionCalledThenTryCpuPollingWithoutTimeout) {
    overrideKmdNotifyParams(false, 0, false, 0, false, 0, false, 0);
    auto csr = createMockCsr<FamilyType>();

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, false);
    EXPECT_EQ(0u, csr->waitForFlushStampCalled);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(false, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(0, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_F(KmdNotifyTests, givenNotReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndKmdWait) {
    auto csr = createMockCsr<FamilyType>();
    *csr->getTagAddress() = taskCountToWait - 1;

    ::testing::InSequence is;

    csr->waitForCompletionWithTimeoutResult = WaitStatus::NotReady;

    //we have unrecoverable for this case, this will throw.
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

HWTEST_F(KmdNotifyTests, givenReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndDontCallKmdWait) {
    auto csr = createMockCsr<FamilyType>();

    ::testing::InSequence is;

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, false);
    EXPECT_EQ(0u, csr->waitForFlushStampCalled);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(2, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_F(KmdNotifyTests, givenDefaultArgumentWhenWaitUntilCompleteIsCalledThenDisableQuickKmdSleep) {
    auto csr = createMockCsr<FamilyType>();
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(expectedTimeout, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_F(KmdNotifyTests, givenEnabledQuickSleepWhenWaitUntilCompleteIsCalledThenChangeDelayValue) {
    auto csr = createMockCsr<FamilyType>();
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;

    cmdQ->waitUntilComplete(taskCountToWait, {}, flushStampToWait, true);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(expectedTimeout, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_F(KmdNotifyTests, givenDisabledQuickSleepWhenWaitUntilCompleteWithQuickSleepRequestIsCalledThenUseBaseDelayValue) {
    overrideKmdNotifyParams(true, 1, false, 0, false, 0, false, 0);
    auto csr = createMockCsr<FamilyType>();
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
    EXPECT_NE(NEO::WaitStatus::Ready, success);
}

HWTEST_F(KmdNotifyTests, givenZeroFlushStampWhenWaitIsCalledThenDisableTimeout) {
    auto csr = createMockCsr<FamilyType>();

    EXPECT_TRUE(device->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 0, false, false);
    EXPECT_EQ(0u, csr->waitForFlushStampCalled);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(false, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(taskCountToWait, csr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_F(KmdNotifyTests, givenNonQuickSleepRequestWhenItsSporadicWaitThenOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 1, false, 0);
    auto csr = createMockCsr<FamilyType>();

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;

    int64_t timeSinceLastWait = mockKmdNotifyHelper->properties->delayQuickKmdSleepForSporadicWaitsMicroseconds + 1;

    mockKmdNotifyHelper->lastWaitForCompletionTimestampUs = mockKmdNotifyHelper->getMicrosecondsSinceEpoch() - timeSinceLastWait;
    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(expectedDelay, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_F(KmdNotifyTests, givenNonQuickSleepRequestWhenItsNotSporadicWaitThenOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 9999999, false, 0);
    auto csr = createMockCsr<FamilyType>();

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(expectedDelay, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_F(KmdNotifyTests, givenKmdNotifyDisabledWhenPowerSavingModeIsRequestedThenTimeoutIsEnabled) {
    overrideKmdNotifyParams(false, 3, false, 2, false, 9999999, false, 0);
    auto csr = createMockCsr<FamilyType>();

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, true);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(1, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_F(KmdNotifyTests, givenKmdNotifyDisabledWhenQueueHasPowerSavingModeAndCallWaitThenTimeoutIsEnabled) {
    overrideKmdNotifyParams(false, 3, false, 2, false, 9999999, false, 0);
    auto csr = createMockCsr<FamilyType>();

    cmdQ->throttle = QueueThrottle::LOW;

    cmdQ->waitUntilComplete(1, {}, 1, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(1, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_F(KmdNotifyTests, givenKmdNotifyDisabledWhenQueueHasPowerSavingModButThereIsNoFlushStampeAndCallWaitThenTimeoutIsDisabled) {
    overrideKmdNotifyParams(false, 3, false, 2, false, 9999999, false, 0);
    auto csr = createMockCsr<FamilyType>();

    cmdQ->throttle = QueueThrottle::LOW;

    cmdQ->waitUntilComplete(1, {}, 0, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(false, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(0, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_F(KmdNotifyTests, givenQuickSleepRequestWhenItsSporadicWaitOptimizationIsDisabledThenDontOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, false, 0, false, 0);
    auto csr = createMockCsr<FamilyType>();

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, true, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(expectedDelay, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_F(KmdNotifyTests, givenTaskCountEqualToHwTagWhenWaitCalledThenDontMultiplyTimeout) {
    auto csr = createMockCsr<FamilyType>();
    *csr->getTagAddress() = taskCountToWait;

    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(expectedTimeout, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_F(KmdNotifyTests, givenTaskCountLowerThanHwTagWhenWaitCalledThenDontMultiplyTimeout) {
    auto csr = createMockCsr<FamilyType>();
    *csr->getTagAddress() = taskCountToWait + 5;

    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, false);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(true, csr->waitForCompletionWithTimeoutParamsPassed[0].enableTimeout);
    EXPECT_EQ(expectedTimeout, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_F(KmdNotifyTests, givenDefaultCommandStreamReceiverWhenWaitCalledThenUpdateWaitTimestamp) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 1, false, 0);

    auto csr = createMockCsr<FamilyType>();
    EXPECT_NE(0, mockKmdNotifyHelper->lastWaitForCompletionTimestampUs.load());

    EXPECT_EQ(1u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
    csr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, false);
    EXPECT_EQ(2u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
}

HWTEST_F(KmdNotifyTests, givenDefaultCommandStreamReceiverWithDisabledSporadicWaitOptimizationWhenWaitCalledThenDontUpdateWaitTimestamp) {
    overrideKmdNotifyParams(true, 3, true, 2, false, 0, false, 0);

    auto csr = createMockCsr<FamilyType>();
    EXPECT_EQ(0, mockKmdNotifyHelper->lastWaitForCompletionTimestampUs.load());

    csr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, false);
    EXPECT_EQ(0u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
}

HWTEST_F(KmdNotifyTests, givenNewHelperWhenItsSetToCsrThenUpdateAcLineStatus) {
    auto helper = new MockKmdNotifyHelper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    EXPECT_EQ(0u, helper->updateAcLineStatusCalled);

    auto csr = createMockCsr<FamilyType>();
    csr->resetKmdNotifyHelper(helper);
    EXPECT_EQ(1u, helper->updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenTaskCountDiffLowerThanMinimumToCheckAcLineWhenObtainingTimeoutPropertiesThenDontCheck) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    uint32_t hwTag = 9;
    uint32_t taskCountToWait = 10;
    EXPECT_TRUE(taskCountToWait - hwTag < KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);
    EXPECT_EQ(10u, KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);

    int64_t timeout = 0;
    helper.obtainTimeoutParams(timeout, false, hwTag, taskCountToWait, 1, false, true, false);

    EXPECT_EQ(0u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenTaskCountDiffGreaterThanMinimumToCheckAcLineAndDisabledKmdNotifyWhenObtainingTimeoutPropertiesThenCheck) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    uint32_t hwTag = 10;
    uint32_t taskCountToWait = 21;
    EXPECT_TRUE(taskCountToWait - hwTag > KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);
    EXPECT_EQ(10u, KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);

    int64_t timeout = 0;
    helper.obtainTimeoutParams(timeout, false, hwTag, taskCountToWait, 1, false, true, false);

    EXPECT_EQ(1u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenKmdWaitModeNotActiveWhenObtainTimeoutParamsThenFalseIsReturned) {
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    int64_t timeout = 0;
    auto enableTimeout = helper.obtainTimeoutParams(timeout, false, 1, 1, 1, false, false, false);

    EXPECT_FALSE(enableTimeout);
    EXPECT_FALSE(timeout);
}

TEST_F(KmdNotifyTests, givenTaskCountDiffGreaterThanMinimumToCheckAcLineAndEnabledKmdNotifyWhenObtainingTimeoutPropertiesThenDontCheck) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    uint32_t hwTag = 10;
    uint32_t taskCountToWait = 21;
    EXPECT_TRUE(taskCountToWait - hwTag > KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);
    EXPECT_EQ(10u, KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);

    int64_t timeout = 0;
    helper.obtainTimeoutParams(timeout, false, hwTag, taskCountToWait, 1, false, true, false);

    EXPECT_EQ(0u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismWhenAcLineIsDisconnectedThenForceEnableTimeout) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    int64_t timeout = 0;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, 2, false, true, false);

    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(KmdNotifyConstants::timeoutInMicrosecondsForDisconnectedAcLine, timeout);
    EXPECT_EQ(10000, KmdNotifyConstants::timeoutInMicrosecondsForDisconnectedAcLine);
}

TEST_F(KmdNotifyTests, givenEnabledKmdNotifyMechanismWhenAcLineIsDisconnectedThenDontChangeTimeoutValue) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds = 5;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    int64_t timeout = 0;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, 2, false, true, false);

    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds, timeout);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismAndFlushStampIsZeroWhenAcLineIsDisconnectedThenDontForceEnableTimeout) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    int64_t timeout = 0;
    FlushStamp flushStampToWait = 0;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, false, true, false);

    EXPECT_FALSE(timeoutEnabled);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismWhenPowerSavingModeIsSetThenKmdNotifyMechanismIsUsedAndReturnsShortestWaitingTimePossible) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PowerSavingMode.set(1u);
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    int64_t timeout = 0;
    FlushStamp flushStampToWait = 1;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, false, true, false);
    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(1, timeout);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismWhenPowerSavingModeIsRequestedThenKmdNotifyMechanismIsUsedAndReturnsShortestWaitingTimePossible) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    int64_t timeout = 0;
    FlushStamp flushStampToWait = 1;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, true, true, false);
    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(1, timeout);
}

TEST_F(KmdNotifyTests, givenEnabledKmdNotifyMechanismWhenPowerSavingModeIsSetAndNoFlushStampProvidedWhenParametersAreObtainedThenFalseIsReturned) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PowerSavingMode.set(1u);
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    int64_t timeout = 0;
    FlushStamp flushStampToWait = 0;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, false, true, false);
    EXPECT_FALSE(timeoutEnabled);
    EXPECT_EQ(0, timeout);
}

TEST_F(KmdNotifyTests, givenEnabledKmdDirectSubmissionNotifyMechanismWhenDirectSubmissionIsEnabledThenSelectDelayTimeoutForDirectSubmission) {
    overrideKmdNotifyParams(true, 150, false, 0, false, 0, true, 20);

    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    constexpr int64_t expectedTimeout = 20;
    constexpr bool directSubmission = true;
    int64_t timeout = 0;
    FlushStamp flushStampToWait = 1;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, false, true, directSubmission);
    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(expectedTimeout, timeout);
}

TEST_F(KmdNotifyTests, givenEnabledKmdDirectSubmissionNotifyMechanismWhenDirectSubmissionIsDisabledThenSelectBaseDelayTimeout) {
    overrideKmdNotifyParams(true, 150, false, 0, false, 0, true, 20);

    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    constexpr int64_t expectedTimeout = 150;
    constexpr bool directSubmission = false;
    int64_t timeout = 0;
    FlushStamp flushStampToWait = 1;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, false, true, directSubmission);
    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(expectedTimeout, timeout);
}

TEST_F(KmdNotifyTests, givenDisabledKmdDirectSubmissionNotifyMechanismWhenDirectSubmissionIsEnabledThenSelectBaseDelayTimeout) {
    overrideKmdNotifyParams(true, 150, false, 0, false, 0, false, 20);

    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    constexpr int64_t expectedTimeout = 150;
    constexpr bool directSubmission = true;
    int64_t timeout = 0;
    FlushStamp flushStampToWait = 1;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, false, true, directSubmission);
    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(expectedTimeout, timeout);
}