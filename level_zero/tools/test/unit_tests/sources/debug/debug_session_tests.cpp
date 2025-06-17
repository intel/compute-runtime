/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_registers_access.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"
#include "level_zero/zet_intel_gpu_debug.h"

#include "common/StateSaveAreaHeader.h"
#include "encode_surface_state_args.h"

namespace L0 {
namespace ult {

using DebugSessionTest = ::testing::Test;

TEST(DeviceWithDebugSessionTest, GivenSlicesEnabledWithEarlierSlicesDisabledThenAllThreadsIsPopulatedCorrectly) {
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.gtSystemInfo.MaxSlicesSupported = 2;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 8;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 64;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = 32;
    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    hwInfo.gtSystemInfo.MaxSlicesSupported = 2;
    for (auto &sliceInfo : hwInfo.gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }
    hwInfo.gtSystemInfo.SliceInfo[2].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[3].Enabled = true;

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto sessionMock = std::make_unique<MockDebugSession>(zet_debug_config_t{0x1234}, &deviceImp);
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxDualSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    EXPECT_EQ(1u, sessionMock->allThreads.count(EuThread::ThreadId(0, 0, numSubslicesPerSlice - 1, 0, 0)));
    EXPECT_EQ(1u, sessionMock->allThreads.count(EuThread::ThreadId(0, 1, numSubslicesPerSlice - 1, 7, 0)));
    EXPECT_EQ(1u, sessionMock->allThreads.count(EuThread::ThreadId(0, 2, numSubslicesPerSlice - 1, 0, 0)));
}

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
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread]->reportAsStopped();
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
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->interruptImpResult = ZE_RESULT_ERROR_UNKNOWN;
    ze_device_thread_t apiThread = {0, 0, 0, 1};
    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    EXPECT_EQ(1u, sessionMock->interruptImpCalled);

    EXPECT_EQ(1u, sessionMock->apiEvents.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->apiEvents.front().type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->apiEvents.front().info.thread.thread));

    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
}

TEST(DebugSessionTest, givenPendingInteruptWhenHandlingThreadWithAttentionThenPendingInterruptIsMarkedComplete) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 0};
    EuThread::ThreadId thread(0, 0, 0, 0, 0);

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(thread, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_TRUE(sessionMock->allThreads[thread]->isStopped());

    EXPECT_FALSE(sessionMock->pendingInterrupts[0].second);
    EXPECT_EQ(1u, sessionMock->newlyStoppedThreads.size());

    sessionMock->triggerEvents = true;

    std::vector<EuThread::ThreadId> resumeThreads;
    std::vector<EuThread::ThreadId> stoppedThreadsToReport;
    std::vector<EuThread::ThreadId> interruptedThreads;

    sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreadsToReport, interruptedThreads);
    EXPECT_EQ(1u, interruptedThreads.size());
    EXPECT_TRUE(sessionMock->pendingInterrupts[0].second);

    sessionMock->generateEventsForPendingInterrupts();

    ASSERT_EQ(1u, sessionMock->apiEvents.size());

    zet_debug_event_t debugEvent = {};
    sessionMock->readEvent(0, &debugEvent);

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, debugEvent.type);
    EXPECT_EQ(apiThread.thread, debugEvent.info.thread.thread.thread);
    EXPECT_EQ(apiThread.eu, debugEvent.info.thread.thread.eu);
    EXPECT_EQ(apiThread.subslice, debugEvent.info.thread.thread.subslice);
    EXPECT_EQ(apiThread.slice, debugEvent.info.thread.thread.slice);

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenPreviouslyStoppedThreadAndPendingInterruptWhenHandlingThreadWithAttentionThenPendingInterruptIsNotMarkedCompleteAndInterruptGeneratesUnavailableEvent) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 0};
    EuThread::ThreadId thread(0, 0, 0, 0, 0);

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    // stop thread for a reason different than interrupt exception (forceException)
    sessionMock->onlyForceException = false;
    sessionMock->allThreads[thread]->stopThread(1u);

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(thread, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_TRUE(sessionMock->allThreads[thread]->isStopped());

    EXPECT_FALSE(sessionMock->pendingInterrupts[0].second);
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());

    sessionMock->expectedAttentionEvents = 0;
    sessionMock->checkTriggerEventsForAttention();
    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(1u, sessionMock->apiEvents.size());
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->apiEvents.front().type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->apiEvents.front().info.thread.thread));
}

TEST(DebugSessionTest, givenThreadsStoppedOnBreakpointAndInterruptedWhenHandlingThreadsStateThenThreadsWithBreakpointExceptionsHaveDistinctEventsTriggered) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    sessionMock->callBaseIsForceExceptionOrForceExternalHaltOnlyExceptionReason = true;

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        sessionMock->stateSaveAreaHeader.resize(size);
    }

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data()))->regHeader.cr;
    uint32_t cr0[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    ze_device_thread_t interruptThread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    ze_device_thread_t bpThread = {0, 0, 0, 0};
    EuThread::ThreadId threadWithBp(0, 0, 0, 0, 0);
    EuThread::ThreadId threadWithFe(0, 0, 0, 0, 1);

    auto result = sessionMock->interrupt(interruptThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    cr0[1] = 1 << 15 | 1 << 31;
    sessionMock->registersAccessHelper(sessionMock->allThreads[threadWithBp].get(), regdesc, 0, 1, cr0, true);

    cr0[1] = 1 << 26;
    sessionMock->registersAccessHelper(sessionMock->allThreads[threadWithFe].get(), regdesc, 0, 1, cr0, true);

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(threadWithBp, 1u, sessionMock->stateSaveAreaHeader.data());
    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(threadWithFe, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_TRUE(sessionMock->allThreads[threadWithBp]->isStopped());
    EXPECT_TRUE(sessionMock->allThreads[threadWithFe]->isStopped());

    sessionMock->expectedAttentionEvents = 0;
    sessionMock->checkTriggerEventsForAttention();

    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(2u, sessionMock->apiEvents.size());

    uint32_t stoppedEvents = 0;
    bool interruptEventFound = false;
    bool bpThreadFound = false;
    for (uint32_t i = 0; i < 2; i++) {

        zet_debug_event_t outputEvent = {};
        auto result = sessionMock->readEvent(0, &outputEvent);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        if (result == ZE_RESULT_SUCCESS) {
            if (outputEvent.type == ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED) {
                stoppedEvents++;
                if (DebugSession::areThreadsEqual(interruptThread, outputEvent.info.thread.thread)) {
                    interruptEventFound = true;
                } else if (DebugSession::areThreadsEqual(bpThread, outputEvent.info.thread.thread)) {
                    bpThreadFound = true;
                }
            }
        }
    }

    EXPECT_EQ(2u, stoppedEvents);
    EXPECT_TRUE(bpThreadFound);
    EXPECT_TRUE(interruptEventFound);
}

TEST(DebugSessionTest, givenStoppedThreadWhenAddingNewlyStoppedThenThreadIsNotAdded) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    sessionMock->allThreads[thread]->stopThread(1u);

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(thread, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenNoPendingInterruptAndStoppedThreadWithForceExceptionOnlyWhenAddingNewlyStoppedThenThreadIsNotReportedAsStopped) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    sessionMock->skipReadSystemRoutineIdent = 1;
    sessionMock->threadStopped = true;
    sessionMock->onlyForceException = true;

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(thread, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_EQ(1u, sessionMock->newlyStoppedThreads.size());
    EXPECT_FALSE(sessionMock->allThreads[thread]->isReportedAsStopped());
}

TEST(DebugSessionTest, givenNoPendingInterruptAndStoppedThreadWhenGeneratingEventsFromStoppedThreadsThenThreadIsReportedAsStoppedAfterReadingEvent) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    sessionMock->skipReadSystemRoutineIdent = 1;
    sessionMock->threadStopped = true;
    sessionMock->onlyForceException = false;
    sessionMock->triggerEvents = true;

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(thread, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_EQ(1u, sessionMock->newlyStoppedThreads.size());
    EXPECT_FALSE(sessionMock->allThreads[thread]->isReportedAsStopped());

    sessionMock->generateEventsAndResumeStoppedThreads();
    EXPECT_FALSE(sessionMock->allThreads[thread]->isReportedAsStopped());

    EXPECT_EQ(1u, sessionMock->apiEvents.size());
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, sessionMock->apiEvents.front().type);

    zet_debug_event_t outputEvent;
    sessionMock->readEvent(0, &outputEvent);
    EXPECT_TRUE(sessionMock->allThreads[thread]->isReportedAsStopped());
}

TEST(DebugSessionTest, givenNoStoppedThreadWhenAddingNewlyStoppedThenThreadIsNotAdded) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->threadStopped = 0;

    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(thread, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenV3SipHeaderWhenCalculatingThreadOffsetThenCorrectResultReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp, true, 3);
    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    auto threadSlotOffset = sessionMock->calculateThreadSlotOffset(threadId);
    auto pStateSaveAreaHeader = reinterpret_cast<NEO::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
    size_t expectedOffset = pStateSaveAreaHeader->versionHeader.size * 8 + pStateSaveAreaHeader->regHeaderV3.state_area_offset + ((((threadId.slice * pStateSaveAreaHeader->regHeaderV3.num_subslices_per_slice + threadId.subslice) * pStateSaveAreaHeader->regHeaderV3.num_eus_per_subslice + threadId.eu) * pStateSaveAreaHeader->regHeaderV3.num_threads_per_eu + threadId.thread) * pStateSaveAreaHeader->regHeaderV3.state_save_size);
    EXPECT_EQ(threadSlotOffset, expectedOffset);
}

