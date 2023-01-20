/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/include/zet_intel_gpu_debug.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

#include "common/StateSaveAreaHeader.h"
#include "encode_surface_state_args.h"

namespace L0 {
namespace ult {

using DebugSessionTest = ::testing::Test;

TEST(DeviceWithDebugSessionTest, GivenDeviceWithDebugSessionWhenCallingReleaseResourcesThenCloseConnectionIsCalled) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);

    zet_debug_config_t config = {};
    auto session = new DebugSessionMock(config, device.get());
    static_cast<DeviceImp *>(device.get())->setDebugSession(session);
    static_cast<DeviceImp *>(device.get())->releaseResources();

    EXPECT_TRUE(session->closeConnectionCalled);
}

TEST(DebugSessionTest, givenNullDeviceWhenDebugSessionCreatedThenAllThreadsAreEmpty) {
    auto sessionMock = std::make_unique<MockDebugSession>(zet_debug_config_t{0x1234}, nullptr);
    EXPECT_TRUE(sessionMock->allThreads.empty());
}

TEST(DebugSessionTest, givenApplyResumeWaCalledThenWAIsApplied) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    size_t bitmaskSize = 32;
    auto bitmask = std::make_unique<uint8_t[]>(bitmaskSize);
    bitmask.get()[0] = 1;
    sessionMock->applyResumeWa(bitmask.get(), bitmaskSize);
    if (l0GfxCoreHelper.isResumeWARequired()) {
        EXPECT_EQ(1, bitmask.get()[4]);
    } else {
        EXPECT_EQ(0, bitmask.get()[4]);
    }
}

TEST(DebugSessionTest, givenAllStoppedThreadsWhenInterruptCalledThenErrorNotAvailableReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
    }

    ze_device_thread_t apiThread = {0, 0, 0, UINT32_MAX};
    auto result = sessionMock->interrupt(apiThread);

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST(DebugSessionTest, givenNoPendingInterruptWhenSendInterruptCalledThenInterruptImpNotCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->interruptSent = true;

    sessionMock->sendInterrupts();
    EXPECT_EQ(0u, sessionMock->interruptImpCalled);
}

TEST(DebugSessionTest, givenPendingInterruptWhenNewInterruptForThreadCalledThenErrorNotReadyReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 1};

    sessionMock->pendingInterrupts.push_back({apiThread, false});
    auto result = sessionMock->interrupt(apiThread);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST(DebugSessionTest, givenInterruptAlreadySentWhenSendInterruptCalledSecondTimeThenEarlyReturns) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    sessionMock->threadStopped = 0;

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    EXPECT_EQ(1u, sessionMock->interruptImpCalled);
    EXPECT_EQ(0u, sessionMock->expectedAttentionEvents);

    ze_device_thread_t apiThread2 = {0, 0, 0, 2};
    result = sessionMock->interrupt(apiThread2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    EXPECT_EQ(1u, sessionMock->interruptImpCalled);
}

TEST(DebugSessionTest, givenInterruptRequestWhenInterruptImpFailsInSendInterruptThenApiEventsArePushed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->interruptImpResult = ZE_RESULT_ERROR_UNKNOWN;
    ze_device_thread_t apiThread = {0, 0, 0, 1};
    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    EXPECT_EQ(1u, sessionMock->interruptImpCalled);

    EXPECT_EQ(1u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));

    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
}

TEST(DebugSessionTest, givenPendingInteruptWhenHandlingThreadWithAttentionThenPendingInterruptIsMarkedComplete) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 0};
    EuThread::ThreadId thread(0, 0, 0, 0, 0);

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(thread, 1u);

    EXPECT_TRUE(sessionMock->allThreads[thread]->isStopped());

    EXPECT_TRUE(sessionMock->pendingInterrupts[0].second);
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenPreviouslyStoppedThreadAndPendingInterruptWhenHandlingThreadWithAttentionThenPendingInterruptIsNotMarkedCompleteAndInterruptGeneratesUnavailableEvent) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 0};
    EuThread::ThreadId thread(0, 0, 0, 0, 0);

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    // stop thread for a reason different than interrupt exception (forceException)
    sessionMock->onlyForceException = false;
    sessionMock->allThreads[thread]->stopThread(1u);

    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(thread, 1u);

    EXPECT_TRUE(sessionMock->allThreads[thread]->isStopped());

    EXPECT_FALSE(sessionMock->pendingInterrupts[0].second);
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());

    sessionMock->expectedAttentionEvents = 0;
    sessionMock->checkTriggerEventsForAttention();
    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(1u, sessionMock->events.size());
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));
}

TEST(DebugSessionTest, givenStoppedThreadWhenAddingNewlyStoppedThenThreadIsNotAdded) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    sessionMock->allThreads[thread]->stopThread(1u);

    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(thread, 1u);

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenNoStoppedThreadWhenAddingNewlyStoppedThenThreadIsNotAdded) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->threadStopped = 0;

    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(thread, 1u);

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenNoInterruptsSentWhenGenerateEventsAndResumeCalledThenTriggerEventsNotChanged) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    sessionMock->interruptSent = false;
    sessionMock->triggerEvents = false;

    sessionMock->generateEventsAndResumeStoppedThreads();
    EXPECT_FALSE(sessionMock->triggerEvents);
}

TEST(DebugSessionTest, givenTriggerEventsWhenGenerateEventsAndResumeCalledThenEventsGeneratedAndThreadsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    ze_device_thread_t apiThread2 = {0, 0, 1, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, true});

    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(EuThread::ThreadId(0, apiThread2), 1u);

    sessionMock->triggerEvents = true;
    sessionMock->interruptSent = true;

    sessionMock->onlyForceException = false;

    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(2u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, sessionMock->events[1].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread2, sessionMock->events[1].info.thread.thread));

    EXPECT_EQ(1u, sessionMock->resumeAccidentallyStoppedCalled);
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenPendingInterruptAfterTimeoutWhenGenerateEventsAndResumeStoppedThreadsIsCalledThenEventsUnavailableAreSentAndFlagsSetFalse) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});

    sessionMock->triggerEvents = false;
    sessionMock->interruptSent = true;
    sessionMock->returnTimeDiff = 5 * DebugSessionImp::interruptTimeout;

    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(1u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));

    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
    EXPECT_FALSE(sessionMock->interruptSent);
}

TEST(DebugSessionTest, givenPendingInterruptBeforeTimeoutWhenGenerateEventsAndResumeStoppedThreadsIsCalledThenEventsNotTriggered) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});

    sessionMock->triggerEvents = false;
    sessionMock->interruptSent = true;
    sessionMock->returnTimeDiff = DebugSessionImp::interruptTimeout / 2;

    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(0u, sessionMock->events.size());
    EXPECT_EQ(1u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
    EXPECT_TRUE(sessionMock->interruptSent);
}

TEST(DebugSessionTest, givenErrorFromReadSystemRoutineIdentWhenCheckingThreadStateThenThreadIsNotStopped) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->readSystemRoutineIdentRetVal = false;
    EuThread::ThreadId thread(0, 0, 0, 0, 0);

    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(thread, 1u);

    EXPECT_FALSE(sessionMock->allThreads[thread]->isStopped());
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenPendingInterruptsWhenGeneratingEventsThenStoppedEventIsGeneratedForCompletedInterrupt) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    ze_device_thread_t apiThread2 = {0, 0, 1, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});
    sessionMock->pendingInterrupts.push_back({apiThread2, true});

    sessionMock->generateEventsForPendingInterrupts();

    EXPECT_EQ(2u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, sessionMock->events[1].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread2, sessionMock->events[1].info.thread.thread));

    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
}

TEST(DebugSessionTest, givenEmptyStateSaveAreaWhenGetStateSaveAreaCalledThenReadStateSaveAreaCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    auto stateSaveArea = sessionMock->getStateSaveAreaHeader();
    EXPECT_EQ(nullptr, stateSaveArea);
    EXPECT_EQ(1u, sessionMock->readStateSaveAreaHeaderCalled);
}

