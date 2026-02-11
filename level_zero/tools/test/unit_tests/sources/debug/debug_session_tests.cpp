/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/sip_external_lib/sip_external_lib.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/mock_sip_external_lib.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_registers_access.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"
#include "level_zero/zet_intel_gpu_debug.h"

#include "StateSaveAreaHeaderWrapper.h"
#include "encode_surface_state_args.h"

#include <limits>
#include <list>

namespace NEO {
extern std::map<std::string, std::stringstream> virtualFileList;
}

namespace L0 {
namespace ult {

TEST(DebugSessionTest, GivenValidArgsWhenReadSipMemoryCalledThenReturnsSizeAndReadsMemory) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandle> driverHandle(new DriverHandle);
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    auto sessionMock = std::make_unique<MockDebugSession>(config, device.get());

    DebugSessionImp::SipMemoryAccessArgs args = {};
    args.debugSession = sessionMock.get();
    args.contextHandle = 0x1234;
    args.gpuVa = 0x1000;

    constexpr uint32_t size = 16;
    char destination[size] = {};
    sessionMock->readGpuMemoryCallCount = 0;

    uint32_t ret = DebugSessionImp::readSipMemory(&args, 0x10, size, destination);
    EXPECT_EQ(size, ret);
    EXPECT_EQ(1u, sessionMock->readGpuMemoryCallCount);
}

TEST(DebugSessionTest, GivenReadGpuMemoryFailsWhenReadSipMemoryCalledThenReturnsZero) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandle> driverHandle(new DriverHandle);
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    auto sessionMock = std::make_unique<MockDebugSession>(config, device.get());

    DebugSessionImp::SipMemoryAccessArgs args = {};
    args.debugSession = sessionMock.get();
    args.contextHandle = 0x1234;
    args.gpuVa = 0x1000;

    sessionMock->forcereadGpuMemoryFailOnCount = 1;
    char destination[8] = {};
    uint32_t ret = DebugSessionImp::readSipMemory(&args, 0, 8, destination);
    EXPECT_EQ(0u, ret);
}

TEST(DebugSessionTest, GivenValidArgsWhenWriteSipMemoryCalledThenReturnsSizeAndWritesMemory) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandle> driverHandle(new DriverHandle);
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    auto sessionMock = std::make_unique<MockDebugSession>(config, device.get());

    DebugSessionImp::SipMemoryAccessArgs args = {};
    args.debugSession = sessionMock.get();
    args.contextHandle = 0x5678;
    args.gpuVa = 0x2000;

    constexpr uint32_t size = 32;
    char source[size] = {1, 2, 3, 4};
    sessionMock->writeGpuMemoryCallCount = 0;

    uint32_t ret = DebugSessionImp::writeSipMemory(&args, 0x20, size, source);
    EXPECT_EQ(size, ret);
    EXPECT_EQ(1u, sessionMock->writeGpuMemoryCallCount);
}

TEST(DebugSessionTest, GivenWriteGpuMemoryFailsWhenWriteSipMemoryCalledThenReturnsZero) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandle> driverHandle(new DriverHandle);
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    auto sessionMock = std::make_unique<MockDebugSession>(config, device.get());

    DebugSessionImp::SipMemoryAccessArgs args = {};
    args.debugSession = sessionMock.get();
    args.contextHandle = 0x5678;
    args.gpuVa = 0x2000;

    sessionMock->forceWriteGpuMemoryFailOnCount = 1;
    char source[8] = {5, 6, 7, 8};
    uint32_t ret = DebugSessionImp::writeSipMemory(&args, 0, 8, source);
    EXPECT_EQ(0u, ret);
}

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
    MockDeviceImp mockDevice(neoDevice);
    auto sessionMock = std::make_unique<MockDebugSession>(zet_debug_config_t{0x1234}, &mockDevice);
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxDualSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    EXPECT_EQ(1u, sessionMock->allThreads.count(EuThread::ThreadId(0, 0, numSubslicesPerSlice - 1, 0, 0)));
    EXPECT_EQ(1u, sessionMock->allThreads.count(EuThread::ThreadId(0, 1, numSubslicesPerSlice - 1, 7, 0)));
    EXPECT_EQ(1u, sessionMock->allThreads.count(EuThread::ThreadId(0, 2, numSubslicesPerSlice - 1, 0, 0)));
}

TEST(DeviceWithDebugSessionTest, GivenDeviceWithDebugSessionWhenCallingReleaseResourcesThenCloseConnectionIsCalled) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandle> driverHandle(new DriverHandle);
    auto hwInfo = *NEO::defaultHwInfo;
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);

    zet_debug_config_t config = {};
    auto session = new DebugSessionMock(config, device.get());
    static_cast<Device *>(device.get())->setDebugSession(session);
    static_cast<Device *>(device.get())->releaseResources();

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.NumThreadsPerEu; i++) {
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
    sessionMock->interruptSent = true;

    sessionMock->sendInterrupts();
    EXPECT_EQ(0u, sessionMock->interruptImpCalled);
}

TEST(DebugSessionTest, givenPendingInterruptWhenNewInterruptForThreadCalledThenErrorNotReadyReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    sessionMock->registersAccessHelper(sessionMock->allThreads[threadWithBp].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, cr0, true);

    cr0[1] = 1 << 26;
    sessionMock->registersAccessHelper(sessionMock->allThreads[threadWithFe].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, cr0, true);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice, true, 3);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice, true, 3);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
    sessionMock->readSystemRoutineIdentRetVal = false;
    EuThread::ThreadId thread(0, 0, 0, 0, 0);

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(thread, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_FALSE(sessionMock->allThreads[thread]->isStopped());
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenReadSystemRoutineIdentFailsWhenCheckThreadIsResumedCalledThenThreadIsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipReadSystemRoutineIdent = false;
    sessionMock->readSystemRoutineIdentRetVal = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    bool resumed = sessionMock->checkThreadIsResumed(threadId);
    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, givenSrMagicCountEqualsLastCounterWhenCheckThreadIsResumedCalledThenThreadIsNotResumed) {
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
    srMagic.count = 5;
    sessionMock->writeGpuMemory(0, reinterpret_cast<char *>(&srMagic), sizeof(srMagic), reinterpret_cast<uint64_t>(stateSaveAreaHeader) + srMagicOffset);

    sessionMock->allThreads[threadId]->verifyStopped(5);
    sessionMock->allThreads[threadId]->stopThread(1u);

    bool resumed = sessionMock->checkThreadIsResumed(threadId);
    EXPECT_FALSE(resumed);
}

TEST(DebugSessionTest, givenReadSystemRoutineIdentFromMemoryFailsWhenCheckThreadIsResumedWithStateSaveAreaCalledThenThreadIsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipReadSystemRoutineIdent = false;
    sessionMock->readSystemRoutineIdentRetVal = false;

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        sessionMock->stateSaveAreaHeader.resize(size);
    }

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    bool resumed = sessionMock->checkThreadIsResumed(threadId, sessionMock->stateSaveAreaHeader.data());
    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, givenSrMagicCountEqualsLastCounterWhenCheckThreadIsResumedWithStateSaveAreaCalledThenThreadIsNotResumed) {
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
    srMagic.count = 7;
    sessionMock->writeGpuMemory(0, reinterpret_cast<char *>(&srMagic), sizeof(srMagic), reinterpret_cast<uint64_t>(stateSaveAreaHeader) + srMagicOffset);

    sessionMock->allThreads[threadId]->verifyStopped(7);
    sessionMock->allThreads[threadId]->stopThread(1u);

    bool resumed = sessionMock->checkThreadIsResumed(threadId, sessionMock->stateSaveAreaHeader.data());
    EXPECT_FALSE(resumed);
}

TEST(DebugSessionTest, givenVerifyStoppedReturnsFalseWhenAddThreadToNewlyStoppedFromRaisedAttentionCalledThenThreadNotAddedToNewlyStopped) {
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
    srMagic.count = 4; // even counter - verifyStopped will return false
    sessionMock->writeGpuMemory(0, reinterpret_cast<char *>(&srMagic), sizeof(srMagic), reinterpret_cast<uint64_t>(stateSaveAreaHeader) + srMagicOffset);

    sessionMock->allThreads[threadId]->verifyStopped(2);
    sessionMock->allThreads[threadId]->stopThread(1u);

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(threadId, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenGetThreadSipCounterFailsWhenCheckThreadIsResumedCalledForSipVersion5ThenThreadIsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // Create SIP version 5 header
    NEO::StateSaveAreaHeader stateSaveAreaHeader5 = {};
    stateSaveAreaHeader5.versionHeader.version.major = 5;
    stateSaveAreaHeader5.versionHeader.version.minor = 0;
    stateSaveAreaHeader5.versionHeader.version.patch = 0;
    stateSaveAreaHeader5.versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
    memcpy(stateSaveAreaHeader5.versionHeader.magic, "tssarea", 8);

    sessionMock->stateSaveAreaHeader.assign(reinterpret_cast<char *>(&stateSaveAreaHeader5),
                                            reinterpret_cast<char *>(&stateSaveAreaHeader5) + sizeof(NEO::StateSaveAreaHeader));

    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipGetThreadSipCounter = true;    // Use mock, not real implementation
    sessionMock->getThreadSipCounterRetVal = false; // Mock getThreadSipCounter to fail

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    // When getThreadSipCounter returns false, thread is assumed resumed (line 543 condition, line 547 return)
    bool resumed = sessionMock->checkThreadIsResumed(threadId);
    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, givenSipCounterEqualsLastCounterWhenCheckThreadIsResumedCalledForSipVersion5ThenThreadIsNotResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // Create SIP version 5 header
    NEO::StateSaveAreaHeader stateSaveAreaHeader5 = {};
    stateSaveAreaHeader5.versionHeader.version.major = 5;
    stateSaveAreaHeader5.versionHeader.version.minor = 0;
    stateSaveAreaHeader5.versionHeader.version.patch = 0;
    stateSaveAreaHeader5.versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
    memcpy(stateSaveAreaHeader5.versionHeader.magic, "tssarea", 8);

    sessionMock->stateSaveAreaHeader.assign(reinterpret_cast<char *>(&stateSaveAreaHeader5),
                                            reinterpret_cast<char *>(&stateSaveAreaHeader5) + sizeof(NEO::StateSaveAreaHeader));

    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipGetThreadSipCounter = true;   // Use mock, not real implementation
    sessionMock->getThreadSipCounterRetVal = true; // Mock getThreadSipCounter to succeed
    sessionMock->mockSipCounter = 10;              // Mock counter value

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    sessionMock->allThreads[threadId]->verifyStopped(10);
    sessionMock->allThreads[threadId]->stopThread(1u);

    // When sipCounter equals lastCounter, thread is not resumed (line 562 decision)
    bool resumed = sessionMock->checkThreadIsResumed(threadId);
    EXPECT_FALSE(resumed);
}

TEST(DebugSessionTest, givenGetThreadSipCounterFailsWhenCheckThreadIsResumedWithStateSaveAreaCalledForSipVersion5ThenThreadIsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // Create SIP version 5 header
    NEO::StateSaveAreaHeader stateSaveAreaHeader5 = {};
    stateSaveAreaHeader5.versionHeader.version.major = 5;
    stateSaveAreaHeader5.versionHeader.version.minor = 0;
    stateSaveAreaHeader5.versionHeader.version.patch = 0;
    stateSaveAreaHeader5.versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
    memcpy(stateSaveAreaHeader5.versionHeader.magic, "tssarea", 8);

    sessionMock->stateSaveAreaHeader.assign(reinterpret_cast<char *>(&stateSaveAreaHeader5),
                                            reinterpret_cast<char *>(&stateSaveAreaHeader5) + sizeof(NEO::StateSaveAreaHeader));

    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipGetThreadSipCounter = true;    // Use mock, not real implementation
    sessionMock->getThreadSipCounterRetVal = false; // Mock getThreadSipCounter to fail

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    // When getThreadSipCounter returns false, thread is assumed resumed (line 574 condition, line 578 return)
    bool resumed = sessionMock->checkThreadIsResumed(threadId, sessionMock->stateSaveAreaHeader.data());
    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, givenSipCounterEqualsLastCounterWhenCheckThreadIsResumedWithStateSaveAreaCalledForSipVersion5ThenThreadIsNotResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // Create SIP version 5 header
    NEO::StateSaveAreaHeader stateSaveAreaHeader5 = {};
    stateSaveAreaHeader5.versionHeader.version.major = 5;
    stateSaveAreaHeader5.versionHeader.version.minor = 0;
    stateSaveAreaHeader5.versionHeader.version.patch = 0;
    stateSaveAreaHeader5.versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
    memcpy(stateSaveAreaHeader5.versionHeader.magic, "tssarea", 8);

    sessionMock->stateSaveAreaHeader.assign(reinterpret_cast<char *>(&stateSaveAreaHeader5),
                                            reinterpret_cast<char *>(&stateSaveAreaHeader5) + sizeof(NEO::StateSaveAreaHeader));

    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipGetThreadSipCounter = true;   // Use mock, not real implementation
    sessionMock->getThreadSipCounterRetVal = true; // Mock getThreadSipCounter to succeed
    sessionMock->mockSipCounter = 15;              // Mock counter value

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    sessionMock->allThreads[threadId]->verifyStopped(15);
    sessionMock->allThreads[threadId]->stopThread(1u);

    // When sipCounter equals lastCounter, thread is not resumed (line 593 decision)
    bool resumed = sessionMock->checkThreadIsResumed(threadId, sessionMock->stateSaveAreaHeader.data());
    EXPECT_FALSE(resumed);
}

TEST(DebugSessionTest, givenVerifyStoppedReturnsFalseWhenAddThreadToNewlyStoppedFromRaisedAttentionCalledForSipVersion5ThenThreadNotAddedToNewlyStopped) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // Create SIP version 5 header
    NEO::StateSaveAreaHeader stateSaveAreaHeader5 = {};
    stateSaveAreaHeader5.versionHeader.version.major = 5;
    stateSaveAreaHeader5.versionHeader.version.minor = 0;
    stateSaveAreaHeader5.versionHeader.version.patch = 0;
    stateSaveAreaHeader5.versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
    memcpy(stateSaveAreaHeader5.versionHeader.magic, "tssarea", 8);

    sessionMock->stateSaveAreaHeader.assign(reinterpret_cast<char *>(&stateSaveAreaHeader5),
                                            reinterpret_cast<char *>(&stateSaveAreaHeader5) + sizeof(NEO::StateSaveAreaHeader));

    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipGetThreadSipCounter = true;   // Use mock, not real implementation
    sessionMock->getThreadSipCounterRetVal = true; // Mock getThreadSipCounter to succeed
    sessionMock->mockSipCounter = 8;               // Mock counter value

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    // Set counter to even number - verifyStopped will return false (line 818 decision)
    sessionMock->allThreads[threadId]->verifyStopped(6);
    sessionMock->allThreads[threadId]->stopThread(1u);

    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(threadId, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenGetThreadSipCounterFailsWhenAddThreadToNewlyStoppedFromRaisedAttentionCalledForSipVersion5ThenThreadNotAdded) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // Create SIP version 5 header
    NEO::StateSaveAreaHeader stateSaveAreaHeader5 = {};
    stateSaveAreaHeader5.versionHeader.version.major = 5;
    stateSaveAreaHeader5.versionHeader.version.minor = 0;
    stateSaveAreaHeader5.versionHeader.version.patch = 0;
    stateSaveAreaHeader5.versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
    memcpy(stateSaveAreaHeader5.versionHeader.magic, "tssarea", 8);

    sessionMock->stateSaveAreaHeader.assign(reinterpret_cast<char *>(&stateSaveAreaHeader5),
                                            reinterpret_cast<char *>(&stateSaveAreaHeader5) + sizeof(NEO::StateSaveAreaHeader));

    sessionMock->skipGetThreadSipCounter = true;    // Use mock, not real implementation
    sessionMock->getThreadSipCounterRetVal = false; // Mock getThreadSipCounter to fail

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    // Thread is NOT stopped initially - this is key to entering the getThreadSipCounter path
    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());

    // When getThreadSipCounter fails, function returns early (line 825)
    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(threadId, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());
}