TEST(DebugSessionTest, givenSipHeaderGreaterThan3WhenCalculatingThreadOffsetThenZeroReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp, true, 3);
    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    auto versionHeader = &reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data())->versionHeader;
    versionHeader->version.major = 4;

    EXPECT_EQ(0u, sessionMock->calculateThreadSlotOffset(threadId));
}

TEST(DebugSessionTest, givenStoppedThreadAndNoSrMagicWhenAddingNewlyStoppedThenThreadIsNotAddedToNewlyStopped) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipReadSystemRoutineIdent = false;

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

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(threadId, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenStoppedThreadAndValidSrMagicWhenAddingNewlyStoppedThenThreadIsAddedToNewlyStopped) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipReadSystemRoutineIdent = false;

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
    SIP::sr_ident srMagic;
    srMagic.version.major = 2;
    srMagic.version.minor = 0;
    srMagic.count = 1;
    sessionMock->writeGpuMemory(0, reinterpret_cast<char *>(&srMagic), sizeof(srMagic), reinterpret_cast<uint64_t>(stateSaveAreaHeader) + srMagicOffset);

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(threadId, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_EQ(1u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenNoInterruptsSentWhenGenerateEventsAndResumeCalledThenTriggerEventsNotChanged) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    ze_device_thread_t apiThread2 = {0, 0, 1, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, true});

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(EuThread::ThreadId(0, apiThread2), 1u, sessionMock->stateSaveAreaHeader.data());

    sessionMock->triggerEvents = true;
    sessionMock->interruptSent = true;

    sessionMock->onlyForceException = false;

    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(2u, sessionMock->apiEvents.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, sessionMock->apiEvents.front().type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->apiEvents.front().info.thread.thread));

    sessionMock->apiEvents.pop();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, sessionMock->apiEvents.front().type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread2, sessionMock->apiEvents.front().info.thread.thread));

    EXPECT_EQ(1u, sessionMock->resumeAccidentallyStoppedCalled);
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenPendingInterruptAfterTimeoutWhenGenerateEventsAndResumeStoppedThreadsIsCalledThenEventsUnavailableAreSentAndFlagsSetFalse) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});

    sessionMock->triggerEvents = false;
    sessionMock->interruptSent = true;
    sessionMock->returnTimeDiff = 5 * sessionMock->interruptTimeout;

    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(1u, sessionMock->apiEvents.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->apiEvents.front().type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->apiEvents.front().info.thread.thread));

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});

    sessionMock->triggerEvents = false;
    sessionMock->interruptSent = true;
    sessionMock->returnTimeDiff = sessionMock->interruptTimeout / 2;

    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(0u, sessionMock->apiEvents.size());
    EXPECT_EQ(1u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
    EXPECT_TRUE(sessionMock->interruptSent);
}

TEST(DebugSessionTest, givenErrorFromReadSystemRoutineIdentWhenCheckingThreadStateThenThreadIsNotStopped) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->readSystemRoutineIdentRetVal = false;
    EuThread::ThreadId thread(0, 0, 0, 0, 0);

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(thread, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_FALSE(sessionMock->allThreads[thread]->isStopped());
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenPendingInterruptsWhenGeneratingEventsThenStoppedEventIsGeneratedForCompletedInterrupt) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    ze_device_thread_t apiThread2 = {0, 0, 1, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});
    sessionMock->pendingInterrupts.push_back({apiThread2, true});

    sessionMock->generateEventsForPendingInterrupts();

    EXPECT_EQ(2u, sessionMock->apiEvents.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->apiEvents.front().type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->apiEvents.front().info.thread.thread));
    sessionMock->apiEvents.pop();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, sessionMock->apiEvents.front().type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread2, sessionMock->apiEvents.front().info.thread.thread));

    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
}

TEST(DebugSessionTest, givenEmptyStateSaveAreaWhenGetStateSaveAreaCalledThenReadStateSaveAreaCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    decltype(sessionMock->stateSaveAreaHeader) emptyHeader;
    sessionMock->stateSaveAreaHeader.swap(emptyHeader);

    auto stateSaveArea = sessionMock->getStateSaveAreaHeader();
    EXPECT_EQ(nullptr, stateSaveArea);
    EXPECT_EQ(1u, sessionMock->readStateSaveAreaHeaderCalled);
}

TEST(DebugSessionTest, givenStoppedThreadsWhenFillingResumeAndStoppedThreadsFromNewlyStoppedThenContainerFilledBasedOnExceptionReason) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread = {0, 0, 0, 0, 1};
    EuThread::ThreadId thread2 = {0, 0, 0, 1, 1};
    EuThread::ThreadId thread3 = {0, 0, 0, 1, 2};

    sessionMock->newlyStoppedThreads.push_back(thread);
    sessionMock->newlyStoppedThreads.push_back(thread2);

    sessionMock->onlyForceException = true;

    {
        std::vector<EuThread::ThreadId> resumeThreads;
        std::vector<EuThread::ThreadId> stoppedThreads;
        std::vector<EuThread::ThreadId> interruptedThreads;

        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread2]->stopThread(1u);

        sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads, interruptedThreads);
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
        std::vector<EuThread::ThreadId> interruptedThreads;

        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread2]->stopThread(1u);

        sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads, interruptedThreads);
        EXPECT_EQ(0u, resumeThreads.size());
        EXPECT_EQ(2u, stoppedThreads.size());
    }
}

TEST(DebugSessionTest, givenPFThreadWithAIPEqStartIPWhenCallingFillResumeAndStoppedThreadsFromNewlyStoppedThenCRIsNotWritten) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread = {0, 0, 0, 0, 1};

    sessionMock->newlyStoppedThreads.push_back(thread);
    sessionMock->onlyForceException = false;
    sessionMock->forceAIPEqualStartIP = true;

    std::vector<EuThread::ThreadId> resumeThreads;
    std::vector<EuThread::ThreadId> stoppedThreads;
    std::vector<EuThread::ThreadId> interruptedThreads;

    sessionMock->allThreads[thread]->stopThread(1u);
    sessionMock->allThreads[thread]->setPageFault(true);

    sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads, interruptedThreads);
    EXPECT_EQ(0u, resumeThreads.size());
    EXPECT_EQ(1u, stoppedThreads.size());
    EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
    EXPECT_EQ(false, sessionMock->allThreads[thread]->getPageFault());
}