TEST(DebugSessionTest, givenStoppedThreadsWhenFillingResumeAndStoppedThreadsFromNewlyStoppedThenContainerFilledBasedOnExceptionReason) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread = {0, 0, 0, 0, 1};
    EuThread::ThreadId thread2 = {0, 0, 0, 1, 1};
    EuThread::ThreadId thread3 = {0, 0, 0, 1, 2};

    sessionMock->newlyStoppedThreads.push_back(thread);
    sessionMock->newlyStoppedThreads.push_back(thread2);
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    sessionMock->onlyForceException = true;

    {
        std::vector<EuThread::ThreadId> resumeThreads;
        std::vector<EuThread::ThreadId> stoppedThreads;

        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread2]->stopThread(1u);

        sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads);
        EXPECT_EQ(2u, resumeThreads.size());
        EXPECT_EQ(0u, stoppedThreads.size());
    }

    sessionMock->newlyStoppedThreads.push_back(thread);
    sessionMock->newlyStoppedThreads.push_back(thread2);
    sessionMock->newlyStoppedThreads.push_back(thread3);

    sessionMock->onlyForceException = false;
    {
        std::vector<EuThread::ThreadId> resumeThreads;
        std::vector<EuThread::ThreadId> stoppedThreads;

        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread2]->stopThread(1u);

        sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads);
        EXPECT_EQ(0u, resumeThreads.size());
        EXPECT_EQ(2u, stoppedThreads.size());
    }
}

TEST(DebugSessionTest, givenNoThreadsStoppedWhenCallingfillResumeAndStoppedThreadsFromNewlyStoppedThenReadStateSaveAreaNotCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    std::vector<EuThread::ThreadId> resumeThreads;
    std::vector<EuThread::ThreadId> stoppedThreads;

    sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads);

    EXPECT_EQ(0u, sessionMock->readStateSaveAreaHeaderCalled);
}

TEST(DebugSessionTest, givenThreadsToResumeWhenResumeAccidentallyStoppedThreadsCalledThenThreadsResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = counter++;
            return true;
        }

        int counter = 0;
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    EuThread::ThreadId thread = {0, 0, 0, 0, 1};
    EuThread::ThreadId thread2 = {0, 0, 0, 1, 1};

    std::vector<EuThread::ThreadId> threadIds;
    sessionMock->allThreads[thread]->verifyStopped(1u);
    sessionMock->allThreads[thread2]->verifyStopped(1u);

    threadIds.push_back(thread);
    threadIds.push_back(thread2);

    sessionMock->resumeAccidentallyStoppedThreads(threadIds);

    EXPECT_EQ(2u, sessionMock->resumeThreadCount);
    EXPECT_EQ(1u, sessionMock->resumeImpCalled);
    EXPECT_EQ(3u, sessionMock->checkThreadIsResumedCalled);

    EXPECT_TRUE(sessionMock->allThreads[thread]->isRunning());
    EXPECT_TRUE(sessionMock->allThreads[thread2]->isRunning());
}

TEST(DebugSessionTest, givenCr0RegisterWhenIsFEOrFEHOnlyExceptionReasonThenTrueReturnedForFEorFEHBitsOnly) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    uint32_t cr0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    cr0[1] = 1 << 26;
    EXPECT_TRUE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
    cr0[1] = 1 << 30;
    EXPECT_TRUE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
    cr0[1] = (1 << 26) | (1 << 30);
    EXPECT_TRUE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));

    cr0[1] = (1 << 27) | (1 << 30);
    EXPECT_FALSE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
    cr0[1] = (1 << 27) | (1 << 26);
    EXPECT_FALSE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
    cr0[1] = (1 << 28);
    EXPECT_FALSE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
}

TEST(DebugSessionTest, givenSomeThreadsRunningWhenResumeCalledThenOnlyStoppedThreadsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
    }

    EuThread::ThreadId euthread(0, 0, 0, 0, 3);
    sessionMock->allThreads[euthread]->resumeThread();

    auto result = sessionMock->resume(thread);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount - 1u, sessionMock->resumeThreadCount);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        EXPECT_TRUE(sessionMock->allThreads[thread]->isRunning());
    }
}

TEST(DebugSessionTest, givenAllThreadsRunningWhenResumeCalledThenErrorUnavailableReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t thread = {0, 0, 0, 1};
    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST(DebugSessionTest, givenErrorFromResumeImpWhenResumeCalledThenErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->resumeImpResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t thread = {0, 0, 0, 1};
    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST(DebugSessionTest, givenErrorFromReadSbaBufferWhenReadSbaRegistersCalledThenErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->readSbaBufferResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t thread = {0, 0, 0, 0};
    uint64_t sba[9];
    auto result = sessionMock->readSbaRegisters(EuThread::ThreadId(0, thread), 0, 9, sba);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST(DebugSessionTest, givenErrorFromReadRegistersWhenReadSbaRegistersCalledThenErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->readRegistersResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t thread = {0, 0, 0, 0};
    uint64_t sba[9];
    auto result = sessionMock->readSbaRegisters(EuThread::ThreadId(0, thread), 0, 9, sba);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(DebugSessionTest, givenErrorFromReadMemoryWhenReadSbaRegistersCalledThenErrorReturned, IsAtLeastXeHpCore) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->readRegistersSizeToFill = 6 * sizeof(uint32_t);

    uint32_t *scratchIndex = reinterpret_cast<uint32_t *>(ptrOffset(sessionMock->regs, 5 * sizeof(uint32_t)));
    *scratchIndex = 1u << 10u;

    ze_device_thread_t thread = {0, 0, 0, 0};
    uint64_t sba[9];

    auto result = sessionMock->readSbaRegisters(EuThread::ThreadId(0, thread), 0, 9, sba);

    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    EXPECT_TRUE(gfxCoreHelper.isScratchSpaceSurfaceStateAccessible());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST(DebugSessionTest, givenUnknownRegsetTypeWhenTypeToRegsetFlagsCalledThenZeroIsReturned) {
    EXPECT_EQ(0u, DebugSessionImp::typeToRegsetFlags(0));
    EXPECT_EQ(0u, DebugSessionImp::typeToRegsetFlags(UINT32_MAX));
}

TEST(DebugSessionTest, givenNullptrStateSaveAreaGpuVaWhenCallingCheckIsThreadResumedThenReportThreadHasBeenResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
            return 0;
        };
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, whenCallingCheckThreadIsResumedWithoutSrMagicThenThreadIsAssumedRunning) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipReadSystemRoutineIdent = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        sessionMock->stateSaveAreaHeader.resize(size);
    }

    auto stateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    auto threadSlotOffset = sessionMock->calculateThreadSlotOffset(threadId);
    auto srMagicOffset = threadSlotOffset + stateSaveAreaHeader->regHeader.sr_magic_offset;
    SIP::sr_ident srMagic = {{0}};
    sessionMock->writeGpuMemory(0, reinterpret_cast<char *>(&srMagic), sizeof(srMagic), reinterpret_cast<uint64_t>(stateSaveAreaHeader) + srMagicOffset);

    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, givenErrorFromReadSystemRoutineIdentWhenCallingCheckThreadIsResumedThenThreadIsAssumedRunning) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->readSystemRoutineIdentRetVal = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, givenSrMagicWithCounterLessThanlLastThreadCounterThenThreadHasBeenResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = 0;
            return true;
        }
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1u);
    sessionMock->allThreads[threadId]->stopThread(1u);
    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, givenSrMagicWithCounterEqualToPrevousThenThreadHasNotBeenResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = 1;
            return true;
        }
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1u);
    sessionMock->allThreads[threadId]->stopThread(1u);

    EXPECT_EQ(1u, sessionMock->allThreads[threadId]->getLastCounter());

    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_FALSE(resumed);
}