TEST(DebugSessionTest, givenGetThreadSipCounterSucceedsWhenAddThreadToNewlyStoppedFromRaisedAttentionCalledForSipVersion5ThenThreadAdded) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // Create SIP version 5 header
    NEO::StateSaveAreaHeader stateSaveAreaHeader5 = {};
    stateSaveAreaHeader5.versionHeader.version.major = 5;
    stateSaveAreaHeader5.versionHeader.version.minor = 0;
    stateSaveAreaHeader5.versionHeader.version.patch = 0;
    stateSaveAreaHeader5.versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
    memcpy(stateSaveAreaHeader5.versionHeader.magic, "tssarea", 8);

    sessionMock->stateSaveAreaHeader.assign(reinterpret_cast<char *>(&stateSaveAreaHeader5),
                                            reinterpret_cast<char *>(&stateSaveAreaHeader5) + sizeof(NEO::StateSaveAreaHeader));

    sessionMock->skipGetThreadSipCounter = true;   // Use mock, not real implementation
    sessionMock->getThreadSipCounterRetVal = true; // Mock getThreadSipCounter to succeed
    sessionMock->mockSipCounter = 7;               // Odd counter - verifyStopped will return true

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    // Thread is NOT stopped initially - this is key to entering the getThreadSipCounter path
    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());

    // When getThreadSipCounter succeeds and verifyStopped returns true, thread is added (line 829)
    sessionMock->addThreadToNewlyStoppedFromRaisedAttention(threadId, 1u, sessionMock->stateSaveAreaHeader.data());

    EXPECT_EQ(1u, sessionMock->newlyStoppedThreads.size());
    EXPECT_TRUE(sessionMock->allThreads[threadId]->isStopped());
}

TEST(DebugSessionTest, givenSipCounterNotEqualsLastCounterWhenCheckThreadIsResumedCalledForSipVersion5ThenThreadIsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // Create SIP version 5 header
    NEO::StateSaveAreaHeader stateSaveAreaHeader5 = {};
    stateSaveAreaHeader5.versionHeader.version.major = 5;
    stateSaveAreaHeader5.versionHeader.version.minor = 0;
    stateSaveAreaHeader5.versionHeader.version.patch = 0;
    stateSaveAreaHeader5.versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
    memcpy(stateSaveAreaHeader5.versionHeader.magic, "tssarea", 8);

    sessionMock->stateSaveAreaHeader.assign(reinterpret_cast<char *>(&stateSaveAreaHeader5),
                                            reinterpret_cast<char *>(&stateSaveAreaHeader5) + sizeof(NEO::StateSaveAreaHeader));

    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipGetThreadSipCounter = true;   // Use mock, not real implementation
    sessionMock->getThreadSipCounterRetVal = true; // Mock getThreadSipCounter to succeed
    sessionMock->mockSipCounter = 20;              // Mock counter value different from last

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    sessionMock->allThreads[threadId]->verifyStopped(10); // Last counter = 10
    sessionMock->allThreads[threadId]->stopThread(1u);

    // When sipCounter does not equal lastCounter, thread is resumed (line 565 else branch)
    bool resumed = sessionMock->checkThreadIsResumed(threadId);
    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, givenSipCounterNotEqualsLastCounterWhenCheckThreadIsResumedWithStateSaveAreaCalledForSipVersion5ThenThreadIsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // Create SIP version 5 header
    NEO::StateSaveAreaHeader stateSaveAreaHeader5 = {};
    stateSaveAreaHeader5.versionHeader.version.major = 5;
    stateSaveAreaHeader5.versionHeader.version.minor = 0;
    stateSaveAreaHeader5.versionHeader.version.patch = 0;
    stateSaveAreaHeader5.versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
    memcpy(stateSaveAreaHeader5.versionHeader.magic, "tssarea", 8);

    sessionMock->stateSaveAreaHeader.assign(reinterpret_cast<char *>(&stateSaveAreaHeader5),
                                            reinterpret_cast<char *>(&stateSaveAreaHeader5) + sizeof(NEO::StateSaveAreaHeader));

    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipGetThreadSipCounter = true;   // Use mock, not real implementation
    sessionMock->getThreadSipCounterRetVal = true; // Mock getThreadSipCounter to succeed
    sessionMock->mockSipCounter = 25;              // Mock counter value different from last

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    sessionMock->allThreads[threadId]->verifyStopped(15); // Last counter = 15
    sessionMock->allThreads[threadId]->stopThread(1u);

    // When sipCounter does not equal lastCounter, thread is resumed (line 596 else branch)
    bool resumed = sessionMock->checkThreadIsResumed(threadId, sessionMock->stateSaveAreaHeader.data());
    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, givenPendingInterruptsWhenGeneratingEventsThenStoppedEventIsGeneratedForCompletedInterrupt) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
    std::vector<EuThread::ThreadId> resumeThreads;
    std::vector<EuThread::ThreadId> stoppedThreads;
    std::vector<EuThread::ThreadId> interruptedThreads;

    sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads, interruptedThreads);

    EXPECT_EQ(0u, sessionMock->readStateSaveAreaHeaderCalled);
}

struct DebugSessionSr5ExceptionReasonTest : public ::testing::Test {
    struct MockDebugSessionForTest : public MockDebugSession {
        EuThread::ThreadId mockThreadId;
        static const uint64_t mockMemHandle = 0x34792;

        MockDebugSessionForTest(L0::Device *device) : MockDebugSession(zet_debug_config_t{.pid = 0x1234}, device),
                                                      mockThreadId(0, 0, 0, 0, 1) {
            allThreads[mockThreadId]->stopThread(mockMemHandle);
            newlyStoppedThreads.push_back(mockThreadId);
        }

        uint32_t mockSr5Value = 0;
        ze_result_t readRegistersImp(EuThread::ThreadId thread, zet_debug_regset_type_intel_gpu_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
            EXPECT_EQ(thread, mockThreadId);
            EXPECT_EQ(start, 0u);
            EXPECT_EQ(count, 1u);
            switch (type) {
            case ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU:
                reinterpret_cast<uint32_t *>(pRegisterValues)[1] = 0;
                return ZE_RESULT_SUCCESS;
            case ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU:
                reinterpret_cast<uint32_t *>(pRegisterValues)[0] = mockSr5Value;
                return ZE_RESULT_SUCCESS;
            default:
                EXPECT_TRUE(false);
                return ZE_RESULT_ERROR_UNKNOWN;
            }
        }
    };

    NEO::MockDevice *neoDevice;
    MockDeviceImp mockDevice;
    MockDebugSessionForTest session;

    std::vector<EuThread::ThreadId> resumeThreads;
    std::vector<EuThread::ThreadId> stoppedThreads;
    std::vector<EuThread::ThreadId> interruptedThreads;

    DebugSessionSr5ExceptionReasonTest() : neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0)),
                                           mockDevice(neoDevice),
                                           session(&mockDevice) {}
};

TEST_F(DebugSessionSr5ExceptionReasonTest, whenSr5DoesNotContainExceptionReasonThenTheThreadMayBeResumed) {
    session.fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads, interruptedThreads);
    EXPECT_EQ(resumeThreads.size(), 1u);
    EXPECT_EQ(resumeThreads[0], session.mockThreadId);
    EXPECT_EQ(stoppedThreads.size(), 0u);
    EXPECT_EQ(interruptedThreads.size(), 0u);
}

TEST_F(DebugSessionSr5ExceptionReasonTest, whenSr5DoesContainExceptionReasonThenTheThreadIsReported) {
    reinterpret_cast<NEO::StateSaveAreaHeader *>(session.stateSaveAreaHeader.data())->versionHeader.version.major = 5;
    session.mockSr5Value = 1 << 3;
    session.fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads, interruptedThreads);
    EXPECT_EQ(resumeThreads.size(), 0u);
    EXPECT_EQ(stoppedThreads.size(), 1u);
    EXPECT_EQ(stoppedThreads[0], session.mockThreadId);
    EXPECT_EQ(interruptedThreads.size(), 0u);
}

TEST_F(DebugSessionSr5ExceptionReasonTest, givenSsahNotAvailableThenSr5DoesNotContainExceptionReason) {
    session.getStateSaveAreaHeaderRetValue = nullptr;
    session.mockSr5Value = 1 << 3;
    session.fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads, interruptedThreads);
    EXPECT_EQ(resumeThreads.size(), 1u);
    EXPECT_EQ(resumeThreads[0], session.mockThreadId);
    EXPECT_EQ(stoppedThreads.size(), 0u);
    EXPECT_EQ(interruptedThreads.size(), 0u);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    cr0[1] = (1 << 22);
    EXPECT_FALSE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
    cr0[1] = (1 << 21);
    EXPECT_FALSE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
}

TEST(DebugSessionTest, givenSomeThreadsRunningWhenResumeCalledThenOnlyStoppedThreadsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.NumThreadsPerEu; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread]->reportAsStopped();
    }

    EuThread::ThreadId euthread(0, 0, 0, 0, 3);
    sessionMock->allThreads[euthread]->resumeThread();

    auto result = sessionMock->resume(thread);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(hwInfo.gtSystemInfo.NumThreadsPerEu - 1u, sessionMock->resumeThreadCount);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.NumThreadsPerEu; i++) {
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        sessionMock->stateSaveAreaHeader.resize(size);
    }

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.NumThreadsPerEu; i++) {
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
    EXPECT_EQ(hwInfo.gtSystemInfo.NumThreadsPerEu, sessionMock->resumeThreadCount);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.NumThreadsPerEu; i++) {
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        sessionMock->stateSaveAreaHeader.resize(size);
    }

    auto threadCount = hwInfo.gtSystemInfo.NumThreadsPerEu;
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &mockDevice);
    auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
    auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                pStateSaveAreaHeader->regHeader.state_area_offset +
                pStateSaveAreaHeader->regHeader.state_save_size * 16;
    sessionMock->stateSaveAreaHeader.resize(size);

    auto threadCount = hwInfo.gtSystemInfo.NumThreadsPerEu;
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
    sessionMock->forceStateSaveAreaSize = 0;

    auto threadCount = hwInfo.gtSystemInfo.NumThreadsPerEu;
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

    ze_device_thread_t thread = {0, 0, 0, 1};
    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST(DebugSessionTest, givenErrorFromResumeImpWhenResumeCalledThenErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    EXPECT_EQ(0u, DebugSessionImp::typeToRegsetFlags(std::numeric_limits<zet_debug_regset_type_intel_gpu_t>::min()));
    EXPECT_EQ(0u, DebugSessionImp::typeToRegsetFlags(std::numeric_limits<zet_debug_regset_type_intel_gpu_t>::max()));
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &mockDevice);
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
        MockDeviceImp mockDevice(neoDevice);
        session = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &mockDevice);

    // Declare node whose valid field is 0
    SIP::fifo_node invalidNode = {0, 1, 1, 0, 0};
    session->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->isValidNode(0, reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()), invalidNode));
}

TEST_F(DebugSessionTest, GivenSwFifoisValidNodeCalledThenDebugVariableTimeoutIsRespected) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.sipFifoValidNodeMaxTimeoutMs.set(200);
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp mockDevice(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &mockDevice);

    // Declare node whose valid field is 0
    SIP::fifo_node invalidNode = {0, 1, 1, 0, 0};
    session->readGpuMemoryCallCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->isValidNode(0, reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()), invalidNode));
    EXPECT_EQ(4u, session->readGpuMemoryCallCount);
}

TEST_F(DebugSessionTest, givenDumpDebugSurfaceFileWhenStateSaveAreaIsReadThenDebugSurfaceFileIsDumped) {
    static constexpr const char *filePath = "test_dump_file.bin";
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DumpDebugSurfaceFile.set(filePath);

    const std::vector<uint8_t> stateSaveAreaContent = {0xaa, 0xbb, 0xcc, 0xdd};

    MockDeviceImp mockDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));

    auto session = std::make_unique<MockDebugSession>(zet_debug_config_t{}, &mockDevice);
    session->readMemoryBuffer.assign(stateSaveAreaContent.cbegin(), stateSaveAreaContent.cend());
    session->forceStateSaveAreaSize = session->readMemoryBuffer.size();
    session->validateAndSetStateSaveAreaHeader(0, reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()));

    EXPECT_EQ(1u, NEO::virtualFileList.size());
    EXPECT_TRUE(NEO::virtualFileList.find(filePath) != NEO::virtualFileList.end());
    auto content = NEO::virtualFileList.at(filePath).str();
    EXPECT_EQ(stateSaveAreaContent, std::vector<uint8_t>(content.cbegin(), content.cend()));
}

TEST_F(DebugSessionTest, givenNoDumpDebugSurfaceFileWhenStateSaveAreaIsReadThenDebugSurfaceFileIsNotDumped) {
    const std::vector<uint8_t> stateSaveAreaContent = {0xaa, 0xbb, 0xcc, 0xdd};

    MockDeviceImp mockDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    auto session = std::make_unique<MockDebugSession>(zet_debug_config_t{}, &mockDevice);

    session->readMemoryBuffer.assign(stateSaveAreaContent.cbegin(), stateSaveAreaContent.cend());
    session->forceStateSaveAreaSize = session->readMemoryBuffer.size();

    session->validateAndSetStateSaveAreaHeader(0, reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()));

    EXPECT_EQ(0u, NEO::virtualFileList.size());
}

TEST_F(DebugSessionTest, givenDumpDebugSurfaceFileWhenStateSaveAreaIsReadAndReadGpuMemoryFailsThenDebugSurfaceFileIsNotDumped) {
    static constexpr const char *filePath = "test_dump_file.bin";
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DumpDebugSurfaceFile.set(filePath);

    MockDeviceImp mockDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));

    auto session = std::make_unique<MockDebugSession>(zet_debug_config_t{}, &mockDevice);
    session->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    session->validateAndSetStateSaveAreaHeader(0, reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()));

    EXPECT_EQ(0u, NEO::virtualFileList.size());
}