TEST(DebugSessionTest, givenDbgRegAndCRThenisAIPequalToThreadStartIPReturnsCorrectResult) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->callBaseisAIPequalToThreadStartIP = true;
    uint32_t dbg[4] = {0xDEADBEEF, 0, 0, 0};
    uint32_t cr[4] = {0, 0, 0xDEADBEEF, 0};
    EXPECT_TRUE(sessionMock->isAIPequalToThreadStartIP(cr, dbg));
    cr[2] = 0x1234567;
    EXPECT_FALSE(sessionMock->isAIPequalToThreadStartIP(cr, dbg));
}

TEST(DebugSessionTest, givenThreadsStoppedWithPageFaultWhenCallingfillResumeAndStoppedThreadsFromNewlyStoppedThenCRIsWritten) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread = {0, 0, 0, 0, 1};

    sessionMock->newlyStoppedThreads.push_back(thread);
    sessionMock->onlyForceException = false;

    std::vector<EuThread::ThreadId> resumeThreads;
    std::vector<EuThread::ThreadId> stoppedThreads;
    std::vector<EuThread::ThreadId> interruptedThreads;

    sessionMock->allThreads[thread]->stopThread(1u);
    sessionMock->allThreads[thread]->setPageFault(true);

    sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads, interruptedThreads);
    EXPECT_EQ(0u, resumeThreads.size());
    EXPECT_EQ(1u, stoppedThreads.size());
    EXPECT_EQ(1u, sessionMock->writeRegistersCallCount);
    EXPECT_EQ(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, sessionMock->writeRegistersReg);
    EXPECT_EQ(false, sessionMock->allThreads[thread]->getPageFault());

    resumeThreads.clear();
    stoppedThreads.clear();
    sessionMock->newlyStoppedThreads.push_back(thread);
    sessionMock->onlyForceException = true;
    sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads, interruptedThreads);
    EXPECT_EQ(1u, resumeThreads.size());
    EXPECT_EQ(0u, stoppedThreads.size());
    EXPECT_EQ(1u, sessionMock->writeRegistersCallCount);
}

TEST(DebugSessionTest, givenNoThreadsStoppedWhenCallingfillResumeAndStoppedThreadsFromNewlyStoppedThenReadStateSaveAreaNotCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    std::vector<EuThread::ThreadId> resumeThreads;
    std::vector<EuThread::ThreadId> stoppedThreads;
    std::vector<EuThread::ThreadId> interruptedThreads;

    sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads, interruptedThreads);

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    sessionMock->skipCheckThreadIsResumed = false;

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
    MockDeviceImp deviceImp(neoDevice);

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
    cr0[1] = (1 << 23);
    EXPECT_FALSE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
}

TEST(DebugSessionTest, givenSomeThreadsRunningWhenResumeCalledThenOnlyStoppedThreadsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread]->reportAsStopped();
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

TEST(DebugSessionTest, givenStoppedThreadsWhenResumeAllCalledThenOnlyReportedStoppedThreadsAreResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    hwInfo.gtSystemInfo.EUCount = 8;
    hwInfo.gtSystemInfo.ThreadCount = 8 * hwInfo.gtSystemInfo.EUCount;

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        sessionMock->stateSaveAreaHeader.resize(size);
    }

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        // set reportAsStopped threads from EU0
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread]->reportAsStopped();

        // stop threads from EU1, but do not report as stopped
        EuThread::ThreadId thread1(0, 0, 0, 1, i);
        sessionMock->allThreads[thread1]->stopThread(1u); // do not report as stopped
    }

    ze_device_thread_t threadAll = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    auto result = sessionMock->resume(threadAll);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    // only threads from EU0 resumed
    EXPECT_EQ(hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount, sessionMock->resumeThreadCount);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        EXPECT_TRUE(sessionMock->allThreads[thread]->isRunning());

        EuThread::ThreadId thread1(0, 0, 0, 1, i);
        EXPECT_FALSE(sessionMock->allThreads[thread1]->isRunning());
    }
}

TEST(DebugSessionTest, givenMultipleStoppedThreadsWhenResumeAllCalledThenStateSaveAreaIsReadOnceAndUsedByAllThreads) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.gtSystemInfo.EUCount = 8;
    hwInfo.gtSystemInfo.ThreadCount = 8 * hwInfo.gtSystemInfo.EUCount;

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        sessionMock->stateSaveAreaHeader.resize(size);
    }

    auto threadCount = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
    for (uint32_t i = 0; i < threadCount; i++) {
        // set reportAsStopped threads from EU0
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread]->reportAsStopped();
    }

    ze_device_thread_t threadAll = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    auto result = sessionMock->resume(threadAll);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(threadCount, sessionMock->resumeThreadCount);

    EXPECT_EQ(threadCount, sessionMock->checkThreadIsResumedFromPassedSaveAreaCalled);
    EXPECT_EQ(0u, sessionMock->checkThreadIsResumedCalled);

    for (uint32_t i = 0; i < threadCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        EXPECT_TRUE(sessionMock->allThreads[thread]->isRunning());
    }

    // One thread stopped
    threadCount = 1;
    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    sessionMock->allThreads[thread]->stopThread(3u);
    sessionMock->allThreads[thread]->reportAsStopped();
    sessionMock->checkThreadIsResumedFromPassedSaveAreaCalled = 0;
    sessionMock->resumeThreadCount = 0;
    sessionMock->checkThreadIsResumedCalled = 0;

    result = sessionMock->resume(threadAll);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(threadCount, sessionMock->resumeThreadCount);
    EXPECT_EQ(0u, sessionMock->checkThreadIsResumedFromPassedSaveAreaCalled);
    EXPECT_EQ(threadCount, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, givenMultipleStoppedThreadsWhenResumeAllCalledThenStateSaveAreaIsReadUntilThreadsConfirmedToBeResumed) {

    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        ze_result_t readGpuMemory(uint64_t memoryHandle, char *output, size_t size, uint64_t gpuVa) override {
            if (readCount == 0) {
                skipCheckThreadIsResumed = true;
            } else {
                readCount--;
            }
            return MockDebugSession::readGpuMemory(memoryHandle, output, size, gpuVa);
        }

        int readCount = 0;
    };

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.gtSystemInfo.EUCount = 8;
    hwInfo.gtSystemInfo.ThreadCount = 8 * hwInfo.gtSystemInfo.EUCount;

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
    auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                pStateSaveAreaHeader->regHeader.state_area_offset +
                pStateSaveAreaHeader->regHeader.state_save_size * 16;
    sessionMock->stateSaveAreaHeader.resize(size);

    auto threadCount = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
    for (uint32_t i = 0; i < threadCount; i++) {
        // set reportAsStopped threads from EU0
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread]->reportAsStopped();
        sessionMock->allThreads[thread]->verifyStopped(1);

        auto threadSlotOffset = sessionMock->calculateThreadSlotOffset(thread);
        auto srMagicOffset = threadSlotOffset + pStateSaveAreaHeader->regHeader.sr_magic_offset;
        SIP::sr_ident srMagic;
        srMagic.count = 1;
        sessionMock->writeGpuMemory(0, reinterpret_cast<char *>(&srMagic), sizeof(srMagic), reinterpret_cast<uint64_t>(sessionMock->stateSaveAreaHeader.data()) + srMagicOffset);
    }

    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->readCount = threadCount;

    ze_device_thread_t threadAll = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    auto result = sessionMock->resume(threadAll);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(threadCount, sessionMock->resumeThreadCount);

    // After threadCount reads, debug session sets skipCheckThreadIsResumed and treats
    // threads as resumed
    EXPECT_EQ(2 * threadCount, sessionMock->checkThreadIsResumedFromPassedSaveAreaCalled);
    EXPECT_EQ(0u, sessionMock->checkThreadIsResumedCalled);

    for (uint32_t i = 0; i < threadCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        EXPECT_TRUE(sessionMock->allThreads[thread]->isRunning());
    }
}

