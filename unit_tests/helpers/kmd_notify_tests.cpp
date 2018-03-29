/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_queue/command_queue.h"

#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using namespace OCLRT;

struct KmdNotifyTests : public ::testing::Test {
    void SetUp() override {
        device.reset(Device::create<MockDevice>(&localHwInfo));
        cmdQ.reset(new CommandQueue(&context, device.get(), nullptr));
        *device->getTagAddress() = taskCountToWait;
        device->getCommandStreamReceiver().waitForFlushStamp(flushStampToWait);
        overrideKmdNotifyParams(true, 2, true, 1, false, 0);
    }

    void overrideKmdNotifyParams(bool kmdNotifyEnable, int64_t kmdNotifyDelay,
                                 bool quickKmdSleepEnable, int64_t quickKmdSleepDelay,
                                 bool quickKmdSleepEnableForSporadicWaits, int64_t quickKmdSleepDelayForSporadicWaits) {
        auto &properties = localHwInfo.capabilityTable.kmdNotifyProperties;
        properties.enableKmdNotify = kmdNotifyEnable;
        properties.delayKmdNotifyMicroseconds = kmdNotifyDelay;
        properties.enableQuickKmdSleep = quickKmdSleepEnable;
        properties.delayQuickKmdSleepMicroseconds = quickKmdSleepDelay;
        properties.enableQuickKmdSleepForSporadicWaits = quickKmdSleepEnableForSporadicWaits;
        properties.delayQuickKmdSleepForSporadicWaitsMicroseconds = quickKmdSleepDelayForSporadicWaits;
    }

    template <typename Family>
    class MyCsr : public UltCommandStreamReceiver<Family> {
      public:
        MyCsr(const HardwareInfo &hwInfo) : UltCommandStreamReceiver<Family>(hwInfo) {}
        MOCK_METHOD1(waitForFlushStamp, bool(FlushStamp &flushStampToWait));
        MOCK_METHOD3(waitForCompletionWithTimeout, bool(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait));
        int64_t computeTimeoutMultiplierRetValue = 1u;

      protected:
        int64_t computeTimeoutMultiplier(bool useQuickKmdSleep, uint32_t taskCountDiff) const override { return computeTimeoutMultiplierRetValue; };
    };

    HardwareInfo localHwInfo = **platformDevices;
    MockContext context;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    FlushStamp flushStampToWait = 1000;
    uint32_t taskCountToWait = 5;
};

HWTEST_F(KmdNotifyTests, givenTaskCountWhenWaitUntilCompletionCalledThenAlwaysTryCpuPolling) {
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 2, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenTaskCountAndKmdNotifyDisabledWhenWaitUntilCompletionCalledThenTryCpuPollingWithoutTimeout) {
    overrideKmdNotifyParams(false, 0, false, 0, false, 0);
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, 0, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenNotReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndKmdWait) {
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);
    *device->getTagAddress() = taskCountToWait - 1;

    ::testing::InSequence is;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 2, taskCountToWait)).Times(1).WillOnce(::testing::Return(false));
    EXPECT_CALL(*csr, waitForFlushStamp(flushStampToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, 0, taskCountToWait)).Times(1).WillOnce(::testing::Return(false));

    //we have unrecoverable for this case, this will throw.
    EXPECT_THROW(cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false), std::exception);
}

HWTEST_F(KmdNotifyTests, givenReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndDontCallKmdWait) {
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);

    ::testing::InSequence is;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 2, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenDefaultArgumentWhenWaitUntilCompleteIsCalledThenDisableQuickKmdSleep) {
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenEnabledQuickSleepWhenWaitUntilCompleteIsCalledThenChangeDelayValue) {
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, true);
}

HWTEST_F(KmdNotifyTests, givenDisabledQuickSleepWhenWaitUntilCompleteWithQuickSleepRequestIsCalledThenUseBaseDelayValue) {
    overrideKmdNotifyParams(true, 1, false, 0, false, 0);
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, true);
}

HWTEST_F(KmdNotifyTests, givenNotReadyTaskCountWhenPollForCompletionCalledThenTimeout) {
    *device->getTagAddress() = taskCountToWait - 1;
    auto success = device->getCommandStreamReceiver().waitForCompletionWithTimeout(true, 1, taskCountToWait);
    EXPECT_FALSE(success);
}

HWTEST_F(KmdNotifyTests, givenZeroFlushStampWhenWaitIsCalledThenDisableTimeout) {
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);

    EXPECT_TRUE(device->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, ::testing::_, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 0, false);
}

HWTEST_F(KmdNotifyTests, givenNonQuickSleepRequestWhenItsSporadicWaitThenOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 0);
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_, expectedDelay, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    auto now = std::chrono::high_resolution_clock::now();
    csr->lastWaitForCompletionTimestamp = now - std::chrono::hours(24);
    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false);
}

HWTEST_F(KmdNotifyTests, givenNonQuickSleepRequestWhenItsNotSporadicWaitThenOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 9999999);
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_, expectedDelay, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false);
}

HWTEST_F(KmdNotifyTests, givenQuickSleepRequestWhenItsSporadicWaitOptimizationIsDisabledThenDontOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, false, 0);
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_, expectedDelay, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, true);
}

HWTEST_F(KmdNotifyTests, givenComputeTimeoutMultiplierWhenWaitCalledThenUseNewTimeout) {
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    csr->computeTimeoutMultiplierRetValue = 3;
    device->resetCommandStreamReceiver(csr);

    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds *
                           csr->computeTimeoutMultiplierRetValue;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false);
}

template <typename Family>
struct MyCsrWithTimestampCheck : public UltCommandStreamReceiver<Family> {
    MyCsrWithTimestampCheck(const HardwareInfo &hwInfo) : UltCommandStreamReceiver<Family>(hwInfo) {}
    void updateLastWaitForCompletionTimestamp() override {
        updateLastWaitForCompletionTimestampCalled++;
    };
    uint32_t updateLastWaitForCompletionTimestampCalled = 0u;
};

HWTEST_F(KmdNotifyTests, givenDefaultCommandStreamReceiverWhenWaitCalledThenUpdateWaitTimestamp) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 1);

    auto csr = new MyCsrWithTimestampCheck<FamilyType>(localHwInfo);
    EXPECT_NE(0, csr->lastWaitForCompletionTimestamp.time_since_epoch().count());
    device->resetCommandStreamReceiver(csr);

    csr->waitForTaskCountWithKmdNotifyFallback(0, 0, false);
    EXPECT_EQ(1u, csr->updateLastWaitForCompletionTimestampCalled);
}

HWTEST_F(KmdNotifyTests, givenDefaultCommandStreamReceiverWithDisabledSporadicWaitOptimizationWhenWaitCalledThenDontUpdateWaitTimestamp) {
    overrideKmdNotifyParams(true, 3, true, 2, false, 0);

    auto csr = new MyCsrWithTimestampCheck<FamilyType>(localHwInfo);
    EXPECT_EQ(0, csr->lastWaitForCompletionTimestamp.time_since_epoch().count());
    device->resetCommandStreamReceiver(csr);

    csr->waitForTaskCountWithKmdNotifyFallback(0, 0, false);
    EXPECT_EQ(0u, csr->updateLastWaitForCompletionTimestampCalled);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