TEST(DebugSessionTest, givenSrMagicWithCounterBiggerThanPreviousThenThreadIsResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = 3;
            return true;
        }
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1u);
    sessionMock->allThreads[threadId]->stopThread(1u);

    EXPECT_EQ(1u, sessionMock->allThreads[threadId]->getLastCounter());

    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, givenSrMagicWithCounterOverflowingZeroThenThreadIsResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = 0;
            return true;
        }
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(255u);
    sessionMock->allThreads[threadId]->stopThread(1u);

    EXPECT_EQ(255u, sessionMock->allThreads[threadId]->getLastCounter());

    bool resumed = sessionMock->checkThreadIsResumed(threadId);
    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, GivenBindlessSipVersion1AndResumeWARequiredWhenCallingResumeThenBitInRegisterIsWritten) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->writeRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->readMemoryResult = ZE_RESULT_SUCCESS;
    sessionMock->debugArea.reserved1 = 1u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    if (l0GfxCoreHelper.isResumeWARequired()) {
        EXPECT_EQ(1u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
        EXPECT_EQ(uint32_t(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), sessionMock->readRegistersReg);
        EXPECT_EQ(uint32_t(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), sessionMock->writeRegistersReg);
    } else {
        EXPECT_EQ(0u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
    }

    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());
}

TEST(DebugSessionTest, GivenErrorFromReadRegisterWhenResumingThreadThenRegisterIsNotWrittenWithResumeBitAndErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->readRegistersResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->writeRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->readMemoryResult = ZE_RESULT_SUCCESS;
    sessionMock->debugArea.reserved1 = 1u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);

    if (l0GfxCoreHelper.isResumeWARequired()) {
        EXPECT_EQ(1u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
        EXPECT_EQ(uint32_t(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), sessionMock->readRegistersReg);
        EXPECT_EQ(0u, sessionMock->writeRegistersReg);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    } else {
        EXPECT_EQ(0u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    }

    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());
}

TEST(DebugSessionTest, GivenErrorFromWriteRegisterWhenResumingThreadThenRegisterIsNotWrittenWithResumeBitAndErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->writeRegistersResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->readMemoryResult = ZE_RESULT_SUCCESS;
    sessionMock->debugArea.reserved1 = 1u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);

    if (l0GfxCoreHelper.isResumeWARequired()) {
        EXPECT_EQ(1u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
        EXPECT_EQ(uint32_t(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), sessionMock->readRegistersReg);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    } else {
        EXPECT_EQ(0u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    }

    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());
}

TEST(DebugSessionTest, GivenNonBindlessSipVersion1AndResumeWARequiredWhenCallingResumeThenBitInRegisterIsWritten) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->writeRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->readMemoryResult = ZE_RESULT_SUCCESS;
    sessionMock->debugArea.reserved1 = 0u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    if (l0GfxCoreHelper.isResumeWARequired()) {
        EXPECT_EQ(1u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
        EXPECT_EQ(uint32_t(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU), sessionMock->writeRegistersReg);
    } else {
        EXPECT_EQ(0u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
    }

    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());
}

TEST(DebugSessionTest, GivenBindlessSipVersion2WhenWritingResumeFailsThenErrorIsReturned) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = ++counter;
            return true;
        }

      private:
        int counter = 0;
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->readRegistersResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->writeRegistersResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->debugArea.reserved1 = 1u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);

    EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
    EXPECT_EQ(2u, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, GivenBindlessSipVersion2WhenResumingThreadThenCheckIfThreadIsResumedIsCalledUntilSipCounterIsIncremented) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            if (counter > 5) {
                srMagic.count = 4;
            } else {
                srMagic.count = 1;
            }

            counter++;
            return true;
        }
        int counter = 0;
    };

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->debugArea.reserved1 = 1u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
    EXPECT_EQ(7u, sessionMock->checkThreadIsResumedCalled);

    sessionMock->allThreads[threadId]->verifyStopped(3);
    sessionMock->allThreads[threadId]->stopThread(1u);

    sessionMock->checkThreadIsResumedCalled = 0;
    result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

using MultiTileDebugSessionTest = Test<MultipleDevicesWithCustomHwInfo>;

TEST_F(MultiTileDebugSessionTest, givenThreadsFromMultipleTilesWhenResumeCalledThenThreadsResumedInAllTiles) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    EuThread::ThreadId threadTile0(0, 0, 0, 0, 0);
    EuThread::ThreadId threadTile1(1, 0, 0, 0, 0);
    sessionMock->allThreads[threadTile0]->stopThread(1u);
    sessionMock->allThreads[threadTile1]->stopThread(1u);

    ze_device_thread_t thread = {UINT32_MAX, 0, 0, 0};

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(sessionMock->allThreads[threadTile0]->isRunning());
    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());

    EXPECT_EQ(2u, sessionMock->resumeImpCalled);

    sessionMock->allThreads[threadTile1]->stopThread(1u);

    result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(3u, sessionMock->resumeImpCalled);
}

TEST_F(MultiTileDebugSessionTest, givenThreadFromSingleTileWhenResumeCalledThenThreadIsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    EuThread::ThreadId threadTile0(0, 0, 0, 0, 0);
    sessionMock->allThreads[threadTile0]->stopThread(1u);

    EuThread::ThreadId threadTile1(1, 0, 0, 0, 0);
    sessionMock->allThreads[threadTile1]->stopThread(1u);

    ze_device_thread_t thread = {sliceCount, 0, 0, 0};

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());

    EXPECT_EQ(1u, sessionMock->resumeImpCalled);
    EXPECT_EQ(1u, sessionMock->resumedDevices[0]);

    ze_device_thread_t resumedThreadExpected = {0, 0, 0, 0};
    EXPECT_EQ(resumedThreadExpected.slice, sessionMock->resumedThreads[0][0].slice);
    EXPECT_EQ(resumedThreadExpected.subslice, sessionMock->resumedThreads[0][0].subslice);
    EXPECT_EQ(resumedThreadExpected.eu, sessionMock->resumedThreads[0][0].eu);
    EXPECT_EQ(resumedThreadExpected.thread, sessionMock->resumedThreads[0][0].thread);

    thread = {0, 0, 0, 0};
    sessionMock->resumeImpCalled = 0;
    sessionMock->resumedDevices.clear();

    result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());

    EXPECT_EQ(1u, sessionMock->resumeImpCalled);
    EXPECT_EQ(0u, sessionMock->resumedDevices[0]);

    resumedThreadExpected = {0, 0, 0, 0};
    EXPECT_EQ(resumedThreadExpected.slice, sessionMock->resumedThreads[0][0].slice);
    EXPECT_EQ(resumedThreadExpected.subslice, sessionMock->resumedThreads[0][0].subslice);
    EXPECT_EQ(resumedThreadExpected.eu, sessionMock->resumedThreads[0][0].eu);
    EXPECT_EQ(resumedThreadExpected.thread, sessionMock->resumedThreads[0][0].thread);
}

TEST_F(MultiTileDebugSessionTest, givenRunningThreadsWhenResumeCalledThenThreadsNotResumedAndErrorNotAvailableReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    EuThread::ThreadId threadTile0(0, 0, 0, 0, 0);
    EuThread::ThreadId threadTile1(1, 0, 0, 0, 0);

    ze_device_thread_t thread = {UINT32_MAX, 0, 0, 0};
    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    EXPECT_EQ(0u, sessionMock->resumeImpCalled);

    EXPECT_TRUE(sessionMock->allThreads[threadTile0]->isRunning());
    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());
}

TEST_F(MultiTileDebugSessionTest, givenErrorFromResumeWithinDeviceWhenResumeCalledThenThreadsAreResumedAndErrorUnkwnownReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    EuThread::ThreadId threadTile0(0, 0, 0, 0, 0);
    EuThread::ThreadId threadTile1(1, 0, 0, 0, 0);
    sessionMock->allThreads[threadTile0]->stopThread(1u);
    sessionMock->allThreads[threadTile1]->stopThread(1u);

    ze_device_thread_t thread = {UINT32_MAX, 0, 0, 0};

    sessionMock->resumeImpResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    EXPECT_EQ(2u, sessionMock->resumeImpCalled);

    EXPECT_TRUE(sessionMock->allThreads[threadTile0]->isRunning());
    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());
}