TEST(DebugSessionTest, givenMultipleStoppedThreadsAndInvalidStateSaveAreaWhenResumeAllCalledThenThreadsAreSwitchedToResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.gtSystemInfo.EUCount = 8;
    hwInfo.gtSystemInfo.ThreadCount = 8 * hwInfo.gtSystemInfo.EUCount;

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->forceZeroStateSaveAreaSize = true;

    auto threadCount = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
    for (uint32_t i = 0; i < threadCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread]->reportAsStopped();
    }

    ze_device_thread_t threadAll = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    auto result = sessionMock->resume(threadAll);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(threadCount, sessionMock->resumeThreadCount);

    EXPECT_EQ(0u, sessionMock->checkThreadIsResumedFromPassedSaveAreaCalled);
    EXPECT_EQ(0u, sessionMock->checkThreadIsResumedCalled);

    for (uint32_t i = 0; i < threadCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        EXPECT_TRUE(sessionMock->allThreads[thread]->isRunning());
    }

    sessionMock->stateSaveAreaHeader.resize(0);

    for (uint32_t i = 0; i < threadCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread]->reportAsStopped();
    }

    result = sessionMock->resume(threadAll);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(threadCount, sessionMock->resumeThreadCount);

    EXPECT_EQ(0u, sessionMock->checkThreadIsResumedFromPassedSaveAreaCalled);
    EXPECT_EQ(0u, sessionMock->checkThreadIsResumedCalled);

    for (uint32_t i = 0; i < threadCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        EXPECT_TRUE(sessionMock->allThreads[thread]->isRunning());
    }
}

TEST(DebugSessionTest, givenStoppedThreadWhenResumeCalledThenStoppedThreadsAreCheckedSynchronously) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    EuThread::ThreadId thread(0, apiThread);
    sessionMock->allThreads[thread]->stopThread(1u);
    sessionMock->allThreads[thread]->reportAsStopped();

    auto result = sessionMock->resume(apiThread);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, sessionMock->checkStoppedThreadsAndGenerateEventsCallCount);
}

TEST(DebugSessionTest, givenAllThreadsRunningWhenResumeCalledThenErrorUnavailableReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->resumeImpResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t thread = {0, 0, 0, 1};
    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->stopThread(1u);
    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->reportAsStopped();

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST(DebugSessionTest, givenErrorFromReadSbaBufferWhenReadSbaRegistersCalledThenErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->readRegistersResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t thread = {0, 0, 0, 0};
    uint64_t sba[9];
    auto result = sessionMock->readSbaRegisters(EuThread::ThreadId(0, thread), 0, 9, sba);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(DebugSessionTest, givenErrorFromReadMemoryWhenReadSbaRegistersCalledThenErrorReturned, IsAtLeastXeCore) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipReadSystemRoutineIdent = false;

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->readSystemRoutineIdentRetVal = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    bool resumed = sessionMock->checkThreadIsResumed(threadId);
    EXPECT_TRUE(resumed);

    resumed = sessionMock->checkThreadIsResumed(threadId, sessionMock->stateSaveAreaHeader.data());
    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, givenSipVersion1WhenCallingCheckThreadIsResumedWithSaveAreaThenThreadIsAssumedRunning) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->readSystemRoutineIdentRetVal = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    bool resumed = sessionMock->checkThreadIsResumed(threadId, sessionMock->stateSaveAreaHeader.data());
    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, givenSrMagicWithCounterLessThanlLastThreadCounterThenThreadHasBeenResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = 0;
            return true;
        }
        bool readSystemRoutineIdentFromMemory(EuThread *thread, const void *stateSaveArea, SIP::sr_ident &srMagic) override {
            srMagic.count = 0;
            return true;
        }
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1u);
    sessionMock->allThreads[threadId]->stopThread(1u);
    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);

    resumed = sessionMock->checkThreadIsResumed(threadId, sessionMock->stateSaveAreaHeader.data());

    EXPECT_TRUE(resumed);
    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedFromPassedSaveAreaCalled);
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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;

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
    MockDeviceImp deviceImp(neoDevice);

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
    sessionMock->allThreads[threadId]->reportAsStopped();

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
    MockDeviceImp deviceImp(neoDevice);

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
    sessionMock->allThreads[threadId]->reportAsStopped();

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
    MockDeviceImp deviceImp(neoDevice);

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
    sessionMock->allThreads[threadId]->reportAsStopped();

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
    MockDeviceImp deviceImp(neoDevice);

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
    sessionMock->allThreads[threadId]->reportAsStopped();

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->writeMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->writeRegistersResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->debugArea.reserved1 = 1u;

    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1);
    sessionMock->allThreads[threadId]->stopThread(1u);
    sessionMock->allThreads[threadId]->reportAsStopped();

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
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->debugArea.reserved1 = 1u;

    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1);
    sessionMock->allThreads[threadId]->stopThread(1u);
    sessionMock->allThreads[threadId]->reportAsStopped();

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
    EXPECT_EQ(7u, sessionMock->checkThreadIsResumedCalled);

    sessionMock->allThreads[threadId]->verifyStopped(3);
    sessionMock->allThreads[threadId]->stopThread(1u);
    sessionMock->allThreads[threadId]->reportAsStopped();

    sessionMock->checkThreadIsResumedCalled = 0;
    result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

struct DebugSessionTestSwFifoFixture : public ::testing::Test {
    void SetUp() override {
        auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(3);
        zet_debug_config_t config = {};
        config.pid = 0x1234;
        auto hwInfo = *NEO::defaultHwInfo.get();
        NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
        MockDeviceImp deviceImp(neoDevice);
        session = std::make_unique<MockDebugSession>(config, &deviceImp);

        stateSaveAreaHeaderPtr = reinterpret_cast<NEO::StateSaveAreaHeader *>(stateSaveAreaHeader.data());
        stateSaveAreaHeaderPtr->regHeaderV3.fifo_head = fifoHead;
        stateSaveAreaHeaderPtr->regHeaderV3.fifo_tail = fifoTail;

        offsetFifo = (stateSaveAreaHeaderPtr->versionHeader.size * 8) + stateSaveAreaHeaderPtr->regHeaderV3.fifo_offset;
        stateSaveAreaHeader.resize(offsetFifo + stateSaveAreaHeaderPtr->regHeaderV3.fifo_size * (sizeof(SIP::fifo_node)));
        memcpy_s(stateSaveAreaHeader.data() + offsetFifo,
                 fifoVecTillHead.size() * sizeof(SIP::fifo_node),
                 fifoVecTillHead.data(),
                 fifoVecTillHead.size() * sizeof(SIP::fifo_node));
        memcpy_s(stateSaveAreaHeader.data() + offsetFifo + fifoTail * sizeof(SIP::fifo_node),
                 fifoVecFromTail.size() * sizeof(SIP::fifo_node),
                 fifoVecFromTail.data(),
                 fifoVecFromTail.size() * sizeof(SIP::fifo_node));
        session->stateSaveAreaHeader = std::move(stateSaveAreaHeader);
    }
    void TearDown() override {
    }