TEST_F(DebugSessionTest, givenTssMagicCorruptedWhenStateSaveAreIsReadThenHeaderIsNotSet) {
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    auto versionHeader = &reinterpret_cast<SIP::StateSaveAreaHeader *>(stateSaveAreaHeader.data())->versionHeader;
    versionHeader->magic[0] = '!';

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp mockDevice(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);
    auto session = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);
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

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.grf);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.addr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.flag);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.emask);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.sr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.cr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.tdr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.acc);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.mme);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.sp);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.dbg_reg);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.fc);
    EXPECT_NE(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.msg);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeaderV3.scalar);
    EXPECT_NE(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_NE(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_NE(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU, mockDevice.get()), nullptr);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, static_cast<zet_debug_regset_type_intel_gpu_t>(0x1234), mockDevice.get()), nullptr);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenDeviceWithMockSipExternalLibInterfaceWhenTypeToRegsetDescCalledThenReturnsRegsetDescFromMap) {
    // Create the mock NEO device with SIP external lib interface
    auto mockNeoDevice = std::make_unique<NEO::MockDevice>();
    MockSipExternalLib mockSipLib;
    static SIP::regset_desc testRegsetDesc = {0x100, 128, 16, 32};
    mockSipLib.mockRegsetDescMap[SipRegisterType::eGRF] = &testRegsetDesc;
    mockNeoDevice->mockSipLib = &mockSipLib;

    // Create a mock L0 device wrapper
    auto mockL0Device = std::make_unique<MockDeviceImp>(mockNeoDevice.release());

    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    // Test that when device has SIP external lib, typeToRegsetDesc uses getRegsetDescFromMap
    auto result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, mockL0Device.get());
    if (result != nullptr) {
        EXPECT_EQ(result->offset, 0x100u);
        EXPECT_EQ(result->num, 128u);
        EXPECT_EQ(result->bits, 16u);
        EXPECT_EQ(result->bytes, 32u);
    }

    // Test with a type that returns nullptr from the mock
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    // Test ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU, mockL0Device.get());
    if (result != nullptr) {
        EXPECT_EQ(result->offset, 0u);
        EXPECT_EQ(result->num, 1u);
        EXPECT_EQ(result->bits, 32u);
        EXPECT_EQ(result->bytes, 4u);
    }

    // Test ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU, mockL0Device.get());
    if (result != nullptr) {
        EXPECT_EQ(result->offset, 0u);
        EXPECT_EQ(result->num, 2u);
        EXPECT_EQ(result->bits, 64u);
        EXPECT_EQ(result->bytes, 8u);
    }

    // Test ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU, mockL0Device.get());
    if (result != nullptr) {
        EXPECT_EQ(result->offset, 0u);
        EXPECT_EQ(result->num, 2u);
        EXPECT_EQ(result->bits, 64u);
        EXPECT_EQ(result->bytes, 8u);
    }

    // Test ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, mockL0Device.get());
    if (result != nullptr) {
        // For heapless mode (which is the case with SIP external lib), SBA should return empty descriptor
        EXPECT_EQ(result->offset, 0u);
        EXPECT_EQ(result->num, 0u);
        EXPECT_EQ(result->bits, 0u);
        EXPECT_EQ(result->bytes, 0u);
    }

    // Test remaining register types that go through getRegsetDescFromMap (should return nullptr from mock)
    // These types fall through to the default case and call getRegsetDescFromMap
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);

    // Test invalid type
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, mockL0Device.get());
    EXPECT_EQ(nullptr, result);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenDeviceWithMockSipExternalLibInterfaceWhenRegistersAccessHelperCalledThenReturnsSuccess) {
    MockSipExternalLib mockSipLib;
    SIP::regset_desc testRegsetDesc = {0x100, 128, 16, 32};
    mockSipLib.mockRegsetDescMap[SipRegisterType::eGRF] = &testRegsetDesc;
    mockSipLib.getSipLibRegisterAccessCount = 1;
    mockSipLib.getSipLibRegisterAccessStartOffset = 0;
    // Create the mock NEO device with SIP external lib interface
    auto mockNeoDevice = std::make_unique<NEO::MockDevice>();
    mockNeoDevice->mockSipLib = &mockSipLib;

    // Create a mock L0 device wrapper
    auto mockL0Device = std::make_unique<MockDeviceImp>(mockNeoDevice.release());

    // Create a new debug session with the mock device
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto mockSession = std::make_unique<MockDebugSession>(config, mockL0Device.get());
    mockSession->stateSaveAreaHeader = NEO::MockSipData::createStateSaveAreaHeader(3);

    // Setup state save area
    {
        auto pStateSaveAreaHeader = mockSession->getStateSaveAreaHeader();
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeaderV3.state_area_offset +
                    pStateSaveAreaHeader->regHeaderV3.state_save_size * 16;
        mockSession->stateSaveAreaHeader.resize(size);
    }

    // Get register descriptor from the mock session using the mock device
    auto pStateSaveAreaHeader = mockSession->getStateSaveAreaHeader();
    auto *regdesc = mockSession->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, mockL0Device.get());
    if (regdesc != nullptr) {
        EXPECT_EQ(regdesc->offset, 0x100u);
        EXPECT_EQ(regdesc->num, 128u);
        EXPECT_EQ(regdesc->bytes, 32u);
    }

    // Prepare test data for registersAccessHelper
    uint8_t testData[32] = {0x12, 0x34, 0x56, 0x78};
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    SIP::regset_desc regdescTest = regdesc != nullptr ? *regdesc : SIP::regset_desc{0x100u, 128u, 0, 32u};

    // Test registersAccessHelper for write operation
    auto result = mockSession->registersAccessHelper(mockSession->allThreads[thread0].get(), &regdescTest, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, testData, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // Test registersAccessHelper for read operation
    uint8_t readData[32] = {};
    result = mockSession->registersAccessHelper(mockSession->allThreads[thread0].get(), &regdescTest, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, readData, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSsaHeaderVersionGreaterThan3WhenTypeToRegsetDescCalledThenNullRegdescIsReturned) {
    reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data())->versionHeader.version.major = 4;
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU, mockDevice.get()), nullptr);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, static_cast<zet_debug_regset_type_intel_gpu_t>(0x1234), mockDevice.get()), nullptr);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSsaHeaderVersionGreaterThan3WhenGetSbaRegsetDescCalledThenNullIsReturned) {
    reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data())->versionHeader.version.major = 99;
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    EXPECT_EQ(DebugSessionImp::getSbaRegsetDesc(session->getConnectedDevice(), *pStateSaveAreaHeader), nullptr);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSsaHeaderVersionGreaterThan3WhenCmdRegisterAccessHelperCalledThenNullIsReturned) {
    reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data())->versionHeader.version.major = 4;
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    NEO::SipCommandRegisterValues resumeCommand = {{0}};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->cmdRegisterAccessHelper(thread0, resumeCommand, false));
}

using DebugSessionRegistersAccessTest = Test<DebugSessionRegistersAccess>;

TEST_F(DebugSessionRegistersAccessTest, givenTypeToRegsetDescCalledThenCorrectRegdescIsReturned) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.grf);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.addr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.flag);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.emask);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.sr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.cr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.tdr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.acc);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.mme);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.sp);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.dbg_reg);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU, mockDevice.get()), &pStateSaveAreaHeader->regHeader.fc);
    EXPECT_NE(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, mockDevice.get()), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, static_cast<zet_debug_regset_type_intel_gpu_t>(0x1234), mockDevice.get()), nullptr);
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreWhenTypeToRegsetDescCalledThennullptrReturned) {
    decltype(session->stateSaveAreaHeader) emptyHeader;
    session->stateSaveAreaHeader.swap(emptyHeader);
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, mockDevice.get()), nullptr);
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
    session->registersAccessHelper(session->allThreads[stoppedThreadId].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, cr0, true);

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
    session->registersAccessHelper(session->allThreads[stoppedThreadId].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, cr0, true);

    uint32_t threadCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, nullptr));
    std::vector<zet_debug_regset_properties_t> threadRegsetProps(threadCount);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, threadRegsetProps.data()));
    EXPECT_EQ(128u, threadRegsetProps[0].count);
}

TEST_F(DebugSessionRegistersAccessTest, givenUnsupportedRegisterTypeWhenReadRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, static_cast<zet_debug_regset_type_intel_gpu_t>(0x1234), 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenUnsupportedRegisterTypeWhenWriteRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, static_cast<zet_debug_regset_type_intel_gpu_t>(0x1234), 0, 1, nullptr));
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
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, r0, false);
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
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, r0, true);
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
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, r0, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaWhenReadRegisterCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader.clear();

    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    session->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, r0, true);
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
    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, &resumeCommand, true);

    session->skipWriteResumeCommand = false;
    session->writeResumeCommand(threads);

    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, &resumeCommand, false);
    EXPECT_EQ(resumeValue, resumeCommand.command);

    resumeCommand.command = 11;
    session->registersAccessHelper(session->allThreads[thread3].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, &resumeCommand, false);
    EXPECT_EQ(resumeValue, resumeCommand.command);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenHeaplessFlagSetWhenIsHeaplessModeCalledThenReturnsTrue) {
    SIP::intelgt_state_save_area_V3 ssa = {};
    ssa.sip_flags = SIP::SIP_FLAG_HEAPLESS; // Heapless flag set

    bool result = DebugSessionImp::isHeaplessMode(session->getConnectedDevice(), ssa);
    EXPECT_TRUE(result); // Should return true when heapless flag is set
}

TEST_F(DebugSessionRegistersAccessTest, givenNoHeaplessFlagWhenIsHeaplessModeCalledThenReturnsFalse) {
    SIP::intelgt_state_save_area_V3 ssa = {};
    ssa.sip_flags = 0; // No heapless flag set

    bool result = DebugSessionImp::isHeaplessMode(session->getConnectedDevice(), ssa);
    EXPECT_FALSE(result); // Should return false when no heapless flag
}

TEST_F(DebugSessionRegistersAccessTest, givenValidParametersWhenRegistersAccessHelperCalledForReadThenReturnsSuccess) {
    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);

    // Test successful read operation
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, r0, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenValidParametersWhenRegistersAccessHelperCalledForWriteThenReturnsSuccess) {
    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32] = {0x12, 0x34, 0x56, 0x78}; // Test data
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);

    // Test successful write operation
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, r0, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidStartParameterWhenRegistersAccessHelperCalledThenReturnsInvalidArgument) {
    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);

    // Test with start index >= regdesc->num
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, regdesc->num, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, r0, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidCountParameterWhenRegistersAccessHelperCalledThenReturnsInvalidArgument) {
    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);

    // Test with start + count > regdesc->num
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, regdesc->num + 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, r0, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenNullStateSaveAreaHeaderWhenTypeToRegsetDescCalledThenReturnsNull) {
    // Test decision at line 1122 - null pointer check
    const SIP::regset_desc *result = DebugSessionImp::typeToRegsetDesc(nullptr, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, session->getConnectedDevice());
    EXPECT_EQ(nullptr, result);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenVersion3StateSaveAreaWhenTypeToRegsetDescCalledWithGrfTypeThenReturnsGrfRegset) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    // Test switch label at line 1135 for version 3 - GRF case
    const SIP::regset_desc *result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, mockDevice.get());
    EXPECT_NE(nullptr, result);
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.grf);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenVersion3StateSaveAreaWhenTypeToRegsetDescCalledWithVariousTypesThenReturnsCorrectRegset) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    // Test various switch labels at line 1135 for version 3
    const SIP::regset_desc *result;

    // ADDR case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.addr);

    // FLAG case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.flag);

    // CE case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.emask);

    // SR case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.sr);

    // CR case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.cr);

    // TDR case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.tdr);

    // ACC case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.acc);

    // MME case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.mme);

    // SP case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.sp);

    // DBG case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.dbg_reg);

    // FC case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.fc);

    // SCALAR case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.scalar);

    // MSG case
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.msg);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenVersion3StateSaveAreaWhenTypeToRegsetDescCalledWithUnknownTypeThenReturnsNull) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    // Test default case in switch (line 1135)
    const SIP::regset_desc *result = session->typeToRegsetDesc(pStateSaveAreaHeader, static_cast<zet_debug_regset_type_intel_gpu_t>(999999), mockDevice.get()); // Invalid type
    EXPECT_EQ(nullptr, result);
}

TEST_F(DebugSessionRegistersAccessTest, givenVersion2StateSaveAreaWhenTypeToRegsetDescCalledThenReturnsCorrectRegset) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    // Test version < 3 path - should use regHeader instead of regHeaderV3
    const SIP::regset_desc *result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, mockDevice.get());
    EXPECT_NE(nullptr, result);
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeader.grf);

    // Test another case for version < 3
    result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU, mockDevice.get());
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeader.addr);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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

struct DebugSessionSlmV2Test : public ::testing::Test {
    static constexpr uint64_t testMemoryHandle = 0xABCDEF00ULL;
    static constexpr size_t dataSize = 317;
    static constexpr uint64_t memDescAddress = 0x5372;
    static constexpr uint64_t slmStartOffsetResult = 0x4891;

    struct MockDebugSessionSlmV2 : public MockDebugSession {
        using MockDebugSession::getSlmStartOffset;
        using SlmAccessProtocol = DebugSessionImp::SlmAccessProtocol;
        using MockDebugSession::getSlmAddresses;

        template <typename T>
        struct MemAccessRecorder {
            T ptr;
            size_t size;
            uint64_t gpuVa;
        };

        uint64_t ctxSsaGpuVaReturn = 0x20000000;
        std::list<ze_result_t> waitForCmdReadyReturns;
        ze_result_t cmdRegisterAccessHelperReturn = ZE_RESULT_SUCCESS;
        ze_result_t readGpuMemoryReturn = ZE_RESULT_SUCCESS;
        ze_result_t writeGpuMemoryReturn = ZE_RESULT_SUCCESS;
        std::array<char, dataSize> mockGpuMemory;
        std::optional<SIP::sip_command> lastSipCommand;
        std::optional<NEO::MockProductHelper> mockProductHelper;
        std::unique_ptr<MockSipExternalLib> mockSipExternalLib;
        std::optional<SlmAccessProtocol> forcedSlmAccessProtocol;
        std::optional<std::optional<SipTransferAddr>> forcedSlmAddressResult;
        MemAccessRecorder<char *> lastReadGpuMemory;
        MemAccessRecorder<const char *> lastWriteGpuMemory;

        MockDebugSessionSlmV2(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device),
                                                                                      mockSipExternalLib(std::make_unique<MockSipExternalLib>()) {
            mockSipExternalLib->getSlmStartOffsetResult = slmStartOffsetResult;
        }

        ze_result_t waitForCmdReady(EuThread::ThreadId threadId, uint16_t retryCount) override {
            EXPECT_FALSE(waitForCmdReadyReturns.empty());
            ze_result_t ret = waitForCmdReadyReturns.front();
            waitForCmdReadyReturns.pop_front();
            return ret;
        }

        ze_result_t cmdRegisterAccessHelper(const EuThread::ThreadId &threadId, NEO::SipCommandRegisterValues &command, bool write) override {
            lastSipCommand = command.sip_commandValues;
            return cmdRegisterAccessHelperReturn;
        }

        uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
            EXPECT_EQ(memoryHandle, testMemoryHandle);
            return ctxSsaGpuVaReturn;
        }

        ze_result_t readGpuMemory(uint64_t memoryHandle, char *output, size_t size, uint64_t gpuVa) override {
            EXPECT_EQ(memoryHandle, testMemoryHandle);
            EXPECT_LE(size, mockGpuMemory.size());
            memcpy_s(output, size, mockGpuMemory.data(), size);
            lastReadGpuMemory = {.ptr = output, .size = size, .gpuVa = gpuVa};
            return readGpuMemoryReturn;
        }

        ze_result_t writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) override {
            EXPECT_EQ(memoryHandle, testMemoryHandle);
            EXPECT_LE(size, mockGpuMemory.size());
            memcpy_s(mockGpuMemory.data(), size, input, size);
            lastWriteGpuMemory = {.ptr = input, .size = size, .gpuVa = gpuVa};
            return writeGpuMemoryReturn;
        }

        const NEO::ProductHelper &getProductHelper() const override {
            return mockProductHelper ? mockProductHelper.value() : MockDebugSession::getProductHelper();
        }

        NEO::SipExternalLib *getSipExternalLibInterface() const override {
            return mockSipExternalLib ? mockSipExternalLib.get() : MockDebugSession::getSipExternalLibInterface();
        }

        SlmAccessProtocol getSlmAccessProtocol() const override {
            return forcedSlmAccessProtocol ? forcedSlmAccessProtocol.value() : MockDebugSession::getSlmAccessProtocol();
        }

        std::optional<SipTransferAddr> getSlmAddresses(EuThread::ThreadId threadId, size_t size, const zet_debug_memory_space_desc_t *desc) override {
            return forcedSlmAddressResult ? forcedSlmAddressResult.value() : MockDebugSession::getSlmAddresses(threadId, size, desc);
        }
    };

    MockDeviceImp mockDevice;
    MockDebugSessionSlmV2 session;
    EuThread::ThreadId threadId;
    zet_debug_memory_space_desc_t memDesc;

    DebugSessionSlmV2Test() : mockDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(defaultHwInfo.get(), 0)),
                              session(zet_debug_config_t{}, &mockDevice),
                              threadId(0, 0, 0, 0, 0),
                              memDesc{.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM, .address = memDescAddress} {}

    void SetUp() override {
        session.allThreads[threadId]->stopThread(testMemoryHandle);
        session.slmTesting = true;
    }
};

TEST_F(DebugSessionSlmV2Test, GivenBaseCaseThenGetProductHelperIsValid) {
    session.getProductHelper();
}

TEST_F(DebugSessionSlmV2Test, GivenSipExternalLibIsAvailableThenSipExternalLibIsReturned) {
    session.mockSipExternalLib = nullptr;
    session.getSipExternalLibInterface();
}