TEST_F(MultiTileDebugSessionTest, givenOneDeviceRequestWhenSendingInterruptsThenOnlyOneInterruptCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;
    // Second subdevice's slice
    ze_device_thread_t apiThread = {sliceCount, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    EXPECT_EQ(1u, sessionMock->interruptImpCalled);
    EXPECT_TRUE(sessionMock->interruptSent);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[0]);
    EXPECT_EQ(1u, sessionMock->expectedAttentionEvents);
}

TEST_F(MultiTileDebugSessionTest, givenTwoDevicesInRequestsWhenSendingInterruptsThenTwoInterruptsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;

    ze_device_thread_t apiThread = {0, 0, 0, 0};
    ze_device_thread_t apiThread2 = {sliceCount, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = sessionMock->interrupt(apiThread2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    EXPECT_EQ(2u, sessionMock->interruptImpCalled);
    EXPECT_TRUE(sessionMock->interruptSent);
    EXPECT_EQ(0u, sessionMock->interruptedDevices[0]);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[1]);
    EXPECT_EQ(2u, sessionMock->expectedAttentionEvents);
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenSendingInterruptsThenInterruptIsSentWithCorrectDeviceIndex) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;
    sessionMock->tileAttachEnabled = true;
    sessionMock->initialize();

    auto tileSession1 = static_cast<MockDebugSession *>(sessionMock->tileSessions[1].first);
    auto tileSession0 = static_cast<MockDebugSession *>(sessionMock->tileSessions[0].first);

    ze_device_thread_t apiThread = {0, 0, 0, 0};

    auto result = tileSession1->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = tileSession0->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    tileSession1->sendInterrupts();
    tileSession0->sendInterrupts();

    EXPECT_TRUE(tileSession1->interruptSent);
    EXPECT_TRUE(tileSession0->interruptSent);
    EXPECT_EQ(1u, tileSession1->interruptedDevices[0]);
    EXPECT_EQ(0u, tileSession0->interruptedDevices[0]);
}

TEST_F(MultiTileDebugSessionTest, givenAllSlicesInRequestWhenSendingInterruptsThenTwoInterruptsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;

    ze_device_thread_t apiThread = {UINT32_MAX, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    EXPECT_EQ(2u, sessionMock->interruptImpCalled);
    EXPECT_TRUE(sessionMock->interruptSent);
    EXPECT_EQ(0u, sessionMock->interruptedDevices[0]);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[1]);
    EXPECT_EQ(2u, sessionMock->expectedAttentionEvents);
}

TEST_F(MultiTileDebugSessionTest, givenTwoInterruptsSentWhenCheckingTriggerEventsThenTriggerEventsWhenAllAttEventsReceived) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;
    ze_device_thread_t apiThread = {UINT32_MAX, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    EXPECT_EQ(2u, sessionMock->expectedAttentionEvents);

    sessionMock->newAttentionRaised(0);
    EXPECT_EQ(1u, sessionMock->expectedAttentionEvents);
    sessionMock->checkTriggerEventsForAttention();

    EXPECT_FALSE(sessionMock->triggerEvents);

    sessionMock->newAttentionRaised(1);
    EXPECT_EQ(0u, sessionMock->expectedAttentionEvents);
    sessionMock->checkTriggerEventsForAttention();

    EXPECT_TRUE(sessionMock->triggerEvents);
}

TEST_F(MultiTileDebugSessionTest, givenTwoDevicesInRequestsWhenAllInterruptsReturnErrorThenAllInterruptRequestsGenerateUnavailableEvents) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->interruptImpResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t apiThread = {0, 0, 0, 0};
    ze_device_thread_t apiThread2 = {sliceCount, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = sessionMock->interrupt(apiThread2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(2u, sessionMock->interruptRequests.size());

    sessionMock->sendInterrupts();

    EXPECT_EQ(2u, sessionMock->interruptImpCalled);
    EXPECT_FALSE(sessionMock->interruptSent);
    EXPECT_EQ(0u, sessionMock->interruptedDevices[0]);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[1]);

    EXPECT_EQ(0u, sessionMock->expectedAttentionEvents);
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());

    EXPECT_EQ(2u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[1].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread2, sessionMock->events[1].info.thread.thread));
}

TEST_F(MultiTileDebugSessionTest, givenAllSlicesInRequestWhenAllInterruptsReturnErrorThenAllInterruptRequestsGenerateUnavailableEvents) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t apiThread = {UINT32_MAX, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, sessionMock->interruptRequests.size());

    sessionMock->sendInterrupts();

    EXPECT_EQ(2u, sessionMock->interruptImpCalled);
    EXPECT_FALSE(sessionMock->interruptSent);
    EXPECT_EQ(0u, sessionMock->interruptedDevices[0]);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[1]);

    EXPECT_EQ(0u, sessionMock->expectedAttentionEvents);
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());

    EXPECT_EQ(1u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));
}

TEST_F(MultiTileDebugSessionTest, GivenMultitileDeviceWhenCallingAreRequestedThreadsStoppedThenCorrectValueIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    ze_device_thread_t thread = {0, 0, 0, 0};
    ze_device_thread_t allSlices = {UINT32_MAX, 0, 0, 0};

    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->stopThread(1u);
    sessionMock->allThreads[EuThread::ThreadId(1, thread)]->stopThread(1u);

    auto stopped = sessionMock->areRequestedThreadsStopped(thread);
    EXPECT_TRUE(stopped);

    stopped = sessionMock->areRequestedThreadsStopped(allSlices);
    EXPECT_FALSE(stopped);

    for (uint32_t i = 0; i < sliceCount; i++) {
        EuThread::ThreadId threadId(0, i, 0, 0, 0);
        sessionMock->allThreads[threadId]->stopThread(1u);
    }

    stopped = sessionMock->areRequestedThreadsStopped(allSlices);
    EXPECT_FALSE(stopped);

    for (uint32_t i = 0; i < sliceCount; i++) {
        EuThread::ThreadId threadId(1, i, 0, 0, 0);
        sessionMock->allThreads[threadId]->stopThread(1u);
    }

    stopped = sessionMock->areRequestedThreadsStopped(allSlices);
    EXPECT_TRUE(stopped);
}

struct DebugSessionRegistersAccess {
    void setUp() {
        zet_debug_config_t config = {};
        config.pid = 0x1234;
        auto hwInfo = *NEO::defaultHwInfo.get();

        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0);
        deviceImp = std::make_unique<Mock<L0::DeviceImp>>(neoDevice, neoDevice->getExecutionEnvironment());

        session = std::make_unique<MockDebugSession>(config, deviceImp.get());

        session->allThreads[stoppedThreadId]->stopThread(1u);
    }

    void tearDown() {
    }

    void dumpRegisterState() {
        if (session->stateSaveAreaHeader.size() == 0) {
            return;
        }
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());

        for (uint32_t thread = 0; thread < pStateSaveAreaHeader->regHeader.num_threads_per_eu; thread++) {
            EuThread::ThreadId threadId(0, 0, 0, 0, thread);
            auto threadSlotOffset = session->calculateThreadSlotOffset(threadId);

            auto srMagicOffset = threadSlotOffset + pStateSaveAreaHeader->regHeader.sr_magic_offset;
            SIP::sr_ident srMagic;
            srMagic.count = 1;
            srMagic.version.major = pStateSaveAreaHeader->versionHeader.version.major;

            session->writeGpuMemory(0, reinterpret_cast<char *>(&srMagic), sizeof(srMagic), reinterpret_cast<uint64_t>(pStateSaveAreaHeader) + srMagicOffset);
        }
    }

    ze_device_thread_t stoppedThread = {0, 0, 0, 0};
    EuThread::ThreadId stoppedThreadId{0, stoppedThread};
    std::unique_ptr<MockDebugSession> session;
    std::unique_ptr<Mock<L0::DeviceImp>> deviceImp;
    NEO::MockDevice *neoDevice = nullptr;
};