    const std::vector<SIP::fifo_node> fifoVecTillHead = {{1, 0, 0, 0, 0}, {1, 1, 0, 0, 0}, {1, 2, 0, 0, 0}, {1, 3, 0, 0, 0}, {1, 4, 0, 0, 0}};
    const std::vector<SIP::fifo_node> fifoVecFromTail = {{1, 0, 1, 0, 0}, {1, 1, 1, 0, 0}, {1, 2, 1, 0, 0}, {1, 3, 1, 0, 0}, {1, 4, 1, 0, 0}, {1, 5, 1, 0, 0}};
    const uint32_t fifoHead = 5;
    const uint32_t fifoTail = 50;
    std::unique_ptr<L0::ult::MockDebugSession> session;
    NEO::StateSaveAreaHeader *stateSaveAreaHeaderPtr = nullptr;
    uint64_t offsetFifo = 0u;
};

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoWhenReadingSwFifoThenFifoIsCorrectlyReadAndDrained) {
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    std::vector<EuThread::ThreadId> threadsWithAttention;
    session->readFifo(0, threadsWithAttention);

    stateSaveAreaHeaderPtr = reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    std::vector<SIP::fifo_node> readFifoForValidation(stateSaveAreaHeaderPtr->regHeaderV3.fifo_size);
    session->readGpuMemory(0, reinterpret_cast<char *>(readFifoForValidation.data()), readFifoForValidation.size() * sizeof(SIP::fifo_node),
                           reinterpret_cast<uint64_t>(session->stateSaveAreaHeader.data()) + offsetFifo);
    for (size_t i = 0; i < readFifoForValidation.size(); i++) {
        EXPECT_EQ(readFifoForValidation[i].valid, 0);
    }
    EXPECT_EQ(stateSaveAreaHeaderPtr->regHeaderV3.fifo_head, stateSaveAreaHeaderPtr->regHeaderV3.fifo_tail);

    EXPECT_EQ(threadsWithAttention.size(), fifoVecFromTail.size() + fifoVecTillHead.size());
    size_t index = 0;
    for (; index < fifoVecFromTail.size(); index++) {
        EXPECT_EQ(threadsWithAttention[index].slice, fifoVecFromTail[index].slice_id);
        EXPECT_EQ(threadsWithAttention[index].subslice, fifoVecFromTail[index].subslice_id);
        EXPECT_EQ(threadsWithAttention[index].eu, fifoVecFromTail[index].eu_id);
        EXPECT_EQ(threadsWithAttention[index].thread, fifoVecFromTail[index].thread_id);
    }
    for (; index < fifoVecTillHead.size(); index++) {
        EXPECT_EQ(threadsWithAttention[index].slice, fifoVecTillHead[index].slice_id);
        EXPECT_EQ(threadsWithAttention[index].subslice, fifoVecTillHead[index].subslice_id);
        EXPECT_EQ(threadsWithAttention[index].eu, fifoVecTillHead[index].eu_id);
        EXPECT_EQ(threadsWithAttention[index].thread, fifoVecTillHead[index].thread_id);
    }
}

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoWithHeadIndexAtZeroWhenReadingSwFifoThenFifoIsCorrectlyReadAndDrained) {
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    stateSaveAreaHeaderPtr = reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    stateSaveAreaHeaderPtr->regHeaderV3.fifo_head = 0;

    std::vector<EuThread::ThreadId> threadsWithAttention;
    session->readFifo(0, threadsWithAttention);

    std::vector<SIP::fifo_node> readFifoForValidation(stateSaveAreaHeaderPtr->regHeaderV3.fifo_size);
    session->readGpuMemory(0, reinterpret_cast<char *>(readFifoForValidation.data()), readFifoForValidation.size() * sizeof(SIP::fifo_node),
                           reinterpret_cast<uint64_t>(session->stateSaveAreaHeader.data()) + offsetFifo);
    for (size_t i = fifoTail; i < readFifoForValidation.size(); i++) {
        EXPECT_EQ(readFifoForValidation[i].valid, 0);
    }
    EXPECT_EQ(stateSaveAreaHeaderPtr->regHeaderV3.fifo_head, stateSaveAreaHeaderPtr->regHeaderV3.fifo_tail);

    EXPECT_EQ(threadsWithAttention.size(), fifoVecFromTail.size());
    size_t index = 0;
    for (; index < fifoVecFromTail.size(); index++) {
        EXPECT_EQ(threadsWithAttention[index].slice, fifoVecFromTail[index].slice_id);
        EXPECT_EQ(threadsWithAttention[index].subslice, fifoVecFromTail[index].subslice_id);
        EXPECT_EQ(threadsWithAttention[index].eu, fifoVecFromTail[index].eu_id);
        EXPECT_EQ(threadsWithAttention[index].thread, fifoVecFromTail[index].thread_id);
    }
}

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoAndAttentionEventContextEmptyWhenPollSwFifoThenReadFifoIsNotCalled) {
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    session->pollFifo();

    EXPECT_TRUE(session->attentionEventContext.empty());
    EXPECT_EQ(session->readFifoCallCount, 0u);
}

TEST_F(DebugSessionTestSwFifoFixture, GivenTimeSinceLastFifoReadLessThanPollIntervalWhenPollSwFifoThenReadFifoIsNotCalled) {
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());

    auto now = std::chrono::steady_clock::now();
    session->lastFifoReadTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    session->fifoPollInterval = INT32_MAX;
    session->attentionEventContext[10] = {11, 12, 1};
    session->pollFifo();

    EXPECT_EQ(session->readFifoCallCount, 0u);
}

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoWithHeadAndTailIndexEqualWhenPollSwFifoThenOneReadAndNoWriteIsDone) {
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    stateSaveAreaHeaderPtr = reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    stateSaveAreaHeaderPtr->regHeaderV3.fifo_head = fifoTail;

    DebugManagerStateRestore stateRestore;
    debugManager.flags.DebugUmdFifoPollInterval.set(0);
    session->attentionEventContext[10] = {11, 12, 1};
    session->pollFifo();

    EXPECT_EQ(session->readFifoCallCount, 1u);
    EXPECT_LE(session->readGpuMemoryCallCount, 1u);
    EXPECT_EQ(session->writeGpuMemoryCallCount, 0u);
}

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoWithHeadAndTailIndexEqualWhenSwFifoReadThenOneReadAndNoWriteIsDone) {
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    stateSaveAreaHeaderPtr = reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    stateSaveAreaHeaderPtr->regHeaderV3.fifo_head = fifoTail;

    std::vector<EuThread::ThreadId> threadsWithAttention;
    session->readFifo(0, threadsWithAttention);

    EXPECT_EQ(session->readFifoCallCount, 1u);
    EXPECT_LE(session->readGpuMemoryCallCount, 1u);
    EXPECT_EQ(session->writeGpuMemoryCallCount, 0u);
}

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoWhenWriteGpuMemoryFailsWhileInValidatingNodeDuringFifoReadThenErrorReturned) {
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    session->writeMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    std::vector<EuThread::ThreadId> threadsWithAttention;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readFifo(0, threadsWithAttention));
}

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoWhenWriteGpuMemoryFailsWhileUpdatingTailIndexDuringFifoReadThenErrorReturned) {

    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    session->forceWriteGpuMemoryFailOnCount = 3;
    std::vector<EuThread::ThreadId> threadsWithAttention;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readFifo(0, threadsWithAttention));
}

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoWhenReadGpuMemoryFailsDuringFifoIndicesReadingThenErrorReturned) {

    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    session->forcereadGpuMemoryFailOnCount = 1;
    std::vector<EuThread::ThreadId> threadsWithAttention;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readFifo(0, threadsWithAttention));
}

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoWhenReadGpuMemoryFailsDuringFifoReadingThenErrorReturned) {

    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    session->forcereadGpuMemoryFailOnCount = 3;
    std::vector<EuThread::ThreadId> threadsWithAttention;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readFifo(0, threadsWithAttention));
}

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoWhenReadGpuMemoryFailsDuringUpdateOfLastHeadThenErrorReturned) {

    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    session->forcereadGpuMemoryFailOnCount = 4;
    std::vector<EuThread::ThreadId> threadsWithAttention;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readFifo(0, threadsWithAttention));
}