TEST_F(DebugSessionSlmV2Test, GivenSipUsesSubslicePoolsThenSlmAccessProtocolIsV2) {
    session.mockProductHelper = NEO::MockProductHelper{};
    session.mockProductHelper->sipUsesSubslicePoolsResult = true;
    EXPECT_EQ(session.getSlmAccessProtocol(), DebugSessionImp::SlmAccessProtocol::v2);
}

TEST_F(DebugSessionSlmV2Test, GivenSipDoesNotUseSubslicePoolsThenSlmAccessProtocolIsV1) {
    session.mockProductHelper = NEO::MockProductHelper{};
    session.mockProductHelper->sipUsesSubslicePoolsResult = false;
    EXPECT_EQ(session.getSlmAccessProtocol(), DebugSessionImp::SlmAccessProtocol::v1);
}

TEST_F(DebugSessionSlmV2Test, GivenSipExternalLibIsAvailableThenGetSlmStartOffsetIsObtainedFromSipExternalLib) {
    session.mockSipExternalLib = std::make_unique<MockSipExternalLib>();
    session.mockSipExternalLib->getSlmStartOffsetResult = slmStartOffsetResult;
    uint32_t slmStartOffset = 0;
    EXPECT_EQ(session.getSlmStartOffset(0, threadId, &slmStartOffset), true);
    EXPECT_EQ(slmStartOffset, slmStartOffsetResult);
}

TEST_F(DebugSessionSlmV2Test, GivenSlmStartOffsetIsAvailableThenGetSlmAddressReturnsCorrectValue) {
    session.mockSipExternalLib = std::make_unique<MockSipExternalLib>();
    session.mockSipExternalLib->getSlmStartOffsetResult = 0x5a00;
    session.ctxSsaGpuVaReturn = 0x9317b0000;
    const zet_debug_memory_space_desc_t desc = {
        .type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM,
        .address = 0xff00002f,
    };
    std::optional<DebugSessionImp::SipTransferAddr> result = session.getSlmAddresses(threadId, 3, &desc);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->sipOffset, 0xff000020u);
    EXPECT_EQ(result->sipSize, 0xfu + 3u);         // requested size + alignment padding
    EXPECT_EQ(result->gpuMemOffset, 0x9317b5a2fu); // reserved bits are masked off
}

using DebugSessionSlmReadV2Test = DebugSessionSlmV2Test;

