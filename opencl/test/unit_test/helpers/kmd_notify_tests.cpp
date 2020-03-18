/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "test.h"

#include "gmock/gmock.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using namespace NEO;

struct KmdNotifyTests : public ::testing::Test {
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
        cmdQ.reset(new MockCommandQueue(&context, device.get(), nullptr));
        *device->getDefaultEngine().commandStreamReceiver->getTagAddress() = taskCountToWait;
        cmdQ->getGpgpuCommandStreamReceiver().waitForFlushStamp(flushStampToWait);
        overrideKmdNotifyParams(true, 2, true, 1, false, 0);
    }

    void overrideKmdNotifyParams(bool kmdNotifyEnable, int64_t kmdNotifyDelay,
                                 bool quickKmdSleepEnable, int64_t quickKmdSleepDelay,
                                 bool quickKmdSleepEnableForSporadicWaits, int64_t quickKmdSleepDelayForSporadicWaits) {
        auto &properties = hwInfo->capabilityTable.kmdNotifyProperties;
        properties.enableKmdNotify = kmdNotifyEnable;
        properties.delayKmdNotifyMicroseconds = kmdNotifyDelay;
        properties.enableQuickKmdSleep = quickKmdSleepEnable;
        properties.delayQuickKmdSleepMicroseconds = quickKmdSleepDelay;
        properties.enableQuickKmdSleepForSporadicWaits = quickKmdSleepEnableForSporadicWaits;
        properties.delayQuickKmdSleepForSporadicWaitsMicroseconds = quickKmdSleepDelayForSporadicWaits;
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
        MockKmdNotifyCsr(const ExecutionEnvironment &executionEnvironment) : UltCommandStreamReceiver<Family>(const_cast<ExecutionEnvironment &>(executionEnvironment), 0) {}
        MOCK_METHOD1(waitForFlushStamp, bool(FlushStamp &flushStampToWait));
        MOCK_METHOD3(waitForCompletionWithTimeout, bool(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait));
    };

    template <typename Family>
    MockKmdNotifyCsr<Family> *createMockCsr() {
        auto csr = new ::testing::NiceMock<MockKmdNotifyCsr<Family>>(*device->executionEnvironment);
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

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 2, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenTaskCountAndKmdNotifyDisabledWhenWaitUntilCompletionCalledThenTryCpuPollingWithoutTimeout) {
    overrideKmdNotifyParams(false, 0, false, 0, false, 0);
    auto csr = createMockCsr<FamilyType>();

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, 0, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenNotReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndKmdWait) {
    auto csr = createMockCsr<FamilyType>();
    *csr->getTagAddress() = taskCountToWait - 1;

    ::testing::InSequence is;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 2, taskCountToWait)).Times(1).WillOnce(::testing::Return(false));
    EXPECT_CALL(*csr, waitForFlushStamp(flushStampToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, 0, taskCountToWait)).Times(1).WillOnce(::testing::Return(false));

    //we have unrecoverable for this case, this will throw.
    EXPECT_THROW(cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false), std::exception);
}

HWTEST_F(KmdNotifyTests, givenReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndDontCallKmdWait) {
    auto csr = createMockCsr<FamilyType>();

    ::testing::InSequence is;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 2, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenDefaultArgumentWhenWaitUntilCompleteIsCalledThenDisableQuickKmdSleep) {
    auto csr = createMockCsr<FamilyType>();
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenEnabledQuickSleepWhenWaitUntilCompleteIsCalledThenChangeDelayValue) {
    auto csr = createMockCsr<FamilyType>();
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, true);
}

HWTEST_F(KmdNotifyTests, givenDisabledQuickSleepWhenWaitUntilCompleteWithQuickSleepRequestIsCalledThenUseBaseDelayValue) {
    overrideKmdNotifyParams(true, 1, false, 0, false, 0);
    auto csr = createMockCsr<FamilyType>();
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, true);
}

HWTEST_F(KmdNotifyTests, givenNotReadyTaskCountWhenPollForCompletionCalledThenTimeout) {
    *device->getDefaultEngine().commandStreamReceiver->getTagAddress() = taskCountToWait - 1;
    auto success = device->getUltCommandStreamReceiver<FamilyType>().waitForCompletionWithTimeout(true, 1, taskCountToWait);
    EXPECT_FALSE(success);
}

HWTEST_F(KmdNotifyTests, givenZeroFlushStampWhenWaitIsCalledThenDisableTimeout) {
    auto csr = createMockCsr<FamilyType>();

    EXPECT_TRUE(device->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, ::testing::_, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 0, false, false);
}

HWTEST_F(KmdNotifyTests, givenNonQuickSleepRequestWhenItsSporadicWaitThenOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 1);
    auto csr = createMockCsr<FamilyType>();

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_, expectedDelay, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    int64_t timeSinceLastWait = mockKmdNotifyHelper->properties->delayQuickKmdSleepForSporadicWaitsMicroseconds + 1;

    mockKmdNotifyHelper->lastWaitForCompletionTimestampUs = mockKmdNotifyHelper->getMicrosecondsSinceEpoch() - timeSinceLastWait;
    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, false);
}