TEST(DebugSessionTest, GivenSwFifoWhenStateSaveAreaVersionIsLessThanThreeDuringFifoReadThenFifoIsNotReadAndSuccessIsReturned) {
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &deviceImp);

    session->stateSaveAreaHeader.clear();
    session->stateSaveAreaHeader.resize(stateSaveAreaHeader.size());
    memcpy_s(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaHeader.data(), stateSaveAreaHeader.size());

    std::vector<EuThread::ThreadId> threadsWithAttention;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readFifo(0, threadsWithAttention));
}

TEST(DebugSessionTest, GivenSwFifoWhenStateSaveAreaVersionIsGreaterThanThreeDuringFifoReadThenFifoIsNotReadAndSuccessIsReturned) {
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    reinterpret_cast<NEO::StateSaveAreaHeader *>(stateSaveAreaHeader.data())->versionHeader.version.major = 4;

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &deviceImp);

    session->stateSaveAreaHeader.clear();
    session->stateSaveAreaHeader.resize(stateSaveAreaHeader.size());
    memcpy_s(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaHeader.data(), stateSaveAreaHeader.size());

    std::vector<EuThread::ThreadId> threadsWithAttention;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readFifo(0, threadsWithAttention));
}

TEST_F(DebugSessionTestSwFifoFixture, GivenSwFifoWhenReadingSwFifoAndIsValidNodeFailsThenFifoReadReturnsError) {
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    std::vector<EuThread::ThreadId> threadsWithAttention;
    session->isValidNodeResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, session->readFifo(0, threadsWithAttention));
}

TEST_F(DebugSessionTest, GivenInvalidSwFifoNodeWhenCheckingIsValidNodeAndOnReadingMemoryAgainNodeTurnsValidThenSuccessReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &deviceImp);

    // Declare node whose valid field is 0
    SIP::fifo_node invalidNode = {0, 1, 1, 0, 0};
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->isValidNode(0, reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()), invalidNode));
    EXPECT_FALSE(invalidNode.valid);

    SIP::fifo_node correctedNode = {1, 1, 1, 0, 0};
    session->readMemoryBuffer.resize(sizeof(SIP::fifo_node));
    memcpy_s(session->readMemoryBuffer.data(), session->readMemoryBuffer.size(), reinterpret_cast<void *>(&correctedNode), sizeof(SIP::fifo_node));

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->isValidNode(0, reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()), invalidNode));
    EXPECT_TRUE(invalidNode.valid);
}

TEST_F(DebugSessionTest, GivenInvalidSwFifoNodeWhenCheckingIsValidNodeAndOnReadingMemoryAgainReadMemoryFailsThenErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &deviceImp);

    // Declare node whose valid field is 0
    SIP::fifo_node invalidNode = {0, 1, 1, 0, 0};
    session->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->isValidNode(0, reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()), invalidNode));
}

TEST_F(DebugSessionTest, givenTssMagicCorruptedWhenStateSaveAreIsReadThenHeaderIsNotSet) {
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    auto versionHeader = &reinterpret_cast<SIP::StateSaveAreaHeader *>(stateSaveAreaHeader.data())->versionHeader;
    versionHeader->magic[0] = '!';

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &deviceImp);

    session->readMemoryBuffer.resize(stateSaveAreaHeader.size());
    memcpy_s(session->readMemoryBuffer.data(), session->readMemoryBuffer.size(), stateSaveAreaHeader.data(), stateSaveAreaHeader.size());

    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    session->stateSaveAreaHeader.clear();
    session->validateAndSetStateSaveAreaHeader(session->allThreads[thread0]->getMemoryHandle(), reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()));
    EXPECT_TRUE(session->stateSaveAreaHeader.empty());
}

TEST_F(DebugSessionTest, givenSsaHeaderVersionGreaterThan3WhenStateSaveAreIsReadThenHeaderIsNotSet) {
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    auto versionHeader = &reinterpret_cast<SIP::StateSaveAreaHeader *>(stateSaveAreaHeader.data())->versionHeader;
    versionHeader->version.major = 4;

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &deviceImp);

    session->readMemoryBuffer.resize(stateSaveAreaHeader.size());
    memcpy_s(session->readMemoryBuffer.data(), session->readMemoryBuffer.size(), stateSaveAreaHeader.data(), stateSaveAreaHeader.size());

    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    session->stateSaveAreaHeader.clear();
    session->validateAndSetStateSaveAreaHeader(session->allThreads[thread0]->getMemoryHandle(), reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()));
    EXPECT_TRUE(session->stateSaveAreaHeader.empty());
}

TEST(DebugSessionTest, givenStoppedThreadWhenGettingNotStoppedThreadsThenOnlyRunningOrUnavailableThreadsAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    sessionMock->allThreads[thread]->stopThread(1u);
    EuThread::ThreadId thread1(0, 0, 0, 0, 1);
    EXPECT_FALSE(sessionMock->allThreads[thread1]->isStopped());

    std::vector<EuThread::ThreadId> threadsWithAtt;
    std::vector<EuThread::ThreadId> newStops;

    threadsWithAtt.push_back(thread);
    threadsWithAtt.push_back(thread1);

    sessionMock->getNotStoppedThreads(threadsWithAtt, newStops);

    ASSERT_EQ(1u, newStops.size());
    EXPECT_EQ(thread1, newStops[0]);
}

TEST(DebugSessionTest, givenSizeBiggerThanPreviousWhenAllocatingStateSaveAreaMemoryThenNewMemoryIsAllocated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EXPECT_EQ(0u, sessionMock->stateSaveAreaMemory.size());
    sessionMock->allocateStateSaveAreaMemory(0x1000);
    EXPECT_EQ(0x1000u, sessionMock->stateSaveAreaMemory.size());

    sessionMock->allocateStateSaveAreaMemory(0x2000);
    EXPECT_EQ(0x2000u, sessionMock->stateSaveAreaMemory.size());
}

TEST(DebugSessionTest, givenTheSameSizeWhenAllocatingStateSaveAreaMemoryThenNewMemoryIsNotAllocated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EXPECT_EQ(0u, sessionMock->stateSaveAreaMemory.size());
    sessionMock->allocateStateSaveAreaMemory(0x1000);
    EXPECT_EQ(0x1000u, sessionMock->stateSaveAreaMemory.size());

    auto oldMem = sessionMock->stateSaveAreaMemory.data();

    sessionMock->allocateStateSaveAreaMemory(0x1000);
    EXPECT_EQ(oldMem, sessionMock->stateSaveAreaMemory.data());
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

    sessionMock->allThreads[threadTile0]->reportAsStopped();
    sessionMock->allThreads[threadTile1]->reportAsStopped();

    ze_device_thread_t thread = {UINT32_MAX, 0, 0, 0};

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(sessionMock->allThreads[threadTile0]->isRunning());
    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());

    EXPECT_EQ(2u, sessionMock->resumeImpCalled);

    sessionMock->allThreads[threadTile1]->stopThread(1u);
    sessionMock->allThreads[threadTile1]->reportAsStopped();

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
    sessionMock->allThreads[threadTile0]->reportAsStopped();

    EuThread::ThreadId threadTile1(1, 0, 0, 0, 0);
    sessionMock->allThreads[threadTile1]->stopThread(1u);
    sessionMock->allThreads[threadTile1]->reportAsStopped();

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
    sessionMock->allThreads[threadTile0]->reportAsStopped();
    sessionMock->allThreads[threadTile1]->reportAsStopped();

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
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);
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

    sessionMock->newAttentionRaised();
    EXPECT_EQ(1u, sessionMock->expectedAttentionEvents);
    sessionMock->checkTriggerEventsForAttention();

    EXPECT_FALSE(sessionMock->triggerEvents);

    sessionMock->newAttentionRaised();
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

    EXPECT_EQ(2u, sessionMock->apiEvents.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->apiEvents.front().type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->apiEvents.front().info.thread.thread));
    sessionMock->apiEvents.pop();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->apiEvents.front().type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread2, sessionMock->apiEvents.front().info.thread.thread));
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

    EXPECT_EQ(1u, sessionMock->apiEvents.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->apiEvents.front().type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->apiEvents.front().info.thread.thread));
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
    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->reportAsStopped();
    sessionMock->allThreads[EuThread::ThreadId(1, thread)]->reportAsStopped();

    auto stopped = sessionMock->areRequestedThreadsStopped(thread);
    EXPECT_TRUE(stopped);

    stopped = sessionMock->areRequestedThreadsStopped(allSlices);
    EXPECT_FALSE(stopped);

    for (uint32_t i = 0; i < sliceCount; i++) {
        EuThread::ThreadId threadId(0, i, 0, 0, 0);
        sessionMock->allThreads[threadId]->stopThread(1u);
        sessionMock->allThreads[threadId]->reportAsStopped();
    }

    stopped = sessionMock->areRequestedThreadsStopped(allSlices);
    EXPECT_FALSE(stopped);

    for (uint32_t i = 0; i < sliceCount; i++) {
        EuThread::ThreadId threadId(1, i, 0, 0, 0);
        sessionMock->allThreads[threadId]->stopThread(1u);
        sessionMock->allThreads[threadId]->reportAsStopped();
    }

    stopped = sessionMock->areRequestedThreadsStopped(allSlices);
    EXPECT_TRUE(stopped);
}