using DebugSessionRegistersAccessTest = Test<DebugSessionRegistersAccess>;

TEST_F(DebugSessionRegistersAccessTest, givenTypeToRegsetDescCalledThenCorrectRegdescIsReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU), &pStateSaveAreaHeader->regHeader.grf);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU), &pStateSaveAreaHeader->regHeader.addr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU), &pStateSaveAreaHeader->regHeader.flag);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU), &pStateSaveAreaHeader->regHeader.emask);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU), &pStateSaveAreaHeader->regHeader.sr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), &pStateSaveAreaHeader->regHeader.cr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU), &pStateSaveAreaHeader->regHeader.tdr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU), &pStateSaveAreaHeader->regHeader.acc);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU), &pStateSaveAreaHeader->regHeader.mme);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU), &pStateSaveAreaHeader->regHeader.sp);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU), &pStateSaveAreaHeader->regHeader.dbg);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU), &pStateSaveAreaHeader->regHeader.fc);
    EXPECT_NE(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU), nullptr);
}

TEST_F(DebugSessionRegistersAccessTest, givenValidRegisterWhenGettingSizeThenCorrectSizeIsReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();
    EXPECT_EQ(pStateSaveAreaHeader->regHeader.grf.bytes, session->getRegisterSize(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidRegisterWhenGettingSizeThenZeroSizeIsReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(0u, session->getRegisterSize(ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU));
}

TEST_F(DebugSessionRegistersAccessTest, givenUnsupportedRegisterTypeWhenReadRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, 0x12345, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenUnsupportedRegisterTypeWhenWriteRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, 0x12345, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidRegistersIndicesWhenReadRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 128, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 127, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidRegistersIndicesWhenWriteRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 128, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 127, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenThreadAllWhenReadWriteRegistersCalledThenErrorNotAvailableIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t threadAll = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugReadRegisters(session->toHandle(), threadAll, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugWriteRegisters(session->toHandle(), threadAll, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenNotReportedRegisterSetAndValidRegisterIndicesWhenReadRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    // MME is not present
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenNotReportedRegisterSetAndValidRegisterIndicesWhenWriteRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    // MME is not present
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidSbaRegistersIndicesWhenReadSbaRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 9, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 8, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenWriteSbaRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoneThreadsStoppedWhenReadRegistersCalledThenErrorNotAvailableReturned) {
    session->areRequestedThreadsStoppedReturnValue = 0;
    session->allThreads[stoppedThreadId]->resumeThread();
    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoneThreadsStoppedWhenWriteRegistersCalledThenErrorNotAvailableReturned) {
    session->areRequestedThreadsStoppedReturnValue = 0;
    session->allThreads[stoppedThreadId]->resumeThread();
    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaWhenReadRegistersCalledThenErrorUnknownReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaWhenWriteRegistersCalledThenErrorUnknownReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaGpuVaWhenRegistersAccessHelperCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    session->returnStateSaveAreaGpuVa = false;
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenTssMagicCorruptedWhenRegistersAccessHelperCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    session->stateSaveAreaHeader[0] = '!';
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenWriteGpuMemoryErrorWhenRegistersAccessHelperCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    session->writeMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, true);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenReadGpuMemoryErrorWhenRegistersAccessHelperCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    session->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, true);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaWhenReadRegisterCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    session->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, true);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, GivenSipVersion2WhenWritingResumeCommandThenCmdRegIsWritten) {
    const uint32_t resumeValue = 0;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    EuThread::ThreadId thread3(0, 0, 0, 0, 3);

    std::vector<EuThread::ThreadId> threads;
    threads.push_back(thread0);
    threads.push_back(thread3);

    dumpRegisterState();

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.cmd;
    SIP::sip_command resumeCommand = {0};
    resumeCommand.command = 11;
    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, &resumeCommand, true);

    session->skipWriteResumeCommand = false;
    session->writeResumeCommand(threads);

    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, &resumeCommand, false);
    EXPECT_EQ(resumeValue, resumeCommand.command);

    resumeCommand.command = 11;
    session->registersAccessHelper(session->allThreads[thread3].get(), regdesc, 0, 1, &resumeCommand, false);
    EXPECT_EQ(resumeValue, resumeCommand.command);
}

TEST_F(DebugSessionRegistersAccessTest, GivenBindlessSipVersion2WhenCallingResumeThenResumeInCmdRegisterIsWritten) {
    session->debugArea.reserved1 = 1u;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }
    session->skipWriteResumeCommand = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    session->allThreads[threadId]->stopThread(1u);

    dumpRegisterState();

    auto result = session->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(0u, session->readRegistersCallCount);
    EXPECT_EQ(0u, session->writeRegistersCallCount);
    EXPECT_EQ(1u, session->writeResumeCommandCalled);
}