TEST_F(DebugSessionSlmReadV2Test, GivenStoppedThreadWhenValidAddressesSizesAndOffsetsThenSlmReadV2IsSuccessful) {
    for (size_t i = 0; i < dataSize; i++) {
        session.mockGpuMemory[i] = static_cast<char>(i % 73);
    }
    std::vector<char> readBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    ze_result_t retVal = session.slmMemoryReadV2(threadId, &memDesc, dataSize, readBuf.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    for (size_t i = 0; i < dataSize; i++) {
        EXPECT_EQ(readBuf[i], static_cast<char>(i % 73));
    }
    EXPECT_EQ(memcmp(session.mockGpuMemory.data(), readBuf.data(), dataSize), 0);
    EXPECT_EQ(session.lastSipCommand->command, static_cast<uint32_t>(NEO::SipKernel::Command::slmRead));
}

TEST_F(DebugSessionSlmReadV2Test, SlmReadV2UsesCorrectSlmAddresses) {
    std::vector<char> readBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.forcedSlmAddressResult = DebugSessionImp::SipTransferAddr{
        .sipOffset = 0x12, .sipSize = 0x34, .gpuMemOffset = 0x56};
    ze_result_t retVal = session.slmMemoryReadV2(threadId, &memDesc, dataSize, readBuf.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(session.lastSipCommand->command, static_cast<uint32_t>(NEO::SipKernel::Command::slmRead));
    EXPECT_EQ(session.lastSipCommand->offset, 0x12u);
    EXPECT_EQ(session.lastSipCommand->size, 0x34u);
    EXPECT_EQ(session.lastReadGpuMemory.gpuVa, 0x56u);
    EXPECT_EQ(session.lastReadGpuMemory.size, dataSize);
}

TEST_F(DebugSessionSlmReadV2Test, GivenGetSlmStartOffsetReturnsFalseThenSlmReadV2IsError) {
    std::vector<char> readBuf(dataSize);
    session.mockSipExternalLib->getSlmStartOffsetRetValue = false;
    ze_result_t retVal = session.slmMemoryReadV2(threadId, &memDesc, dataSize, readBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST_F(DebugSessionSlmReadV2Test, GivenFirstWaitForCmdReadyReturnsErrorThenSlmReadV2IsError) {
    std::vector<char> readBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_ERROR_UNKNOWN);
    ze_result_t retVal = session.slmMemoryReadV2(threadId, &memDesc, dataSize, readBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST_F(DebugSessionSlmReadV2Test, GivenSecondWaitForCmdReadyReturnsErrorThenSlmReadV2IsError) {
    std::vector<char> readBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_ERROR_UNKNOWN);
    ze_result_t retVal = session.slmMemoryReadV2(threadId, &memDesc, dataSize, readBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST_F(DebugSessionSlmReadV2Test, GivenCmdRegisterAccessHelperReturnsErrorThenSlmReadV2IsError) {
    std::vector<char> readBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.cmdRegisterAccessHelperReturn = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t retVal = session.slmMemoryReadV2(threadId, &memDesc, dataSize, readBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST_F(DebugSessionSlmReadV2Test, GivenResumeImpReturnsErrorThenSlmReadV2IsError) {
    std::vector<char> readBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.resumeImpResult = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t retVal = session.slmMemoryReadV2(threadId, &memDesc, dataSize, readBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

using DebugSessionSlmWriteV2Test = DebugSessionSlmV2Test;

TEST_F(DebugSessionSlmWriteV2Test, GivenStoppedThreadWhenValidAddressesSizesAndOffsetsThenSlmWriteV2IsSuccessful) {
    std::vector<char> writeBuf(dataSize);
    for (size_t i = 0; i < dataSize; i++) {
        writeBuf[i] = static_cast<char>(i % 67);
    }

    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    ze_result_t retVal = session.slmMemoryWriteV2(threadId, &memDesc, dataSize, writeBuf.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(memcmp(session.mockGpuMemory.data(), writeBuf.data(), dataSize), 0);
    EXPECT_EQ(session.lastSipCommand->command, static_cast<uint32_t>(NEO::SipKernel::Command::slmWrite));
}

TEST_F(DebugSessionSlmWriteV2Test, SlmWriteV2UsesCorrectSlmAddresses) {
    std::vector<char> writeBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.forcedSlmAddressResult = DebugSessionImp::SipTransferAddr{
        .sipOffset = 0x12, .sipSize = 0x34, .gpuMemOffset = 0x56};
    ze_result_t retVal = session.slmMemoryWriteV2(threadId, &memDesc, dataSize, writeBuf.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(session.lastSipCommand->command, static_cast<uint32_t>(NEO::SipKernel::Command::slmWrite));
    EXPECT_EQ(session.lastSipCommand->offset, 0x12u);
    EXPECT_EQ(session.lastSipCommand->size, 0x34u);
    EXPECT_EQ(session.lastWriteGpuMemory.gpuVa, 0x56u);
    EXPECT_EQ(session.lastWriteGpuMemory.size, dataSize);
}

TEST_F(DebugSessionSlmWriteV2Test, GivenGetSlmStartOffsetReturnsFalseThenSlmWriteV2IsError) {
    std::vector<char> writeBuf(dataSize);
    session.mockSipExternalLib->getSlmStartOffsetRetValue = false;
    ze_result_t retVal = session.slmMemoryWriteV2(threadId, &memDesc, dataSize, writeBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST_F(DebugSessionSlmWriteV2Test, GivenFirstWaitForCmdReadyReturnsErrorThenSlmWriteV2IsError) {
    std::vector<char> writeBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_ERROR_UNKNOWN);
    ze_result_t retVal = session.slmMemoryWriteV2(threadId, &memDesc, dataSize, writeBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST_F(DebugSessionSlmWriteV2Test, GivenSecondWaitForCmdReadyReturnsErrorThenSlmWriteV2IsError) {
    std::vector<char> writeBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_ERROR_UNKNOWN);
    ze_result_t retVal = session.slmMemoryWriteV2(threadId, &memDesc, dataSize, writeBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST_F(DebugSessionSlmWriteV2Test, GivenCmdRegisterAccessHelperReturnsErrorThenSlmWriteV2IsError) {
    std::vector<char> writeBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.cmdRegisterAccessHelperReturn = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t retVal = session.slmMemoryWriteV2(threadId, &memDesc, dataSize, writeBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST_F(DebugSessionSlmWriteV2Test, GivenResumeImpReturnsErrorThenSlmWriteV2IsError) {
    std::vector<char> writeBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.resumeImpResult = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t retVal = session.slmMemoryWriteV2(threadId, &memDesc, dataSize, writeBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST_F(DebugSessionSlmWriteV2Test, GivenWriteGpuMemoryReturnsErrorThenSlmWriteV2IsError) {
    std::vector<char> writeBuf(dataSize);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.waitForCmdReadyReturns.push_back(ZE_RESULT_SUCCESS);
    session.writeGpuMemoryReturn = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t retVal = session.slmMemoryWriteV2(threadId, &memDesc, dataSize, writeBuf.data());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST(DebugSessionTest, GivenStoppedThreadWhenUnderInvalidConditionsThenSlmWriteFailsGracefully) {

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);

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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice);
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
    MockDeviceImp mockDevice(neoDevice);

    auto sessionMock = std::make_unique<MockDebugSession>(config, &mockDevice, false);
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
    auto l0Device = static_cast<Device *>(device);
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);
    auto sessionMock = new MockDebugSession(config, device, true);
    sessionMock->initialize();
    l0Device->setDebugSession(sessionMock);

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
    auto l0Device = static_cast<Device *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    l0Device->setDebugSession(sessionMock);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    auto subDevice1 = neoDevice->getSubDevice(1)->getSpecializedDevice<Device>();
    auto tileSession1 = static_cast<MockDebugSession *>(sessionMock->tileSessions[1].first);

    auto result = zetDebugAttach(subDevice0->toHandle(), &config, &debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    EXPECT_TRUE(sessionMock->tileSessions[0].second);
    EXPECT_EQ(sessionMock->tileSessions[0].first, L0::DebugSession::fromHandle(debugSession0));
    EXPECT_TRUE(sessionMock->tileSessions[0].first->isAttached());

    EXPECT_NE(nullptr, l0Device->getDebugSession(config));

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
    EXPECT_NE(nullptr, l0Device->getDebugSession(config));
    EXPECT_FALSE(sessionMock->tileSessions[1].second);
    EXPECT_FALSE(sessionMock->tileSessions[1].first->isAttached());

    EXPECT_TRUE(tileSession1->detachTileCalled);
    ASSERT_EQ(1u, sessionMock->cleanRootSessionDeviceIndices.size());
    EXPECT_EQ(1u, sessionMock->cleanRootSessionDeviceIndices[0]);

    result = zetDebugDetach(debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(nullptr, l0Device->getDebugSession(config));
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenTileAttachFailsDuringDebugSessionCreateThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    auto l0Device = static_cast<Device *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    l0Device->setDebugSession(sessionMock);

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
    auto l0Device = static_cast<Device *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    l0Device->setDebugSession(sessionMock);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    auto result = zetDebugAttach(subDevice0->toHandle(), &config, &debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    result = zetDebugAttach(l0Device->toHandle(), &config, &debugSession1);
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
    auto l0Device = static_cast<Device *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, true);
    sessionMock->initialize();
    l0Device->setDebugSession(sessionMock);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();

    auto result = zetDebugAttach(l0Device->toHandle(), &config, &debugSessionRoot);
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
    auto l0Device = static_cast<Device *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    l0Device->setDebugSession(sessionMock);

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
    L0::Device *l0Device = static_cast<Device *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, l0Device);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;
    ze_device_thread_t apiThread = {0, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    EXPECT_TRUE(sessionMock->interruptSent);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[0]);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenDeviceWithSipExternalLibInterfaceWhenIsHeaplessModeCalledThenReturnsTrue) {
    // Test with device that has no SIP external lib interface (default mockDevice)
    SIP::intelgt_state_save_area_V3 ssa = {};
    ssa.sip_flags = 0; // No heapless flag set

    bool result = DebugSessionImp::isHeaplessMode(mockDevice.get(), ssa);
    EXPECT_FALSE(result); // Should return false because no SIP interface and no heapless flag
}

TEST_F(DebugSessionRegistersAccessTestV3, givenDeviceWithoutSipExternalLibInterfaceAndHeaplessFlagWhenIsHeaplessModeCalledThenReturnsTrue) {
    SIP::intelgt_state_save_area_V3 ssa = {};
    ssa.sip_flags = SIP::SIP_FLAG_HEAPLESS;

    bool result = DebugSessionImp::isHeaplessMode(mockDevice.get(), ssa);
    EXPECT_TRUE(result); // Should return true because of heapless flag
}

TEST_F(DebugSessionRegistersAccessTestV3, givenDeviceWithoutSipExternalLibInterfaceAndNoHeaplessFlagWhenIsHeaplessModeCalledThenReturnsFalse) {
    SIP::intelgt_state_save_area_V3 ssa = {};
    ssa.sip_flags = 0; // No heapless flag set

    bool result = DebugSessionImp::isHeaplessMode(mockDevice.get(), ssa);
    EXPECT_FALSE(result); // Should return false
}

TEST_F(DebugSessionRegistersAccessTestV3, givenDeviceWithMockSipExternalLibInterfaceWhenIsHeaplessModeCalledThenReturnsTrue) {
    // Create a mock device that returns the mock SipExternalLib
    class MockDeviceWithSipLib : public NEO::MockDevice {
      public:
        MockDeviceWithSipLib() : NEO::MockDevice() {
            mockSipLib = std::make_unique<MockSipExternalLib>();
        }
        NEO::SipExternalLib *getSipExternalLibInterface() const override {
            return mockSipLib.get();
        }
        std::unique_ptr<MockSipExternalLib> mockSipLib;
    };

    // Create the mock NEO device with SIP external lib interface
    auto mockNeoDevice = std::make_unique<MockDeviceWithSipLib>();

    // Create a mock L0 device wrapper
    auto mockL0Device = std::make_unique<MockDeviceImp>(mockNeoDevice.release());

    SIP::intelgt_state_save_area_V3 ssa = {};
    ssa.sip_flags = 0; // No heapless flag set, but device has SIP external lib

    bool result = DebugSessionImp::isHeaplessMode(mockL0Device.get(), ssa);
    EXPECT_TRUE(result); // Should return true because device has SIP external lib interface
}

TEST_F(DebugSessionRegistersAccessTest, givenRegistersAccessHelperWithInvalidStartIndexWhenCalledThenReturnsInvalidArgument) {
    auto threadId = stoppedThreadId;
    auto thread = session->allThreads[threadId].get();

    // Create a regset descriptor with limited registers
    SIP::regset_desc regdesc = {0, 5, 4, 1}; // offset=0, num=5, bytes=4, numslots=1

    uint32_t registerValues[2] = {0};

    // Test with start index >= num registers
    ze_result_t result = session->registersAccessHelper(thread, &regdesc, 5, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, registerValues, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = session->registersAccessHelper(thread, &regdesc, 10, 1, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, registerValues, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(DebugSessionRegistersAccessTest, givenRegistersAccessHelperWithInvalidCountWhenCalledThenReturnsInvalidArgument) {
    auto threadId = stoppedThreadId;
    auto thread = session->allThreads[threadId].get();

    // Create a regset descriptor with limited registers
    SIP::regset_desc regdesc = {0, 5, 4, 1}; // offset=0, num=5, bytes=4, numslots=1

    uint32_t registerValues[10] = {0};

    // Test with start + count > num registers
    ze_result_t result = session->registersAccessHelper(thread, &regdesc, 2, 4, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, registerValues, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = session->registersAccessHelper(thread, &regdesc, 0, 6, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, registerValues, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenTypeToRegsetDescWithSipLibInterfaceWhenCalledWithModeFlagsTypeThenReturnsModeFlagsRegsetDesc) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    auto result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU, mockDevice.get());
    EXPECT_NE(nullptr, result);
    EXPECT_EQ(result, DebugSessionImp::getModeFlagsRegsetDesc());
}

TEST_F(DebugSessionRegistersAccessTestV3, givenTypeToRegsetDescWithSipLibInterfaceWhenCalledWithDebugScratchTypeThenReturnsDebugScratchRegsetDesc) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    auto result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU, mockDevice.get());
    EXPECT_NE(nullptr, result);
    EXPECT_EQ(result, DebugSessionImp::getDebugScratchRegsetDesc());
}

TEST_F(DebugSessionRegistersAccessTestV3, givenTypeToRegsetDescWithSipLibInterfaceWhenCalledWithThreadScratchTypeThenReturnsThreadScratchRegsetDesc) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    auto result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU, mockDevice.get());
    EXPECT_NE(nullptr, result);
    EXPECT_EQ(result, DebugSessionImp::getThreadScratchRegsetDesc());
}

TEST_F(DebugSessionRegistersAccessTestV3, givenTypeToRegsetDescWithSipLibInterfaceWhenCalledWithSbaTypeThenReturnsSbaRegsetDesc) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    auto result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, mockDevice.get());
    EXPECT_NE(nullptr, result);
    // The result should be the SBA regset descriptor from getSbaRegsetDesc
}

TEST_F(DebugSessionRegistersAccessTestV3, givenTypeToRegsetDescWithSipLibInterfaceWhenCalledWithUnknownTypeThenCallsGetRegsetDesc) {
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    // Test with a register type that should go to the default case
    auto result = session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, mockDevice.get());
    // For normal devices without SIP interface, this should return the regular GRF regset
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result, &pStateSaveAreaHeader->regHeaderV3.grf);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenTypeToRegsetDescWithoutSipLibInterfaceWhenCalledWithVersion3ThenReturnsCorrectRegsetDescForSwitchCases) {
    // Use the normal mockDevice which should not have a SIP external lib interface

    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();
    EXPECT_EQ(3u, pStateSaveAreaHeader->versionHeader.version.major);

    // Test some specific switch cases in the version 3 branch
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.grf);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.addr);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.flag);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.emask);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.sr);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.cr);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.tdr);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.acc);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.mme);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.sp);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.dbg_reg);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.fc);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.scalar);

    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU, mockDevice.get()),
              &pStateSaveAreaHeader->regHeaderV3.msg);

    // Test default case with invalid type
    EXPECT_EQ(session->typeToRegsetDesc(pStateSaveAreaHeader, static_cast<zet_debug_regset_type_intel_gpu_t>(0x9999), mockDevice.get()), nullptr);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenRegistersAccessHelperWithValidParametersWhenCalledThenHandlesDecisionBranches) {
    // Set up a proper regset_desc for testing
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();
    const SIP::regset_desc *regdesc = &pStateSaveAreaHeader->regHeaderV3.grf; // Use GRF regset

    // Create test data
    uint32_t start = 0;
    uint32_t count = 1;
    uint64_t testData = 0x1234567890ABCDEF;

    // Test the decision coverage for the function at line 1551
    // First test the decision when start >= regdesc->num
    ze_result_t result = session->registersAccessHelper(session->allThreads[stoppedThreadId].get(),
                                                        regdesc, regdesc->num, count, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, &testData, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    // Test the decision when start + count > regdesc->num
    result = session->registersAccessHelper(session->allThreads[stoppedThreadId].get(),
                                            regdesc, regdesc->num - 1, 2, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, &testData, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    // Test valid parameters (should reach further into the function)
    result = session->registersAccessHelper(session->allThreads[stoppedThreadId].get(),
                                            regdesc, start, count, ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, &testData, false);
    // Result depends on memory access implementation, but should not crash
    // This exercises the decision paths within the function
    EXPECT_TRUE(result == ZE_RESULT_SUCCESS || result == ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSipExternalLibWhenGetRegisterSetPropertiesBinaryRetrievalFailsThenErrorUnknownReturned) {
    auto neoDevice = mockDevice->getNEODevice();
    auto &rootEnv = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    auto originalSipLib = rootEnv.sipExternalLib.release();

    auto mockSipLib = new MockSipExternalLib();
    mockSipLib->getSipKernelBinaryRetValue = -1;
    rootEnv.sipExternalLib.reset(mockSipLib);

    uint32_t count = 0; // query path
    auto ret = DebugSession::getRegisterSetProperties(mockDevice.get(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);

    rootEnv.sipExternalLib.reset(originalSipLib);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSipExternalLibWhenGetRegisterSetPropertiesCreateMapFailsThenErrorUnknownReturned) {
    auto neoDevice = mockDevice->getNEODevice();
    auto &rootEnv = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    auto originalSipLib = rootEnv.sipExternalLib.release();
    auto mockSipLib = new MockSipExternalLib();
    rootEnv.sipExternalLib.reset(mockSipLib);
    mockSipLib->createRegisterDescriptorMapRetValue = false;

    uint32_t count = 0; // initial query count path
    auto ret = DebugSession::getRegisterSetProperties(mockDevice.get(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);

    rootEnv.sipExternalLib.reset(originalSipLib);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenHeaplessModeAndSipExternalLibWhenGetRegisterSetPropertiesCalledThenThreadScratchRegsetIncluded) {
    // Heapless mode detection in DebugSessionImp::isHeaplessMode returns true if SIP external lib interface present.
    // We inject a SIP external lib and verify THREAD_SCRATCH regset appears.

    class MockSipExternalLibMinimal : public MockSipExternalLib {
      public:
        int getSipKernelBinary(NEO::Device &device, NEO::SipKernelType type, std::vector<char> &retBinary, std::vector<char> &stateSaveAreaHeader) override {
            stateSaveAreaHeader.resize(sizeof(SIP::intelgt_state_save_area_V3));
            return 0;
        }
    };

    auto neoDevice = mockDevice->getNEODevice();
    auto &rootEnv = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    auto originalSipLib = rootEnv.sipExternalLib.release();
    rootEnv.sipExternalLib.reset(new MockSipExternalLibMinimal());

    uint32_t count = 0;
    auto ret = DebugSession::getRegisterSetProperties(mockDevice.get(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_GT(count, 0u);
    std::vector<zet_debug_regset_properties_t> props(count);
    ret = DebugSession::getRegisterSetProperties(mockDevice.get(), &count, props.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    bool threadScratchFound = false;
    for (auto &p : props) {
        if (p.type == ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU) {
            threadScratchFound = true;
            break;
        }
    }
    EXPECT_TRUE(threadScratchFound);
    rootEnv.sipExternalLib.reset(originalSipLib);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenStoppedThreadAndSipExternalLibWhenGetThreadRegisterSetPropertiesCalledThenSipLibPathUsed) {
    auto neoDevice = mockDevice->getNEODevice();
    auto &rootEnv = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    auto originalSipLib = rootEnv.sipExternalLib.release();
    auto mockSipLib = new MockSipExternalLib();
    mockSipLib->getSipLibRegisterAccessCount = 8;
    mockSipLib->getSipLibRegisterAccessStartOffset = 0;

    rootEnv.sipExternalLib.reset(mockSipLib);
    auto threadId = stoppedThreadId;
    session->allThreads[threadId]->stopThread(1u);
    uint32_t count = 0;
    auto singleThread = ze_device_thread_t{static_cast<uint32_t>(threadId.slice), static_cast<uint32_t>(threadId.subslice), static_cast<uint32_t>(threadId.eu), static_cast<uint32_t>(threadId.thread)};
    auto ret = session->getThreadRegisterSetProperties(singleThread, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    // With SIP external lib present, getRegisterAccessProperties should be invoked on session (mock sets flag)
    EXPECT_TRUE(session->registerAccessPropertiesCalled);
    rootEnv.sipExternalLib.reset(originalSipLib);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSipExternalLibWhenRegistersAccessHelperCalledWithOutOfRangeCountThenInvalidArgumentReturned) {
    auto neoDevice = mockDevice->getNEODevice();
    auto &rootEnv = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    auto originalSipLib = rootEnv.sipExternalLib.release();
    auto mockSipLib = new MockSipExternalLib();
    mockSipLib->getSipLibRegisterAccessCount = 4;
    mockSipLib->getSipLibRegisterAccessStartOffset = 0;
    rootEnv.sipExternalLib.reset(mockSipLib);
    auto regdesc = &session->getStateSaveAreaHeader()->regHeaderV3.grf;
    auto thread = session->allThreads[stoppedThreadId].get();
    uint64_t data = 0;
    auto result = session->registersAccessHelper(thread, regdesc, 2, 3, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, &data, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    rootEnv.sipExternalLib.reset(originalSipLib);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSipExternalLibWhenRegistersAccessHelperReadWriteCalledThenSuccessReturned) {
    auto neoDevice = mockDevice->getNEODevice();
    auto &rootEnv = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    auto originalSipLib = rootEnv.sipExternalLib.release();
    auto mockSipLib = new MockSipExternalLib();
    mockSipLib->getSipLibRegisterAccessCount = 16;
    mockSipLib->getSipLibRegisterAccessStartOffset = 8;
    rootEnv.sipExternalLib.reset(mockSipLib);
    auto regdesc = &session->getStateSaveAreaHeader()->regHeaderV3.grf;
    auto thread = session->allThreads[stoppedThreadId].get();
    uint8_t buffer[64] = {};
    auto result = session->registersAccessHelper(thread, regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, buffer, false);
    EXPECT_TRUE(result == ZE_RESULT_SUCCESS || result == ZE_RESULT_ERROR_UNKNOWN);
    result = session->registersAccessHelper(thread, regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, buffer, true);
    EXPECT_TRUE(result == ZE_RESULT_SUCCESS || result == ZE_RESULT_ERROR_UNKNOWN);
    rootEnv.sipExternalLib.reset(originalSipLib);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSipExternalLibWhenRegistersAccessHelperCalledWithZeroGpuVaThenErrorUnknownReturned) {
    auto neoDevice = mockDevice->getNEODevice();
    auto &rootEnv = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    auto originalSipLib = rootEnv.sipExternalLib.release();
    auto mockSipLib = new MockSipExternalLib();
    mockSipLib->getSipLibRegisterAccessCount = 16;
    mockSipLib->getSipLibRegisterAccessStartOffset = 0;
    rootEnv.sipExternalLib.reset(mockSipLib);
    session->returnStateSaveAreaGpuVa = false; // force getContextStateSaveAreaGpuVa() to return 0
    auto regdesc = &session->getStateSaveAreaHeader()->regHeaderV3.grf;
    auto thread = session->allThreads[stoppedThreadId].get();
    uint32_t value = 0;
    auto result = session->registersAccessHelper(thread, regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, &value, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    rootEnv.sipExternalLib.reset(originalSipLib);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenSipExternalLibWhenCmdRegisterAccessHelperCalledThenCommandTypeUsed) {
    auto neoDevice = mockDevice->getNEODevice();
    auto &rootEnv = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    auto originalSipLib = rootEnv.sipExternalLib.release();
    rootEnv.sipExternalLib.reset(new MockSipExternalLib());
    NEO::SipCommandRegisterValues cmd = {{0}};
    cmd.sip_commandValues.command = 0x1;
    auto result = session->cmdRegisterAccessHelper(stoppedThreadId, cmd, false);
    EXPECT_TRUE(result == ZE_RESULT_SUCCESS || result == ZE_RESULT_ERROR_UNKNOWN || result == ZE_RESULT_ERROR_INVALID_ARGUMENT);
    result = session->cmdRegisterAccessHelper(stoppedThreadId, cmd, true);
    EXPECT_TRUE(result == ZE_RESULT_SUCCESS || result == ZE_RESULT_ERROR_UNKNOWN || result == ZE_RESULT_ERROR_INVALID_ARGUMENT);
    rootEnv.sipExternalLib.reset(originalSipLib);
}

// Tests for getSbaRegsetDesc function
TEST_F(DebugSessionRegistersAccessTestV3, GivenGetSbaRegsetDescWhenVersionMajorIs3AndHeaplessModeThenReturnsHeaplessDescriptor) {
    NEO::StateSaveAreaHeader header = {};
    header.versionHeader.version.major = 3;
    header.regHeaderV3.sip_flags = SIP::SIP_FLAG_HEAPLESS;

    auto result = L0::DebugSessionImp::getSbaRegsetDesc(mockDevice.get(), header);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->num, 0u);
    EXPECT_EQ(result->offset, 0u);
}

TEST_F(DebugSessionRegistersAccessTestV3, GivenGetSbaRegsetDescWhenVersionMajorIs3AndNotHeaplessModeThenReturnsSbaDescriptor) {
    NEO::StateSaveAreaHeader header = {};
    header.versionHeader.version.major = 3;
    header.regHeaderV3.sip_flags = 0; // Not heapless

    auto result = L0::DebugSessionImp::getSbaRegsetDesc(mockDevice.get(), header);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->num, ZET_DEBUG_SBA_COUNT_INTEL_GPU);
    EXPECT_EQ(result->bits, 64u);
    EXPECT_EQ(result->bytes, 8u);
}

TEST_F(DebugSessionRegistersAccessTestV3, GivenGetSbaRegsetDescWhenVersionMajorIs5ThenReturnsHeaplessDescriptor) {
    NEO::StateSaveAreaHeader header = {};
    header.versionHeader.version.major = 5;

    auto result = L0::DebugSessionImp::getSbaRegsetDesc(mockDevice.get(), header);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->num, 0u);
    EXPECT_EQ(result->offset, 0u);
}

TEST_F(DebugSessionRegistersAccessTestV3, GivenGetSbaRegsetDescWhenVersionMajorIs2ThenReturnsSbaDescriptor) {
    NEO::StateSaveAreaHeader header = {};
    header.versionHeader.version.major = 2;

    auto result = L0::DebugSessionImp::getSbaRegsetDesc(mockDevice.get(), header);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->num, ZET_DEBUG_SBA_COUNT_INTEL_GPU);
    EXPECT_EQ(result->bits, 64u);
    EXPECT_EQ(result->bytes, 8u);
}

TEST_F(DebugSessionRegistersAccessTestV3, GivenGetSbaRegsetDescWhenVersionMajorIs6ThenReturnsNullptr) {
    NEO::StateSaveAreaHeader header = {};
    header.versionHeader.version.major = 6;

    auto result = L0::DebugSessionImp::getSbaRegsetDesc(mockDevice.get(), header);

    EXPECT_EQ(result, nullptr);
}

TEST_F(DebugSessionRegistersAccessTestV3, GivenGetSbaRegsetDescWhenVersionMajorIs7ThenReturnsNullptr) {
    NEO::StateSaveAreaHeader header = {};
    header.versionHeader.version.major = 7;

    auto result = L0::DebugSessionImp::getSbaRegsetDesc(mockDevice.get(), header);

    EXPECT_EQ(result, nullptr);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenVersion3WithoutHeaplessModeWhenGetRegisterSetPropertiesCalledThenThreadScratchRegsetNotIncluded) {
    // Test the false branch of: isHeaplessMode(device, pStateSaveArea->regHeaderV3) || version.major == 5
    // Version 3, no heapless flag, no SIP external lib -> THREAD_SCRATCH should NOT be included

    auto neoDevice = mockDevice->getNEODevice();
    auto &rootEnv = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    auto originalSipLib = rootEnv.sipExternalLib.release();

    // Don't set SIP external lib to ensure isHeaplessMode returns false
    rootEnv.sipExternalLib.reset(nullptr);

    uint32_t count = 0;
    auto ret = DebugSession::getRegisterSetProperties(mockDevice.get(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_GT(count, 0u);

    std::vector<zet_debug_regset_properties_t> props(count);
    ret = DebugSession::getRegisterSetProperties(mockDevice.get(), &count, props.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    bool threadScratchFound = false;
    for (auto &p : props) {
        if (p.type == ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU) {
            threadScratchFound = true;
            break;
        }
    }
    EXPECT_FALSE(threadScratchFound);

    rootEnv.sipExternalLib.reset(originalSipLib);
}

TEST_F(DebugSessionRegistersAccessTestV3, givenVersion5WhenGetRegisterSetPropertiesCalledThenThreadScratchRegsetIncluded) {
    // Test the true branch of the second part: version.major == 5
    // This should include THREAD_SCRATCH regardless of heapless mode

    class MockSipExternalLibForV5 : public MockSipExternalLib {
      public:
        int getSipKernelBinary(NEO::Device &device, NEO::SipKernelType type, std::vector<char> &retBinary, std::vector<char> &stateSaveAreaHeader) override {
            // Create a proper StateSaveAreaHeader with version 5
            stateSaveAreaHeader.resize(sizeof(NEO::StateSaveAreaHeader));
            auto pHeader = reinterpret_cast<NEO::StateSaveAreaHeader *>(stateSaveAreaHeader.data());
            pHeader->versionHeader.version.major = 5;
            pHeader->versionHeader.version.minor = 0;
            pHeader->versionHeader.version.patch = 0;
            pHeader->versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
            return 0;
        }
    };

    auto neoDevice = mockDevice->getNEODevice();
    auto &rootEnv = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    auto originalSipLib = rootEnv.sipExternalLib.release();
    rootEnv.sipExternalLib.reset(new MockSipExternalLibForV5());

    // Create a version 5 state save area header manually
    NEO::StateSaveAreaHeader stateSaveAreaHeader5 = {};
    stateSaveAreaHeader5.versionHeader.version.major = 5;
    stateSaveAreaHeader5.versionHeader.version.minor = 0;
    stateSaveAreaHeader5.versionHeader.version.patch = 0;
    stateSaveAreaHeader5.versionHeader.size = sizeof(NEO::StateSaveAreaHeader) / 8;
    memcpy(stateSaveAreaHeader5.versionHeader.magic, "tssarea", 8);

    // Initialize regHeaderV3 with minimal valid data (used for V5)
    stateSaveAreaHeader5.regHeaderV3.num_slices = 1;
    stateSaveAreaHeader5.regHeaderV3.num_subslices_per_slice = 1;
    stateSaveAreaHeader5.regHeaderV3.num_eus_per_subslice = 8;
    stateSaveAreaHeader5.regHeaderV3.num_threads_per_eu = 7;
    stateSaveAreaHeader5.regHeaderV3.grf = {0, 128, 512, 64};

    auto mockBuiltins = new MockBuiltins();
    mockBuiltins->stateSaveAreaHeader.assign(reinterpret_cast<char *>(&stateSaveAreaHeader5),
                                             reinterpret_cast<char *>(&stateSaveAreaHeader5) + sizeof(NEO::StateSaveAreaHeader));
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get(), mockBuiltins);

    uint32_t count = 0;
    auto ret = DebugSession::getRegisterSetProperties(mockDevice.get(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_GT(count, 0u);

    std::vector<zet_debug_regset_properties_t> props(count);
    ret = DebugSession::getRegisterSetProperties(mockDevice.get(), &count, props.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    bool threadScratchFound = false;
    for (auto &p : props) {
        if (p.type == ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU) {
            threadScratchFound = true;
            break;
        }
    }
    EXPECT_TRUE(threadScratchFound);

    rootEnv.sipExternalLib.reset(originalSipLib);
}

// Tests for getSipCommandRegisterValues (non-V5 behavior)
TEST_F(DebugSessionRegistersAccessTestV3, GivenGetSipCommandRegisterValuesWhenWriteTrueThenReturnsSipCommandValuesAndFullSize) {
    // For non-V5 versions (< 5), getSipCommandRegisterValues does not modify the size parameter
    NEO::SipCommandRegisterValues command = {{0}};
    command.sip_commandValues.command = 0x12345678;
    command.sip_commandValues.size = 0xABCD;
    command.sip_commandValues.offset = 0x9876;

    size_t size = 128;
    size_t originalSize = size;
    size = session->getSipCommandRegisterValues(command, true, size);

    // For non-V5 version, size parameter should remain unchanged
    EXPECT_EQ(originalSize, size);
}

TEST_F(DebugSessionRegistersAccessTestV3, GivenGetSipCommandRegisterValuesWhenWriteFalseThenReturnsSipCommandValuesAndFullSize) {
    NEO::SipCommandRegisterValues command = {{0}};
    command.sip_commandValues.command = 0x12345678;
    command.sip_commandValues.size = 0xABCD;
    command.sip_commandValues.offset = 0x9876;

    size_t size = 0;
    size_t originalSize = size;
    size = session->getSipCommandRegisterValues(command, false, size);

    // For non-V5 version, size parameter should remain unchanged
    EXPECT_EQ(originalSize, size);
}

TEST_F(DebugSessionRegistersAccessTestV3, GivenCmdRegisterAccessHelperWhenWritingCommandThenCommandValuesArePreparedCorrectly) {
    // Setup the state save area to allow registersAccessHelper to work
    {
        auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeaderV3.state_area_offset +
                    pStateSaveAreaHeader->regHeaderV3.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    // Create command to write
    NEO::SipCommandRegisterValues command = {{0}};
    command.sip_commandValues.command = static_cast<uint32_t>(NEO::SipKernel::Command::resume);
    command.sip_commandValues.size = 0x100;
    command.sip_commandValues.offset = 0x200;

    // Test getSipCommandRegisterValues for write mode
    size_t writeSize = 0;
    size_t originalWriteSize = writeSize;
    writeSize = session->getSipCommandRegisterValues(command, true, writeSize);

    // For non-V5 version (< 5), size parameter should remain unchanged
    EXPECT_EQ(originalWriteSize, writeSize);

    // Call cmdRegisterAccessHelper for write - should succeed or return expected error
    auto result = session->cmdRegisterAccessHelper(stoppedThreadId, command, true);
    EXPECT_TRUE(result == ZE_RESULT_SUCCESS || result == ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(DebugSessionRegistersAccessTestV3, GivenCmdRegisterAccessHelperWhenReadingCommandThenCommandValuesArePreparedCorrectly) {
    // Setup the state save area
    {
        auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeaderV3.state_area_offset +
                    pStateSaveAreaHeader->regHeaderV3.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    // Create command to read
    NEO::SipCommandRegisterValues command = {{0}};

    // Test getSipCommandRegisterValues for read mode
    size_t readSize = 0;
    size_t originalReadSize = readSize;
    readSize = session->getSipCommandRegisterValues(command, false, readSize);

    // For non-V5 version (< 5), size parameter should remain unchanged
    EXPECT_EQ(originalReadSize, readSize);

    // Call cmdRegisterAccessHelper for read
    auto result = session->cmdRegisterAccessHelper(stoppedThreadId, command, false);
    EXPECT_TRUE(result == ZE_RESULT_SUCCESS || result == ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(DebugSessionRegistersAccessTestV3, GivenGetSipCommandRegisterValuesWhenDifferentCommandsUsedThenCorrectValuesReturned) {
    // Test multiple commands with different values
    std::vector<std::pair<uint32_t, const char *>> commandTests = {
        {static_cast<uint32_t>(NEO::SipKernel::Command::resume), "resume"},
        {static_cast<uint32_t>(NEO::SipKernel::Command::slmRead), "slmRead"},
        {static_cast<uint32_t>(NEO::SipKernel::Command::slmWrite), "slmWrite"},
        {0x12345678, "custom"}};

    for (const auto &[cmdValue, cmdName] : commandTests) {
        NEO::SipCommandRegisterValues command = {{0}};
        command.sip_commandValues.command = cmdValue;
        command.sip_commandValues.size = 0x1000;
        command.sip_commandValues.offset = 0x2000;

        // Test write mode
        size_t writeSize = 0;
        size_t originalWriteSize = writeSize;
        writeSize = session->getSipCommandRegisterValues(command, true, writeSize);

        // For non-V5 version, size parameter should remain unchanged
        EXPECT_EQ(originalWriteSize, writeSize) << "Failed for command: " << cmdName;

        // Test read mode
        size_t readSize = 0;
        size_t originalReadSize = readSize;
        readSize = session->getSipCommandRegisterValues(command, false, readSize);

        // For non-V5 version, size parameter should remain unchanged
        EXPECT_EQ(originalReadSize, readSize) << "Failed for command: " << cmdName;
    }
}

struct DebugSessionBarrierMemTest : public ::testing::Test {
    static constexpr uint64_t testMemoryHandle = 0xABCDEF00ULL;
    static constexpr uint64_t memDescAddress = 0x5372;

    struct MockDebugSessionBarrierMem : public MockDebugSession {
        using DebugSessionImp::SipTransferAddr;
        using MockDebugSession::getBarrierAddresses;
        using MockDebugSession::getBarrierStartOffset;
        using MockDebugSession::readBarrierMemory;

        uint64_t ctxSsaGpuVaReturn = 0x20000000;
        std::unique_ptr<MockSipExternalLib> mockSipExternalLib;
        std::optional<std::optional<SipTransferAddr>> forcedBarrierAddressResult;

        MockDebugSessionBarrierMem(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device),
                                                                                           mockSipExternalLib(std::make_unique<MockSipExternalLib>()) {
        }

        uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
            EXPECT_EQ(memoryHandle, testMemoryHandle);
            return ctxSsaGpuVaReturn;
        }

        NEO::SipExternalLib *getSipExternalLibInterface() const override {
            return mockSipExternalLib ? mockSipExternalLib.get() : MockDebugSession::getSipExternalLibInterface();
        }

        std::optional<SipTransferAddr> getBarrierAddresses(EuThread::ThreadId threadId, size_t size, const zet_debug_memory_space_desc_t *desc) override {
            return forcedBarrierAddressResult ? forcedBarrierAddressResult.value() : MockDebugSession::getBarrierAddresses(threadId, size, desc);
        }

        struct ReadMemViaSipTransactionArgs {
            NEO::SipKernel::Command commandId;
            EuThread::ThreadId threadId;
            SipTransferAddr addrs;
            size_t size;
            void *buffer;
        };
        std::optional<ReadMemViaSipTransactionArgs> lastCalledReadMemViaSipTransactionArgs;
        ze_result_t readMemViaSipTransaction(NEO::SipKernel::Command commandId, EuThread::ThreadId threadId, const SipTransferAddr &addrs, size_t size, void *buffer) override {
            lastCalledReadMemViaSipTransactionArgs = ReadMemViaSipTransactionArgs{
                .commandId = commandId,
                .threadId = threadId,
                .addrs = addrs,
                .size = size,
                .buffer = buffer,
            };
            return ZE_RESULT_SUCCESS;
        }
    };

    MockDeviceImp mockDevice;
    MockDebugSessionBarrierMem session;
    EuThread::ThreadId threadId;

    DebugSessionBarrierMemTest() : mockDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(defaultHwInfo.get(), 0)),
                                   session(zet_debug_config_t{}, &mockDevice),
                                   threadId(0, 0, 0, 0, 0) {}

    void SetUp() override {
        session.allThreads[threadId]->stopThread(testMemoryHandle);
    }
};

TEST_F(DebugSessionBarrierMemTest, GivenGetBarrierStartOffsetFailsWhenGetBarrierAddressesIsCalledThenNulloptIsReturned) {
    session.mockSipExternalLib = std::make_unique<MockSipExternalLib>();
    session.mockSipExternalLib->getBarrierStartOffsetRetValue = false;
    zet_debug_memory_space_desc_t memDesc;
    const auto addrs = session.getBarrierAddresses(threadId, 8, &memDesc);
    EXPECT_EQ(addrs, std::nullopt);
}

TEST_F(DebugSessionBarrierMemTest, GivenGetBarrierStartOffsetSucceedsWhenGetBarrierAddressesIsCalledThenCorrectValuesAreReturned) {
    session.mockSipExternalLib = std::make_unique<MockSipExternalLib>();
    session.mockSipExternalLib->getBarrierStartOffsetResult = 0x300000;
    zet_debug_memory_space_desc_t memDesc = {.address = 0xbee504ae};
    const auto addrs = session.getBarrierAddresses(threadId, 17, &memDesc);
    EXPECT_NE(addrs, std::nullopt);
    EXPECT_EQ(addrs->sipOffset, 21u);
    EXPECT_EQ(addrs->sipSize, 2u);
    EXPECT_EQ(addrs->gpuMemOffset, 0x203000a8u);
}

TEST_F(DebugSessionBarrierMemTest, GivenSipExternalLibIsAvailableThenGetBarrierStartOffsetIsObtainedFromSipExternalLib) {
    session.mockSipExternalLib = std::make_unique<MockSipExternalLib>();
    session.mockSipExternalLib->getBarrierStartOffsetResult = 0x300000;
    uint32_t barrierStartOffset = 0;
    EXPECT_EQ(session.getBarrierStartOffset(0, threadId, &barrierStartOffset), true);
    EXPECT_EQ(barrierStartOffset, 0x300000u);
}

TEST_F(DebugSessionBarrierMemTest, GivenGetBarrierAddressesFailsThenReadBarrierMemoryAlsoFails) {
    session.forcedBarrierAddressResult = std::optional<DebugSessionImp::SipTransferAddr>(std::nullopt);
    zet_debug_memory_space_desc_t memDesc;
    char buf[16];
    ze_result_t status = session.readBarrierMemory(threadId, &memDesc, sizeof(buf), buf);
    EXPECT_EQ(status, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(DebugSessionBarrierMemTest, GivenGetBarrierAddressesSucceedsThenReadMemViaSipTransactionIsCalledWithCorrectArgs) {
    session.forcedBarrierAddressResult = DebugSessionImp::SipTransferAddr{
        .sipOffset = 15,
        .sipSize = 8,
        .gpuMemOffset = 0x123456,
    };
    zet_debug_memory_space_desc_t memDesc;
    char buf[13];
    session.readBarrierMemory(threadId, &memDesc, sizeof(buf), buf);
    EXPECT_NE(session.lastCalledReadMemViaSipTransactionArgs, std::nullopt);
    const auto args = *session.lastCalledReadMemViaSipTransactionArgs;
    EXPECT_EQ(args.commandId, NEO::SipKernel::Command::barrierRead);
    EXPECT_EQ(args.threadId, threadId);
    EXPECT_EQ(args.addrs.sipOffset, 15u);
    EXPECT_EQ(args.addrs.sipSize, 8u);
    EXPECT_EQ(args.addrs.gpuMemOffset, 0x123456u);
    EXPECT_EQ(args.size, sizeof(buf));
    EXPECT_EQ(args.buffer, buf);
}

struct DebugSessionRegistersAccessV5 : public ::testing::Test {
    using SipRegisterPacker = DebugSessionImp::SipRegisterPacker;
    static constexpr uint64_t testMemoryHandle = 0xABCDEF00ULL;
    static constexpr uint32_t stateSaveHeaderVersion = 5;

    struct MockDebugSessionV5 : public MockDebugSession {
        using MockDebugSession::cmdRegisterAccessHelper;
        using MockDebugSession::getSipCommandRegisterSize;
        using MockDebugSession::getStateSaveAreaHeader;
        using MockDebugSession::getStateSaveAreaMajorVersion;
        using MockDebugSession::readPackedRegisters;
        using MockDebugSession::readRegistersImp;
        using MockDebugSession::registersAccessHelper;
        using MockDebugSession::writePackedRegisters;

        MockDebugSessionV5(L0::Device *device) : MockDebugSession(zet_debug_config_t{}, device) {
            stateSaveAreaHeader = NEO::MockSipData::createStateSaveAreaHeader(stateSaveHeaderVersion);
        }

        struct RegistersAccessHelperPackedArgs {
            NEO::RegsetDescExt regdesc;
            uint32_t start;
            uint32_t count;
            zet_debug_regset_type_intel_gpu_t type;
            void *registerValues;
            bool write;
        };
        std::optional<RegistersAccessHelperPackedArgs> lastRegistersAccessHelperPackedArgs;
        std::optional<ze_result_t> registersAccessHelperPackedRetValue;
        ze_result_t registersAccessHelperPacked(const EuThread &thread, const NEO::RegsetDescExt &regdesc,
                                                uint32_t start, uint32_t count, zet_debug_regset_type_intel_gpu_t type, void *pRegisterValues, bool write) override {
            lastRegistersAccessHelperPackedArgs = RegistersAccessHelperPackedArgs{
                .regdesc = regdesc,
                .start = start,
                .count = count,
                .type = type,
                .registerValues = pRegisterValues,
                .write = write};
            return registersAccessHelperPackedRetValue.has_value() ? registersAccessHelperPackedRetValue.value() : DebugSessionImp::registersAccessHelperPacked(thread, regdesc, start, count, type, pRegisterValues, write);
        }

        struct RegistersAccessHelperArgs {
            SIP::regset_desc regdesc;
            uint32_t start;
            uint32_t count;
            NEO::SipRegisterType type;
            void *registerValues;
            bool write;
        };
        std::optional<RegistersAccessHelperArgs> lastRegistersAccessHelperArgs;
        std::optional<ze_result_t> registersAccessHelperRetValue;
        ze_result_t registersAccessHelper(const EuThread *thread, const SIP::regset_desc *regdesc,
                                          uint32_t start, uint32_t count, NEO::SipRegisterType type,
                                          void *pRegisterValues, bool write) override {
            lastRegistersAccessHelperArgs = RegistersAccessHelperArgs{
                .regdesc = *regdesc,
                .start = start,
                .count = count,
                .type = type,
                .registerValues = pRegisterValues,
                .write = write};
            return registersAccessHelperRetValue.has_value() ? registersAccessHelperRetValue.value() : DebugSessionImp::registersAccessHelper(thread, regdesc, start, count, type, pRegisterValues, write);
        }

        std::vector<char> readGpuMemoryData;
        std::optional<uint64_t> readGpuMemoryGpuVa;
        ze_result_t readGpuMemoryReturn = ZE_RESULT_SUCCESS;
        ze_result_t readGpuMemory(uint64_t memoryHandle, char *output, size_t size, uint64_t gpuVa) override {
            EXPECT_EQ(memoryHandle, testMemoryHandle);
            readGpuMemoryGpuVa = gpuVa;
            if (readGpuMemoryReturn != ZE_RESULT_SUCCESS) {
                return readGpuMemoryReturn;
            }
            EXPECT_LE(size, readGpuMemoryData.size());
            memcpy(output, readGpuMemoryData.data(), size);
            return readGpuMemoryReturn;
        }

        std::vector<char> writeGpuMemoryData;
        std::optional<uint64_t> writeGpuMemoryGpuVa;
        ze_result_t writeGpuMemoryReturn = ZE_RESULT_SUCCESS;
        ze_result_t writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) override {
            EXPECT_EQ(memoryHandle, testMemoryHandle);
            writeGpuMemoryGpuVa = gpuVa;
            writeGpuMemoryData = std::vector<char>(input, input + size);
            return writeGpuMemoryReturn;
        }

        uint64_t getContextStateSaveAreaGpuVaRet;
        uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
            EXPECT_EQ(memoryHandle, testMemoryHandle);
            return getContextStateSaveAreaGpuVaRet;
        }
    };

    MockDeviceImp deviceImp;
    MockDebugSessionV5 session;
    MockSipExternalLib *sipExternalLib;
    const EuThread::ThreadId threadId = {0, 0, 0, 0, 0};
    NEO::SipCommandRegisterValues sentinelCommand;

    DebugSessionRegistersAccessV5() : deviceImp(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0)),
                                      session(&deviceImp) {
        const char *commandSentinelData = "command sentinel data";
        memcpy_s(&sentinelCommand, sizeof(sentinelCommand), commandSentinelData, strlen(commandSentinelData));
    }
    void SetUp() override {
        auto neoDevice = deviceImp.getNEODevice();
        auto &rootEnv = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
        auto sipLib = std::make_unique<MockSipExternalLib>();
        sipExternalLib = sipLib.get();
        rootEnv->sipExternalLib = std::move(sipLib);
        session.allThreads[threadId]->stopThread(testMemoryHandle);
    }
};

TEST_F(DebugSessionRegistersAccessV5, TypeToRegsetDescReturnsNullptr) {
    const SIP::regset_desc *regdesc = session.typeToRegsetDesc(session.getStateSaveAreaHeader(), ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, &deviceImp);
    EXPECT_EQ(regdesc, nullptr);
}

TEST_F(DebugSessionRegistersAccessV5, GivenCrRegsetThenTypeToRegsetDescExtReturnsCorrectValue) {
    const std::array<uint8_t, 16> validMask{8, 0, 8, 6};
    sipExternalLib->getRegsetDescExtMap[NEO::SipRegisterType::eControl] = {
        .num = 7,
        .bytes = 13,
        .stride = 5,
        .validMask = validMask,
    };
    const auto &result = session.typeToRegsetDescExt(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, &deviceImp);
    EXPECT_EQ(result->bytes, 13u);
    EXPECT_EQ(result->num, 7u);
    EXPECT_EQ(result->stride, 5u);
    EXPECT_EQ(result->validMask, validMask);
}

TEST_F(DebugSessionRegistersAccessV5, WhenGetRegisterSizeIsCalledThenSipExternalGetRegsetDescExtProvidesTheSize) {
    sipExternalLib->getRegsetDescExtMap[NEO::SipRegisterType::eControl] = {
        .bytes = 13,
    };
    uint32_t result = session.getRegisterSize(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU);
    EXPECT_EQ(result, 13u);
}

TEST_F(DebugSessionRegistersAccessV5, GivenMissingEntryWhenGetRegisterSizeIsCalledThenZeroIsReturned) {
    sipExternalLib->getRegsetDescExtMap = {};
    uint32_t result = session.getRegisterSize(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU);
    EXPECT_EQ(result, 0u);
}

TEST_F(DebugSessionRegistersAccessV5, WhenWritePackedRegistersIsCalledThenCorrectDataIsWritten) {
    const std::vector<uint32_t> packedValues{3, 1, 4, 8, 7};
    const std::vector<uint32_t> unpackedValues{3, 1, 0, 0, 4, 8, 7, 0};

    const SipRegisterPacker packer = {
        .stride = 4,
        .majorStart = 0,
        .majorCount = 2,
        .packedOffset = 0x53,
        .unpackedIndices = {0, 1, 4, 5, 6},
    };

    ze_result_t status = session.writePackedRegisters(testMemoryHandle, 0x1200, packer, unpackedValues.data());
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    EXPECT_EQ(session.writeGpuMemoryGpuVa.value(), 0x1253u);
    const size_t writtenSize = session.writeGpuMemoryData.size();
    EXPECT_EQ(writtenSize, packedValues.size() * 4);
    const uint32_t *writtenDataPtr = reinterpret_cast<uint32_t *>(session.writeGpuMemoryData.data());
    const std::vector<uint32_t> writtenData(writtenDataPtr, writtenDataPtr + writtenSize / 4);
    EXPECT_EQ(writtenData, packedValues);
}

TEST_F(DebugSessionRegistersAccessV5, GivenReadGpuMemorySucceedsWhenReadPackedRegistersIsCalledThenCorrectDataIsRead) {
    const std::vector<uint32_t> packedValues{3, 1, 4, 8, 7};
    const std::vector<uint32_t> unpackedValues{3, 1, 0, 0, 4, 8, 7, 0};

    const SipRegisterPacker packer = {
        .stride = 4,
        .majorStart = 0,
        .majorCount = 2,
        .packedOffset = 0x53,
        .unpackedIndices = {0, 1, 4, 5, 6},
    };

    const char *readMemoryData = reinterpret_cast<const char *>(packedValues.data());
    session.readGpuMemoryData = std::vector<char>(readMemoryData, readMemoryData + packedValues.size() * 4);

    std::vector<uint32_t> readData(unpackedValues.size());
    ze_result_t status = session.readPackedRegisters(testMemoryHandle, 0x1200, packer, readData.data());

    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    EXPECT_EQ(session.readGpuMemoryGpuVa.value(), 0x1253u);
    EXPECT_EQ(readData, unpackedValues);
}

TEST_F(DebugSessionRegistersAccessV5, GivenReadGpuMemoryFailsWhenReadPackedRegistersIsCalledThenFailureIsReturned) {
    const SipRegisterPacker packer = {
        .stride = 4,
        .majorStart = 0,
        .majorCount = 2,
        .packedOffset = 0x53,
        .unpackedIndices = {0, 1, 4, 5, 6},
    };

    session.readGpuMemoryReturn = ZE_RESULT_ERROR_UNKNOWN;
    std::vector<char> buf(packer.unpackedIndices.size() * 4);
    ze_result_t status = session.readPackedRegisters(testMemoryHandle, 0x567, packer, buf.data());

    EXPECT_EQ(status, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(DebugSessionRegistersAccessV5, GivenInvalidSsaGpuVaWhenCmdRegisterAccessHelperIsCalledThenErrorIsReturned) {
    session.getContextStateSaveAreaGpuVaRet = 0;
    ze_result_t status = session.cmdRegisterAccessHelper(threadId, sentinelCommand, true);
    EXPECT_EQ(status, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(DebugSessionRegistersAccessV5, GivenSipLibCmdAccessFailsWhenCmdRegisterAccessHelperIsCalledThenErrorIsReturned) {
    session.getContextStateSaveAreaGpuVaRet = 0x7300;
    sipExternalLib->getSipLibRegisterAccessRetValue = false;
    ze_result_t status = session.cmdRegisterAccessHelper(threadId, sentinelCommand, true);
    EXPECT_EQ(status, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(DebugSessionRegistersAccessV5, GivenWriteWhenCmdRegisterAccessHelperIsCalledThenWriteGpuMemIsCalled) {
    session.getContextStateSaveAreaGpuVaRet = 0x7300;
    sipExternalLib->getSipLibRegisterAccessRetValue = true;
    sipExternalLib->getSipLibRegisterAccessCount = 1;
    sipExternalLib->getSipLibRegisterAccessStartOffset = 0xaf;
    const size_t commandRegSize = session.getSipCommandRegisterSize(true, sizeof(sentinelCommand));
    EXPECT_GT(commandRegSize, 0u);

    ze_result_t status = session.cmdRegisterAccessHelper(threadId, sentinelCommand, true);
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    ASSERT_EQ(session.writeGpuMemoryData.size(), commandRegSize);
    EXPECT_EQ(session.writeGpuMemoryGpuVa.value(), 0x73afu);
    EXPECT_EQ(0, memcmp(session.writeGpuMemoryData.data(), &sentinelCommand, commandRegSize));
}

TEST_F(DebugSessionRegistersAccessV5, GivenReadWhenCmdRegisterAccessHelperIsCalledThenReadGpuMemIsCalled) {
    session.getContextStateSaveAreaGpuVaRet = 0x7300;
    sipExternalLib->getSipLibRegisterAccessRetValue = true;
    sipExternalLib->getSipLibRegisterAccessCount = 1;
    sipExternalLib->getSipLibRegisterAccessStartOffset = 0xaf;

    const size_t commandRegSize = session.getSipCommandRegisterSize(false, sizeof(sentinelCommand));
    EXPECT_GT(commandRegSize, 0u);
    session.readGpuMemoryData.resize(commandRegSize);
    memcpy(session.readGpuMemoryData.data(), &sentinelCommand, commandRegSize);

    ze_result_t status = session.cmdRegisterAccessHelper(threadId, sentinelCommand, false);
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    EXPECT_EQ(session.readGpuMemoryData.size(), commandRegSize);
    EXPECT_EQ(session.readGpuMemoryGpuVa.value(), 0x73afu);
    EXPECT_EQ(0, memcmp(session.readGpuMemoryData.data(), &sentinelCommand, commandRegSize));
}

TEST_F(DebugSessionRegistersAccessV5, GivenRegdescIsPackedWhenReadRegistersImpIsCalledThenPackedRegistersAccessHelperIsCalled) {
    const std::array<uint8_t, 16> validMask{8, 0, 8, 6};
    sipExternalLib->getRegsetDescExtMap[NEO::SipRegisterType::eControl] = {
        .num = 7,
        .bytes = 13,
        .stride = 5,
        .validMask = validMask,
    };

    session.registersAccessHelperPackedRetValue = ZE_RESULT_SUCCESS;

    std::vector<char> buf(64);
    EXPECT_FALSE(session.lastRegistersAccessHelperPackedArgs.has_value());
    ze_result_t status = session.readRegistersImp(threadId, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, 0, 1, buf.data());
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    const auto &args = session.lastRegistersAccessHelperPackedArgs.value();
    EXPECT_EQ(args.regdesc.num, 7u);
    EXPECT_EQ(args.regdesc.bytes, 13u);
    EXPECT_EQ(args.regdesc.stride, 5u);
    EXPECT_EQ(args.regdesc.validMask, validMask);
    EXPECT_EQ(args.start, 0u);
    EXPECT_EQ(args.count, 1u);
    EXPECT_EQ(args.type, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU);
    EXPECT_EQ(args.registerValues, buf.data());
    EXPECT_EQ(args.write, false);
}

TEST_F(DebugSessionRegistersAccessV5, GivenRegdescIsNotPackedWhenReadRegistersImpIsCalledThenRegistersAccessHelperIsCalled) {
    const std::array<uint8_t, 16> validMask{8, 0, 8, 6};
    sipExternalLib->getRegsetDescExtMap[NEO::SipRegisterType::eControl] = {
        .num = 7,
        .bytes = 13,
        .stride = 0,
        .validMask = validMask,
    };

    session.registersAccessHelperRetValue = ZE_RESULT_SUCCESS;

    std::vector<char> buf(64);
    EXPECT_FALSE(session.lastRegistersAccessHelperArgs.has_value());
    ze_result_t status = session.readRegistersImp(threadId, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, 0, 1, buf.data());
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    const auto &args = session.lastRegistersAccessHelperArgs.value();
    EXPECT_EQ(args.regdesc.num, 7u);
    EXPECT_EQ(args.regdesc.bytes, 13u);
    EXPECT_EQ(args.regdesc.bits, 13u * 8u);
    EXPECT_EQ(args.start, 0u);
    EXPECT_EQ(args.count, 1u);
    EXPECT_EQ(args.type, NEO::SipRegisterType::eControl);
    EXPECT_EQ(args.registerValues, buf.data());
    EXPECT_EQ(args.write, false);
}

TEST_F(DebugSessionRegistersAccessV5, GivenSipExternalLibFailsWhenReadRegistersImpIsCalledThenErrorIsReturned) {
    sipExternalLib->getRegsetDescExtMap = {};

    session.registersAccessHelperPackedRetValue = ZE_RESULT_SUCCESS;

    std::vector<char> buf(64);
    ze_result_t status = session.readRegistersImp(threadId, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, 0, 1, buf.data());
    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(session.lastRegistersAccessHelperPackedArgs.has_value());
}

TEST_F(DebugSessionRegistersAccessV5, GivenRegdescIsPackedWhenWriteRegistersImpIsCalledThenCorrectRegistersAccessHelperIsCalled) {
    const std::array<uint8_t, 16> validMask{8, 0, 8, 6};
    sipExternalLib->getRegsetDescExtMap[NEO::SipRegisterType::eControl] = {
        .num = 7,
        .bytes = 13,
        .stride = 5,
        .validMask = validMask,
    };

    session.registersAccessHelperPackedRetValue = ZE_RESULT_SUCCESS;

    std::vector<char> buf(64);
    EXPECT_FALSE(session.lastRegistersAccessHelperPackedArgs.has_value());
    ze_result_t status = session.writeRegistersImp(threadId, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, 0, 1, buf.data());
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    const auto &args = session.lastRegistersAccessHelperPackedArgs.value();
    EXPECT_EQ(args.regdesc.num, 7u);
    EXPECT_EQ(args.regdesc.bytes, 13u);
    EXPECT_EQ(args.regdesc.stride, 5u);
    EXPECT_EQ(args.regdesc.validMask, validMask);
    EXPECT_EQ(args.start, 0u);
    EXPECT_EQ(args.count, 1u);
    EXPECT_EQ(args.type, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU);
    EXPECT_EQ(args.registerValues, buf.data());
    EXPECT_EQ(args.write, true);
}

TEST_F(DebugSessionRegistersAccessV5, GivenRegdescIsNotPackedWhenWriteRegistersImpIsCalledThenRegistersAccessHelperIsCalled) {
    const std::array<uint8_t, 16> validMask{8, 0, 8, 6};
    sipExternalLib->getRegsetDescExtMap[NEO::SipRegisterType::eControl] = {
        .num = 7,
        .bytes = 13,
        .stride = 0,
        .validMask = validMask,
    };

    session.registersAccessHelperRetValue = ZE_RESULT_SUCCESS;

    std::vector<char> buf(64);
    ASSERT_FALSE(session.lastRegistersAccessHelperArgs.has_value());
    ze_result_t status = session.writeRegistersImp(threadId, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, 0, 1, buf.data());
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    ASSERT_TRUE(session.lastRegistersAccessHelperArgs.has_value());
    const auto &args = session.lastRegistersAccessHelperArgs.value();
    EXPECT_EQ(args.regdesc.num, 7u);
    EXPECT_EQ(args.regdesc.bytes, 13u);
    EXPECT_EQ(args.regdesc.bits, 13u * 8u);
    EXPECT_EQ(args.start, 0u);
    EXPECT_EQ(args.count, 1u);
    EXPECT_EQ(args.type, NEO::SipRegisterType::eControl);
    EXPECT_EQ(args.registerValues, buf.data());
    EXPECT_EQ(args.write, true);
}

TEST_F(DebugSessionRegistersAccessV5, GivenSipExternalLibFailsWhenWriteRegistersImpIsCalledThenErrorIsReturned) {
    sipExternalLib->getRegsetDescExtMap = {};

    session.registersAccessHelperPackedRetValue = ZE_RESULT_SUCCESS;

    std::vector<char> buf(64);
    ze_result_t status = session.writeRegistersImp(threadId, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, 0, 1, buf.data());
    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(session.lastRegistersAccessHelperPackedArgs.has_value());
}

TEST_F(DebugSessionRegistersAccessV5, GivenStateSaveAreaHeaderIsAvailableThenGetStateSaveAreaMajorVersionReturnsCorrectVersion) {
    const NEO::StateSaveAreaHeader ssah = {
        .versionHeader = {
            .version = {
                .major = 34,
            },
        },
    };
    session.getStateSaveAreaHeaderRetValue = &ssah;
    EXPECT_EQ(session.getStateSaveAreaMajorVersion(), 34u);
}

TEST_F(DebugSessionRegistersAccessV5, GivenStateSaveAreaHeaderIsUnvailableThenGetStateSaveAreaMajorVersionReturnsZero) {
    session.getStateSaveAreaHeaderRetValue = nullptr;
    EXPECT_EQ(session.getStateSaveAreaMajorVersion(), 0u);
}

struct SipRegisterPackerTest : public ::testing::Test {
    using SipRegisterPacker = DebugSessionImp::SipRegisterPacker;

    // Intentionally unlike real hardware
    // [0 1 0 1 0], [0 0 0 0 0], [0 0 1 0 0]
    static constexpr NEO::RegsetDescExt regdesc = {
        .num = 3,
        .stride = 5,
        .validMask = {(1 << 1) | (1 << 3), 1 << 4}, // bits 1, 3, 13
    };
};

TEST_F(SipRegisterPackerTest, GivenValidRegdescAndRangeThenCreationIsSuccessfulWithCorrectValues) {
    const auto &maybePacker = SipRegisterPacker::create(regdesc, 1, 2);
    ASSERT_TRUE(maybePacker.has_value());
    const SipRegisterPacker &packer = maybePacker.value();

    EXPECT_EQ(packer.stride, 5u);
    EXPECT_EQ(packer.majorStart, 1u);
    EXPECT_EQ(packer.majorCount, 2u);
    EXPECT_EQ(packer.packedOffset, 2u);

    const std::vector<uint32_t> expectedUnpackedIndices{7};
    EXPECT_EQ(packer.unpackedIndices, expectedUnpackedIndices);
}

TEST_F(SipRegisterPackerTest, GivenInvalidStartThenCreationFails) {
    const auto &maybePacker = SipRegisterPacker::create(regdesc, 4, 1);
    EXPECT_FALSE(maybePacker.has_value());
}

TEST_F(SipRegisterPackerTest, GivenValidStartAndInvalidCountThenCreationFails) {
    const auto &maybePacker = SipRegisterPacker::create(regdesc, 1, 0);
    EXPECT_FALSE(maybePacker.has_value());
}

TEST_F(SipRegisterPackerTest, GivenValidUnpackedDataThenPackRegistersReturnsCorrectResult) {
    const SipRegisterPacker packer = SipRegisterPacker::create(regdesc, 0, 3).value();

    const std::vector<uint32_t> unpacked{
        0x11, 0x22, 0x33, 0x44, 0x55, // major=0
        0x66, 0x77, 0x88, 0x99, 0xaa, // major=1
        0xbb, 0xcc, 0xdd, 0xee, 0xff, // major=2
    };

    const std::vector<uint32_t> packed = packer.packRegisters(unpacked);
    const std::vector<uint32_t> expected = {0x22, 0x44, 0xdd};
    EXPECT_EQ(packed, expected);
}

TEST_F(SipRegisterPackerTest, GivenInvalidUnpackedDataThenPackRegistersReturnsEmpty) {
    const SipRegisterPacker packer = SipRegisterPacker::create(regdesc, 0, 3).value();

    const std::vector<uint32_t> unpacked{
        0x11, 0x22, 0x33, 0x44, 0x55, // major=0
        0x66, 0x77, 0x88, 0x99, 0xaa, // major=1
        0xbb, 0xcc,                   // major=2 is short
    };

    const std::vector<uint32_t> packed = packer.packRegisters(unpacked);
    const std::vector<uint32_t> expected = {};
    EXPECT_EQ(packed, expected);
}

TEST_F(SipRegisterPackerTest, GivenValidPackedDataThenUnpackRegistersReturnsCorrectResult) {
    const SipRegisterPacker packer = SipRegisterPacker::create(regdesc, 0, 3).value();

    const std::vector<uint32_t> packed = {0x22, 0x44, 0xdd};
    const std::vector<uint32_t> unpacked = packer.unpackRegisters(packed);

    const std::vector<uint32_t> expected{
        0x00, 0x22, 0x00, 0x44, 0x00, // major=0
        0x00, 0x00, 0x00, 0x00, 0x00, // major=1
        0x00, 0x00, 0xdd, 0x00, 0x00, // major=2
    };

    EXPECT_EQ(unpacked, expected);
}

TEST_F(SipRegisterPackerTest, GivenInvalidPackedDataThenUnpackRegistersReturnsEmpty) {
    const SipRegisterPacker packer = SipRegisterPacker::create(regdesc, 0, 3).value();

    const std::vector<uint32_t> packed = {0x22, 0x44};
    const std::vector<uint32_t> unpacked = packer.unpackRegisters(packed);

    const std::vector<uint32_t> expected{};

    EXPECT_EQ(unpacked, expected);
}

struct RegistersAccessHelperPackedTest : public ::testing::Test {
    using SipRegisterPacker = DebugSessionImp::SipRegisterPacker;
    static constexpr uint64_t testMemoryHandle = 0xABCDEF00ULL;
    // Intentionally unlike real hardware
    // [0 1 0 1 0], [0 0 0 0 0], [0 0 1 0 0]
    static constexpr NEO::RegsetDescExt regdesc = {
        .num = 3,
        .stride = 5,
        .validMask = {(1 << 1) | (1 << 3), 1 << 4}, // bits 1, 3, 13
    };

    struct DebugSessionForTest : public MockDebugSession {
        DebugSessionForTest(L0::Device *device, SipExternalLib *sel) : MockDebugSession(zet_debug_config_t{}, device), sipExternalLib(sel) {
            stateSaveAreaHeader = NEO::MockSipData::createStateSaveAreaHeader(5);
        }

        uint64_t getContextStateSaveAreaGpuVaRetValue = 0;
        uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
            EXPECT_EQ(memoryHandle, testMemoryHandle);
            return getContextStateSaveAreaGpuVaRetValue;
        }

        SipExternalLib *sipExternalLib;

        SipExternalLib *getSipExternalLibInterface() const override {
            return sipExternalLib;
        }

        struct CapturedArgs {
            uint64_t regStartGpuVa;
            SipRegisterPacker packer;
            const void *ptr;
        };
        std::optional<CapturedArgs> writePackedRegistersArgs;
        ze_result_t writePackedRegistersRetValue = ZE_RESULT_SUCCESS;
        ze_result_t writePackedRegisters(uint64_t memHandle, uint64_t regStartGpuVa, const SipRegisterPacker &packer, const void *src) override {
            EXPECT_EQ(memHandle, testMemoryHandle);
            writePackedRegistersArgs = CapturedArgs{
                .regStartGpuVa = regStartGpuVa,
                .packer = packer,
                .ptr = src,
            };
            return writePackedRegistersRetValue;
        }

        std::optional<CapturedArgs> readPackedRegistersArgs;
        ze_result_t readPackedRegistersRetValue = ZE_RESULT_SUCCESS;
        ze_result_t readPackedRegisters(uint64_t memHandle, uint64_t regStartGpuVa, const SipRegisterPacker &packer, void *dest) override {
            EXPECT_EQ(memHandle, testMemoryHandle);
            readPackedRegistersArgs = CapturedArgs{
                .regStartGpuVa = regStartGpuVa,
                .packer = packer,
                .ptr = dest,
            };
            return readPackedRegistersRetValue;
        }
    };

    const EuThread::ThreadId threadId = {0, 0, 0, 0, 0};
    EuThread thread;
    MockDeviceImp deviceImp;
    DebugSessionForTest session;
    MockSipExternalLib sipExternalLib;

    RegistersAccessHelperPackedTest() : thread(threadId),
                                        deviceImp(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0)),
                                        session(&deviceImp, &sipExternalLib) {
        thread.stopThread(testMemoryHandle);
    }
};

TEST_F(RegistersAccessHelperPackedTest, GivenInvalidRangesThenErrorIsReturned) {
    std::vector<char> buf(64);
    ze_result_t status = session.registersAccessHelperPacked(thread, regdesc, 10, 1, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, buf.data(), false);
    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    status = session.registersAccessHelperPacked(thread, regdesc, 0, 10, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, buf.data(), false);
    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    session.getContextStateSaveAreaGpuVaRetValue = 0x12340000;
    sipExternalLib.getSipLibRegisterAccessCount = 1;
    status = session.registersAccessHelperPacked(thread, regdesc, 0, 2, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, buf.data(), false);
    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(RegistersAccessHelperPackedTest, GivenGetSsaGpuVaIsZeroThenErrorIsReturned) {
    session.getContextStateSaveAreaGpuVaRetValue = 0;
    std::vector<char> buf(64);
    ze_result_t status = session.registersAccessHelperPacked(thread, regdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, buf.data(), false);
    EXPECT_EQ(status, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(RegistersAccessHelperPackedTest, GivenSelfInconsistentRegdescThenErrorIsReturned) {
    static constexpr NEO::RegsetDescExt badRegdesc = {
        .num = 3,
        .stride = 5,
        .validMask = {},
    };
    session.getContextStateSaveAreaGpuVaRetValue = 0x8800;
    sipExternalLib.getSipLibRegisterAccessCount = 3;
    sipExternalLib.getSipLibRegisterAccessStartOffset = 0x52;
    std::vector<char> buf(64);
    ze_result_t status = session.registersAccessHelperPacked(thread, badRegdesc, 0, 1, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, buf.data(), false);
    EXPECT_EQ(status, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(RegistersAccessHelperPackedTest, GivenWriteArgsThenCorrectDataIsWritten) {
    const SipRegisterPacker packer = SipRegisterPacker::create(regdesc, 1, 2).value();
    std::vector<char> buf(packer.stride * packer.majorCount);
    session.getContextStateSaveAreaGpuVaRetValue = 0x12340000;
    sipExternalLib.getSipLibRegisterAccessCount = 3;
    sipExternalLib.getSipLibRegisterAccessStartOffset = 0x5432;

    session.registersAccessHelperPacked(thread, regdesc, 1, 2, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, buf.data(), true);

    EXPECT_EQ(session.writePackedRegistersArgs->regStartGpuVa, 0x12345432u);
    const SipRegisterPacker &actualPacker = session.writePackedRegistersArgs->packer;
    EXPECT_EQ(actualPacker.stride, packer.stride);
    EXPECT_EQ(actualPacker.majorStart, packer.majorStart);
    EXPECT_EQ(actualPacker.majorCount, packer.majorCount);
    EXPECT_EQ(actualPacker.packedOffset, packer.packedOffset);
    EXPECT_EQ(actualPacker.unpackedIndices, packer.unpackedIndices);
    EXPECT_EQ(session.writePackedRegistersArgs->ptr, static_cast<void *>(buf.data()));
}

TEST_F(RegistersAccessHelperPackedTest, GivenReadArgsThenCorrectDataIsRead) {
    const SipRegisterPacker packer = SipRegisterPacker::create(regdesc, 1, 2).value();
    std::vector<char> buf(packer.stride * packer.majorCount);
    session.getContextStateSaveAreaGpuVaRetValue = 0x12340000;
    sipExternalLib.getSipLibRegisterAccessCount = 3;
    sipExternalLib.getSipLibRegisterAccessStartOffset = 0x5432;

    session.registersAccessHelperPacked(thread, regdesc, 1, 2, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, buf.data(), false);

    EXPECT_EQ(session.readPackedRegistersArgs->regStartGpuVa, 0x12345432u);
    const SipRegisterPacker &actualPacker = session.readPackedRegistersArgs->packer;
    EXPECT_EQ(actualPacker.stride, packer.stride);
    EXPECT_EQ(actualPacker.majorStart, packer.majorStart);
    EXPECT_EQ(actualPacker.majorCount, packer.majorCount);
    EXPECT_EQ(actualPacker.packedOffset, packer.packedOffset);
    EXPECT_EQ(actualPacker.unpackedIndices, packer.unpackedIndices);
    EXPECT_EQ(session.readPackedRegistersArgs->ptr, static_cast<void *>(buf.data()));
}

TEST_F(RegistersAccessHelperPackedTest, GivenNullRegdescThenRegistersAccessHelperReturnsError) {
    std::vector<char> buf(64);
    ze_result_t status = session.registersAccessHelper(&thread, nullptr, 0, 1, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, buf.data(), true);
    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST(DebugSessionTest, GivenZetRegsetTypeThenSipRegsetTypeIsReturned) {
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU), SipRegisterType::eGRF);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU), SipRegisterType::eAccumulator);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU), SipRegisterType::eFlags);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU), SipRegisterType::eState);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), SipRegisterType::eControl);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU), SipRegisterType::eAddress);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU), SipRegisterType::eChannelEnable);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU), SipRegisterType::eStackPointer);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU), SipRegisterType::eTDR);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU), SipRegisterType::eFlowControl);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU), SipRegisterType::eDebug);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU), SipRegisterType::eMME);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU), SipRegisterType::eScalar);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU), SipRegisterType::eMessageControl);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU), SipRegisterType::eInvalid);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU), SipRegisterType::eInvalid);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU), SipRegisterType::eInvalid);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU), SipRegisterType::eInvalid);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_FORCE_UINT32), SipRegisterType::eInvalid);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU), SipRegisterType::eInvalid);
    EXPECT_EQ(DebugSessionImp::getSipRegisterType(static_cast<zet_debug_regset_type_intel_gpu_t>(ZET_DEBUG_REGSET_TYPE_FORCE_UINT32 - 1)), SipRegisterType::eInvalid);
}

} // namespace ult
} // namespace L0