using DebugSessionRegistersAccessTestV3 = Test<DebugSessionRegistersAccessV3>;

TEST_F(DebugSessionRegistersAccessTestV3, GivenSipVersion3WhenCallingResumeThenResumeInCmdRegisterIsWritten) {
    session->debugArea.reserved1 = 1u;

    {
        auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeaderV3.state_area_offset +
                    pStateSaveAreaHeader->regHeaderV3.state_save_size * 16;
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

TEST_F(DebugSessionRegistersAccessTestV3, givenV3StateSaveHeaderWhenCalculatingSrMagicOffsetResultIsCorrect) {

    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto threadSlotOffset = session->calculateThreadSlotOffset(thread0);
    auto size = session->calculateSrMagicOffset(pStateSaveAreaHeader, session->allThreads[thread0].get());
    ASSERT_EQ(size, threadSlotOffset + pStateSaveAreaHeader->regHeaderV3.sr_magic_offset);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenStateSaveHeaderGreaterThanV3WhenCalculatingSrMagicOffsetResultIsZero) {
    reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data())->versionHeader.version.major = 4;
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    EXPECT_EQ(0u, session->calculateSrMagicOffset(pStateSaveAreaHeader, session->allThreads[thread0].get()));
}

TEST_F(DebugSessionRegistersAccessTestV3, givenTypeToRegsetDescCalledThenCorrectRegdescIsReturned) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.grf);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.addr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.flag);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.emask);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.sr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.cr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.tdr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.acc);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.mme);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.sp);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.dbg_reg);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.fc);
    EXPECT_NE(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.msg);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU), &pStateSaveAreaHeader->regHeaderV3.scalar);
    EXPECT_NE(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU), nullptr);
    EXPECT_NE(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU), nullptr);
    EXPECT_NE(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU), nullptr);

    EXPECT_EQ(session->typeToRegsetDesc(0x1234), nullptr);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSsaHeaderVersionGreaterThan3WhenTypeToRegsetDescCalledThenNullRegdescIsReturned) {
    reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data())->versionHeader.version.major = 4;

    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU), nullptr);

    EXPECT_EQ(session->typeToRegsetDesc(0x1234), nullptr);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSsaHeaderVersionGreaterThan3WhenGetSbaRegsetDescCalledThenNullIsReturned) {
    reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data())->versionHeader.version.major = 4;
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    EXPECT_EQ(DebugSessionImp::getSbaRegsetDesc(*pStateSaveAreaHeader), nullptr);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSsaHeaderVersionGreaterThan3WhenCmdRegisterAccessHelperCalledThenNullIsReturned) {
    reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data())->versionHeader.version.major = 4;
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    SIP::sip_command resumeCommand = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->cmdRegisterAccessHelper(thread0, resumeCommand, false));
}

using DebugSessionRegistersAccessTest = Test<DebugSessionRegistersAccess>;

TEST_F(DebugSessionRegistersAccessTest, givenTypeToRegsetDescCalledThenCorrectRegdescIsReturned) {
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
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU), &pStateSaveAreaHeader->regHeader.dbg_reg);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU), &pStateSaveAreaHeader->regHeader.fc);
    EXPECT_NE(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(0x1234), nullptr);
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreWhenTypeToRegsetDescCalledThennullptrReturned) {
    decltype(session->stateSaveAreaHeader) emptyHeader;
    session->stateSaveAreaHeader.swap(emptyHeader);

    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU), nullptr);
}

TEST_F(DebugSessionRegistersAccessTest, givenValidRegisterWhenGettingSizeThenCorrectSizeIsReturned) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();
    EXPECT_EQ(pStateSaveAreaHeader->regHeader.grf.bytes, session->getRegisterSize(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidRegisterWhenGettingSizeThenZeroSizeIsReturned) {
    EXPECT_EQ(0u, session->getRegisterSize(ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU));
}

TEST_F(DebugSessionRegistersAccessTest, givenGetThreadRegisterSetPropertiesCalledWithInvalidThreadThenErrorIsReturned) {

    uint32_t threadCount = 0;
    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, nullptr));
    session->areRequestedThreadsStoppedReturnValue = 0;
    session->allThreads[stoppedThreadId]->resumeThread();
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugGetThreadRegisterSetProperties(session->toHandle(), stoppedThread, &threadCount, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest,
       givenNonZeroCountAndNullRegsetPointerWhenGetThreadRegisterSetPropertiesCalledTheniInvalidNullPointerIsReturned) {
    uint32_t threadCount = 10;
    ze_device_thread_t thread = stoppedThread;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER,
              zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, nullptr));
}

HWTEST2_F(DebugSessionRegistersAccessTest,
          givenGetThreadRegisterSetPropertiesCalledWhenLargeGrfIsSetThen256GrfRegisterCountIsReported,
          IsXeCore) {
    auto mockBuiltins = new MockBuiltins();
    mockBuiltins->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2, 256);
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltins);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    ze_device_thread_t thread = stoppedThread;

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.cr;
    uint32_t cr0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    cr0[0] = 0x80002000;
    session->registersAccessHelper(session->allThreads[stoppedThreadId].get(), regdesc, 0, 1, cr0, true);

    uint32_t threadCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, nullptr));
    std::vector<zet_debug_regset_properties_t> threadRegsetProps(threadCount);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, threadRegsetProps.data()));
    EXPECT_EQ(256u, threadRegsetProps[0].count);
}

HWTEST2_F(DebugSessionRegistersAccessTest,
          givenGetThreadRegisterSetPropertiesCalledWhenLargeGrfIsNotSetThen128GrfRegisterCountIsReported,
          IsXeCore) {
    auto mockBuiltins = new MockBuiltins();
    mockBuiltins->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2, 256);
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltins);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    ze_device_thread_t thread = stoppedThread;

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.cr;
    uint32_t cr0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    cr0[0] = 0x80000000;
    session->registersAccessHelper(session->allThreads[stoppedThreadId].get(), regdesc, 0, 1, cr0, true);

    uint32_t threadCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, nullptr));
    std::vector<zet_debug_regset_properties_t> threadRegsetProps(threadCount);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, threadRegsetProps.data()));
    EXPECT_EQ(128u, threadRegsetProps[0].count);
}