TEST_F(DebugSessionRegistersAccessTest, WhenReadingSbaRegistersThenCorrectAddressesAreReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    ze_device_thread_t thread = {0, 0, 0, 0};
    ze_device_thread_t thread1 = {0, 0, 0, 1};
    EuThread::ThreadId threadId(0, thread);
    EuThread::ThreadId threadId1(0, thread1);
    session->allThreads[threadId]->stopThread(1u);
    session->allThreads[threadId1]->stopThread(1u);

    dumpRegisterState();

    auto threadSlotOffset = session->calculateThreadSlotOffset(threadId);
    auto stateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    auto grfOffset = threadSlotOffset + stateSaveAreaHeader->regHeader.grf.offset;
    uint32_t *r0Thread0 = reinterpret_cast<uint32_t *>(ptrOffset(session->stateSaveAreaHeader.data(), grfOffset));
    r0Thread0[4] = 0x20 << 5;

    threadSlotOffset = session->calculateThreadSlotOffset(threadId1);
    grfOffset = threadSlotOffset + stateSaveAreaHeader->regHeader.grf.offset;
    uint32_t *r0Thread1 = reinterpret_cast<uint32_t *>(ptrOffset(session->stateSaveAreaHeader.data(), grfOffset));
    r0Thread1[4] = 0x24 << 5;

    uint64_t sba[ZET_DEBUG_SBA_COUNT_INTEL_GPU];
    uint64_t sbaExpected[ZET_DEBUG_SBA_COUNT_INTEL_GPU];

    for (uint32_t i = 0; i < ZET_DEBUG_SBA_COUNT_INTEL_GPU; i++) {
        sbaExpected[i] = i * 0x1000;
    }

    sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU] = reinterpret_cast<uint64_t>(session->readMemoryBuffer);

    session->sba.InstructionBaseAddress = sbaExpected[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU];
    session->sba.IndirectObjectBaseAddress = sbaExpected[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU];
    session->sba.GeneralStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU];
    session->sba.DynamicStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU];
    session->sba.BindlessSurfaceStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU];
    session->sba.BindlessSamplerStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU];
    session->sba.SurfaceStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU];

    auto scratchAllocationBase = 0ULL;
    if (gfxCoreHelper.isScratchSpaceSurfaceStateAccessible()) {
        const uint32_t ptss = 128;
        gfxCoreHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                              &session->readMemoryBuffer[1 * (gfxCoreHelper.getRenderSurfaceStateSize() / sizeof(std::decay<decltype(MockDebugSession::readMemoryBuffer[0])>::type))], 1, scratchAllocationBase, 0,
                                                              ptss, nullptr, false, 6, false, true);

        r0Thread0[5] = 1 << 10; // first surface state
    } else {
        r0Thread0[5] = 0;
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readSbaRegisters(threadId, 0, ZET_DEBUG_SBA_COUNT_INTEL_GPU, sba));
    EXPECT_EQ(0ULL, sba[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU]);

    scratchAllocationBase = sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU];
    uint64_t scratchAllocationBase2 = (1ULL << 47) + 0x12000u;
    auto gmmHelper = neoDevice->getGmmHelper();
    auto scratchAllocationBase2Canonized = gmmHelper->canonize(scratchAllocationBase2);

    if (gfxCoreHelper.isScratchSpaceSurfaceStateAccessible()) {
        const uint32_t ptss = 128;
        gfxCoreHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                              &session->readMemoryBuffer[1 * (gfxCoreHelper.getRenderSurfaceStateSize() / sizeof(std::decay<decltype(MockDebugSession::readMemoryBuffer[0])>::type))], 1, scratchAllocationBase, 0,
                                                              ptss, nullptr, false, 6, false, true);

        r0Thread0[5] = 1 << 10; // first surface state

        sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU] = scratchAllocationBase;
    } else {
        r0Thread0[3] = 1; // (2) ^ 1
        r0Thread0[5] = 2 << 10;
        sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU] = ((r0Thread0[5] >> 10) << 10) + sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU];
    }

    sbaExpected[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU] = ((r0Thread0[4] >> 5) << 5) + sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU];

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readSbaRegisters(threadId, 0, ZET_DEBUG_SBA_COUNT_INTEL_GPU, sba));

    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU], sba[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU], sba[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU], sba[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU]);

    if (gfxCoreHelper.isScratchSpaceSurfaceStateAccessible()) {
        const uint32_t ptss = 128;
        gfxCoreHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                              &session->readMemoryBuffer[2 * (gfxCoreHelper.getRenderSurfaceStateSize() / sizeof(std::decay<decltype(MockDebugSession::readMemoryBuffer[0])>::type))], 1, scratchAllocationBase2Canonized, 0,
                                                              ptss, nullptr, false, 6, false, true);

        r0Thread1[5] = 2 << 10; // second surface state

        sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU] = scratchAllocationBase2 + ptss;
    } else {
        r0Thread1[3] = 2; // (2) ^ 2
        r0Thread1[5] = 2 << 10;
        sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU] = ((r0Thread1[5] >> 10) << 10) + sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU];
    }

    sbaExpected[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU] = ((r0Thread1[4] >> 5) << 5) + sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU];

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readSbaRegisters(threadId1, 0, ZET_DEBUG_SBA_COUNT_INTEL_GPU, sba));

    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU], sba[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU], sba[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU], sba[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU]);

    if (gfxCoreHelper.isScratchSpaceSurfaceStateAccessible()) {
        const uint32_t ptss = 0;
        gfxCoreHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                              &session->readMemoryBuffer[0], 1, 0xdeadbeef, 0,
                                                              ptss, nullptr, false, 6, false, true);

        r0Thread1[5] = 0; // Surface state at index 0
    } else {
        r0Thread1[3] = 0;
        r0Thread1[5] = 0;
    }
    sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU] = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readSbaRegisters(threadId1, 0, ZET_DEBUG_SBA_COUNT_INTEL_GPU, sba));
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU], sba[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU]);
}

TEST_F(DebugSessionRegistersAccessTest, GivenBindlessSipWhenCheckingDifferentSipVersionsThenExpectedResultIsReturned) {
    session->debugArea.reserved1 = 1u;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    session->allThreads[threadId]->stopThread(1u);

    dumpRegisterState();
    session->minSlmSipVersion = {2, 2, 2};
    // test tssarea version
    // Major version cases
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.patch = 2;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, true);

    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 1;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, false);

    // Minor version cases
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 1;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, false);

    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 3;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 1;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, true);

    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 3;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 2;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, true);

    // patch version cases
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.patch = 1;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, false);

    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 3;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.patch = 1;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, true);

    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 3;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.patch = 2;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, true);

    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 3;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.patch = 1;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, true);

    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 3;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.patch = 2;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, true);

    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 3;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 3;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.patch = 1;
    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, true);
}

TEST(DebugSessionTest, GivenStoppedThreadWhenValidAddressesSizesAndOffsetsThenSlmReadIsSuccessful) {

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId threadId(0, 0, 0, 0, 0);
    sessionMock->allThreads[threadId]->stopThread(1u);

    int i;
    for (i = 0; i < sessionMock->slmSize; i++) {
        sessionMock->slmMemory[i] = i & 127;
    }

    zet_debug_memory_space_desc_t desc;
    desc.address = 0x10000000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;

    char output[EXCHANGE_BUFFER_SIZE * 3];
    memset(output, 0, EXCHANGE_BUFFER_SIZE * 3);

    sessionMock->slmTesting = true;
    sessionMock->sipSupportsSlm = true;

    int readSize = 7;
    auto retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(memcmp(output, sessionMock->slmMemory, readSize), 0);
    memset(output, 0, readSize);

    int offset = 0x05;
    desc.address = 0x10000000 + offset;
    retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(memcmp(output, sessionMock->slmMemory + offset, readSize), 0);
    memset(output, 0, readSize);

    offset = 0x0f;
    desc.address = 0x10000000 + offset;
    retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(memcmp(output, sessionMock->slmMemory + offset, readSize), 0);
    memset(output, 0, readSize);

    readSize = 132;
    retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(memcmp(output, sessionMock->slmMemory + offset, readSize), 0);
    memset(output, 0, readSize);

    readSize = 230;
    offset = 0x0a;
    desc.address = 0x10000000 + offset;
    retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(memcmp(output, sessionMock->slmMemory + offset, readSize), 0);

    sessionMock->slmTesting = false;
}

TEST(DebugSessionTest, GivenStoppedThreadWhenUnderInvalidConditionsThenSlmReadFailsGracefully) {

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId threadId(0, 0, 0, 0, 0);
    sessionMock->allThreads[threadId]->stopThread(1u);

    sessionMock->skipWriteResumeCommand = false;

    zet_debug_memory_space_desc_t desc;
    desc.address = 0x10000000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;

    constexpr int readSize = EXCHANGE_BUFFER_SIZE * 2;
    char output[readSize];
    ze_result_t retVal;

    sessionMock->slmTesting = true;

    memcpy_s(sessionMock->originalSlmMemory, sessionMock->slmSize, sessionMock->slmMemory, sessionMock->slmSize);

    sessionMock->sipSupportsSlm = false;

    retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, retVal);

    sessionMock->sipSupportsSlm = true;

    sessionMock->resumeImpResult = ZE_RESULT_ERROR_UNKNOWN;
    retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    sessionMock->resumeImpResult = ZE_RESULT_SUCCESS;

    sessionMock->slmCmdRegisterAccessCount = 0;
    sessionMock->slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);

    sessionMock->forceCmdAccessFail = true;
    retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_FORCE_UINT32, retVal);
    sessionMock->forceCmdAccessFail = false;

    memcpy_s(sessionMock->slmMemory, strlen("FailReadingData"), "FailReadingData", strlen("FailReadingData"));
    retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_FORCE_UINT32, retVal);
    memcpy_s(sessionMock->slmMemory, strlen("FailReadingData"), sessionMock->originalSlmMemory, strlen("FailReadingData"));

    sessionMock->slmCmdRegisterAccessReadyCount = 13;
    retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);
    sessionMock->slmCmdRegisterAccessReadyCount = 2;
    sessionMock->slmCmdRegisterAccessCount = 0;
    sessionMock->slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);

    memcpy_s(sessionMock->slmMemory, strlen("FailWaiting"), "FailWaiting", strlen("FailWaiting"));
    retVal = sessionMock->slmMemoryAccess<void *, false>(threadId, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_FORCE_UINT32, retVal);
    memcpy_s(sessionMock->slmMemory, strlen("FailWaiting"), sessionMock->originalSlmMemory, strlen("FailWaiting"));

    sessionMock->slmTesting = false;
}