HWTEST_F(KmdNotifyTests, givenNonQuickSleepRequestWhenItsNotSporadicWaitThenOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 9999999);
    auto csr = createMockCsr<FamilyType>();

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_, expectedDelay, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, false);
}

HWTEST_F(KmdNotifyTests, givenKmdNotifyDisabledWhenPowerSavingModeIsRequestedThenTimeoutIsEnabled) {
    overrideKmdNotifyParams(false, 3, false, 2, false, 9999999);
    auto csr = createMockCsr<FamilyType>();
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 1, ::testing::_)).Times(1).WillOnce(::testing::Return(true));
    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, true);
}

HWTEST_F(KmdNotifyTests, givenKmdNotifyDisabledWhenQueueHasPowerSavingModeAndCallWaitThenTimeoutIsEnabled) {
    overrideKmdNotifyParams(false, 3, false, 2, false, 9999999);
    auto csr = createMockCsr<FamilyType>();
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 1, ::testing::_)).Times(1).WillOnce(::testing::Return(true));
    cmdQ->throttle = QueueThrottle::LOW;
    cmdQ->waitUntilComplete(1, 1, false);
}

HWTEST_F(KmdNotifyTests, givenKmdNotifyDisabledWhenQueueHasPowerSavingModButThereIsNoFlushStampeAndCallWaitThenTimeoutIsDisabled) {
    overrideKmdNotifyParams(false, 3, false, 2, false, 9999999);
    auto csr = createMockCsr<FamilyType>();
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, 0, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->throttle = QueueThrottle::LOW;
    cmdQ->waitUntilComplete(1, 0, false);
}

HWTEST_F(KmdNotifyTests, givenQuickSleepRequestWhenItsSporadicWaitOptimizationIsDisabledThenDontOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, false, 0);
    auto csr = createMockCsr<FamilyType>();

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_, expectedDelay, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, true, false);
}

HWTEST_F(KmdNotifyTests, givenTaskCountEqualToHwTagWhenWaitCalledThenDontMultiplyTimeout) {
    auto csr = createMockCsr<FamilyType>();
    *csr->getTagAddress() = taskCountToWait;

    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, false);
}

HWTEST_F(KmdNotifyTests, givenTaskCountLowerThanHwTagWhenWaitCalledThenDontMultiplyTimeout) {
    auto csr = createMockCsr<FamilyType>();
    *csr->getTagAddress() = taskCountToWait + 5;

    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false, false);
}

HWTEST_F(KmdNotifyTests, givenDefaultCommandStreamReceiverWhenWaitCalledThenUpdateWaitTimestamp) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 1);

    auto csr = createMockCsr<FamilyType>();
    EXPECT_NE(0, mockKmdNotifyHelper->lastWaitForCompletionTimestampUs.load());

    EXPECT_EQ(1u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
    csr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, false);
    EXPECT_EQ(2u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
}

HWTEST_F(KmdNotifyTests, givenDefaultCommandStreamReceiverWithDisabledSporadicWaitOptimizationWhenWaitCalledThenDontUpdateWaitTimestamp) {
    overrideKmdNotifyParams(true, 3, true, 2, false, 0);

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
    helper.obtainTimeoutParams(timeout, false, hwTag, taskCountToWait, 1, false);

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
    helper.obtainTimeoutParams(timeout, false, hwTag, taskCountToWait, 1, false);

    EXPECT_EQ(1u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenTaskCountDiffGreaterThanMinimumToCheckAcLineAndEnabledKmdNotifyWhenObtainingTimeoutPropertiesThenDontCheck) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    uint32_t hwTag = 10;
    uint32_t taskCountToWait = 21;
    EXPECT_TRUE(taskCountToWait - hwTag > KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);
    EXPECT_EQ(10u, KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);

    int64_t timeout = 0;
    helper.obtainTimeoutParams(timeout, false, hwTag, taskCountToWait, 1, false);

    EXPECT_EQ(0u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismWhenAcLineIsDisconnectedThenForceEnableTimeout) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    int64_t timeout = 0;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, 2, false);

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
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, 2, false);

    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds, timeout);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismAndFlushStampIsZeroWhenAcLineIsDisconnectedThenDontForceEnableTimeout) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    int64_t timeout = 0;
    FlushStamp flushStampToWait = 0;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, false);

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
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, false);
    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(1, timeout);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismWhenPowerSavingModeIsRequestedThenKmdNotifyMechanismIsUsedAndReturnsShortestWaitingTimePossible) {
    hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(hwInfo->capabilityTable.kmdNotifyProperties));

    int64_t timeout = 0;
    FlushStamp flushStampToWait = 1;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, true);
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
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait, false);
    EXPECT_FALSE(timeoutEnabled);
    EXPECT_EQ(0, timeout);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