TEST_F(DebugSessionRegistersAccessTest, givenUnsupportedRegisterTypeWhenReadRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, 0x12345, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenUnsupportedRegisterTypeWhenWriteRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, 0x12345, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidRegistersIndicesWhenReadRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 128, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 127, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidRegistersIndicesWhenWriteRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 128, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 127, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenThreadAllWhenReadWriteRegistersCalledThenErrorNotAvailableIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;

    ze_device_thread_t threadAll = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugReadRegisters(session->toHandle(), threadAll, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugWriteRegisters(session->toHandle(), threadAll, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidSbaRegistersIndicesWhenReadSbaRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 9, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 8, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenWriteSbaRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;

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
    decltype(session->stateSaveAreaHeader) empty;
    session->stateSaveAreaHeader.swap(empty);

    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaWhenWriteRegistersCalledThenErrorUnknownReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    decltype(session->stateSaveAreaHeader) empty;
    session->stateSaveAreaHeader.swap(empty);

    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaGpuVaWhenRegistersAccessHelperCalledThenErrorUnknownReturned) {

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

TEST_F(DebugSessionRegistersAccessTest, givenWriteGpuMemoryErrorWhenRegistersAccessHelperCalledForWriteThenErrorUnknownReturned) {

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

TEST_F(DebugSessionRegistersAccessTest, givenReadGpuMemoryErrorWhenRegistersAccessHelperCalledForReadThenErrorUnknownReturned) {

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
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaWhenReadRegisterCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader.clear();

    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    session->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, true);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, GivenSipVersion2WhenWritingResumeCommandThenCmdRegIsWritten) {
    const uint32_t resumeValue = 0;

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

TEST_F(DebugSessionRegistersAccessTestV3, WhenReadingDebugScratchRegisterThenErrorsHandled) {
    uint64_t scratch[2];
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, session->readDebugScratchRegisters(5, 0, scratch));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, session->readDebugScratchRegisters(0, 5, scratch));
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readDebugScratchRegisters(0, 2, scratch));
    EXPECT_EQ(scratch[0], 0u);
    EXPECT_EQ(scratch[1], 0u);
}

TEST_F(DebugSessionRegistersAccessTestV3, WhenReadingModeRegisterThenCorrectResultReturned) {
    auto mockBuiltins = new MockBuiltins();
    mockBuiltins->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(3);
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get(), mockBuiltins);

    uint32_t modeFlags;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, session->readModeFlags(1, 1, &modeFlags));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, session->readModeFlags(0, 2, &modeFlags));
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readModeFlags(0, 1, &modeFlags));
    EXPECT_EQ(modeFlags, SIP::SIP_FLAG_HEAPLESS);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset();
}

TEST_F(DebugSessionRegistersAccessTest, WhenReadingSbaRegistersThenCorrectAddressesAreReturned) {

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

    session->readMemoryBuffer.assign(4 * gfxCoreHelper.getRenderSurfaceStateSize(), 5);

    sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU] = reinterpret_cast<uint64_t>(session->readMemoryBuffer.data());

    session->sba.instructionBaseAddress = sbaExpected[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU];
    session->sba.indirectObjectBaseAddress = sbaExpected[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU];
    session->sba.generalStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU];
    session->sba.dynamicStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU];
    session->sba.bindlessSurfaceStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU];
    session->sba.bindlessSamplerStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU];
    session->sba.surfaceStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU];

    auto scratchAllocationBase = 0ULL;
    if (gfxCoreHelper.isScratchSpaceSurfaceStateAccessible()) {
        const uint32_t ptss = 128;
        gfxCoreHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                              &session->readMemoryBuffer[1 * (gfxCoreHelper.getRenderSurfaceStateSize())], 1, scratchAllocationBase, 0,
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
                                                              &session->readMemoryBuffer[1 * (gfxCoreHelper.getRenderSurfaceStateSize())], 1, scratchAllocationBase, 0,
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
                                                              &session->readMemoryBuffer[2 * (gfxCoreHelper.getRenderSurfaceStateSize())], 1, scratchAllocationBase2Canonized, 0,
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

TEST_F(DebugSessionRegistersAccessTest, GivenBindlessSipWhenCheckingMinimumSipVersionForSLMThenExpectedResultIsReturned) {
    session->debugArea.reserved1 = 1u;

    auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                pStateSaveAreaHeader->regHeader.state_area_offset +
                pStateSaveAreaHeader->regHeader.state_save_size * 16;
    session->stateSaveAreaHeader.resize(size);

    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 0;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.patch = 0;

    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, false);

    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.major = 2;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.minor = 1;
    reinterpret_cast<SIP::StateSaveArea *>(session->stateSaveAreaHeader.data())->version.patch = 0;

    session->slmSipVersionCheck();
    EXPECT_EQ(session->sipSupportsSlm, true);
}

TEST_F(DebugSessionRegistersAccessTest, GivenBindlessSipWhenCheckingDifferentSipVersionsThenExpectedResultIsReturned) {
    session->debugArea.reserved1 = 1u;

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
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

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
    sessionMock->slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::Command::resume);

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
    sessionMock->slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::Command::resume);

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
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

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
    sessionMock->slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::Command::resume);

    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);

    desc.address = 0x1000000f; // force a read
    retVal = sessionMock->slmMemoryAccess<const void *, true>(threadId, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);
    sessionMock->slmCmdRegisterAccessReadyCount = 2;
    sessionMock->slmCmdRegisterAccessCount = 0;
    sessionMock->slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::Command::resume);

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
    MockDeviceImp deviceImp(neoDevice);

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
    MockDeviceImp deviceImp(neoDevice);

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
    sessionMock->newAttentionRaised();
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
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp, false);
    ASSERT_NE(nullptr, sessionMock);

    EXPECT_EQ(0u, sessionMock->allThreads.size());
}

TEST_F(MultiTileDebugSessionTest, GivenSubDeviceAndTileAttachWhenRootDeviceDebugSessionCreateFailsThenTileAttachFails) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

    if (!hwInfo.capabilityTable.l0DebuggerSupported) {
        GTEST_SKIP();
    }
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    device->getNEODevice()->getExecutionEnvironment()->releaseRootDeviceEnvironmentResources(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get(), mockBuiltIns);

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
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

    if (!hwInfo.capabilityTable.l0DebuggerSupported) {
        GTEST_SKIP();
    }

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    device->getNEODevice()->getExecutionEnvironment()->releaseRootDeviceEnvironmentResources(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get(), mockBuiltIns);

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
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

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
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

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
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

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
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

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
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

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

TEST_F(MultiTileDebugSessionTest, givenTileSessionAndStoppedThreadWhenGettingNotStoppedThreadsThenOnlyRunningOrUnavailableThreadsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    auto deviceImp = static_cast<DeviceImp *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    static_cast<MockDebugSession *>(sessionMock->tileSessions[0].first)->allThreads[thread]->stopThread(1u);
    EuThread::ThreadId thread1(0, 0, 0, 0, 1);
    EXPECT_FALSE(static_cast<MockDebugSession *>(sessionMock->tileSessions[0].first)->allThreads[thread1]->isStopped());

    std::vector<EuThread::ThreadId> threadsWithAtt;
    std::vector<EuThread::ThreadId> newStops;

    threadsWithAtt.push_back(thread);
    threadsWithAtt.push_back(thread1);

    sessionMock->getNotStoppedThreads(threadsWithAtt, newStops);
    ASSERT_EQ(1u, newStops.size());
    EXPECT_EQ(thread1, newStops[0]);
}

struct AffinityMaskForSingleSubDevice : MultipleDevicesWithCustomHwInfo {
    void setUp() {
        debugManager.flags.ZE_AFFINITY_MASK.set("0.1");
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