TEST(DebugSessionTest, GivenStoppedThreadWhenValidAddressesSizesAndOffsetsThenSlmWriteIsSuccessful) {

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId threadId(0, 0, 0, 0, 0);
    sessionMock->allThreads[threadId]->stopThread(1u);

    sessionMock->slmTesting = true;
    sessionMock->sipSupportsSlm = true;

    int i;
    for (i = 0; i < sessionMock->slmSize; i++) {
        sessionMock->slmMemory[i] = i & 127;
    }
    memcpy_s(sessionMock->originalSlmMemory, sessionMock->slmSize, sessionMock->slmMemory, sessionMock->slmSize);

    zet_debug_memory_space_desc_t desc;
    desc.address = 0x10000000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;

    char input1[EXCHANGE_BUFFER_SIZE * 3];
    char input2[EXCHANGE_BUFFER_SIZE * 3];
    ze_result_t retVal;

    int inputSize = 7;
    memset(input1, 0xff, inputSize);

    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, inputSize, input1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(memcmp(sessionMock->slmMemory, input1, inputSize), 0);
    EXPECT_EQ(memcmp(sessionMock->slmMemory + inputSize + 1, sessionMock->originalSlmMemory + inputSize + 1, sessionMock->slmSize - inputSize - 1), 0);

    memset(input2, 0xbb, inputSize);
    int offset = 0x05;
    desc.address = 0x10000000 + offset;
    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, inputSize, input2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    EXPECT_EQ(memcmp(sessionMock->slmMemory, input1, offset), 0);
    EXPECT_EQ(memcmp(sessionMock->slmMemory + offset, input2, inputSize), 0);
    EXPECT_EQ(memcmp(sessionMock->slmMemory + offset + inputSize + 1, sessionMock->originalSlmMemory + offset + inputSize + 1,
                     sessionMock->slmSize - offset - inputSize - 1),
              0);
    inputSize = 23;
    desc.address = 0x10000000;
    memset(input1, 0x0a, inputSize);

    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, inputSize, input1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    EXPECT_EQ(memcmp(sessionMock->slmMemory, input1, inputSize), 0);
    EXPECT_EQ(memcmp(sessionMock->slmMemory + inputSize + 1, sessionMock->originalSlmMemory + inputSize + 1, sessionMock->slmSize - inputSize - 1), 0);

    inputSize = 25;
    offset = 0x07;
    desc.address = 0x10000000 + offset;

    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, inputSize, input2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    EXPECT_EQ(memcmp(sessionMock->slmMemory, input1, offset), 0);
    EXPECT_EQ(memcmp(sessionMock->slmMemory + offset, input2, inputSize), 0);
    EXPECT_EQ(memcmp(sessionMock->slmMemory + offset + inputSize + 1, sessionMock->originalSlmMemory + offset + inputSize + 1,
                     sessionMock->slmSize - offset - inputSize - 1),
              0);

    memcpy_s(sessionMock->slmMemory, offset + inputSize, sessionMock->originalSlmMemory, offset + inputSize);

    inputSize = 27;
    offset = 0x0f;
    desc.address = 0x10000000 + offset;

    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, inputSize, input1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    EXPECT_EQ(memcmp(sessionMock->slmMemory, sessionMock->originalSlmMemory, offset), 0);
    EXPECT_EQ(memcmp(sessionMock->slmMemory + offset, input1, inputSize), 0);
    EXPECT_EQ(memcmp(sessionMock->slmMemory + offset + inputSize + 1, sessionMock->originalSlmMemory + offset + inputSize + 1,
                     sessionMock->slmSize - offset - inputSize - 1),
              0);

    inputSize = 230;
    offset = 0x0a;
    desc.address = 0x10000000 + offset;

    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, inputSize, input2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    EXPECT_EQ(memcmp(sessionMock->slmMemory, sessionMock->originalSlmMemory, offset), 0);
    EXPECT_EQ(memcmp(sessionMock->slmMemory + offset, input2, inputSize), 0);
    EXPECT_EQ(memcmp(sessionMock->slmMemory + offset + inputSize + 1, sessionMock->originalSlmMemory + offset + inputSize + 1,
                     sessionMock->slmSize - offset - inputSize - 1),
              0);

    sessionMock->slmTesting = false;
}

TEST(DebugSessionTest, GivenStoppedThreadWhenUnderInvalidConditionsThenSlmWriteFailsGracefully) {

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId threadId(0, 0, 0, 0, 0);
    sessionMock->allThreads[threadId]->stopThread(1u);

    sessionMock->slmTesting = true;

    zet_debug_memory_space_desc_t desc;
    desc.address = 0x10000000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;

    constexpr int writeSize = 16;
    char input[writeSize];
    ze_result_t retVal;

    sessionMock->sipSupportsSlm = false;

    retVal = sessionMock->slmMemoryAccess<void *, true>(threadId, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, retVal);

    sessionMock->sipSupportsSlm = true;

    sessionMock->resumeImpResult = ZE_RESULT_ERROR_UNKNOWN;
    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    sessionMock->resumeImpResult = ZE_RESULT_SUCCESS;

    sessionMock->forceCmdAccessFail = true;
    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_FORCE_UINT32, retVal);
    sessionMock->forceCmdAccessFail = false;

    sessionMock->slmCmdRegisterAccessReadyCount = 13;
    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);

    sessionMock->slmCmdRegisterAccessCount = 0;
    sessionMock->slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);

    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);

    desc.address = 0x1000000f; // force a read
    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);
    sessionMock->slmCmdRegisterAccessReadyCount = 2;
    sessionMock->slmCmdRegisterAccessCount = 0;
    sessionMock->slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);

    desc.address = 0x10000000;
    memcpy_s(sessionMock->slmMemory, strlen("FailWaiting"), "FailWaiting", strlen("FailWaiting"));
    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_FORCE_UINT32, retVal);
    memcpy_s(sessionMock->slmMemory, strlen("FailWaiting"), sessionMock->originalSlmMemory, strlen("FailWaiting"));

    sessionMock->slmTesting = false;
}

TEST(DebugSessionTest, givenCanonicalOrDecanonizedAddressWhenCheckingValidAddressThenTrueReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    zet_debug_memory_space_desc_t desc;
    desc.address = 0xffffffff12345678;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    EXPECT_TRUE(sessionMock->isValidGpuAddress(&desc));

    auto gmmHelper = neoDevice->getGmmHelper();
    auto decanonized = gmmHelper->decanonize(desc.address);
    desc.address = decanonized;
    EXPECT_TRUE(sessionMock->isValidGpuAddress(&desc));
}

TEST(DebugSessionTest, givenInvalidAddressWhenCheckingValidAddressThenFalseIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    auto gmmHelper = neoDevice->getGmmHelper();
    auto addressWidth = gmmHelper->getAddressWidth();
    uint64_t address = maxNBitValue(addressWidth) << 1 | 0x4000;

    zet_debug_memory_space_desc_t desc;
    desc.address = address;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    EXPECT_FALSE(sessionMock->isValidGpuAddress(&desc));

    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_FORCE_UINT32;
    EXPECT_FALSE(sessionMock->isValidGpuAddress(&desc));
}

TEST(DebugSessionTest, givenDebugSessionWhenGettingTimeDiffThenDiffReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSession>(config, nullptr);

    auto now = std::chrono::high_resolution_clock::now();
    auto diff = sessionMock->getTimeDifferenceMilliseconds(now);

    EXPECT_LE(0, diff);
}

TEST(DebugSessionTest, givenDebugSessionWhenGetContextSaveAreaGpuVaCalledThenZeroIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSession>(config, nullptr);
    sessionMock->returnStateSaveAreaGpuVa = false;

    EXPECT_EQ(0ULL, sessionMock->getContextStateSaveAreaGpuVa(1ULL));
}

TEST(DebugSessionTest, givenNewAttentionRaisedWithNoExpectedAttentionThenExpectedAttentionNotChanged) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSession>(config, nullptr);
    EXPECT_EQ(0UL, sessionMock->expectedAttentionEvents);
    sessionMock->newAttentionRaised(0);
    EXPECT_EQ(0UL, sessionMock->expectedAttentionEvents);
}

TEST(DebugSessionTest, givenNoPendingInterruptsOrNoNewlyStoppedThreadsAndCheckTriggerEventsForAttentionThenDoNotTriggerEvents) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSession>(config, nullptr);

    sessionMock->triggerEvents = false;
    sessionMock->expectedAttentionEvents = 0;
    sessionMock->pendingInterrupts.clear();
    sessionMock->newlyStoppedThreads.clear();

    sessionMock->checkTriggerEventsForAttention();
    EXPECT_FALSE(sessionMock->triggerEvents);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});

    sessionMock->checkTriggerEventsForAttention();
    EXPECT_TRUE(sessionMock->triggerEvents);

    EuThread::ThreadId thread = {0, 0, 0, 0, 1};
    sessionMock->pendingInterrupts.clear();
    sessionMock->newlyStoppedThreads.push_back(thread);

    EXPECT_TRUE(sessionMock->triggerEvents);
}

TEST(DebugSessionTest, givenNoPendingInterruptsWhenSendInterruptsCalledThenNoInterruptsSent) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSession>(config, nullptr);
    sessionMock->interruptSent = false;
    sessionMock->pendingInterrupts.clear();
    sessionMock->sendInterrupts();
    EXPECT_FALSE(sessionMock->interruptSent);
}

TEST(DebugSessionTest, GivenRootDeviceAndTileAttachWhenDebugSessionIsCreatedThenThreadsAreNotCreated) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp, false);
    ASSERT_NE(nullptr, sessionMock);

    EXPECT_EQ(0u, sessionMock->allThreads.size());
}

TEST_F(MultiTileDebugSessionTest, GivenSubDeviceAndTileAttachWhenRootDeviceDebugSessionCreateFailsThenTileAttachFails) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    if (!hwInfo.capabilityTable.l0DebuggerSupported) {
        GTEST_SKIP();
    }
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    device->getNEODevice()->getExecutionEnvironment()->releaseRootDeviceEnvironmentResources(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);

    auto osInterface = new OsInterfaceWithDebugAttach;
    osInterface->debugAttachAvailable = false;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();

    zet_debug_session_handle_t debugSesion = nullptr;
    auto retVal = zetDebugAttach(subDevice0->toHandle(), &config, &debugSesion);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, retVal);
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenGettingDebugPropertiesThenDebugAttachIsSetForRootAndSubdevices) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    if (!hwInfo.capabilityTable.l0DebuggerSupported) {
        GTEST_SKIP();
    }

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    device->getNEODevice()->getExecutionEnvironment()->releaseRootDeviceEnvironmentResources(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    auto subDevice1 = neoDevice->getSubDevice(1)->getSpecializedDevice<Device>();

    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(zet_device_debug_property_flag_t::ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);

    result = zetDeviceGetDebugProperties(subDevice0->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(zet_device_debug_property_flag_t::ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);

    result = zetDeviceGetDebugProperties(subDevice1->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(zet_device_debug_property_flag_t::ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenAttachingToRootDeviceThenTileAttachIsDisabledAndSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession = nullptr;

    L0::Device *device = driverHandle->devices[0];
    auto deviceImp = static_cast<DeviceImp *>(device);
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);
    auto sessionMock = new MockDebugSession(config, device, true);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, debugSession);
    EXPECT_TRUE(sessionMock->isAttached());
    EXPECT_FALSE(sessionMock->tileAttachEnabled);
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenAttachingToTileDevicesThenDebugSessionForRootIsCreatedAndTileSessionsAreReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr, debugSession1 = nullptr;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    auto deviceImp = static_cast<DeviceImp *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    auto subDevice1 = neoDevice->getSubDevice(1)->getSpecializedDevice<Device>();
    auto tileSession1 = static_cast<MockDebugSession *>(sessionMock->tileSessions[1].first);

    auto result = zetDebugAttach(subDevice0->toHandle(), &config, &debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    EXPECT_TRUE(sessionMock->tileSessions[0].second);
    EXPECT_EQ(sessionMock->tileSessions[0].first, L0::DebugSession::fromHandle(debugSession0));
    EXPECT_TRUE(sessionMock->tileSessions[0].first->isAttached());

    EXPECT_NE(nullptr, deviceImp->getDebugSession(config));

    EXPECT_FALSE(tileSession1->attachTileCalled);
    result = zetDebugAttach(subDevice1->toHandle(), &config, &debugSession1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession1);

    EXPECT_TRUE(sessionMock->tileSessions[1].second);
    EXPECT_EQ(sessionMock->tileSessions[1].first, L0::DebugSession::fromHandle(debugSession1));
    EXPECT_TRUE(sessionMock->tileSessions[1].first->isAttached());

    EXPECT_TRUE(tileSession1->attachTileCalled);
    EXPECT_FALSE(tileSession1->detachTileCalled);

    result = zetDebugDetach(debugSession1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, deviceImp->getDebugSession(config));
    EXPECT_FALSE(sessionMock->tileSessions[1].second);
    EXPECT_FALSE(sessionMock->tileSessions[1].first->isAttached());

    EXPECT_TRUE(tileSession1->detachTileCalled);
    ASSERT_EQ(1u, sessionMock->cleanRootSessionDeviceIndices.size());
    EXPECT_EQ(1u, sessionMock->cleanRootSessionDeviceIndices[0]);

    result = zetDebugDetach(debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(nullptr, deviceImp->getDebugSession(config));
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenTileAttachFailsDuringDebugSessionCreateThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    auto deviceImp = static_cast<DeviceImp *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    sessionMock->tileSessions[0].second = true;

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    ze_result_t result = ZE_RESULT_SUCCESS;

    auto debugSession = subDevice0->createDebugSession(config, result, false);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    EXPECT_EQ(nullptr, debugSession);
}

TEST_F(MultiTileDebugSessionTest, givenAttachedTileDeviceWhenAttachingToRootDeviceThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr, debugSession1 = nullptr;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    auto deviceImp = static_cast<DeviceImp *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    auto result = zetDebugAttach(subDevice0->toHandle(), &config, &debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    result = zetDebugAttach(deviceImp->toHandle(), &config, &debugSession1);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    result = zetDebugDetach(debugSession0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MultiTileDebugSessionTest, givenAttachedRootDeviceWhenAttachingToTiletDeviceThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr, debugSessionRoot = nullptr;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    auto deviceImp = static_cast<DeviceImp *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, true);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();

    auto result = zetDebugAttach(deviceImp->toHandle(), &config, &debugSessionRoot);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetDebugAttach(subDevice0->toHandle(), &config, &debugSession0);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    EXPECT_EQ(nullptr, debugSession0);

    result = zetDebugDetach(debugSessionRoot);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

struct AffinityMaskForSingleSubDevice : MultipleDevicesWithCustomHwInfo {
    void setUp() {
        DebugManager.flags.ZE_AFFINITY_MASK.set("0.1");
        MultipleDevicesWithCustomHwInfo::numSubDevices = 2;
        MultipleDevicesWithCustomHwInfo::setUp();
    }

    void tearDown() {
        MultipleDevicesWithCustomHwInfo::tearDown();
    }
    DebugManagerStateRestore restorer;
};

using AffinityMaskForSingleSubDeviceTest = Test<AffinityMaskForSingleSubDevice>;

TEST_F(AffinityMaskForSingleSubDeviceTest, givenDeviceDebugSessionWhenSendingInterruptsThenInterruptIsSentWithCorrectDeviceIndex) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;
    ze_device_thread_t apiThread = {0, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    EXPECT_TRUE(sessionMock->interruptSent);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[0]);
}

} // namespace ult
} // namespace L0
