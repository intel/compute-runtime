/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/i915_prelim.h"
// Force prelim headers over upstream headers
// prevent including any other headers to avoid redefintion errors
#define _I915_DRM_H_
#define _UAPI_I915_DRM_H_

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/include/zet_intel_gpu_debug.h"
#include "level_zero/tools/source/debug/debug_handlers.h"
#include "level_zero/tools/source/debug/linux/prelim/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/debug_session_fixtures_linux.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

#include "common/StateSaveAreaHeader.h"

#include <fstream>
#include <type_traits>

namespace NEO {
namespace SysCalls {
extern uint32_t closeFuncCalled;
extern int closeFuncArgPassed;
extern int closeFuncRetVal;
extern int setErrno;
extern uint32_t preadFuncCalled;
extern uint32_t pwriteFuncCalled;
extern uint32_t mmapFuncCalled;
extern uint32_t munmapFuncCalled;
} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace ult {

TEST(IoctlHandler, GivenHandlerWhenPreadCalledThenSysCallIsCalled) {
    L0::DebugSessionLinux::IoctlHandler handler;
    NEO::SysCalls::preadFuncCalled = 0;
    auto retVal = handler.pread(0, nullptr, 0, 0);

    EXPECT_EQ(0, retVal);
    EXPECT_EQ(1u, NEO::SysCalls::preadFuncCalled);
    NEO::SysCalls::preadFuncCalled = 0;
}

TEST(IoctlHandler, GivenHandlerWhenPwriteCalledThenSysCallIsCalled) {
    L0::DebugSessionLinux::IoctlHandler handler;
    NEO::SysCalls::pwriteFuncCalled = 0;
    auto retVal = handler.pwrite(0, nullptr, 0, 0);

    EXPECT_EQ(0, retVal);
    EXPECT_EQ(1u, NEO::SysCalls::pwriteFuncCalled);
    NEO::SysCalls::pwriteFuncCalled = 0;
}

TEST(IoctlHandler, GivenHandlerWhenMmapAndMunmapCalledThenRedirectedToSysCall) {
    L0::DebugSessionLinux::IoctlHandler handler;
    NEO::SysCalls::mmapFuncCalled = 0;
    NEO::SysCalls::munmapFuncCalled = 0;
    auto mappedPtr = handler.mmap(0, 0, 0, 0, 0, 0);

    EXPECT_EQ(0, mappedPtr);

    auto retVal = handler.munmap(0, 0);
    EXPECT_EQ(0, retVal);

    EXPECT_EQ(1u, NEO::SysCalls::mmapFuncCalled);
    EXPECT_EQ(1u, NEO::SysCalls::munmapFuncCalled);
    NEO::SysCalls::mmapFuncCalled = 0;
    NEO::SysCalls::munmapFuncCalled = 0;
}

TEST(DebugSessionTest, GivenDebugSessionWhenExtractingCpuVaFromUuidThenCorrectCpuVaReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, nullptr, 10);

    std::string uuid = "00000000-0000-0000-0000-5500044f4000";
    auto va = sessionMock->extractVaFromUuidString(uuid);
    uint64_t epxectedVa = 0x00005500044f4000;

    EXPECT_EQ(epxectedVa, va);

    uuid = "00000000-0000-0000-00ff-5500044f4000";
    va = sessionMock->extractVaFromUuidString(uuid);
    epxectedVa = 0x00ff5500044f4000;

    EXPECT_EQ(epxectedVa, va);
}

TEST(DebugSessionTest, WhenConvertingThreadIdsThenDeviceFunctionsAreCalled) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto mockDrm = new DrmQueryMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    ASSERT_NE(nullptr, sessionMock);

    ze_device_thread_t thread = {0, 0, 0, 0};

    auto threadID = sessionMock->convertToThreadId(thread);

    EXPECT_EQ(0u, threadID.tileIndex);
    EXPECT_EQ(0u, threadID.slice);
    EXPECT_EQ(0u, threadID.subslice);
    EXPECT_EQ(0u, threadID.eu);
    EXPECT_EQ(0u, threadID.thread);

    auto apiThread = sessionMock->convertToApi(threadID);

    EXPECT_EQ(0u, apiThread.slice);
    EXPECT_EQ(0u, apiThread.subslice);
    EXPECT_EQ(0u, apiThread.eu);
    EXPECT_EQ(0u, apiThread.thread);

    uint32_t deviceIndex = 1;

    auto physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(0u, physicalThread.slice);
    EXPECT_EQ(0u, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);

    thread.slice = UINT32_MAX;
    physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(uint32_t(UINT32_MAX), physicalThread.slice);
    EXPECT_EQ(0u, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);

    thread.slice = 0;
    thread.subslice = UINT32_MAX;
    thread.eu = 1;
    thread.thread = 3;
    physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(0u, physicalThread.slice);
    EXPECT_EQ(uint32_t(UINT32_MAX), physicalThread.subslice);
    EXPECT_EQ(1u, physicalThread.eu);
    EXPECT_EQ(3u, physicalThread.thread);
}

TEST(DebugSessionTest, WhenConvertingThreadIDsForDeviceWithSingleSliceThenSubsliceIsCorrectlyRemapped) {
    auto hwInfo = *NEO::defaultHwInfo.get();

    hwInfo.gtSystemInfo.SliceCount = 1;
    hwInfo.gtSystemInfo.SubSliceCount = 8;

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto mockDrm = new DrmQueryMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);

    mockDrm->storedSVal = 2; // slice 0 disabled in topology
    mockDrm->storedSSVal = hwInfo.gtSystemInfo.SubSliceCount;
    mockDrm->storedEUVal = hwInfo.gtSystemInfo.EUCount;
    mockDrm->disableSomeTopology = true;

    NEO::Drm::QueryTopologyData topologyData = {};
    mockDrm->queryTopology(neoDevice->getHardwareInfo(), topologyData);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    ASSERT_NE(nullptr, sessionMock);

    ze_device_thread_t thread = {UINT32_MAX, 1, 0, 0};
    uint32_t deviceIndex = 0;

    auto physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, physicalThread.slice);
    EXPECT_EQ(3u, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);

    thread.slice = 0;
    physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, physicalThread.slice);
    EXPECT_EQ(3u, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);
}

TEST(DebugSessionTest, WhenConvertingThreadIDsForDeviceWithMultipleSlicesThenSubsliceIsNotRemapped) {
    auto hwInfo = *NEO::defaultHwInfo.get();

    hwInfo.gtSystemInfo.SliceCount = 8;
    hwInfo.gtSystemInfo.SubSliceCount = 8;

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto mockDrm = new DrmQueryMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);

    mockDrm->storedSVal = hwInfo.gtSystemInfo.SliceCount;
    mockDrm->storedSSVal = hwInfo.gtSystemInfo.SubSliceCount;
    mockDrm->storedEUVal = hwInfo.gtSystemInfo.EUCount;
    mockDrm->disableSomeTopology = true;

    NEO::Drm::QueryTopologyData topologyData = {};
    mockDrm->queryTopology(neoDevice->getHardwareInfo(), topologyData);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    ASSERT_NE(nullptr, sessionMock);

    ze_device_thread_t thread = {UINT32_MAX, 1, 0, 0};
    uint32_t deviceIndex = 0;

    auto physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(UINT32_MAX, physicalThread.slice);
    EXPECT_EQ(thread.subslice, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);

    thread.slice = 0;
    physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, physicalThread.slice);
    EXPECT_EQ(thread.subslice, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);
}

TEST(DebugSessionTest, GivenDeviceWithSingleSliceWhenCallingAreRequestedThreadsStoppedForSliceAllThenCorrectValuesAreReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();

    hwInfo.gtSystemInfo.SliceCount = 1;
    hwInfo.gtSystemInfo.MaxSlicesSupported = 1;

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto mockDrm = new DrmQueryMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    mockDrm->storedSVal = 1;
    mockDrm->storedSSVal = hwInfo.gtSystemInfo.SubSliceCount;
    mockDrm->storedEUVal = hwInfo.gtSystemInfo.EUCount;

    NEO::Drm::QueryTopologyData topologyData = {};
    mockDrm->queryTopology(neoDevice->getHardwareInfo(), topologyData);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto sessionMock = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    ASSERT_NE(nullptr, sessionMock);

    ze_device_thread_t thread = {UINT32_MAX, 0, 0, 0};

    auto stopped = sessionMock->areRequestedThreadsStopped(thread);
    EXPECT_FALSE(stopped);

    EuThread::ThreadId threadId(0, 0, 0, 0, 0);
    sessionMock->allThreads[threadId]->stopThread(1u);

    uint32_t subDeviceCount = std::max(1u, neoDevice->getNumSubDevices());
    for (uint32_t i = 0; i < subDeviceCount; i++) {
        EuThread::ThreadId threadId(i, 0, 0, 0, 0);
        sessionMock->allThreads[threadId]->stopThread(1u);
    }
    stopped = sessionMock->areRequestedThreadsStopped(thread);
    EXPECT_TRUE(stopped);
}

TEST(DebugSessionTest, WhenEnqueueApiEventCalledThenEventPushed) {
    auto sessionMock = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, nullptr, 10);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;
    zet_debug_event_t debugEvent = {};

    sessionMock->enqueueApiEvent(debugEvent);

    EXPECT_EQ(1u, sessionMock->apiEvents.size());
}

TEST(DebugSessionTest, GivenLogsEnabledWhenPrintContextVmsCalledThenMapIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(255);

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, nullptr, 10);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    DebugSessionLinux::ContextParams param = {};
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[0] = param;
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[1] = param;

    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[0].handle = 0;
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[1].handle = 1;

    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[0].vm = 1;
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[1].vm = 2;

    EXPECT_EQ(2u, sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated.size());

    ::testing::internal::CaptureStdout();

    sessionMock->printContextVms();

    auto map = ::testing::internal::GetCapturedStdout();

    EXPECT_TRUE(hasSubstr(map, std::string("INFO: Context - VM map:")));
    EXPECT_TRUE(hasSubstr(map, std::string("Context = 0 : 1")));
    EXPECT_TRUE(hasSubstr(map, std::string("Context = 1 : 2")));
}

TEST(DebugSessionTest, GivenLogsDisabledWhenPrintContextVmsCalledThenMapIsiNotPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(0);

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, nullptr, 10);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    DebugSessionLinux::ContextParams param = {};
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[0] = param;
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[1] = param;

    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[0].handle = 0;
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[1].handle = 1;

    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[0].vm = 1;
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated[1].vm = 2;

    EXPECT_EQ(2u, sessionMock->clientHandleToConnection[sessionMock->clientHandle]->contextsCreated.size());

    ::testing::internal::CaptureStdout();

    sessionMock->printContextVms();

    auto map = ::testing::internal::GetCapturedStdout();

    EXPECT_TRUE(map.empty());
}

TEST(DebugSessionTest, GivenNullptrEventWhenReadingEventThenErrorNullptrReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, nullptr, 10);
    ASSERT_NE(nullptr, session);

    auto result = session->readEvent(10, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, result);
}

TEST(DebugSessionTest, GivenRootDebugSessionWhenCreateTileSessionCalledThenSessionIsCreated) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto mockDrm = new DrmQueryMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    struct DebugSession : public DebugSessionLinux {
        using DebugSessionLinux::createTileSession;
        using DebugSessionLinux::DebugSessionLinux;
    };

    auto session = std::make_unique<DebugSession>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    ASSERT_NE(nullptr, session);

    std::unique_ptr<TileDebugSessionLinux> tileSession = std::unique_ptr<TileDebugSessionLinux>{session->createTileSession(zet_debug_config_t{0x1234}, &deviceImp, nullptr)};
    EXPECT_NE(nullptr, tileSession);
}

TEST(DebugSessionTest, GivenRootLinuxSessionWhenCallingTileSepcificFunctionsThenUnrecoverableIsCalled) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto mockDrm = new DrmQueryMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    ASSERT_NE(nullptr, sessionMock);

    EXPECT_THROW(sessionMock->attachTile(), std::exception);
    EXPECT_THROW(sessionMock->detachTile(), std::exception);
}

using DebugApiLinuxTest = Test<DebugApiLinuxFixture>;

TEST_F(DebugApiLinuxTest, givenDeviceWhenCallingDebugAttachThenSuccessAndValidSessionHandleAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession = nullptr;

    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession);
}

TEST_F(DebugApiLinuxTest, givenDebugSessionWhenCallingDebugDetachThenSessionIsClosedAndSuccessReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession = nullptr;

    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession);

    result = zetDebugDetach(debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(DebugApiLinuxTest, WhenCallingResumeThenProperIoctlsAreCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    SIP::version version = {2, 0, 0};
    initStateSaveArea(sessionMock->stateSaveAreaHeader, version);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->vmToContextStateSaveAreaBindInfo[1u] = {0x1000, 0x1000};

    zet_debug_session_handle_t session = sessionMock->toHandle();
    ze_device_thread_t thread = {};

    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->stopThread(1u);

    auto result = L0::DebugApiHandlers::debugResume(session, thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(uint32_t(PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME), handler->euControl.cmd);
}

TEST_F(DebugApiLinuxTest, GivenUnknownEventWhenAcknowledgeEventCalledThenErrorUninitializedIsReturned) {
    auto sessionMock = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);

    zet_debug_session_handle_t session = sessionMock->toHandle();
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;
    zet_debug_event_t debugEvent = {};

    // No events to acknowledge
    auto result = L0::DebugApiHandlers::debugAcknowledgeEvent(session, &debugEvent);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNINITIALIZED);

    // One event to acknowledge
    prelim_drm_i915_debug_event eventToAck = {};
    eventToAck.type = 500;
    eventToAck.seqno = 5;
    eventToAck.flags = PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    eventToAck.size = sizeof(prelim_drm_i915_debug_event);

    debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
    debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
    debugEvent.info.module.load = 0x1000;
    debugEvent.info.module.moduleBegin = 0x2000;
    debugEvent.info.module.moduleEnd = 0x3000;

    sessionMock->pushApiEvent(debugEvent, &eventToAck);

    // Different event acknowledged
    debugEvent.info.module.load = 0x2221000;
    debugEvent.info.module.moduleBegin = 0x2222000;
    debugEvent.info.module.moduleEnd = 0x2223000;

    result = L0::DebugApiHandlers::debugAcknowledgeEvent(session, &debugEvent);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNINITIALIZED);

    EXPECT_EQ(0, handler->ioctlCalled);
}

TEST_F(DebugApiLinuxTest, GivenEventRequiringAckWhenAcknowledgeEventCalledThenSuccessIsReturned) {
    auto sessionMock = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);

    zet_debug_session_handle_t session = sessionMock->toHandle();
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    zet_debug_event_t debugEvent = {};
    debugEvent.flags = ZET_DEBUG_EVENT_FLAG_NEED_ACK;

    prelim_drm_i915_debug_event eventToAck = {};
    eventToAck.type = 500;
    eventToAck.seqno = 10;
    eventToAck.flags = PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    eventToAck.size = sizeof(prelim_drm_i915_debug_event);
    sessionMock->pushApiEvent(debugEvent, &eventToAck);

    auto result = zetDebugAcknowledgeEvent(session, &debugEvent);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(500u, handler->debugEventAcked.type);
    EXPECT_EQ(0u, handler->debugEventAcked.flags);
    EXPECT_EQ(10u, handler->debugEventAcked.seqno);
}

TEST_F(DebugApiLinuxTest, GivenSuccessfulInitializationWhenCreatingDebugSessionThenAsyncThreadIsStarted) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    ze_result_t result = ZE_RESULT_SUCCESS;

    mockDrm->context.debuggerOpenRetval = 10;
    mockDrm->baseErrno = false;
    mockDrm->errnoRetVal = 0;

    auto session = std::unique_ptr<DebugSession>(DebugSession::create(config, device, result));

    EXPECT_NE(nullptr, session);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto mockSession = static_cast<DebugSessionMock *>(session.get());
    EXPECT_TRUE(mockSession->asyncThreadStarted);
}

TEST_F(DebugApiLinuxTest, GivenRootDeviceWhenDebugSessionIsCreatedForTheSecondTimeThenSuccessIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto sessionMock = device->createDebugSession(config, result);
    ASSERT_NE(nullptr, sessionMock);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto sessionMock2 = device->createDebugSession(config, result);
    EXPECT_EQ(sessionMock, sessionMock2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(DebugApiLinuxTest, GivenClientAndMatchingUuidEventsWhenReadingEventsThenProcessEntryIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    prelim_drm_i915_debug_event_client clientCreate = {};
    clientCreate.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    clientCreate.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    clientCreate.base.size = sizeof(prelim_drm_i915_debug_event_client);
    clientCreate.handle = MockDebugSessionLinux::mockClientHandle;

    auto uuidName = NEO::uuidL0CommandQueueName;
    auto uuidNameSize = strlen(uuidName);
    auto uuidHash = NEO::uuidL0CommandQueueHash;

    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuid.handle = 2;
    uuid.payload_size = uuidNameSize;

    prelim_drm_i915_debug_read_uuid readUuid = {};
    readUuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    memcpy(readUuid.uuid, uuidHash, strlen(uuidHash));
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(uuidName);
    readUuid.payload_size = uuid.payload_size;
    readUuid.handle = 3;

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&clientCreate), static_cast<uint64_t>(clientCreate.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&uuid), static_cast<uint64_t>(uuid.base.size)});
    handler->pollRetVal = 1;
    handler->returnUuid = &readUuid;

    session->ioctlHandler.reset(handler);
    session->initialize();

    handler->pollRetVal = 1;

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(session->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY, event.type);
}

TEST_F(DebugApiLinuxTest, GivenValidClassNameUuidWhenHandlingEventThenClientHandleIsSaved) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    auto uuidName = NEO::classNamesToUuid[0].first;
    auto uuidNameSize = strlen(uuidName);
    auto uuidHash = NEO::classNamesToUuid[0].second;

    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuid.handle = 2;
    uuid.payload_size = uuidNameSize;

    prelim_drm_i915_debug_read_uuid readUuid = {};
    memcpy(readUuid.uuid, uuidHash.data(), uuidHash.size());
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(uuidName);
    readUuid.payload_size = uuid.payload_size;
    readUuid.handle = 3;

    auto handler = new MockIoctlHandler;
    handler->returnUuid = &readUuid;
    session->ioctlHandler.reset(handler);

    session->handleEvent(&uuid.base);
    EXPECT_EQ(MockDebugSessionLinux::mockClientHandle, session->clientHandle);
    EXPECT_EQ(1, handler->ioctlCalled);
}

TEST_F(DebugApiLinuxTest, GivenUuidCommandQueueCreatedHandledThenProcessEntryEventIsGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    auto uuidName = NEO::uuidL0CommandQueueName;
    auto uuidNameSize = strlen(uuidName);
    auto uuidHash = "InvalidUuidL0CommandQueueHash";

    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = 0;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuid.handle = 2;
    uuid.payload_size = uuidNameSize;

    prelim_drm_i915_debug_read_uuid readUuid = {};
    memcpy(readUuid.uuid, uuidHash, strlen(uuidHash));
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(uuidName);
    readUuid.payload_size = uuid.payload_size;
    readUuid.handle = 3;

    auto handler = new MockIoctlHandler;
    handler->returnUuid = &readUuid;
    session->ioctlHandler.reset(handler);

    session->handleEvent(&uuid.base);
    EXPECT_EQ(DebugSessionLinux::invalidClientHandle, session->clientHandle);
    EXPECT_EQ(0, handler->ioctlCalled);
    EXPECT_EQ(0u, session->apiEvents.size());

    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    handler->returnUuid = &readUuid;
    session->clientHandle = 2;
    session->handleEvent(&uuid.base);
    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(0u, session->apiEvents.size());

    uuidHash = NEO::uuidL0CommandQueueHash;
    memcpy(readUuid.uuid, uuidHash, strlen(uuidHash));
    handler->returnUuid = &readUuid;
    session->handleEvent(&uuid.base);
    EXPECT_EQ(2u, session->clientHandle);
    EXPECT_EQ(2, handler->ioctlCalled);
    EXPECT_EQ(0u, session->apiEvents.size());

    session->clientHandle = DebugSessionLinux::invalidClientHandle;
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    handler->returnUuid = &readUuid;
    session->handleEvent(&uuid.base);
    EXPECT_EQ(1u, session->apiEvents.size());
    EXPECT_EQ(MockDebugSessionLinux::mockClientHandle, session->clientHandle);
    EXPECT_EQ(3, handler->ioctlCalled);

    EXPECT_EQ(0u, session->uuidL0CommandQueueHandleToDevice[2]);

    auto event = session->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY, event.type);

    session->clientHandle = MockDebugSessionLinux::mockClientHandle;
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    handler->returnUuid = &readUuid;
    session->handleEvent(&uuid.base);
    EXPECT_EQ(1u, session->apiEvents.size());
    EXPECT_EQ(MockDebugSessionLinux::mockClientHandle, session->clientHandle);
    EXPECT_EQ(4, handler->ioctlCalled);
}

TEST_F(DebugApiLinuxTest, GivenUuidCommandQueueWhenQueuesOnToSubdevicesCreatedAndDestroyedThenProcessEntryAndExitEventIsGeneratedOnce) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuid.handle = 2;
    uuid.payload_size = sizeof(NEO::DebuggerL0::CommandQueueNotification);

    auto uuidHash = NEO::uuidL0CommandQueueHash;
    prelim_drm_i915_debug_read_uuid readUuid = {};
    memcpy(readUuid.uuid, uuidHash, strlen(uuidHash));

    NEO::DebuggerL0::CommandQueueNotification notification;
    notification.subDeviceCount = 2;
    notification.subDeviceIndex = 1;
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(&notification);
    readUuid.payload_size = sizeof(NEO::DebuggerL0::CommandQueueNotification);
    readUuid.handle = uuid.handle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    handler->returnUuid = &readUuid;

    // Handle UUID create for commandQueue on subdevice 1
    session->handleEvent(&uuid.base);
    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(1u, session->apiEvents.size());
    auto event = session->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY, event.type);

    EXPECT_EQ(1u, session->uuidL0CommandQueueHandleToDevice.size());

    notification.subDeviceCount = 2;
    notification.subDeviceIndex = 0;
    uuid.handle = 3;
    handler->returnUuid = &readUuid;

    // Handle UUID create for commandQueue on subdevice 0
    session->handleEvent(&uuid.base);
    EXPECT_EQ(2, handler->ioctlCalled);
    EXPECT_EQ(1u, session->apiEvents.size());

    EXPECT_EQ(2u, session->uuidL0CommandQueueHandleToDevice.size());
    EXPECT_EQ(1u, session->uuidL0CommandQueueHandleToDevice[2]);
    EXPECT_EQ(0u, session->uuidL0CommandQueueHandleToDevice[3]);

    // Handle UUID destroy for commandQueue on subdevice 0
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    session->handleEvent(&uuid.base);

    EXPECT_EQ(1u, session->uuidL0CommandQueueHandleToDevice.size());
    EXPECT_EQ(1u, session->apiEvents.size());

    // Handle UUID destroy with invalid uuid handle
    uuid.handle = 300;
    session->handleEvent(&uuid.base);
    EXPECT_EQ(1u, session->uuidL0CommandQueueHandleToDevice.size());

    // Handle UUID destroy for commandQueue on subdevice 1
    uuid.handle = 2;
    session->handleEvent(&uuid.base);
    EXPECT_EQ(0u, session->uuidL0CommandQueueHandleToDevice.size());
    EXPECT_EQ(2u, session->apiEvents.size());

    session->apiEvents.pop();
    event = session->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT, event.type);
}

TEST_F(DebugApiLinuxTest, GivenCommandQueueDestroyedWhenHandlingEventThenExitEventIsGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidHandle;
    const uint64_t validUuidCmdQHandle = 5u;
    session->uuidL0CommandQueueHandleToDevice[validUuidCmdQHandle] = 0;

    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = 0;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = 10;
    uuid.handle = 4;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandleToConnection[10u].reset(new L0::DebugSessionLinux::ClientConnection);

    session->handleEvent(&uuid.base);
    EXPECT_EQ(0u, session->apiEvents.size());

    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    session->handleEvent(&uuid.base);
    EXPECT_EQ(0u, session->apiEvents.size());

    uuid.handle = validUuidCmdQHandle;
    session->handleEvent(&uuid.base);
    EXPECT_EQ(0u, session->apiEvents.size());

    session->clientHandle = uuid.client_handle;
    session->handleEvent(&uuid.base);
    EXPECT_EQ(1u, session->apiEvents.size());
    auto event = session->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT, event.type);
}

TEST_F(DebugApiLinuxTest, GivenDestroyClientForClientNotSavedWhenHandlingEventThenNoEventIsGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = 1;

    prelim_drm_i915_debug_event_client clientDestroy = {};

    clientDestroy.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    clientDestroy.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    clientDestroy.base.size = sizeof(prelim_drm_i915_debug_event_client);
    clientDestroy.handle = 10;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandleToConnection[10u].reset(new L0::DebugSessionLinux::ClientConnection);

    session->handleEvent(&clientDestroy.base);
    EXPECT_EQ(0u, session->apiEvents.size());
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWhenReadingEventThenResultNotReadyIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto mockSession = new MockDebugSessionLinux(config, &deviceImp, 10);
    mockSession->clientHandle = MockDebugSessionLinux::mockClientHandle;
    deviceImp.debugSession.reset(mockSession);

    zet_debug_session_handle_t session = deviceImp.debugSession->toHandle();

    zet_debug_event_t event = {};
    auto result = L0::DebugApiHandlers::debugReadEvent(session, 0, &event);
    EXPECT_EQ(result, ZE_RESULT_NOT_READY);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerLogsWhenOpenDebuggerFailsThenCorrectMessageIsPrintedAndResultSet) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(255);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    ze_result_t result = ZE_RESULT_SUCCESS;

    mockDrm->context.debuggerOpenRetval = -1;
    mockDrm->baseErrno = false;
    mockDrm->errnoRetVal = 22;
    ::testing::internal::CaptureStderr();

    auto session = DebugSession::create(config, device, result);

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    auto errorMessage = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string("\nERROR: PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN failed: open.pid: 4660, open.events: 0, retCode: -1, errno: 22\n"), errorMessage);
}

TEST_F(DebugApiLinuxTest, WhenOpenDebuggerFailsThenCorrectErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    ze_result_t result = ZE_RESULT_SUCCESS;

    mockDrm->context.debuggerOpenRetval = -1;
    mockDrm->baseErrno = false;
    mockDrm->errnoRetVal = EBUSY;
    auto session = DebugSession::create(config, device, result);

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    mockDrm->errnoRetVal = ENODEV;
    session = DebugSession::create(config, device, result);

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    mockDrm->errnoRetVal = EACCES;
    session = DebugSession::create(config, device, result);

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);

    mockDrm->errnoRetVal = ESRCH;
    session = DebugSession::create(config, device, result);

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerLogsWhenOpenDebuggerSucceedsThenCorrectMessageIsPrintedAndResultSet) {
    DebugManagerStateRestore restorer;

    NEO::DebugManager.flags.DebuggerLogBitmask.set(255);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    ze_result_t result = ZE_RESULT_SUCCESS;

    mockDrm->context.debuggerOpenRetval = 10;
    mockDrm->baseErrno = false;
    mockDrm->errnoRetVal = 0;
    ::testing::internal::CaptureStdout();

    auto session = std::unique_ptr<DebugSession>(DebugSession::create(config, device, result));

    EXPECT_NE(nullptr, session);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto errorMessage = ::testing::internal::GetCapturedStdout();
    EXPECT_EQ(std::string("\nINFO: PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN: open.pid: 4660, open.events: 0, debugFd: 10\n"), errorMessage);
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWhenClosingConnectionThenSysCallCloseOnFdIsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);

    EXPECT_NE(nullptr, session);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;

    auto ret = session->closeConnection();
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);
    EXPECT_EQ(10, NEO::SysCalls::closeFuncArgPassed);
    EXPECT_FALSE(session->asyncThread.threadActive);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWhenDestroyedThenSysCallCloseOnFdIsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    EXPECT_NE(nullptr, session);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;

    session.reset(nullptr);
    EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);
    EXPECT_EQ(10, NEO::SysCalls::closeFuncArgPassed);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWithFdEqualZeroWhenClosingConnectionThenSysCallIsNotCalledAndFalseReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 0);

    EXPECT_NE(nullptr, session);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;
    auto ret = session->closeConnection();
    EXPECT_FALSE(ret);

    EXPECT_EQ(0u, NEO::SysCalls::closeFuncCalled);
    EXPECT_EQ(0, NEO::SysCalls::closeFuncArgPassed);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;
}

TEST_F(DebugApiLinuxTest, GivenPrintDebugMessagesWhenDebugSessionClosesConnectionWithErrorThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_ERROR);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    ::testing::internal::CaptureStderr();

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);

    EXPECT_NE(nullptr, session);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncRetVal = -1;
    auto ret = session->closeConnection();
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);
    EXPECT_EQ(10, NEO::SysCalls::closeFuncArgPassed);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;
    NEO::SysCalls::closeFuncRetVal = 0;

    auto errorMessage = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string("\nERROR: Debug connection close() on fd: 10 failed: retCode: -1\n"), errorMessage);
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWhenCallingIoctlThenIoctlHandlerIsInvokedWithDebugFd) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    EXPECT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    session->ioctl(0, nullptr);
    EXPECT_EQ(1, handler->ioctlCalled);
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWhenIoctlFailsThenErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    EXPECT_NE(nullptr, session);

    auto ret = session->ioctl(-1, nullptr);
    EXPECT_EQ(-1, ret);

    NEO::SysCalls::setErrno = EBUSY;
    ret = session->ioctl(-1, nullptr);
    EXPECT_EQ(-1, ret);

    NEO::SysCalls::setErrno = EAGAIN;
    ret = session->ioctl(-1, nullptr);
    EXPECT_EQ(-1, ret);

    NEO::SysCalls::setErrno = EINTR;
    ret = session->ioctl(-1, nullptr);
    EXPECT_EQ(-1, ret);
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWhenIoctlIsCalledThenItIsForwardedToSysCall) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    EXPECT_NE(nullptr, session);

    auto ret = session->ioctl(10, nullptr);
    EXPECT_EQ(0, ret);
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWhenCallingPollThenDefaultHandlerRedirectsToSysCall) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    EXPECT_EQ(0, session->ioctlHandler->poll(nullptr, 0, 0));
}

TEST_F(DebugApiLinuxTest, WhenCallingReadOrWriteGpuMemoryThenVmOpenIoctlIsCalled) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    for (int i = 0; i < 2; i++) {

        NEO::SysCalls::closeFuncCalled = 0;

        char buffer[bufferSize];
        int retVal = 0;
        if (i == 0) {
            handler->preadRetVal = bufferSize;
            retVal = session->readGpuMemory(7, buffer, bufferSize, 0x23000);
            EXPECT_EQ(static_cast<uint64_t>(PRELIM_I915_DEBUG_VM_OPEN_READ_ONLY), handler->vmOpen.flags);
        } else {
            handler->pwriteRetVal = bufferSize;
            retVal = session->writeGpuMemory(7, buffer, bufferSize, 0x23000);
            EXPECT_EQ(static_cast<uint64_t>(PRELIM_I915_DEBUG_VM_OPEN_READ_WRITE), handler->vmOpen.flags);
        }

        EXPECT_EQ(MockDebugSessionLinux::mockClientHandle, handler->vmOpen.client_handle);
        EXPECT_EQ(static_cast<uint64_t>(7), handler->vmOpen.handle);

        EXPECT_EQ(0, retVal);
        EXPECT_EQ(handler->vmOpenRetVal, NEO::SysCalls::closeFuncArgPassed);
        EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);

        handler->vmOpen.flags = 0;
    }
}

TEST_F(DebugApiLinuxTest, WhenCallingReadOrWriteGpuMemoryThenGpuAddressIsDecanonized) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;

    char buffer[bufferSize];
    int retVal = 0;
    auto gmmHelper = neoDevice->getGmmHelper();

    handler->preadRetVal = bufferSize;
    retVal = session->readGpuMemory(7, buffer, bufferSize, gmmHelper->canonize(0x23000));
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(0x23000u, handler->preadOffset);

    handler->pwriteRetVal = bufferSize;
    retVal = session->writeGpuMemory(7, buffer, bufferSize, gmmHelper->canonize(0x23000));
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(0x23000u, handler->pwriteOffset);
}

TEST_F(DebugApiLinuxTest, WhenCallingReadGpuMemoryThenMemoryIsRead) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;

    char output[bufferSize];
    handler->preadRetVal = bufferSize;
    auto retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(0, retVal);

    EXPECT_EQ(1u, handler->mmapCalled == 1 || handler->preadCalled == 1);

    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(static_cast<char>(0xaa), output[i]);
    }
}

TEST_F(DebugApiLinuxTest, GivenDebuggerMmapMemoryAccessTrueWhenCallingReadGpuMemoryThenMemoryIsReadWithMmap) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableDebuggerMmapMemoryAccess.set(true);

    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    char output[bufferSize];
    auto retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(0, retVal);

    EXPECT_EQ(1u, handler->mmapCalled);
    EXPECT_EQ(1u, handler->munmapCalled);

    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(static_cast<char>(0xaa), output[i]);
    }
}

TEST_F(DebugApiLinuxTest, GivenDebuggerMmapMemoryAccessFalseWhenCallingReadGpuMemoryThenMemoryIsReadWithPread) {

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableDebuggerMmapMemoryAccess.set(false);

    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;
    handler->preadRetVal = bufferSize; //16 bytes to read
    char output[bufferSize] = {};
    auto retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(0, retVal);

    EXPECT_EQ(1u, handler->preadCalled);
    EXPECT_EQ(0u, handler->mmapCalled);

    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(static_cast<char>(0xaa), output[i]);
    }

    handler->preadCalled = 0;
    handler->preadRetVal = 8; // read less bytes, iterate
    handler->midZeroReturn = 2;
    retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    EXPECT_EQ(4u, handler->preadCalled);
    EXPECT_EQ(0u, handler->mmapCalled);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerMmapMemoryAccessFalseWhenPreadFailsThenErrorReturned) {

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableDebuggerMmapMemoryAccess.set(false);

    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;
    handler->preadRetVal = -1; // fail
    char output[bufferSize] = {};
    auto retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);

    EXPECT_EQ(1u, handler->preadCalled);
    EXPECT_EQ(0u, handler->mmapCalled);

    for (int i = 0; i < bufferSize; i++) {
        EXPECT_NE(static_cast<char>(0xaa), output[i]);
    }

    handler->preadRetVal = 0; // read 0 and retry
    retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);

    EXPECT_EQ(4u, handler->preadCalled);
    EXPECT_EQ(0u, handler->mmapCalled);
}

TEST_F(DebugApiLinuxTest, WhenCallingWriteGpuMemoryThenMemoryIsWritten) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;

    char buffer[bufferSize];
    memset(buffer, 'O', bufferSize);
    handler->pwriteRetVal = bufferSize;
    auto retVal = session->writeGpuMemory(7, buffer, bufferSize, 0x23000);
    EXPECT_EQ(0, retVal);

    EXPECT_TRUE(handler->memoryModifiedInMunmap || handler->pwriteCalled == 1);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerMmapMemoryAccessTrueWhenCallingWriteGpuMemoryThenMemoryIsWrittenWithMmap) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableDebuggerMmapMemoryAccess.set(true);

    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    char buffer[bufferSize];
    memset(buffer, 'O', bufferSize);
    auto retVal = session->writeGpuMemory(7, buffer, bufferSize, 0x23000);
    EXPECT_EQ(0, retVal);

    EXPECT_EQ(1u, handler->mmapCalled);
    EXPECT_EQ(1u, handler->munmapCalled);
    EXPECT_TRUE(handler->memoryModifiedInMunmap);
}
TEST_F(DebugApiLinuxTest, GivenDebuggerMmapMemoryAccessFalseWhenCallingWriteGpuMemoryThenMemoryIsWrittenWithPwrite) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableDebuggerMmapMemoryAccess.set(false);

    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;
    handler->pwriteRetVal = bufferSize;

    char buffer[bufferSize];
    memset(buffer, 'O', bufferSize);
    auto retVal = session->writeGpuMemory(7, buffer, bufferSize, 0x23000);
    EXPECT_EQ(0, retVal);

    EXPECT_EQ(1u, handler->pwriteCalled);
    EXPECT_EQ(0u, handler->mmapCalled);

    handler->pwriteCalled = 0;
    handler->pwriteRetVal = 8; // not everything written, iterate
    handler->midZeroReturn = 2;
    retVal = session->writeGpuMemory(7, buffer, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    EXPECT_EQ(4u, handler->pwriteCalled);
    EXPECT_EQ(0u, handler->mmapCalled);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerMmapMemoryAccessTrueWhenMmapFailesThenReadOrWriteGpuMemoryFails) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableDebuggerMmapMemoryAccess.set(true);

    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    handler->mmapFail = true;
    char buffer[bufferSize];
    auto retVal = session->readGpuMemory(7, buffer, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);

    retVal = session->writeGpuMemory(7, buffer, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);

    EXPECT_EQ(2u, handler->mmapCalled);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerMmapMemoryAccessFalseWhenWhenPwriteFailsThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableDebuggerMmapMemoryAccess.set(false);

    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;
    handler->pwriteRetVal = -1; // fail

    char buffer[bufferSize];
    memset(buffer, 'O', bufferSize);
    auto retVal = session->writeGpuMemory(7, buffer, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);

    EXPECT_EQ(1u, handler->pwriteCalled);
    EXPECT_EQ(0u, handler->mmapCalled);

    handler->pwriteRetVal = 0; // write 0, retry
    retVal = session->writeGpuMemory(7, buffer, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);

    EXPECT_EQ(4u, handler->pwriteCalled);
    EXPECT_EQ(0u, handler->mmapCalled);
}

TEST_F(DebugApiLinuxTest, WhenCallingReadMemoryForISAThenMemoryIsRead) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    ze_device_thread_t thread;
    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = UINT32_MAX;
    zet_debug_memory_space_desc_t desc;
    desc.address = 0x1000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    char output[bufferSize];
    // No ISA, ElF or VM yet created.
    auto retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, retVal);

    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;

    auto isa = std::make_unique<DebugSessionLinux::IsaAllocation>();
    isa->bindInfo = {isaGpuVa, isaSize};
    isa->vmHandle = 3;
    isa->elfUuidHandle = DebugSessionLinux::invalidHandle;
    isa->moduleBegin = 0;
    isa->moduleEnd = 0;

    auto &isaMap = session->clientHandleToConnection[session->clientHandle]->isaMap[0];
    isaMap[isaGpuVa] = std::move(isa);
    isaMap[isaGpuVa]->vmBindCounter = 5;

    desc.address = isaGpuVa;
    memset(output, 0, bufferSize);

    handler->preadRetVal = bufferSize;
    retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(static_cast<char>(0xaa), output[i]);
    }

    session->allThreadsStopped = true;

    thread.slice = 1;
    thread.subslice = 1;
    thread.eu = 1;
    thread.thread = 1;

    retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
}

TEST_F(DebugApiLinuxTest, GivenCanonizedAddressWhenGettingIsaVmHandleThenCorrectVmIsReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    const uint64_t isaGpuVa = device->getHwInfo().capabilityTable.gpuAddressSpace - 0x3000;
    const uint64_t isaSize = 0x2000;
    auto gmmHelper = neoDevice->getGmmHelper();

    auto isa = std::make_unique<DebugSessionLinux::IsaAllocation>();
    isa->bindInfo = {isaGpuVa, isaSize};
    isa->vmHandle = 3;
    isa->elfUuidHandle = DebugSessionLinux::invalidHandle;
    isa->moduleBegin = 0;
    isa->moduleEnd = 0;

    auto &isaMap = session->clientHandleToConnection[session->clientHandle]->isaMap[0];
    isaMap[isaGpuVa] = std::move(isa);
    isaMap[isaGpuVa]->vmBindCounter = 1;

    zet_debug_memory_space_desc_t desc;
    desc.address = gmmHelper->canonize(isaGpuVa);
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    if (Math::log2(device->getHwInfo().capabilityTable.gpuAddressSpace + 1) >= 48) {
        EXPECT_NE(isaGpuVa, desc.address);
    }

    uint64_t vmHandle = 0;
    auto retVal = session->getISAVMHandle(0, &desc, isaSize, vmHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(3u, vmHandle);
}

TEST_F(DebugApiLinuxTest, WhenCallingReadMemoryForELFThenMemoryIsRead) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    ze_device_thread_t thread;
    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = UINT32_MAX;

    zet_debug_memory_space_desc_t desc;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    uint64_t elfVa = 0x345000;
    uint64_t elfSize = 0xFF;
    uint64_t elfUUIDiHandle = 4;
    char *elfData = new char[elfSize];
    memset(elfData, 0xa, elfSize);

    session->clientHandleToConnection[session->clientHandle]->elfMap[elfVa] = elfUUIDiHandle;
    session->clientHandleToConnection[session->clientHandle]->uuidMap[elfUUIDiHandle].data.reset(elfData);
    session->clientHandleToConnection[session->clientHandle]->uuidMap[elfUUIDiHandle].dataSize = elfSize;

    desc.address = elfVa + 0X0F;
    char output[bufferSize];
    memset(output, 0, bufferSize);

    handler->pwriteRetVal = bufferSize;
    auto retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (int i = 0; i < static_cast<int>(bufferSize); i++) {
        EXPECT_EQ(static_cast<char>(0xa), output[i]);
    }

    session->allThreadsStopped = true;

    thread.slice = 1;
    thread.subslice = 1;
    thread.eu = 1;
    thread.thread = 1;

    retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    char *output2 = nullptr;
    retVal = session->readMemory(thread, &desc, bufferSize, output2);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
}

TEST_F(DebugApiLinuxTest, WhenCallingReadMemoryforAllThreadsOnDefaultMemoryThenMemoryIsRead) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    ze_device_thread_t thread;
    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = UINT32_MAX;
    zet_debug_memory_space_desc_t desc;
    desc.address = 0x1000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    char output[bufferSize];

    uint64_t vmId = 10;
    session->clientHandleToConnection[session->clientHandle]->vmIds.insert(vmId);

    handler->preadRetVal = bufferSize;
    auto retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(static_cast<char>(0xaa), output[i]);
    }

    // Fail with a found VMid.
    handler->mmapFail = true;
    handler->preadRetVal = -1;
    retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);
}

TEST_F(DebugApiLinuxTest, WhenCallingReadMemoryforASingleThreadThenMemoryIsRead) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;

    session->allThreadsStopped = true;

    ze_device_thread_t thread;
    thread.slice = 0;
    thread.subslice = 0;
    thread.eu = 0;
    thread.thread = 0;
    zet_debug_memory_space_desc_t desc;
    desc.address = 0x1000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    char output[bufferSize];
    session->vmHandle = UINT64_MAX;
    session->ensureThreadStopped(thread);

    handler->preadRetVal = bufferSize;
    auto retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);

    session->vmHandle = 7;
    session->ensureThreadStopped(thread);
    session->clientHandleToConnection[session->clientHandle]->contextsCreated[1].vm = session->vmHandle;

    retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(static_cast<char>(0xaa), output[i]);
    }
}

TEST_F(DebugApiLinuxTest, GivenInvalidAddressWhenCallingReadMemoryThenErrorIsReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;

    session->allThreadsStopped = true;

    ze_device_thread_t thread{0, 0, 0, 0};

    zet_debug_memory_space_desc_t desc;
    desc.address = 0xf0ffffff00000000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    EXPECT_FALSE(session->isValidGpuAddress(desc.address));

    char output[bufferSize];
    session->vmHandle = UINT64_MAX;
    session->ensureThreadStopped(thread);

    handler->preadRetVal = bufferSize;
    session->vmHandle = 7;
    session->ensureThreadStopped(thread);
    session->clientHandleToConnection[session->clientHandle]->contextsCreated[1].vm = session->vmHandle;

    auto retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);
}

TEST_F(DebugApiLinuxTest, WhenCallingReadMemoryForISAForExpectedFailureCasesThenErrorIsReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    auto &isaMap = session->clientHandleToConnection[session->clientHandle]->isaMap[0];
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;

    auto isa = std::make_unique<DebugSessionLinux::IsaAllocation>();
    isa->bindInfo = {isaGpuVa, isaSize};
    isa->vmHandle = 3;
    isa->elfUuidHandle = DebugSessionLinux::invalidHandle;
    isa->moduleBegin = 0;
    isa->moduleEnd = 0;
    isaMap[isaGpuVa] = std::move(isa);
    isaMap[isaGpuVa]->vmBindCounter = 5;

    NEO::SysCalls::closeFuncCalled = 0;

    ze_device_thread_t thread;
    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa;

    char output[bufferSize];
    size_t size = bufferSize;

    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;

    thread.slice = 1;
    thread.subslice = 1;
    thread.eu = 1;
    thread.thread = 1;

    auto retVal = session->readMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, retVal);

    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = UINT32_MAX;

    retVal = session->readMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    thread.slice = UINT32_MAX;
    thread.subslice = 0;
    thread.eu = 0;
    thread.thread = 0;

    retVal = session->readMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = 0;
    thread.thread = 0;

    retVal = session->readMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = 0;

    retVal = session->readMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = UINT32_MAX;

    NEO::SysCalls::setErrno = EINVAL;
    handler->mmapFail = true;
    retVal = session->readMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    handler->mmapFail = false;

    desc.address = isaGpuVa + isaSize + 0XF;
    retVal = session->readMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, retVal);

    desc.address = 0x333000;
    retVal = session->readMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, retVal);

    desc.address = isaGpuVa;
    size = 0x200F;
    retVal = session->readMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    thread.slice = 0;
    thread.subslice = 0;
    thread.eu = 0;
    thread.thread = 0;

    retVal = session->readMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);
}

TEST_F(DebugApiLinuxTest, WhenCallingReadMemoryForElfForExpectedFailureCasesThenErrorIsReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    ze_device_thread_t thread;
    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = UINT32_MAX;
    zet_debug_memory_space_desc_t desc;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    uint64_t elfVa = 0x345000;
    uint64_t elfSize = 0xFF;
    uint64_t elfUUIDiHandle = 4;
    char *elfData = new char[elfSize];
    memset(elfData, 0xa, elfSize);

    session->clientHandleToConnection[session->clientHandle]->elfMap[elfVa] = elfUUIDiHandle;
    session->clientHandleToConnection[session->clientHandle]->uuidMap[elfUUIDiHandle].data.reset(elfData);
    session->clientHandleToConnection[session->clientHandle]->uuidMap[elfUUIDiHandle].dataSize = elfSize;

    char output[bufferSize];

    desc.address = elfVa + elfSize + 0XF;
    auto retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, retVal);

    desc.address = 0x333000;
    retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, retVal);

    desc.address = elfVa;
    retVal = session->readMemory(thread, &desc, 0x200F, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);
}
TEST_F(DebugApiLinuxTest, WhenCallingWriteMemoryForISAThenMemoryIsWritten) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    ze_device_thread_t thread;
    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = UINT32_MAX;
    zet_debug_memory_space_desc_t desc;
    desc.address = 0x1000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    char output[bufferSize];
    // No ISA, ElF or VM yet created.
    auto retVal = session->readMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, retVal);

    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;

    auto isa = std::make_unique<DebugSessionLinux::IsaAllocation>();
    isa->bindInfo = {isaGpuVa, isaSize};
    isa->vmHandle = 3;
    isa->elfUuidHandle = DebugSessionLinux::invalidHandle;
    isa->moduleBegin = 0;
    isa->moduleEnd = 0;

    auto &isaMap = session->clientHandleToConnection[session->clientHandle]->isaMap[0];
    isaMap[isaGpuVa] = std::move(isa);
    isaMap[isaGpuVa]->vmBindCounter = 5;

    desc.address = isaGpuVa;
    handler->pwriteRetVal = bufferSize;

    retVal = session->writeMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(1u, handler->pwriteCalled);
    EXPECT_EQ(0u, handler->mmapCalled);

    session->allThreadsStopped = true;
    handler->pwriteRetVal = bufferSize;

    thread.slice = 1;
    thread.subslice = 1;
    thread.eu = 1;
    thread.thread = 1;

    retVal = session->writeMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
}

TEST_F(DebugApiLinuxTest, WhenCallingWriteMemoryForAllThreadsOnDefaultMemoryThenMemoryIsWritten) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    ze_device_thread_t thread;
    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = UINT32_MAX;
    zet_debug_memory_space_desc_t desc;
    desc.address = 0x1000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    // No ISA or VM yet created.
    char output[bufferSize] = {0, 1, 2, 3};
    auto retVal = session->writeMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, retVal);

    uint64_t vmId = 10;
    session->clientHandleToConnection[session->clientHandle]->vmIds.insert(vmId);

    handler->pwriteRetVal = bufferSize;

    retVal = session->writeMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_EQ(1u, handler->pwriteCalled);
    EXPECT_EQ(0u, handler->mmapCalled);

    // Fail with a found VMid.
    handler->pwriteRetVal = -1;
    retVal = session->writeMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);
}

TEST_F(DebugApiLinuxTest, WhenCallingWriteMemoryForASignleThreadThenMemoryIsWritten) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    prelim_drm_i915_debug_event_vm vmEvent = {};
    vmEvent.base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM;
    vmEvent.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmEvent.base.size = sizeof(prelim_drm_i915_debug_event_vm);
    vmEvent.client_handle = MockDebugSessionLinux::mockClientHandle;
    vmEvent.handle = 4;

    session->handleEvent(&vmEvent.base);
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmIds.size());

    session->allThreadsStopped = true;

    ze_device_thread_t thread;
    thread.slice = 0;
    thread.subslice = 0;
    thread.eu = 0;
    thread.thread = 0;
    zet_debug_memory_space_desc_t desc;
    desc.address = 0x1000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    char output[bufferSize] = {0, 1, 2, 3};
    session->vmHandle = UINT64_MAX;
    session->ensureThreadStopped(thread);
    auto retVal = session->writeMemory(thread, &desc, bufferSize, output);

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);

    session->vmHandle = 7;
    session->ensureThreadStopped(thread);
    session->clientHandleToConnection[session->clientHandle]->contextsCreated[1].vm = session->vmHandle;

    handler->pwriteRetVal = bufferSize;
    retVal = session->writeMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    EXPECT_TRUE(handler->memoryModifiedInMunmap || handler->pwriteCalled == 1);
}

TEST_F(DebugApiLinuxTest, GivenInvalidAddressWhenCallingWriteMemoryThenErrorIsReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;

    ze_device_thread_t thread{0, 0, 0, 0};

    zet_debug_memory_space_desc_t desc;
    desc.address = 0xf0ffffff00000000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    EXPECT_FALSE(session->isValidGpuAddress(desc.address));

    char output[bufferSize];
    session->vmHandle = UINT64_MAX;

    handler->pwriteRetVal = bufferSize;
    session->vmHandle = 7;
    session->ensureThreadStopped(thread);
    session->clientHandleToConnection[session->clientHandle]->contextsCreated[1].vm = session->vmHandle;

    auto retVal = session->writeMemory(thread, &desc, bufferSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);
}

TEST_F(DebugApiLinuxTest, WhenCallingWriteMemoryForExpectedFailureCasesThenErrorIsReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    auto &isaMap = session->clientHandleToConnection[session->clientHandle]->isaMap[0];
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;

    auto isa = std::make_unique<DebugSessionLinux::IsaAllocation>();
    isa->bindInfo = {isaGpuVa, isaSize};
    isa->vmHandle = 3;
    isa->elfUuidHandle = DebugSessionLinux::invalidHandle;
    isa->moduleBegin = 0;
    isa->moduleEnd = 0;
    isaMap[isaGpuVa] = std::move(isa);
    isaMap[isaGpuVa]->vmBindCounter = 5;

    NEO::SysCalls::closeFuncCalled = 0;

    ze_device_thread_t thread;
    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa;

    char output[bufferSize];
    size_t size = bufferSize;

    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;
    auto retVal = session->writeMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, retVal);

    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    thread.slice = UINT32_MAX;
    thread.subslice = 0;
    thread.eu = 0;
    thread.thread = 0;

    retVal = session->writeMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = 0;
    thread.thread = 0;

    retVal = session->writeMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = 0;

    retVal = session->writeMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    thread.slice = UINT32_MAX;
    thread.subslice = UINT32_MAX;
    thread.eu = UINT32_MAX;
    thread.thread = UINT32_MAX;

    NEO::SysCalls::setErrno = EINVAL;
    errno = EACCES;
    handler->mmapFail = true;
    retVal = session->writeMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    handler->mmapFail = false;

    desc.address = isaGpuVa + isaSize + 0XF;
    retVal = session->writeMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, retVal);

    desc.address = isaGpuVa;
    size = 0x200F;
    retVal = session->writeMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    desc.address = 0x333000;
    retVal = session->writeMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, retVal);

    isaMap.clear();
    retVal = session->writeMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, retVal);

    thread.slice = 0;
    thread.subslice = 0;
    thread.eu = 0;
    thread.thread = 0;

    retVal = session->writeMemory(thread, &desc, size, output);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);
}

TEST_F(DebugApiLinuxTest, GivenErrorFromVmOpenWhenCallingReadGpuMemoryThenCloseIsNotCalledAndErrorReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;
    handler->vmOpenRetVal = -30;
    char output[bufferSize];
    auto retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);

    EXPECT_EQ(MockDebugSessionLinux::mockClientHandle, handler->vmOpen.client_handle);
    EXPECT_EQ(static_cast<uint64_t>(7), handler->vmOpen.handle);
    EXPECT_EQ(static_cast<uint64_t>(PRELIM_I915_DEBUG_VM_OPEN_READ_ONLY), handler->vmOpen.flags);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    EXPECT_EQ(0u, NEO::SysCalls::closeFuncCalled);
}

TEST_F(DebugApiLinuxTest, GivenErrorFromVmOpenWhenCallingWriteGpuMemoryThenCloseIsNotCalledAndErrorReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;
    handler->vmOpenRetVal = -30;
    char buffer[bufferSize];
    auto retVal = session->writeGpuMemory(7, buffer, bufferSize, 0x23000);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    EXPECT_EQ(0u, NEO::SysCalls::closeFuncCalled);
}

TEST_F(DebugApiLinuxTest, GivenErrorFromMmapWhenCallingReadGpuMemoryThenCloseIsCalledAndErrnoErrorIsReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;
    errno = EACCES;
    handler->mmapFail = true;
    char output[bufferSize];
    auto retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);

    EXPECT_EQ(MockDebugSessionLinux::mockClientHandle, handler->vmOpen.client_handle);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);
}
TEST_F(DebugApiLinuxTest, GivenErrorFromMmapWhenCallingWriteGpuMemoryThenCloseIsCalledAndErrnoErrorIsReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    NEO::SysCalls::closeFuncCalled = 0;
    errno = EACCES;
    handler->mmapFail = true;
    char buffer[bufferSize];
    auto retVal = session->writeGpuMemory(7, buffer, bufferSize, 0x23000);

    EXPECT_EQ(MockDebugSessionLinux::mockClientHandle, handler->vmOpen.client_handle);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);
}

TEST_F(DebugApiLinuxTest, givenErrorReturnedFromInitializeWhenDebugSessionIsCreatedThenNullptrIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto session = DebugSession::create(config, device, result);
    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWhenPollReturnsZeroThenNotReadyIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    ze_result_t result = ZE_RESULT_SUCCESS;

    mockDrm->context.debuggerOpenRetval = 10;
    mockDrm->baseErrno = false;
    mockDrm->errnoRetVal = 0;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    result = session->initialize();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWhenClientHandleIsInvalidDuringInitializationThenNotReadyIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    handler->debugEventRetVal = -1;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::invalidClientHandle;

    ze_result_t result = session->initialize();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    EXPECT_EQ(0, session->getInternalEventCounter);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerLogsWhenReadEventFailsDuringInitializationThenErrorIsPrintedAndReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_ERROR);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    ::testing::internal::CaptureStderr();

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    handler->pollRetVal = 1;
    handler->debugEventRetVal = -1;
    session->ioctlHandler.reset(handler);

    prelim_drm_i915_debug_event_client client = {};

    client.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    client.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    client.base.size = sizeof(prelim_drm_i915_debug_event_client);
    client.handle = 1;
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});

    ze_result_t result = session->initialize();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    EXPECT_EQ(2, session->getInternalEventCounter);

    auto errorMessage = ::testing::internal::GetCapturedStderr();
    auto pos = errorMessage.find("PRELIM_I915_DEBUG_IOCTL_READ_EVENT failed: retCode: -1 errno =");
    EXPECT_NE(std::string::npos, pos);
}

TEST_F(DebugApiLinuxTest, GivenBindInfoForVmHandleWhenReadingModuleDebugAreaThenGpuMemoryIsRead) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t vmHandle = 6;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToModuleDebugAreaBindInfo[vmHandle] = {0x1234000, 0x1000};

    DebugAreaHeader debugArea;
    debugArea.reserved1 = 1;
    debugArea.pgsize = uint8_t(4);
    debugArea.version = 1;

    handler->mmapRet = reinterpret_cast<char *>(&debugArea);
    handler->setPreadMemory(reinterpret_cast<char *>(&debugArea), sizeof(session->debugArea), 0x1234000);
    handler->preadRetVal = sizeof(session->debugArea);

    auto retVal = session->readModuleDebugArea();

    EXPECT_TRUE(retVal);

    if (DebugManager.flags.EnableDebuggerMmapMemoryAccess.get()) {
        EXPECT_EQ(1, handler->mmapCalled);
        EXPECT_EQ(1, handler->munmapCalled);
    } else {
        EXPECT_EQ(1, handler->preadCalled);
    }
    EXPECT_EQ(MockDebugSessionLinux::mockClientHandle, handler->vmOpen.client_handle);
    EXPECT_EQ(vmHandle, handler->vmOpen.handle);
    EXPECT_EQ(static_cast<uint64_t>(PRELIM_I915_DEBUG_VM_OPEN_READ_ONLY), handler->vmOpen.flags);

    EXPECT_EQ(1u, session->debugArea.reserved1);
    EXPECT_EQ(1u, session->debugArea.version);
    EXPECT_EQ(4u, session->debugArea.pgsize);
}

TEST_F(DebugApiLinuxTest, GivenBindInfoForVmHandleWhenReadingModuleDebugAreaReturnsIncorrectDataThenFailIsReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t vmHandle = 6;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToModuleDebugAreaBindInfo[vmHandle] = {0x1234000, 0x1000};

    std::vector<char> dummy;
    dummy.resize(sizeof(DebugAreaHeader));
    memset(dummy.data(), 2, dummy.size());

    handler->mmapRet = dummy.data();
    handler->setPreadMemory(dummy.data(), sizeof(session->debugArea), 0x1234000);
    handler->preadRetVal = sizeof(session->debugArea);

    auto retVal = session->readModuleDebugArea();

    EXPECT_FALSE(retVal);

    if (DebugManager.flags.EnableDebuggerMmapMemoryAccess.get()) {
        EXPECT_EQ(1, handler->mmapCalled);
        EXPECT_EQ(1, handler->munmapCalled);
    } else {
        EXPECT_EQ(1, handler->preadCalled);
    }
}

TEST_F(DebugApiLinuxTest, GivenBindInfoForVmHandleWhenReadingStateSaveAreaThenGpuMemoryIsRead) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    handler->mmapRet = stateSaveAreaHeader.data();

    handler->setPreadMemory(stateSaveAreaHeader.data(), stateSaveAreaHeader.size(), 0x1000);

    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t vmHandle = 6;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle] = {0x1000, stateSaveAreaHeader.size()};

    session->readStateSaveAreaHeader();

    if (DebugManager.flags.EnableDebuggerMmapMemoryAccess.get()) {
        EXPECT_EQ(1, handler->mmapCalled);
        EXPECT_EQ(1, handler->munmapCalled);
    } else {
        EXPECT_EQ(1, handler->preadCalled);
    }
    EXPECT_EQ(MockDebugSessionLinux::mockClientHandle, handler->vmOpen.client_handle);
    EXPECT_EQ(vmHandle, handler->vmOpen.handle);
    EXPECT_EQ(static_cast<uint64_t>(PRELIM_I915_DEBUG_VM_OPEN_READ_ONLY), handler->vmOpen.flags);
}

TEST_F(DebugApiLinuxTest, GivenSizeTooSmallWhenReadingStateSaveAreThenMemoryIsNotRead) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    handler->mmapRet = stateSaveAreaHeader.data();

    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;
    handler->vmOpen.handle = 0u;
    handler->vmOpen.flags = 0u;

    uint64_t vmHandle = 6;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle] = {0x1000, sizeof(SIP::StateSaveAreaHeader) - 4};

    session->readStateSaveAreaHeader();

    EXPECT_EQ(0, handler->mmapCalled);
    EXPECT_EQ(0, handler->preadCalled);
    EXPECT_EQ(0, handler->munmapCalled);
    EXPECT_EQ(0u, handler->vmOpen.handle);
    EXPECT_EQ(0u, handler->vmOpen.flags);
}

TEST_F(DebugApiLinuxTest, GivenErrorInVmOpenWhenReadingModuleDebugAreaThenGpuMemoryIsNotReadAndFalseReturned) {
    auto session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    handler->vmOpenRetVal = -19;
    uint64_t vmHandle = 6;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToModuleDebugAreaBindInfo[vmHandle] = {0x1234000, 0x1000};

    DebugAreaHeader debugArea;
    debugArea.reserved1 = 1;
    debugArea.pgsize = uint8_t(4);
    debugArea.version = 1;

    auto retVal = session->readModuleDebugArea();

    EXPECT_FALSE(retVal);
    EXPECT_EQ(0, handler->mmapCalled);
    EXPECT_EQ(0, handler->munmapCalled);
    EXPECT_EQ(vmHandle, handler->vmOpen.handle);
    EXPECT_STRNE("dbgarea", session->debugArea.magic);
}

TEST_F(DebugApiLinuxTest, GivenClientContextAndModuleDebugAreaEventsWhenInitializingThenSuccessIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    prelim_drm_i915_debug_event_client client = {};

    client.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    client.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    client.base.size = sizeof(prelim_drm_i915_debug_event_client);
    client.handle = 1;

    prelim_drm_i915_debug_event_context context = {};

    context.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT;
    context.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    context.base.size = sizeof(prelim_drm_i915_debug_event_context);
    context.client_handle = MockDebugSessionLinux::mockClientHandle;
    context.handle = 3;

    auto uuidName = NEO::classNamesToUuid[2].first;
    auto uuidNameSize = strlen(uuidName);
    auto uuidHash = NEO::classNamesToUuid[2].second;

    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuid.handle = 2;
    uuid.class_handle = PRELIM_I915_UUID_CLASS_STRING;
    uuid.payload_size = uuidNameSize;

    prelim_drm_i915_debug_read_uuid readUuid = {};
    readUuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    memcpy(readUuid.uuid, uuidHash.data(), uuidHash.size());
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(uuidName);
    readUuid.payload_size = uuid.payload_size;
    readUuid.handle = 3;

    prelim_drm_i915_debug_event_uuid uuidDebugArea = {};

    uuidDebugArea.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuidDebugArea.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuidDebugArea.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuidDebugArea.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuidDebugArea.handle = 3;
    uuidDebugArea.class_handle = 2;

    uint64_t moduleDebugAddress = 0x34567000;
    uint64_t vmBindDebugAreaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindDebugArea = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindDebugAreaData);

    vmBindDebugArea->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindDebugArea->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindDebugArea->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + sizeof(typeOfUUID);
    vmBindDebugArea->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindDebugArea->va_start = moduleDebugAddress;
    vmBindDebugArea->va_length = 0x1000;
    vmBindDebugArea->vm_handle = 50;
    vmBindDebugArea->num_uuids = 1;
    auto uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindDebugAreaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidTemp = 3;
    memcpy(uuids, &uuidTemp, sizeof(typeOfUUID));

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&context), static_cast<uint64_t>(context.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&uuid), static_cast<uint64_t>(uuid.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&uuidDebugArea), static_cast<uint64_t>(uuidDebugArea.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(vmBindDebugArea), static_cast<uint64_t>(vmBindDebugArea->base.size)});

    handler->returnUuid = &readUuid;

    auto eventsCount = handler->eventQueue.size();
    handler->pollRetVal = 1;

    DebugAreaHeader debugArea;
    debugArea.reserved1 = 1;
    debugArea.pgsize = uint8_t(4);
    debugArea.version = 1;

    handler->mmapRet = reinterpret_cast<char *>(&debugArea);
    handler->setPreadMemory(reinterpret_cast<char *>(&debugArea), sizeof(session->debugArea), moduleDebugAddress);
    handler->preadRetVal = sizeof(session->debugArea);

    session->ioctlHandler.reset(handler);

    ze_result_t result = session->initialize();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(eventsCount, static_cast<size_t>(session->getInternalEventCounter.load()));

    // Expect VM OPEN called
    EXPECT_EQ(50u, handler->vmOpen.handle);
}

TEST_F(DebugApiLinuxTest, GivenClientContextAndModuleDebugAreaEventsWhenIncorrectModuleDebugAreaReadThenErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    prelim_drm_i915_debug_event_client client = {};

    client.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    client.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    client.base.size = sizeof(prelim_drm_i915_debug_event_client);
    client.handle = 1;

    prelim_drm_i915_debug_event_context context = {};

    context.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT;
    context.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    context.base.size = sizeof(prelim_drm_i915_debug_event_context);
    context.client_handle = MockDebugSessionLinux::mockClientHandle;
    context.handle = 3;

    auto uuidName = NEO::classNamesToUuid[2].first;
    auto uuidNameSize = strlen(uuidName);
    auto uuidHash = NEO::classNamesToUuid[2].second;

    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuid.handle = 2;
    uuid.class_handle = PRELIM_I915_UUID_CLASS_STRING;
    uuid.payload_size = uuidNameSize;

    prelim_drm_i915_debug_read_uuid readUuid = {};
    readUuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    memcpy(readUuid.uuid, uuidHash.data(), uuidHash.size());
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(uuidName);
    readUuid.payload_size = uuid.payload_size;
    readUuid.handle = 3;

    prelim_drm_i915_debug_event_uuid uuidDebugArea = {};

    uuidDebugArea.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuidDebugArea.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuidDebugArea.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuidDebugArea.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuidDebugArea.handle = 3;
    uuidDebugArea.class_handle = 2;

    uint64_t moduleDebugAddress = 0x34567000;
    uint64_t vmBindDebugAreaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindDebugArea = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindDebugAreaData);

    vmBindDebugArea->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindDebugArea->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindDebugArea->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + sizeof(typeOfUUID);
    vmBindDebugArea->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindDebugArea->va_start = moduleDebugAddress;
    vmBindDebugArea->va_length = 0x1000;
    vmBindDebugArea->vm_handle = 0;
    vmBindDebugArea->num_uuids = 1;
    auto uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindDebugAreaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidTemp = 3;
    memcpy(uuids, &uuidTemp, sizeof(typeOfUUID));

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&context), static_cast<uint64_t>(context.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&uuid), static_cast<uint64_t>(uuid.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&uuidDebugArea), static_cast<uint64_t>(uuidDebugArea.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(vmBindDebugArea), static_cast<uint64_t>(vmBindDebugArea->base.size)});

    handler->returnUuid = &readUuid;
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);

    ze_result_t result = session->initialize();
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    EXPECT_STRNE("dbgarea", session->debugArea.magic);
}

TEST_F(DebugApiLinuxTest, GivenNoClientHandleSetWhenCheckingEventsCollectedThenFalseIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = MockDebugSessionLinux::invalidClientHandle;

    EXPECT_FALSE(session->checkAllEventsCollected());
}

TEST_F(DebugApiLinuxTest, GivenClientHandleSetButNoModuleDebugAreaWhenCheckingEventsCollectedThenFalseIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    EXPECT_FALSE(session->checkAllEventsCollected());
}

TEST_F(DebugApiLinuxTest, GivenTwoClientEventsWithTheSameHandleWhenInitializingThenDebugBreakIsCalledAndConnectionIsOverriden) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    const uint64_t clientHandle = 2;

    prelim_drm_i915_debug_event_client client = {};
    client.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    client.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    client.base.size = sizeof(prelim_drm_i915_debug_event_client);
    client.handle = clientHandle;

    prelim_drm_i915_debug_event_client client2 = {};
    client2.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    client2.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    client2.base.size = sizeof(prelim_drm_i915_debug_event_client);
    client2.handle = clientHandle;

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&client2), static_cast<uint64_t>(client2.base.size)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);
    session->initialize();

    EXPECT_NE(nullptr, session->clientHandleToConnection[clientHandle]);
    EXPECT_EQ(3, handler->ioctlCalled);
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionWhenClientCreateAndDestroyEventsReadOnInitializationThenDeviceLostErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    prelim_drm_i915_debug_event_client clientCreate = {};
    clientCreate.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    clientCreate.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    clientCreate.base.size = sizeof(prelim_drm_i915_debug_event_client);
    clientCreate.handle = 1;

    prelim_drm_i915_debug_event_client clientDestroy = {};
    clientDestroy.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    clientDestroy.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    clientDestroy.base.size = sizeof(prelim_drm_i915_debug_event_client);
    clientDestroy.handle = 1;

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&clientCreate), static_cast<uint64_t>(clientCreate.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&clientDestroy), static_cast<uint64_t>(clientDestroy.base.size)});
    handler->pollRetVal = 1;
    auto eventsCount = handler->eventQueue.size() + 1;

    session->ioctlHandler.reset(handler);
    session->clientHandle = 1;

    ze_result_t result = session->initialize();
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
    EXPECT_EQ(eventsCount, static_cast<size_t>(session->getInternalEventCounter.load()));
}

TEST_F(DebugApiLinuxTest, GivenEventWithInvalidFlagsWhenReadingEventThenUnknownErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    prelim_drm_i915_debug_event_client clientInvalidFlag = {};
    clientInvalidFlag.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    clientInvalidFlag.base.flags = 0x8000;
    clientInvalidFlag.base.size = sizeof(prelim_drm_i915_debug_event_client);
    clientInvalidFlag.handle = 1;

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&clientInvalidFlag), static_cast<uint64_t>(clientInvalidFlag.base.size)});
    handler->pollRetVal = 1;
    auto eventsCount = handler->eventQueue.size();

    session->ioctlHandler.reset(handler);

    auto memory = std::make_unique<uint64_t[]>(MockDebugSessionLinux::maxEventSize / sizeof(uint64_t));
    prelim_drm_i915_debug_event *event = reinterpret_cast<prelim_drm_i915_debug_event *>(memory.get());
    event->type = PRELIM_DRM_I915_DEBUG_EVENT_READ;
    event->flags = 0;
    event->size = MockDebugSessionLinux::maxEventSize;

    ze_result_t result = session->readEventImp(event);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    EXPECT_EQ(eventsCount, static_cast<size_t>(handler->ioctlCalled));
}

TEST_F(DebugApiLinuxTest, GivenDebugSessionInitializationWhenNoValidEventsAreReadThenResultNotReadyIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    prelim_drm_i915_debug_event_client clientInvalidFlag = {};
    clientInvalidFlag.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    clientInvalidFlag.base.flags = 0x8000;
    clientInvalidFlag.base.size = sizeof(prelim_drm_i915_debug_event_client);
    clientInvalidFlag.handle = 1;

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&clientInvalidFlag), static_cast<uint64_t>(clientInvalidFlag.base.size)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);

    ze_result_t result = session->initialize();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(DebugApiLinuxTest, GivenInvalidFlagsWhenReadingEventThenUnknownErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    prelim_drm_i915_debug_event_client clientInvalidFlag = {};
    clientInvalidFlag.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    clientInvalidFlag.base.flags = 0x8000;
    clientInvalidFlag.base.size = sizeof(prelim_drm_i915_debug_event_client);
    clientInvalidFlag.handle = 1;

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&clientInvalidFlag), static_cast<uint64_t>(clientInvalidFlag.base.size)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);

    uint64_t data[512];
    auto drmDebugEvent = reinterpret_cast<prelim_drm_i915_debug_event *>(data);

    ze_result_t result = session->readEventImp(drmDebugEvent);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    EXPECT_EQ(1u, static_cast<size_t>(handler->ioctlCalled));
}

TEST_F(DebugApiLinuxTest, GivenValidFlagsWhenReadingEventThenEventIsNotProcessed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;

    prelim_drm_i915_debug_event_eu_attention attEvent = {};
    attEvent.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attEvent.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    attEvent.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention);
    attEvent.bitmask_size = 0;
    attEvent.client_handle = MockDebugSessionLinux::mockClientHandle;
    attEvent.lrc_handle = lrcHandle;
    attEvent.flags = 0;

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&attEvent), static_cast<uint64_t>(attEvent.base.size)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);

    uint64_t data[512];
    auto drmDebugEvent = reinterpret_cast<prelim_drm_i915_debug_event *>(data);
    ze_result_t result = session->readEventImp(drmDebugEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, session->handleEventCalledCount);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerLogsAndUnhandledEventTypeWhenHandlingEventThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_INFO);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    prelim_drm_i915_debug_event event = {};

    event.type = PRELIM_DRM_I915_DEBUG_EVENT_MAX_EVENT + 1;
    event.flags = 0;
    event.size = sizeof(prelim_drm_i915_debug_event);

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&event), static_cast<uint64_t>(event.size)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);

    ::testing::internal::CaptureStdout();

    session->handleEvent(&event);

    auto errorMessage = ::testing::internal::GetCapturedStdout();
    std::stringstream expectedMessage;
    expectedMessage << "PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: UNHANDLED ";
    expectedMessage << PRELIM_DRM_I915_DEBUG_EVENT_MAX_EVENT + 1;
    expectedMessage << " flags = 0 size =";
    auto pos = errorMessage.find(expectedMessage.str());
    EXPECT_NE(std::string::npos, pos);
}

TEST_F(DebugApiLinuxTest, GivenContextCreateAndDestroyEventsWhenInitializingThenOnlyValidContextsAreStored) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    uint64_t clientHandle = 2;
    prelim_drm_i915_debug_event_client client = {};
    client.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    client.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    client.base.size = sizeof(prelim_drm_i915_debug_event_client);
    client.handle = clientHandle;

    prelim_drm_i915_debug_event_context context = {};
    context.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT;
    context.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    context.base.size = sizeof(prelim_drm_i915_debug_event_context);
    context.client_handle = clientHandle;
    context.handle = 3;

    prelim_drm_i915_debug_event_context context2 = {};
    context2.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT,
    context2.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE,
    context2.base.size = sizeof(prelim_drm_i915_debug_event_context);
    context2.client_handle = clientHandle;
    context2.handle = 4;

    prelim_drm_i915_debug_event_context contextDestroy = {};
    contextDestroy.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT;
    contextDestroy.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    contextDestroy.base.size = sizeof(prelim_drm_i915_debug_event_context);
    contextDestroy.client_handle = clientHandle;
    contextDestroy.handle = 3;

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&context), static_cast<uint64_t>(context.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&context2), static_cast<uint64_t>(context2.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&contextDestroy), static_cast<uint64_t>(contextDestroy.base.size)});

    auto eventsCount = handler->eventQueue.size() + 1;
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);
    session->initialize();

    EXPECT_EQ(eventsCount, static_cast<size_t>(session->getInternalEventCounter.load()));

    EXPECT_EQ(1u, session->clientHandleToConnection[clientHandle]->contextsCreated.size());
    EXPECT_EQ(session->clientHandleToConnection[clientHandle]->contextsCreated.end(), session->clientHandleToConnection[clientHandle]->contextsCreated.find(context.handle));
}

TEST_F(DebugApiLinuxTest, GivenUuidEventForClassWhenHandlingEventThenClassHandleIsSavedWithNameAndIndex) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto uuidName = NEO::classNamesToUuid[0].first;
    auto uuidNameSize = strlen(uuidName);
    auto uuidHash = NEO::classNamesToUuid[0].second;
    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = 1;
    uuid.handle = 2;
    uuid.payload_size = uuidNameSize;

    prelim_drm_i915_debug_read_uuid readUuid = {};
    readUuid.client_handle = 1;
    memcpy(readUuid.uuid, uuidHash.data(), uuidHash.size());
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(uuidName);
    readUuid.payload_size = uuid.payload_size;
    readUuid.handle = 2;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    handler->returnUuid = &readUuid;

    session->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(&uuid));

    auto iter = session->getClassHandleToIndex().find(readUuid.handle);
    EXPECT_NE(session->getClassHandleToIndex().end(), iter);

    auto classNameIndex = iter->second;
    EXPECT_EQ(uuidName, classNameIndex.first);
    EXPECT_EQ(0u, classNameIndex.second);
}

TEST_F(DebugApiLinuxTest, GivenUuidEventWithPayloadWhenHandlingEventThenReadEventIoctlIsCalledAndUuidDataIsStored) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    const char uuidString[] = "31203221-8069-5a0a-0000-55742d6ea000";
    std::string elf("ELF");
    prelim_drm_i915_debug_event_uuid uuidElf = {};
    uuidElf.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuidElf.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuidElf.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuidElf.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuidElf.handle = 3;
    uuidElf.class_handle = 2;

    uuidElf.payload_size = elf.size();

    prelim_drm_i915_debug_read_uuid readUuidElf = {};
    memcpy(readUuidElf.uuid, uuidString, 36);
    readUuidElf.payload_ptr = reinterpret_cast<uint64_t>(elf.c_str());
    readUuidElf.payload_size = elf.size();
    readUuidElf.handle = 3;

    handler->returnUuid = &readUuidElf;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[2] = {"ELF", static_cast<uint32_t>(NEO::DrmResourceClass::Elf)};

    session->handleEvent(&uuidElf.base);

    EXPECT_EQ(uuidElf.handle, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuidElf.handle].handle);
    EXPECT_EQ(uuidElf.class_handle, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuidElf.handle].classHandle);
    EXPECT_NE(nullptr, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuidElf.handle].data);
    EXPECT_EQ(elf.size(), session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuidElf.handle].dataSize);
    EXPECT_EQ(0x55742d6ea000u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuidElf.handle].ptr);

    std::string registeredElf(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuidElf.handle].data.get(), session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuidElf.handle].dataSize);
    EXPECT_EQ(elf, registeredElf);

    EXPECT_EQ(1, handler->ioctlCalled);
}

TEST_F(DebugApiLinuxTest, GivenUuidEventWhenHandlingThenUuidIsInsertedToMap) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuid.handle = 2;
    uuid.payload_size = 0;

    auto handler = new MockIoctlHandler;
    handler->eventQueue.push({reinterpret_cast<char *>(&uuid), static_cast<uint64_t>(uuid.base.size)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);
    session->handleEvent(&uuid.base);

    EXPECT_EQ(1u, session->clientHandleToConnection[uuid.client_handle]->uuidMap.size());
    EXPECT_NE(session->clientHandleToConnection[uuid.client_handle]->uuidMap.end(), session->clientHandleToConnection[uuid.client_handle]->uuidMap.find(uuid.handle));

    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    session->handleEvent(&uuid.base);
    EXPECT_NE(session->clientHandleToConnection[uuid.client_handle]->uuidMap.end(), session->clientHandleToConnection[uuid.client_handle]->uuidMap.find(uuid.handle));
}

TEST_F(DebugApiLinuxTest, GivenUuidEventForL0ZebinModuleWhenHandlingEventThenKernelCountFromPayloadIsRead) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    const char uuidString[] = "31203221-8069-5a0a-0000-55742d6ea000";
    uint32_t kernelCount = 5;
    prelim_drm_i915_debug_event_uuid l0ModuleUuid = {};
    l0ModuleUuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    l0ModuleUuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    l0ModuleUuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    l0ModuleUuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    l0ModuleUuid.handle = 3;
    l0ModuleUuid.class_handle = 2;

    l0ModuleUuid.payload_size = sizeof(kernelCount);

    prelim_drm_i915_debug_read_uuid readUuidElf = {};
    memcpy(readUuidElf.uuid, uuidString, 36);
    readUuidElf.payload_ptr = reinterpret_cast<uint64_t>(&kernelCount);
    readUuidElf.payload_size = sizeof(kernelCount);
    readUuidElf.handle = 3;

    handler->returnUuid = &readUuidElf;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[2] = {"L0_MODULE", static_cast<uint32_t>(NEO::DrmResourceClass::L0ZebinModule)};

    session->handleEvent(&l0ModuleUuid.base);

    EXPECT_NE(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.end(),
              session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.find(l0ModuleUuid.handle));

    EXPECT_EQ(kernelCount, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[l0ModuleUuid.handle].segmentCount);

    EXPECT_EQ(l0ModuleUuid.handle, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[l0ModuleUuid.handle].handle);
    EXPECT_EQ(l0ModuleUuid.class_handle, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[l0ModuleUuid.handle].classHandle);
    EXPECT_NE(nullptr, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[l0ModuleUuid.handle].data);
    EXPECT_EQ(sizeof(kernelCount), session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[l0ModuleUuid.handle].dataSize);

    EXPECT_EQ(1, handler->ioctlCalled);

    // inject module segment
    auto &module = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[l0ModuleUuid.handle];
    module.loadAddresses[0].insert(0x12340000);

    l0ModuleUuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    l0ModuleUuid.payload_size = 0;
    session->handleEvent(&l0ModuleUuid.base);

    EXPECT_EQ(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.end(),
              session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.find(l0ModuleUuid.handle));
}

TEST_F(DebugApiLinuxTest, GivenUuidEventWithNonElfClassHandleWhenHandlingEventThenUuidDataPtrIsNotSet) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    const char uuidString[] = "31203221-8069-5a0a-0000-55742d6ea000";
    std::string elf("ELF");
    prelim_drm_i915_debug_event_uuid uuidElf = {};
    uuidElf.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuidElf.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuidElf.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuidElf.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuidElf.handle = 3;
    uuidElf.class_handle = 2;

    uuidElf.payload_size = elf.size();

    prelim_drm_i915_debug_read_uuid readUuidElf = {};
    memcpy(readUuidElf.uuid, uuidString, 36);
    readUuidElf.payload_ptr = reinterpret_cast<uint64_t>(elf.c_str());
    readUuidElf.payload_size = elf.size();
    readUuidElf.handle = 3;
    handler->returnUuid = &readUuidElf;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex.erase(2);

    session->handleEvent(&uuidElf.base);

    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuidElf.handle].ptr);
    EXPECT_EQ(1, handler->ioctlCalled);
}

TEST_F(DebugApiLinuxTest, GivenVmBindEventWithUnknownUUIDWhenHandlingEventThenEventIsIgnored) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint64_t sbaClassHandle = 1;
    uint64_t sbaHandle = 4;
    uint64_t sbaAddress = 0x1234000;

    DebugSessionLinux::UuidData sbaUuidData = {
        .handle = sbaHandle,
        .classHandle = sbaClassHandle,
        .classIndex = NEO::DrmResourceClass::SbaTrackingBuffer};

    uint64_t vmBindSbaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindSba = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindSbaData);

    vmBindSba->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindSba->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindSba->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + sizeof(typeOfUUID);
    vmBindSba->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindSba->va_start = sbaAddress;
    vmBindSba->va_length = 0x1000;
    vmBindSba->vm_handle = 0;
    vmBindSba->num_uuids = 1;
    auto uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindSbaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    auto uuidTemp = static_cast<typeOfUUID>(sbaHandle);
    memcpy(uuids, &uuidTemp, sizeof(typeOfUUID));

    session->handleEvent(&vmBindSba->base);

    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToStateBaseAreaBindInfo.size());
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToModuleDebugAreaBindInfo.size());
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo.size());
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.size());
}

TEST_F(DebugApiLinuxTest, GivenVmBindEventWithUuidOfUnknownClassWhenHandlingEventThenNoBindInfoIsStored) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint64_t sbaClassHandle = 1;
    uint64_t sbaHandle = 4;
    uint64_t sbaAddress = 0x1234000;

    DebugSessionLinux::UuidData sbaUuidData = {
        .handle = sbaHandle,
        .classHandle = sbaClassHandle,
        .classIndex = NEO::DrmResourceClass::SbaTrackingBuffer};

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(sbaHandle, std::move(sbaUuidData));

    uint64_t vmBindSbaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindSba = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindSbaData);

    vmBindSba->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindSba->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindSba->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + sizeof(typeOfUUID);
    vmBindSba->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindSba->va_start = sbaAddress;
    vmBindSba->va_length = 0x1000;
    vmBindSba->vm_handle = 0;
    vmBindSba->num_uuids = 1;
    auto uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindSbaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidTemp = static_cast<typeOfUUID>(sbaHandle);
    memcpy(uuids, &uuidTemp, sizeof(typeOfUUID));

    session->handleEvent(&vmBindSba->base);

    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToStateBaseAreaBindInfo.size());
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToModuleDebugAreaBindInfo.size());
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.size());
}

TEST_F(DebugApiLinuxTest, GivenUuidEventOfKnownClassWhenHandlingEventThenGpuAddressIsSavedFromPayload) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint64_t sbaClassHandle = 1;
    uint64_t moduleDebugClassHandle = 2;
    uint64_t contextSaveClassHandle = 3;

    uint64_t sbaAddress = 0x1234000;
    uint64_t moduleDebugAddress = 0x34567000;
    uint64_t contextSaveAddress = 0x56789000;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[sbaClassHandle] = {"SBA AREA", static_cast<uint32_t>(NEO::DrmResourceClass::SbaTrackingBuffer)};
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[moduleDebugClassHandle] = {"DEBUG AREA", static_cast<uint32_t>(NEO::DrmResourceClass::ModuleHeapDebugArea)};
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[contextSaveClassHandle] = {"CONTEXT SAVE AREA", static_cast<uint32_t>(NEO::DrmResourceClass::ContextSaveArea)};

    prelim_drm_i915_debug_event_uuid uuidSba = {};
    uuidSba.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuidSba.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuidSba.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuidSba.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuidSba.handle = 4;
    uuidSba.class_handle = sbaClassHandle;
    uuidSba.payload_size = sizeof(sbaAddress);

    prelim_drm_i915_debug_read_uuid readUuid = {};
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(&sbaAddress);
    readUuid.payload_size = sizeof(sbaAddress);
    readUuid.handle = 4;

    handler->returnUuid = &readUuid;
    session->handleEvent(&uuidSba.base);

    prelim_drm_i915_debug_event_uuid uuidModule = {};
    uuidModule.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuidModule.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuidModule.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuidModule.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuidModule.handle = 5;
    uuidModule.class_handle = moduleDebugClassHandle;
    uuidModule.payload_size = sizeof(moduleDebugAddress);

    readUuid = {};
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(&moduleDebugAddress);
    readUuid.payload_size = sizeof(moduleDebugAddress);
    readUuid.handle = 5;

    handler->returnUuid = &readUuid;
    session->handleEvent(&uuidModule.base);

    prelim_drm_i915_debug_event_uuid uuidContextSave = {};

    uuidContextSave.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuidContextSave.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuidContextSave.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuidContextSave.client_handle = MockDebugSessionLinux::mockClientHandle,
    uuidContextSave.handle = 6,
    uuidContextSave.class_handle = contextSaveClassHandle;
    uuidContextSave.payload_size = sizeof(contextSaveAddress);

    readUuid = {};
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(&contextSaveAddress);
    readUuid.payload_size = sizeof(contextSaveAddress);
    readUuid.handle = 6;

    handler->returnUuid = &readUuid;
    session->handleEvent(&uuidContextSave.base);

    EXPECT_EQ(moduleDebugAddress, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->moduleDebugAreaGpuVa);
    EXPECT_EQ(sbaAddress, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->stateBaseAreaGpuVa);
    EXPECT_EQ(contextSaveAddress, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextStateSaveAreaGpuVa);
}

struct DebugApiLinuxVmBindFixture : public DebugApiLinuxFixture, public MockDebugSessionLinuxHelper {
    void setUp() {
        DebugApiLinuxFixture::setUp();

        zet_debug_config_t config = {};
        config.pid = 0x1234;

        session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
        ASSERT_NE(nullptr, session);
        session->clientHandle = MockDebugSessionLinux::mockClientHandle;

        handler = new MockIoctlHandler;
        session->ioctlHandler.reset(handler);

        setupSessionClassHandlesAndUuidMap(session.get());
    }

    void tearDown() {
        DebugApiLinuxFixture::tearDown();
    }

    const uint64_t zebinModuleClassHandle = 101;
    const uint64_t vmHandleForVmBind = 3;

    const uint64_t zebinModuleUUID = 9;

    MockIoctlHandler *handler = nullptr;
    std::unique_ptr<MockDebugSessionLinux> session;
};

using DebugApiLinuxVmBindTest = Test<DebugApiLinuxVmBindFixture>;

TEST_F(DebugApiLinuxVmBindTest, GivenVmBindEventWithKnownUuidClassWhenHandlingEventThenBindInfoIsStoredForVm) {
    uint64_t sbaAddress = 0x1234000;
    uint64_t moduleDebugAddress = 0x34567000;
    uint64_t contextSaveAddress = 0x56789000;

    uint64_t vmBindSbaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + sizeof(typeOfUUID)];
    uint64_t vmBindDebugAreaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + sizeof(typeOfUUID)];
    uint64_t vmBindContextSaveAreaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + sizeof(typeOfUUID)];

    prelim_drm_i915_debug_event_vm_bind *vmBindSba = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindSbaData);
    prelim_drm_i915_debug_event_vm_bind *vmBindDebugArea = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindDebugAreaData);
    prelim_drm_i915_debug_event_vm_bind *vmBindContextArea = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindContextSaveAreaData);

    vmBindSba->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindSba->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindSba->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + sizeof(decltype(prelim_drm_i915_debug_event_vm_bind::uuids[0]));
    vmBindSba->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindSba->va_start = sbaAddress;
    vmBindSba->va_length = 0x1000;
    vmBindSba->vm_handle = vmHandleForVmBind;
    vmBindSba->num_uuids = 1;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindSbaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    auto temp = static_cast<typeOfUUID>(sbaUUID);
    memcpy(uuids, &temp, sizeof(typeOfUUID));

    vmBindDebugArea->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindDebugArea->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindDebugArea->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + sizeof(decltype(prelim_drm_i915_debug_event_vm_bind::uuids[0]));
    vmBindDebugArea->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindDebugArea->va_start = moduleDebugAddress;
    vmBindDebugArea->va_length = 0x1000;
    vmBindDebugArea->vm_handle = 4;
    vmBindDebugArea->num_uuids = 1;
    uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindDebugAreaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    temp = static_cast<typeOfUUID>(debugAreaUUID);
    memcpy(uuids, &temp, sizeof(typeOfUUID));

    vmBindContextArea->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindContextArea->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindContextArea->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + sizeof(decltype(prelim_drm_i915_debug_event_vm_bind::uuids[0]));
    vmBindContextArea->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindContextArea->va_start = contextSaveAddress;
    vmBindContextArea->va_length = 0x1000;
    vmBindContextArea->vm_handle = 5;
    vmBindContextArea->num_uuids = 1;
    uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindContextSaveAreaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    temp = static_cast<typeOfUUID>(stateSaveUUID);
    memcpy(uuids, &temp, sizeof(typeOfUUID));

    session->handleEvent(&vmBindSba->base);
    session->handleEvent(&vmBindDebugArea->base);
    session->handleEvent(&vmBindContextArea->base);

    EXPECT_EQ(moduleDebugAddress, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToModuleDebugAreaBindInfo[4].gpuVa);
    EXPECT_EQ(0x1000u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToModuleDebugAreaBindInfo[4].size);

    EXPECT_EQ(sbaAddress, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToStateBaseAreaBindInfo[3].gpuVa);
    EXPECT_EQ(0x1000u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToStateBaseAreaBindInfo[3].size);

    EXPECT_EQ(contextSaveAddress, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo[5].gpuVa);
    EXPECT_EQ(0x1000u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo[5].size);
}

TEST_F(DebugApiLinuxVmBindTest, GivenZeruNumUuidsWhenHandlingVmBindEventThenNoBindInfoIsStored) {
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint64_t sbaAddress = 0x1234000;
    prelim_drm_i915_debug_event_vm_bind vmBindSba = {};

    vmBindSba.base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindSba.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindSba.base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + sizeof(typeOfUUID);
    vmBindSba.client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindSba.va_start = sbaAddress;
    vmBindSba.va_length = 0x1000;
    vmBindSba.num_uuids = 0;

    session->handleVmBindEvent(&vmBindSba);

    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToStateBaseAreaBindInfo.size());
}

TEST_F(DebugApiLinuxVmBindTest, GivenNeedsAckFlagWhenHandlingVmBindEventThenAckIsSent) {
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint64_t address = 0x1234000;
    prelim_drm_i915_debug_event_vm_bind vmBind = {};

    vmBind.base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBind.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBind.base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + sizeof(typeOfUUID);
    vmBind.base.seqno = 45;
    vmBind.client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBind.va_start = address;
    vmBind.va_length = 0x1000;
    vmBind.num_uuids = 0;

    session->handleVmBindEvent(&vmBind);

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(vmBind.base.seqno, handler->debugEventAcked.seqno);
    EXPECT_EQ(vmBind.base.type, handler->debugEventAcked.type);
}

TEST_F(DebugApiLinuxVmBindTest, GivenEventWithAckFlagWhenHandlingEventForISAThenEventToAckIsPushedToIsaEvents) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 20u;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;

    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, session->apiEvents.size());
    EXPECT_EQ(0u, session->eventsToAck.size());

    auto event = session->apiEvents.front();
    session->apiEvents.pop();

    EXPECT_EQ(ZET_DEBUG_EVENT_FLAG_NEED_ACK, event.flags);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);

    auto isaIter = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0].find(isaGpuVa);
    ASSERT_NE(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0].end(), isaIter);

    EXPECT_EQ(1u, isaIter->second->ackEvents.size());
    auto ackedEvent = isaIter->second->ackEvents[0];

    EXPECT_EQ(vmBindIsa->base.flags, ackedEvent.flags);
    EXPECT_EQ(vmBindIsa->base.seqno, ackedEvent.seqno);
    EXPECT_EQ(vmBindIsa->base.type, ackedEvent.type);
}

TEST_F(DebugApiLinuxVmBindTest, GivenEventForISAWhenModuleLoadEventAlreadyAckedThenEventIsAckedImmediatelyAndNotPushed) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 20u;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;

    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    auto isaIter = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0].find(isaGpuVa);
    ASSERT_NE(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0].end(), isaIter);
    EXPECT_EQ(1u, isaIter->second->ackEvents.size());

    auto event = session->apiEvents.front();
    session->apiEvents.pop();

    session->acknowledgeEvent(&event);
    EXPECT_EQ(0u, isaIter->second->ackEvents.size());

    vmBindIsa->vm_handle = vmHandleForVmBind + 100;
    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(0u, isaIter->second->ackEvents.size());
}

TEST_F(DebugApiLinuxVmBindTest, GivenEventForIsaWithoutAckTriggeredBeforeAttachWhenHandlingSubsequentEventsWithAckThenEventsAreAckedImmediatelyAndNotPushed) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    // CREATE flag without ACK - triggered before attach
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 20u;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;

    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    auto isaIter = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0].find(isaGpuVa);
    ASSERT_NE(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0].end(), isaIter);
    EXPECT_EQ(0u, isaIter->second->ackEvents.size());
    // Auto-acked event
    EXPECT_TRUE(isaIter->second->moduleLoadEventAck);

    auto event = session->apiEvents.front();
    session->apiEvents.pop();
    EXPECT_EQ(0u, event.flags & ZET_DEBUG_EVENT_FLAG_NEED_ACK);

    // VM BIND after attach needs ACK
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.seqno = 150;
    vmBindIsa->vm_handle = vmHandleForVmBind + 100;

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(0u, isaIter->second->ackEvents.size());
    EXPECT_EQ(vmBindIsa->base.seqno, handler->debugEventAcked.seqno);
}

TEST_F(DebugApiLinuxVmBindTest, GivenIsaRemovedWhenModuleLoadEventIsAckedThenErrorReturned) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 20u;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;

    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    auto isaIter = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0].find(isaGpuVa);
    ASSERT_NE(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0].end(), isaIter);
    EXPECT_EQ(1u, isaIter->second->ackEvents.size());

    zet_debug_event_t event = {};
    auto result = session->readEvent(0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0].clear();

    result = session->acknowledgeEvent(&event);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
}

TEST_F(DebugApiLinuxVmBindTest, GivenVmBindEventWithInvalidNumUUIDsWhenHandlingEventThenBindInfoIsNotStored) {
    prelim_drm_i915_debug_event_vm_bind vmBindSba = {};
    uint64_t sbaAddress = 0x1234000;

    vmBindSba.base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindSba.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindSba.base.size = sizeof(prelim_drm_i915_debug_event_vm_bind);
    vmBindSba.client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindSba.va_start = sbaAddress;
    vmBindSba.va_length = 0x1000;
    vmBindSba.num_uuids = 10;

    session->handleEvent(&vmBindSba.base);

    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToStateBaseAreaBindInfo.size());
}

TEST_F(DebugApiLinuxVmBindTest, GivenVmBindEventWithAckNeededForIsaWhenHandlingEventThenIsaAllocationIsSavedWithEventToAck) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 3;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;

    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    EXPECT_EQ(0u, session->eventsToAck.size());

    session->handleEvent(&vmBindIsa->base);

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];

    EXPECT_EQ(1u, isaMap.size());
    EXPECT_NE(isaMap.end(), isaMap.find(isaGpuVa));

    auto isaAllocation = isaMap[isaGpuVa].get();
    EXPECT_EQ(isaGpuVa, isaAllocation->bindInfo.gpuVa);
    EXPECT_EQ(isaSize, isaAllocation->bindInfo.size);
    EXPECT_EQ(elfUUID, isaAllocation->elfUuidHandle);
    EXPECT_EQ(3u, isaAllocation->vmHandle);
    EXPECT_TRUE(isaAllocation->tileInstanced);

    ASSERT_EQ(1u, isaAllocation->ackEvents.size());
    auto eventToAck = isaAllocation->ackEvents[0];
    EXPECT_EQ(vmBindIsa->base.type, eventToAck.type);
    EXPECT_EQ(vmBindIsa->base.seqno, eventToAck.seqno);
    EXPECT_EQ(0u, handler->debugEventAcked.seqno);
}

TEST_F(DebugApiLinuxVmBindTest, GivenCookieWhenHandlingVmBindForIsaThenIsaAllocationIsTileInstanced) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 3;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;

    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);
    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];

    EXPECT_EQ(1u, isaMap.size());
    EXPECT_NE(isaMap.end(), isaMap.find(isaGpuVa));

    auto isaAllocation = isaMap[isaGpuVa].get();
    EXPECT_EQ(isaGpuVa, isaAllocation->bindInfo.gpuVa);
    EXPECT_EQ(isaSize, isaAllocation->bindInfo.size);
    EXPECT_EQ(elfUUID, isaAllocation->elfUuidHandle);
    EXPECT_EQ(3u, isaAllocation->vmHandle);
    EXPECT_TRUE(isaAllocation->tileInstanced);
}

TEST_F(DebugApiLinuxVmBindTest, GivenNoCookieWhenHandlingVmBindForIsaThenIsaAllocationIsNotTileInstanced) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 3;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 2;

    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

    typeOfUUID uuidsTemp[2];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(elfUUID);
    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];

    EXPECT_EQ(1u, isaMap.size());
    EXPECT_NE(isaMap.end(), isaMap.find(isaGpuVa));

    auto isaAllocation = isaMap[isaGpuVa].get();
    EXPECT_EQ(isaGpuVa, isaAllocation->bindInfo.gpuVa);
    EXPECT_EQ(isaSize, isaAllocation->bindInfo.size);
    EXPECT_EQ(elfUUID, isaAllocation->elfUuidHandle);
    EXPECT_EQ(3u, isaAllocation->vmHandle);
    EXPECT_FALSE(isaAllocation->tileInstanced);
}

TEST_F(DebugApiLinuxVmBindTest, GivenTwoVmBindEventForTheSameIsaInDifferentVMWhenHandlingEventThenIsaVmHandleIsNotOverriden) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];
    EXPECT_EQ(1u, isaMap.size());
    EXPECT_NE(isaMap.end(), isaMap.find(isaGpuVa));
    auto isaAllocation = isaMap[isaGpuVa].get();
    EXPECT_EQ(3u, isaAllocation->vmHandle);

    // Override VM HANDLE
    vmBindIsa->vm_handle = 100;
    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, isaMap.size());
    isaAllocation = isaMap[isaGpuVa].get();
    EXPECT_EQ(3u, isaAllocation->vmHandle);
}

TEST_F(DebugApiLinuxVmBindTest, GivenVmBindDestroyEventForIsaWhenHandlingEventThenIsaEntryIsNotSaved) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];
    EXPECT_EQ(0u, isaMap.size());
}

TEST_F(DebugApiLinuxVmBindTest, GivenVmBindEventForIsaWhenReadingEventThenModuleLoadEventIsReturned) {
    uint64_t isaGpuVa = device->getHwInfo().capabilityTable.gpuAddressSpace - 0x3000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];
    EXPECT_EQ(0u, isaMap.size());

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, isaMap.size());
    EXPECT_EQ(isaGpuVa, isaMap[isaGpuVa]->bindInfo.gpuVa);

    // No event to ACK if vmBind doesn't have ACK flag
    EXPECT_EQ(0u, session->eventsToAck.size());

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(session->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(0u, event.flags);

    auto gmmHelper = neoDevice->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(isaGpuVa), event.info.module.load);

    if (Math::log2(device->getHwInfo().capabilityTable.gpuAddressSpace + 1) >= 48) {
        EXPECT_NE(isaGpuVa, event.info.module.load);
    }
    auto elf = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr;
    auto elfSize = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].dataSize;
    EXPECT_EQ(elf, event.info.module.moduleBegin);
    EXPECT_EQ(elf + elfSize, event.info.module.moduleEnd);
}

TEST_F(DebugApiLinuxVmBindTest, GivenVmBindCreateAndDestroyEventsForIsaWhenReadingEventsThenModuleLoadAndUnloadEventsReturned) {
    uint64_t isaGpuVa = device->getHwInfo().capabilityTable.gpuAddressSpace - 0x3000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];
    EXPECT_EQ(0u, isaMap.size());

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, isaMap.size());
    EXPECT_EQ(isaGpuVa, isaMap[isaGpuVa]->bindInfo.gpuVa);

    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(0u, isaMap.size());

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(session->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);

    auto gmmHelper = neoDevice->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(isaGpuVa), event.info.module.load);

    memset(&event, 0, sizeof(zet_debug_event_t));
    result = zetDebugReadEvent(session->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD, event.type);

    EXPECT_EQ(gmmHelper->canonize(isaGpuVa), event.info.module.load);

    auto elf = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr;
    auto elfSize = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].dataSize;
    EXPECT_EQ(elf, event.info.module.moduleBegin);
    EXPECT_EQ(elf + elfSize, event.info.module.moduleEnd);
}

TEST_F(DebugApiLinuxVmBindTest, GivenIsaBoundMultipleTimesWhenHandlingVmBindDestroyEventThenModuleUnloadEventIsNotPushedToQueue) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];

    auto isa = std::make_unique<DebugSessionLinux::IsaAllocation>();
    isa->bindInfo = {isaGpuVa, isaSize};
    isa->vmHandle = 3;
    isa->elfUuidHandle = DebugSessionLinux::invalidHandle;
    isa->moduleBegin = 0;
    isa->moduleEnd = 0;
    isa->vmBindCounter = 5;
    isaMap[isaGpuVa] = std::move(isa);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));
    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, isaMap.size());
    EXPECT_EQ(0u, session->apiEvents.size());
}

TEST_F(DebugApiLinuxVmBindTest, GivenVmBindEventForIsaWithInvalidElfWhenReadingEventThenModuleLoadEventReturnZeroElf) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 3;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[3];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID + 1000);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(session->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);

    EXPECT_EQ(isaGpuVa, event.info.module.load);
    EXPECT_EQ(0u, event.info.module.moduleBegin);
    EXPECT_EQ(0u, event.info.module.moduleEnd);
}

TEST_F(DebugApiLinuxVmBindTest, GivenEventWithL0ZebinModuleWhenHandlingEventThenModuleLoadAndUnloadEventsAreReportedForLastKernel) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaGpuVa2 = 0x340000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    const uint32_t kernelCount = 2;
    DebugSessionLinux::UuidData zebinModuleUuidData = {
        .handle = zebinModuleUUID,
        .classHandle = zebinModuleClassHandle,
        .classIndex = NEO::DrmResourceClass::L0ZebinModule,
        .data = std::make_unique<char[]>(sizeof(kernelCount)),
        .dataSize = sizeof(kernelCount)};

    memcpy(zebinModuleUuidData.data.get(), &kernelCount, sizeof(kernelCount));

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[zebinModuleClassHandle] = {"L0_ZEBIN_MODULE", static_cast<uint32_t>(NEO::DrmResourceClass::L0ZebinModule)};
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(zebinModuleUUID, std::move(zebinModuleUuidData));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount = 2;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr = 0x1000;

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 10;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 4;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[4];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);
    uuidsTemp[3] = static_cast<typeOfUUID>(zebinModuleUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));
    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(0u, session->apiEvents.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[0].size());

    // event not pushed to ack
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0][isaGpuVa]->ackEvents.size());
    EXPECT_EQ(1, handler->ioctlCalled); // ACK
    EXPECT_EQ(vmBindIsa->base.seqno, handler->debugEventAcked.seqno);

    vmBindIsa->va_start = isaGpuVa2;
    vmBindIsa->base.seqno = 11;
    handler->ioctlCalled = 0;
    handler->debugEventAcked.seqno = 0;

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, session->apiEvents.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.size());
    EXPECT_EQ(2u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[0].size());
    EXPECT_EQ(2u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount);

    // event not pushed to ack
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0][isaGpuVa2]->ackEvents.size());
    EXPECT_EQ(0, handler->ioctlCalled);
    EXPECT_EQ(0u, handler->debugEventAcked.seqno);

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];
    EXPECT_EQ(2u, isaMap.size());

    EXPECT_FALSE(isaMap[isaGpuVa]->moduleLoadEventAck);
    EXPECT_FALSE(isaMap[isaGpuVa2]->moduleLoadEventAck);

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(session->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(ZET_DEBUG_EVENT_FLAG_NEED_ACK, event.flags & ZET_DEBUG_EVENT_FLAG_NEED_ACK);
    EXPECT_EQ(isaGpuVa2, event.info.module.load);

    auto elfAddress = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr;
    auto elfSize = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].dataSize;
    EXPECT_EQ(elfAddress, event.info.module.moduleBegin);
    EXPECT_EQ(elfAddress + elfSize, event.info.module.moduleEnd);

    result = zetDebugAcknowledgeEvent(session->toHandle(), &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(isaMap[isaGpuVa]->moduleLoadEventAck);
    EXPECT_FALSE(isaMap[isaGpuVa2]->moduleLoadEventAck);
    // vm_bind acked
    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(11u, handler->debugEventAcked.seqno);

    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    vmBindIsa->va_start = isaGpuVa2;

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(0u, session->apiEvents.size());

    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    vmBindIsa->va_start = isaGpuVa;

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, session->apiEvents.size());

    memset(&event, 0, sizeof(zet_debug_event_t));

    result = zetDebugReadEvent(session->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD, event.type);
    EXPECT_EQ(isaGpuVa2, event.info.module.load);
    EXPECT_EQ(0u, event.flags & ZET_DEBUG_EVENT_FLAG_NEED_ACK);

    EXPECT_EQ(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr, event.info.module.moduleBegin);
    EXPECT_EQ(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr +
                  session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].dataSize,
              event.info.module.moduleEnd);
}

TEST_F(DebugApiLinuxVmBindTest, GivenAttachAfterModuleCreateWhenHandlingEventWithforL0ZebinModuleThenModuleLoadAndUnloadEventsAreReportedForLastKernel) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaGpuVa2 = 0x340000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    const uint32_t kernelCount = 2;
    DebugSessionLinux::UuidData zebinModuleUuidData = {
        .handle = zebinModuleUUID,
        .classHandle = zebinModuleClassHandle,
        .classIndex = NEO::DrmResourceClass::L0ZebinModule,
        .data = std::make_unique<char[]>(sizeof(kernelCount)),
        .dataSize = sizeof(kernelCount)};

    memcpy(zebinModuleUuidData.data.get(), &kernelCount, sizeof(kernelCount));

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[zebinModuleClassHandle] = {"L0_ZEBIN_MODULE", static_cast<uint32_t>(NEO::DrmResourceClass::L0ZebinModule)};
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(zebinModuleUUID, std::move(zebinModuleUuidData));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount = 2;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr = 0x1000;

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    // No ACK flag - vm bind called before debugger attached
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 20;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 4;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[4];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);
    uuidsTemp[3] = static_cast<typeOfUUID>(zebinModuleUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));
    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(0u, session->apiEvents.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[0].size());

    // event not pushed to ack
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0][isaGpuVa]->ackEvents.size());
    EXPECT_EQ(0, handler->ioctlCalled);
    EXPECT_EQ(0u, handler->debugEventAcked.seqno);

    vmBindIsa->va_start = isaGpuVa2;
    vmBindIsa->base.seqno = 21;

    handler->ioctlCalled = 0;
    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, session->apiEvents.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.size());
    EXPECT_EQ(2u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[0].size());
    EXPECT_EQ(2u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount);

    // event not pushed to ack
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0][isaGpuVa]->ackEvents.size());
    EXPECT_EQ(0, handler->ioctlCalled);
    EXPECT_EQ(0u, handler->debugEventAcked.seqno); // Not acked

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];
    EXPECT_EQ(2u, isaMap.size());

    EXPECT_TRUE(isaMap[isaGpuVa2]->moduleLoadEventAck);
    EXPECT_TRUE(isaMap[isaGpuVa]->moduleLoadEventAck);

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(session->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(0u, event.flags & ZET_DEBUG_EVENT_FLAG_NEED_ACK);

    EXPECT_EQ(isaGpuVa2, event.info.module.load);

    auto elfAddress = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr;
    auto elfSize = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].dataSize;
    EXPECT_EQ(elfAddress, event.info.module.moduleBegin);
    EXPECT_EQ(elfAddress + elfSize, event.info.module.moduleEnd);

    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    vmBindIsa->va_start = isaGpuVa2;

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(0u, session->apiEvents.size());

    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    vmBindIsa->va_start = isaGpuVa;

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, session->apiEvents.size());

    memset(&event, 0, sizeof(zet_debug_event_t));

    result = zetDebugReadEvent(session->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD, event.type);
    EXPECT_EQ(isaGpuVa2, event.info.module.load);
    EXPECT_EQ(0u, event.flags & ZET_DEBUG_EVENT_FLAG_NEED_ACK);

    EXPECT_EQ(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr, event.info.module.moduleBegin);
    EXPECT_EQ(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr +
                  session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].dataSize,
              event.info.module.moduleEnd);
}

TEST_F(DebugApiLinuxVmBindTest, GivenMultipleSegmentsInL0ZebinModuleWhenLoadAddressCountIsNotEqualSegmentsCountThenModuleLoadEventIsNotTriggered) {
    const uint64_t isaGpuVa = 0x345000;
    const uint64_t dataGpuVa = isaGpuVa; // the same load address used, error condition, no module load event triggered
    const uint64_t isaSize = 0x2000;
    const uint64_t dataSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    const uint32_t segmentCount = 2;
    DebugSessionLinux::UuidData zebinModuleUuidData = {
        .handle = zebinModuleUUID,
        .classHandle = zebinModuleClassHandle,
        .classIndex = NEO::DrmResourceClass::L0ZebinModule,
        .data = std::make_unique<char[]>(sizeof(segmentCount)),
        .dataSize = sizeof(segmentCount)};

    memcpy(zebinModuleUuidData.data.get(), &segmentCount, sizeof(segmentCount));

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[zebinModuleClassHandle] = {"L0_ZEBIN_MODULE", static_cast<uint32_t>(NEO::DrmResourceClass::L0ZebinModule)};
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(zebinModuleUUID, std::move(zebinModuleUuidData));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount = segmentCount;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr = 0x1000;

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 4;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[4];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);
    uuidsTemp[3] = static_cast<typeOfUUID>(zebinModuleUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));
    session->handleEvent(&vmBindIsa->base);

    vmBindIsa->num_uuids = 1;
    vmBindIsa->va_start = dataGpuVa;
    vmBindIsa->va_length = dataSize;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 1 * sizeof(typeOfUUID);
    uuidsTemp[0] = static_cast<typeOfUUID>(zebinModuleUUID);

    memcpy(uuids, uuidsTemp, sizeof(typeOfUUID));

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(0u, session->apiEvents.size());
}

TEST_F(DebugApiLinuxVmBindTest, GivenMultipleBindEventsWithZebinModuleWhenHandlingEventsThenModuleLoadIsReportedOnceOnly) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    const uint32_t kernelCount = 2;
    DebugSessionLinux::UuidData zebinModuleUuidData = {
        .handle = zebinModuleUUID,
        .classHandle = zebinModuleClassHandle,
        .classIndex = NEO::DrmResourceClass::L0ZebinModule,
        .data = std::make_unique<char[]>(sizeof(kernelCount)),
        .dataSize = sizeof(kernelCount)};

    memcpy(zebinModuleUuidData.data.get(), &kernelCount, sizeof(kernelCount));

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[zebinModuleClassHandle] = {"L0_ZEBIN_MODULE", static_cast<uint32_t>(NEO::DrmResourceClass::L0ZebinModule)};
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(zebinModuleUUID, std::move(zebinModuleUuidData));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount = 1;

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vmHandleForVmBind;
    vmBindIsa->num_uuids = 4;
    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
    typeOfUUID uuidsTemp[4];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
    uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);
    uuidsTemp[3] = static_cast<typeOfUUID>(zebinModuleUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, session->apiEvents.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[0].size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount);

    vmBindIsa->vm_handle = vmHandleForVmBind + 1000;
    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, session->apiEvents.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[0].size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount);

    auto &isaMap = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[0];
    EXPECT_EQ(1u, isaMap.size());

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(session->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(isaGpuVa, event.info.module.load);
}

TEST_F(DebugApiLinuxTest, GivenVmEventWithCreateAndDestroyFlagsWhenHandlingEventThenVmIdIsStoredAndErased) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    prelim_drm_i915_debug_event_vm vmEvent = {};
    vmEvent.base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM;
    vmEvent.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmEvent.base.size = sizeof(prelim_drm_i915_debug_event_vm);
    vmEvent.client_handle = MockDebugSessionLinux::mockClientHandle;
    vmEvent.handle = 4;

    session->handleEvent(&vmEvent.base);
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmIds.size());
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmIds.count(4));

    vmEvent.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    session->handleEvent(&vmEvent.base);
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmIds.size());
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmIds.count(4));
}

TEST_F(DebugApiLinuxTest, GivenNonClassUuidEventWithoutPayloadWhenHandlingEventThenReadEventIoctlIsNotCalledAndUuidIsSaved) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuid.handle = 2;
    uuid.payload_size = 0;
    uuid.class_handle = 50;

    const char uuidString[] = "00000000-0000-0000-0000-000000000001";

    prelim_drm_i915_debug_read_uuid readUuid = {};
    readUuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    memcpy(readUuid.uuid, uuidString, 36);
    readUuid.payload_ptr = 0;
    readUuid.payload_size = 0;
    readUuid.handle = 2;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    handler->returnUuid = &readUuid;

    session->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(&uuid));

    EXPECT_EQ(50u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuid.handle].classHandle);
    EXPECT_EQ(uuid.handle, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuid.handle].handle);
    EXPECT_EQ(uuid.class_handle, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuid.handle].classHandle);
    EXPECT_EQ(nullptr, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuid.handle].data);
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[uuid.handle].dataSize);

    EXPECT_EQ(0, handler->ioctlCalled);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerLogsAndFailingReadUuidEventIoctlWhenHandlingEventThenErrorIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_ERROR);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto uuidName = NEO::classNamesToUuid[0].first;
    auto uuidHash = NEO::classNamesToUuid[0].second;
    auto uuidNameSize = strlen(uuidName);
    prelim_drm_i915_debug_event_uuid uuid = {};

    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);

    uuid.client_handle = 1;
    uuid.handle = 2;
    uuid.payload_size = uuidNameSize;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    ::testing::internal::CaptureStderr();
    errno = 0;
    session->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(&uuid));

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(0u, session->getClassHandleToIndex().size());

    auto errorMessage = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string("\nERROR: PRELIM_I915_DEBUG_IOCTL_READ_UUID res = -1 errno = 0\n"), errorMessage);
}

TEST_F(DebugApiLinuxTest, GivenEventsAvailableWhenReadingEventThenEventsAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    zet_debug_event_t debugEvent = {};
    debugEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY;
    session->apiEvents.push(debugEvent);

    zet_debug_event_t debugEvent2 = {};
    debugEvent2.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
    debugEvent2.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
    debugEvent2.info.module.moduleBegin = 1000;
    debugEvent2.info.module.moduleEnd = 2000;
    debugEvent2.info.module.load = 0x1234000;
    session->apiEvents.push(debugEvent2);

    zet_debug_event_t debugEvent3 = {};
    debugEvent3.type = ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT;
    session->apiEvents.push(debugEvent3);

    zet_debug_event_t outputEvent = {};

    auto result = session->readEvent(10, &outputEvent);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY, outputEvent.type);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = session->readEvent(10, &outputEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, outputEvent.type);
    EXPECT_EQ(ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF, outputEvent.info.module.format);
    EXPECT_EQ(1000u, outputEvent.info.module.moduleBegin);
    EXPECT_EQ(0x1234000u, outputEvent.info.module.load);

    result = session->readEvent(10, &outputEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT, outputEvent.type);
}

TEST_F(DebugApiLinuxTest, GivenContextParamEventWhenTypeIsParamVmThenVmIdIsStoredForContext) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    uint64_t vmId = 10;
    uint64_t contextHandle = 20;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].handle = contextHandle;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmIds.insert(vmId);

    prelim_drm_i915_debug_event_context_param contextParamEvent = {};
    contextParamEvent.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT_PARAM;
    contextParamEvent.base.flags = 0;
    contextParamEvent.base.size = sizeof(prelim_drm_i915_debug_event_context_param);
    contextParamEvent.client_handle = MockDebugSessionLinux::mockClientHandle;
    contextParamEvent.ctx_handle = contextHandle;
    contextParamEvent.param = {.ctx_id = 3, .size = 8, .param = I915_CONTEXT_PARAM_VM, .value = vmId};

    session->handleEvent(&contextParamEvent.base);
    EXPECT_EQ(vmId, session->clientHandleToConnection[contextParamEvent.client_handle]->contextsCreated[contextHandle].vm);
}

TEST_F(DebugApiLinuxTest, GivenContextParamEventWhenTypeIsParamEngineThenEventIsHandled) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_INFO);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    uint64_t vmId = 10;
    uint32_t contextHandle = 20;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].handle = contextHandle;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].vm = vmId;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmIds.insert(vmId);

    constexpr auto size = sizeof(prelim_drm_i915_debug_event_context_param) + sizeof(i915_context_param_engines) + sizeof(i915_engine_class_instance);

    auto memory = alignedMalloc(size, sizeof(prelim_drm_i915_debug_event_context_param));
    auto *contextParamEvent = static_cast<prelim_drm_i915_debug_event_context_param *>(memory);

    {
        contextParamEvent->base.type = PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT_PARAM;
        contextParamEvent->base.flags = 0;
        contextParamEvent->base.size = size;
        contextParamEvent->client_handle = MockDebugSessionLinux::mockClientHandle;
        contextParamEvent->ctx_handle = contextHandle;
    }

    auto offset = offsetof(prelim_drm_i915_debug_event_context_param, param);

    GemContextParam paramToCopy = {};
    paramToCopy.contextId = contextHandle;
    paramToCopy.size = sizeof(i915_context_param_engines) + sizeof(i915_engine_class_instance);
    paramToCopy.param = I915_CONTEXT_PARAM_ENGINES;
    paramToCopy.value = 0;
    memcpy(ptrOffset(memory, offset), &paramToCopy, sizeof(GemContextParam));

    auto valueOffset = offsetof(GemContextParam, value);
    auto *engines = ptrOffset(memory, offset + valueOffset);
    i915_context_param_engines enginesParam;
    enginesParam.extensions = 0;
    memcpy(engines, &enginesParam, sizeof(i915_context_param_engines));

    auto enginesOffset = offsetof(i915_context_param_engines, engines);
    auto *classInstance = ptrOffset(memory, offset + valueOffset + enginesOffset);
    i915_engine_class_instance ci = {drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER, 1};
    memcpy(classInstance, &ci, sizeof(i915_engine_class_instance));

    ::testing::internal::CaptureStdout();
    session->handleEvent(&contextParamEvent->base);
    alignedFree(memory);

    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].engines.size());
    EXPECT_EQ(static_cast<uint32_t>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER), session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].engines[0].engine_class);
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].engines[0].engine_instance);

    EXPECT_NE(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToTile.end(),
              session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToTile.find(vmId));
    EXPECT_EQ(0u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToTile[vmId]);

    auto infoMessage = ::testing::internal::GetCapturedStdout();
    EXPECT_TRUE(hasSubstr(infoMessage, std::string("I915_CONTEXT_PARAM_ENGINES ctx_id = 20 param = 10 value = 0")));
}

TEST_F(DebugApiLinuxTest, GivenNoVmIdWhenOrZeroEnginesContextParamEventIsHandledThenVmToTileIsNotSet) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    uint64_t vmId = 10;
    uint32_t contextHandle = 20;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].handle = contextHandle;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].vm = MockDebugSessionLinux::invalidHandle;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmIds.insert(vmId);

    constexpr auto size = sizeof(prelim_drm_i915_debug_event_context_param) + sizeof(i915_context_param_engines) + sizeof(i915_engine_class_instance);

    auto memory = alignedMalloc(size, sizeof(prelim_drm_i915_debug_event_context_param));
    auto *contextParamEvent = static_cast<prelim_drm_i915_debug_event_context_param *>(memory);

    {
        contextParamEvent->base.type = PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT_PARAM;
        contextParamEvent->base.flags = 0;
        contextParamEvent->base.size = size;
        contextParamEvent->client_handle = MockDebugSessionLinux::mockClientHandle;
        contextParamEvent->ctx_handle = contextHandle;
    }

    auto offset = offsetof(prelim_drm_i915_debug_event_context_param, param);

    GemContextParam paramToCopy = {};
    paramToCopy.contextId = contextHandle;
    paramToCopy.size = sizeof(i915_context_param_engines) + sizeof(i915_engine_class_instance);
    paramToCopy.param = I915_CONTEXT_PARAM_ENGINES;
    paramToCopy.value = 0;
    memcpy(ptrOffset(memory, offset), &paramToCopy, sizeof(GemContextParam));

    auto valueOffset = offsetof(GemContextParam, value);
    auto *engines = ptrOffset(memory, offset + valueOffset);
    i915_context_param_engines enginesParam;
    enginesParam.extensions = 0;
    memcpy(engines, &enginesParam, sizeof(i915_context_param_engines));

    auto enginesOffset = offsetof(i915_context_param_engines, engines);
    auto *classInstance = ptrOffset(memory, offset + valueOffset + enginesOffset);
    i915_engine_class_instance ci = {drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER, 1};
    memcpy(classInstance, &ci, sizeof(i915_engine_class_instance));

    session->handleEvent(&contextParamEvent->base);

    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].engines.size());
    EXPECT_EQ(static_cast<uint32_t>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER), session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].engines[0].engine_class);
    EXPECT_EQ(1u, session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].engines[0].engine_instance);

    EXPECT_EQ(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToTile.end(),
              session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToTile.find(MockDebugSessionLinux::invalidHandle));

    paramToCopy.size = sizeof(i915_context_param_engines);
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].vm = vmId;
    memcpy(ptrOffset(memory, offset), &paramToCopy, sizeof(GemContextParam));

    session->handleEvent(&contextParamEvent->base);

    EXPECT_EQ(session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToTile.end(),
              session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToTile.find(vmId));

    alignedFree(memory);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerErrorLogsWhenContextParamWithInvalidContextIsHandledThenErrorIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_ERROR);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    uint64_t vmId = 10;
    uint64_t contextHandle = 20;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].handle = contextHandle;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmIds.insert(vmId);

    prelim_drm_i915_debug_event_context_param contextParamEvent = {};
    contextParamEvent.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT_PARAM;
    contextParamEvent.base.flags = 0;
    contextParamEvent.base.size = sizeof(prelim_drm_i915_debug_event_context_param);
    contextParamEvent.client_handle = MockDebugSessionLinux::mockClientHandle;
    contextParamEvent.ctx_handle = 77;
    contextParamEvent.param = {.ctx_id = 3, .size = 8, .param = I915_CONTEXT_PARAM_VM, .value = vmId};

    ::testing::internal::CaptureStderr();
    session->handleEvent(&contextParamEvent.base);

    EXPECT_EQ(session->clientHandleToConnection[contextParamEvent.client_handle]->contextsCreated.end(),
              session->clientHandleToConnection[contextParamEvent.client_handle]->contextsCreated.find(77));

    auto errorMessage = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string("\nERROR: CONTEXT handle does not exist\n"), errorMessage);
}

TEST_F(DebugApiLinuxTest, GivenDebuggerInfoLogsWhenHandlingContextParamEventWithUnknownParamThenInfoIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_INFO);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinux::invalidClientHandle;

    uint64_t vmId = 10;
    uint64_t contextHandle = 20;

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[contextHandle].handle = contextHandle;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmIds.insert(vmId);

    prelim_drm_i915_debug_event_context_param contextParamEvent = {};
    contextParamEvent.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT_PARAM;
    contextParamEvent.base.flags = 0;
    contextParamEvent.base.size = sizeof(prelim_drm_i915_debug_event_context_param);
    contextParamEvent.client_handle = MockDebugSessionLinux::mockClientHandle;
    contextParamEvent.ctx_handle = contextHandle;
    contextParamEvent.param = {.ctx_id = 3, .size = 8, .param = I915_CONTEXT_PARAM_BAN_PERIOD, .value = vmId};

    ::testing::internal::CaptureStdout();
    session->handleEvent(&contextParamEvent.base);

    EXPECT_EQ(session->clientHandleToConnection[contextParamEvent.client_handle]->contextsCreated.end(),
              session->clientHandleToConnection[contextParamEvent.client_handle]->contextsCreated.find(77));

    auto errorMessage = ::testing::internal::GetCapturedStdout();
    EXPECT_TRUE(hasSubstr(errorMessage, std::string("client_handle = 1 ctx_handle = 20\n\nINFO: I915_CONTEXT_PARAM UNHANDLED = 1\n")));
}

TEST_F(DebugApiLinuxTest, WhenCallingThreadControlForInterruptThenProperIoctlsIsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);
    std::vector<EuThread::ThreadId> threads({});

    std::unique_ptr<uint8_t[]> bitmaskOut;
    size_t bitmaskSizeOut = 0;

    auto result = sessionMock->threadControl(threads, 0, MockDebugSessionLinux::ThreadControlCmd::Interrupt, bitmaskOut, bitmaskSizeOut);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(uint32_t(PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT), handler->euControl.cmd);
    EXPECT_NE(0u, handler->euControl.bitmask_size);
    EXPECT_NE(0u, handler->euControl.bitmask_ptr);
    EXPECT_EQ(handler->euControlOutputSeqno, sessionMock->euControlInterruptSeqno[0]);

    EXPECT_EQ(0u, bitmaskSizeOut);
    EXPECT_EQ(nullptr, bitmaskOut.get());

    handler->euControlOutputSeqno = handler->euControlOutputSeqno + 3;
    handler->ioctlCalled = 0;
    result = sessionMock->threadControl(threads, 0, MockDebugSessionLinux::ThreadControlCmd::InterruptAll, bitmaskOut, bitmaskSizeOut);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(uint32_t(PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT_ALL), handler->euControl.cmd);
    EXPECT_EQ(0u, handler->euControl.bitmask_size);
    EXPECT_EQ(0u, handler->euControl.bitmask_ptr);
    EXPECT_EQ(handler->euControlOutputSeqno, sessionMock->euControlInterruptSeqno[0]);

    EXPECT_EQ(0u, bitmaskSizeOut);
    EXPECT_EQ(nullptr, bitmaskOut.get());
}

TEST_F(DebugApiLinuxTest, GivenErrorFromIoctlWhenCallingThreadControlForInterruptThenSeqnoIsNotUpdated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);
    std::vector<EuThread::ThreadId> threads({});

    std::unique_ptr<uint8_t[]> bitmaskOut;
    size_t bitmaskSizeOut = 0;

    handler->ioctlRetVal = -1;

    auto result = sessionMock->threadControl(threads, 0, MockDebugSessionLinux::ThreadControlCmd::InterruptAll, bitmaskOut, bitmaskSizeOut);
    EXPECT_NE(0, result);

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(uint32_t(PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT_ALL), handler->euControl.cmd);
    EXPECT_NE(handler->euControlOutputSeqno, sessionMock->euControlInterruptSeqno[0]);
}

TEST_F(DebugApiLinuxTest, WhenCallingThreadControlForResumeThenProperIoctlsIsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);
    std::vector<EuThread::ThreadId> threads({});

    std::unique_ptr<uint8_t[]> bitmaskOut;
    size_t bitmaskSizeOut = 0;

    auto result = sessionMock->threadControl(threads, 0, MockDebugSessionLinux::ThreadControlCmd::Resume, bitmaskOut, bitmaskSizeOut);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(uint32_t(PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME), handler->euControl.cmd);
    EXPECT_NE(0u, handler->euControl.bitmask_size);
    EXPECT_NE(0u, handler->euControl.bitmask_ptr);

    EXPECT_EQ(0u, bitmaskSizeOut);
    EXPECT_EQ(nullptr, bitmaskOut.get());
}

TEST_F(DebugApiLinuxTest, GivenResumeWARequiredWhenCallingResumeThenWaIsAppliedToBitmask) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto &l0HwHelper = L0HwHelper::get(neoDevice->getHardwareInfo().platform.eRenderCoreFamily);
    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    SIP::version version = {2, 0, 0};
    initStateSaveArea(sessionMock->stateSaveAreaHeader, version);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);
    ze_device_thread_t thread = {0, 0, 0, 0};
    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(uint32_t(PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME), handler->euControl.cmd);
    EXPECT_NE(0u, handler->euControl.bitmask_size);
    EXPECT_NE(0u, handler->euControl.bitmask_ptr);

    auto bitmask = handler->euControlBitmask.get();

    EXPECT_EQ(1u, bitmask[0]);
    if (l0HwHelper.isResumeWARequired()) {
        EXPECT_EQ(1u, bitmask[4]);
    } else {
        EXPECT_EQ(0u, bitmask[4]);
    }

    thread = {0, 0, 4, 0};
    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->stopThread(1u);

    result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    bitmask = handler->euControlBitmask.get();

    if (l0HwHelper.isResumeWARequired()) {
        EXPECT_EQ(1u, bitmask[0]);
        EXPECT_EQ(1u, bitmask[4]);
    } else {
        EXPECT_EQ(0u, bitmask[0]);
        EXPECT_EQ(1u, bitmask[4]);
    }
}

TEST_F(DebugApiLinuxTest, GivenSliceALLWhenCallingResumeThenSliceIdIsNotRemapped) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);
    ze_device_thread_t thread = {};
    thread.slice = UINT32_MAX;

    for (uint32_t i = 0; i < device->getHwInfo().gtSystemInfo.MaxSlicesSupported; i++) {
        ze_device_thread_t singleThread = thread;
        singleThread.slice = i;
        sessionMock->allThreads[EuThread::ThreadId(0, singleThread)]->stopThread(1u);
    }

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(uint32_t(PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME), handler->euControl.cmd);

    auto sliceCount = mockDrm->getTopologyMap().at(0).sliceIndices.size();
    EXPECT_EQ(sliceCount, sessionMock->numThreadsPassedToThreadControl);
}

TEST_F(DebugApiLinuxTest, GivenErrorFromIoctlWhenCallingResumeThenErrorUnknownIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);
    handler->ioctlRetVal = -1;

    ze_device_thread_t thread = {};
    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(DebugApiLinuxTest, GivenErrorFromIoctlWhenCallingInterruptImpThenErrorNotAvailableIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);
    handler->ioctlRetVal = -1;

    auto result = sessionMock->interruptImp(0);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

TEST_F(DebugApiLinuxTest, WhenCallingThreadControlForThreadStoppedThenProperIoctlsIsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);

    std::vector<EuThread::ThreadId> threads({});

    std::unique_ptr<uint8_t[]> bitmaskOut;
    size_t bitmaskSizeOut = 0;

    auto result = sessionMock->threadControl(threads, 0, MockDebugSessionLinux::ThreadControlCmd::Stopped, bitmaskOut, bitmaskSizeOut);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(uint32_t(PRELIM_I915_DEBUG_EU_THREADS_CMD_STOPPED), handler->euControl.cmd);

    EXPECT_NE(0u, bitmaskSizeOut);
    EXPECT_NE(nullptr, bitmaskOut.get());
}

TEST_F(DebugApiLinuxTest, givenEnginesEventHandledThenLrcToContextHandleMapIsFilled) {
    DebugManagerStateRestore restorer;

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t clientHandle = 34;
    prelim_drm_i915_debug_event_client client = {};
    client.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    client.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    client.base.size = sizeof(prelim_drm_i915_debug_event_client);
    client.handle = clientHandle;
    session->handleEvent(&client.base);

    unsigned char bytes1[sizeof(prelim_drm_i915_debug_event_engines) + 2 * sizeof(prelim_drm_i915_debug_engine_info)];
    prelim_drm_i915_debug_event_engines *engines1 = reinterpret_cast<prelim_drm_i915_debug_event_engines *>(bytes1);
    engines1->base.type = PRELIM_DRM_I915_DEBUG_EVENT_ENGINES;
    engines1->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    engines1->client_handle = clientHandle;
    engines1->ctx_handle = 20;
    engines1->num_engines = 2;
    engines1->engines[0].lrc_handle = 1;
    engines1->engines[1].lrc_handle = 2;

    unsigned char bytes2[sizeof(prelim_drm_i915_debug_event_engines) + 4 * sizeof(prelim_drm_i915_debug_engine_info)];
    prelim_drm_i915_debug_event_engines *engines2 = reinterpret_cast<prelim_drm_i915_debug_event_engines *>(bytes2);
    engines2->base.type = PRELIM_DRM_I915_DEBUG_EVENT_ENGINES;
    engines2->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    engines2->client_handle = clientHandle;
    engines2->ctx_handle = 40;
    engines2->num_engines = 4;
    engines2->engines[0].lrc_handle = 3;
    engines2->engines[1].lrc_handle = 4;
    engines2->engines[2].lrc_handle = 5;
    engines2->engines[3].lrc_handle = 0;

    ::testing::internal::CaptureStdout();
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_INFO);

    session->handleEvent(&engines1->base);
    session->handleEvent(&engines2->base);

    EXPECT_EQ(6u, session->clientHandleToConnection[clientHandle]->lrcToContextHandle.size());
    EXPECT_EQ(20u, session->clientHandleToConnection[clientHandle]->lrcToContextHandle[1]);
    EXPECT_EQ(20u, session->clientHandleToConnection[clientHandle]->lrcToContextHandle[2]);
    EXPECT_EQ(40u, session->clientHandleToConnection[clientHandle]->lrcToContextHandle[3]);
    EXPECT_EQ(40u, session->clientHandleToConnection[clientHandle]->lrcToContextHandle[4]);
    EXPECT_EQ(40u, session->clientHandleToConnection[clientHandle]->lrcToContextHandle[5]);

    engines1->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    session->handleEvent(&engines1->base);
    EXPECT_EQ(4u, session->clientHandleToConnection[clientHandle]->lrcToContextHandle.size());
    EXPECT_EQ(session->clientHandleToConnection[clientHandle]->lrcToContextHandle.find(1), session->clientHandleToConnection[clientHandle]->lrcToContextHandle.end());
    EXPECT_EQ(session->clientHandleToConnection[clientHandle]->lrcToContextHandle.find(2), session->clientHandleToConnection[clientHandle]->lrcToContextHandle.end());
    EXPECT_EQ(40u, session->clientHandleToConnection[clientHandle]->lrcToContextHandle[3]);
    EXPECT_EQ(40u, session->clientHandleToConnection[clientHandle]->lrcToContextHandle[4]);
    EXPECT_EQ(40u, session->clientHandleToConnection[clientHandle]->lrcToContextHandle[5]);

    auto infoMessage = ::testing::internal::GetCapturedStdout();
    EXPECT_TRUE(hasSubstr(infoMessage, std::string("ENGINES event: client_handle = 34, ctx_handle = 20, num_engines = 2 CREATE")));
    EXPECT_TRUE(hasSubstr(infoMessage, std::string("ENGINES event: client_handle = 34, ctx_handle = 40, num_engines = 4 CREATE")));
    EXPECT_TRUE(hasSubstr(infoMessage, std::string("ENGINES event: client_handle = 34, ctx_handle = 20, num_engines = 2 DESTROY")));
}

TEST_F(DebugApiLinuxTest, givenTileAttachEnabledWhenDeviceDoesNotHaveTilesThenTileSessionsAreNotEnabled) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    session->createTileSessionsIfEnabled();
    EXPECT_FALSE(session->tileSessionsEnabled);
    EXPECT_EQ(0u, session->tileSessions.size());
}

using DebugApiLinuxAttentionTest = Test<DebugApiLinuxFixture>;

TEST_F(DebugApiLinuxAttentionTest, GivenEuAttentionEventForThreadsWhenHandlingEventThenNewlyStoppedThreadsSaved) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;

    uint8_t data[sizeof(prelim_drm_i915_debug_event_eu_attention) + 128];
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    std::vector<EuThread::ThreadId> threads{
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 1},
        {0, 0, 0, 0, 2},
        {0, 0, 0, 0, 3},
        {0, 0, 0, 0, 4},
        {0, 0, 0, 0, 5},
        {0, 0, 0, 0, 6}};

    for (auto thread : threads) {
        sessionMock->stoppedThreads[thread.packed] = 1;
    }

    l0HwHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);

    prelim_drm_i915_debug_event_eu_attention attention = {};
    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention) + std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));
    attention.client_handle = MockDebugSessionLinux::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.ci.engine_class = 0;
    attention.ci.engine_instance = 0;
    attention.bitmask_size = std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));

    memcpy(data, &attention, sizeof(prelim_drm_i915_debug_event_eu_attention));
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_eu_attention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    sessionMock->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));

    EXPECT_EQ(threads.size(), sessionMock->newlyStoppedThreads.size());
}

TEST_F(DebugApiLinuxAttentionTest, GivenEuAttentionEventWithInvalidClientWhenHandlingEventThenNoStoppedThreadsSet) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint8_t data[sizeof(prelim_drm_i915_debug_event_eu_attention) + 128];
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    std::vector<EuThread::ThreadId> threads{
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 1},
        {0, 0, 0, 0, 2},
        {0, 0, 0, 0, 3},
        {0, 0, 0, 0, 4},
        {0, 0, 0, 0, 5},
        {0, 0, 0, 0, 6}};

    l0HwHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);

    prelim_drm_i915_debug_event_eu_attention attention = {};

    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention) + std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));

    attention.client_handle = MockDebugSessionLinux::invalidClientHandle;
    attention.flags = 0;
    attention.ci.engine_class = 0;
    attention.ci.engine_instance = 0;
    attention.bitmask_size = std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));

    memcpy(data, &attention, sizeof(prelim_drm_i915_debug_event_eu_attention));
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_eu_attention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    sessionMock->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
}

TEST_F(DebugApiLinuxAttentionTest, GivenEuAttentionEventEmptyBitmaskWhenHandlingEventThenNoStoppedThreadsSetAndTriggerEventsFalse) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;

    uint8_t data[sizeof(prelim_drm_i915_debug_event_eu_attention) + sizeof(uint64_t)];
    prelim_drm_i915_debug_event_eu_attention attention = {};

    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention) + sizeof(uint64_t);
    attention.client_handle = MockDebugSessionLinux::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.ci.engine_class = 0;
    attention.ci.engine_instance = 0;
    attention.bitmask_size = sizeof(uint64_t);

    uint64_t bitmask = 0;

    memcpy(data, &attention, sizeof(prelim_drm_i915_debug_event_eu_attention));
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_eu_attention, bitmask)), &bitmask, sizeof(uint64_t));

    sessionMock->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
}

TEST_F(DebugApiLinuxAttentionTest, GivenInterruptedThreadsWhenOnlySomeThreadsRaisesAttentionThenPendingInterruptsAreMarked) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;

    uint8_t data[sizeof(prelim_drm_i915_debug_event_eu_attention) + 128];
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    std::vector<EuThread::ThreadId> threads{
        {0, 0, 0, 0, 0}};

    sessionMock->stoppedThreads[threads[0].packed] = 1;

    if (hwInfo.gtSystemInfo.MaxEuPerSubSlice > 8) {
        sessionMock->allThreads[EuThread::ThreadId(0, 0, 0, 4, 0)]->stopThread(1u);
    }

    l0HwHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};
    ze_device_thread_t thread2 = {0, 0, 1, UINT32_MAX};
    sessionMock->pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread, false));
    sessionMock->pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread2, false));
    sessionMock->interruptSent = true;
    sessionMock->euControlInterruptSeqno[0] = 1;

    prelim_drm_i915_debug_event_eu_attention attention = {};
    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention) + std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinux::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.ci.engine_class = 0;
    attention.ci.engine_instance = 0;
    attention.bitmask_size = std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));

    memcpy(data, &attention, sizeof(prelim_drm_i915_debug_event_eu_attention));
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_eu_attention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    sessionMock->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_TRUE(sessionMock->pendingInterrupts[0].second);
    EXPECT_FALSE(sessionMock->pendingInterrupts[1].second);
}

TEST_F(DebugApiLinuxAttentionTest, GivenInterruptedThreadsWhenAttentionEventReceivedThenEventsTriggeredAfterExpectedAttentionEventCount) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;

    uint8_t data[sizeof(prelim_drm_i915_debug_event_eu_attention) + 128];
    ze_device_thread_t thread{0, 0, 0, 0};

    sessionMock->stoppedThreads[EuThread::ThreadId(0, thread).packed] = 1;
    sessionMock->pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread, false));

    sessionMock->interruptSent = true;
    sessionMock->euControlInterruptSeqno[0] = 1;

    prelim_drm_i915_debug_event_eu_attention attention = {};
    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention);
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinux::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.ci.engine_class = 0;
    attention.ci.engine_instance = 0;
    attention.bitmask_size = 0;

    memcpy(data, &attention, sizeof(prelim_drm_i915_debug_event_eu_attention));

    sessionMock->expectedAttentionEvents = 2;

    sessionMock->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));

    EXPECT_FALSE(sessionMock->triggerEvents);

    attention.base.seqno = 10;
    memcpy(data, &attention, sizeof(prelim_drm_i915_debug_event_eu_attention));
    sessionMock->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));

    EXPECT_TRUE(sessionMock->triggerEvents);
}

TEST_F(DebugApiLinuxAttentionTest, GivenEventSeqnoLowerEqualThanSentInterruptWhenHandlingAttentionEventThenEventIsNotProcessed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint8_t data[sizeof(prelim_drm_i915_debug_event_eu_attention) + 128];
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    std::vector<EuThread::ThreadId> threads{
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 1},
        {0, 0, 0, 0, 2},
        {0, 0, 0, 0, 3},
        {0, 0, 0, 0, 4},
        {0, 0, 0, 0, 5},
        {0, 0, 0, 0, 6}};

    l0HwHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};
    sessionMock->pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread, false));
    sessionMock->interruptSent = true;
    sessionMock->euControlInterruptSeqno[0] = 3;

    prelim_drm_i915_debug_event_eu_attention attention = {};
    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    attention.base.seqno = 3;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention) + std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));
    attention.client_handle = MockDebugSessionLinux::mockClientHandle;
    attention.flags = 0;
    attention.ci.engine_class = 0;
    attention.ci.engine_instance = 0;
    attention.bitmask_size = std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));

    memcpy(data, &attention, sizeof(prelim_drm_i915_debug_event_eu_attention));
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_eu_attention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    sessionMock->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_EQ(1u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->pendingInterrupts[0].second);
    EXPECT_FALSE(sessionMock->triggerEvents);
}

TEST_F(DebugApiLinuxAttentionTest, GivenLrcToContextMapEmptyWhenHandlingAttentionEventThenEventIsNotProcessed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;

    prelim_drm_i915_debug_event_eu_attention attention = {};
    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention);
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinux::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.ci.engine_class = 0;
    attention.ci.engine_instance = 0;
    attention.bitmask_size = 0;

    sessionMock->handleEvent(&attention.base);

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
}

TEST_F(DebugApiLinuxAttentionTest, GivenContextHandleNotFoundWhenHandlingAttentionEventThenEventIsNotProcessed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t ctxHandle = 2;
    uint64_t lrcHandle = 8;

    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;

    prelim_drm_i915_debug_event_eu_attention attention = {};
    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention);
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinux::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.ci.engine_class = 0;
    attention.ci.engine_instance = 0;
    attention.bitmask_size = 0;

    sessionMock->handleEvent(&attention.base);

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
}

TEST_F(DebugApiLinuxAttentionTest, GivenInvalidVmHandleWhenHandlingAttentionEventThenEventIsNotProcessed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = DebugSessionLinux::invalidHandle;
    uint64_t lrcHandle = 8;

    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    sessionMock->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;

    prelim_drm_i915_debug_event_eu_attention attention = {};
    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention);
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinux::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.ci.engine_class = 0;
    attention.ci.engine_instance = 0;
    attention.bitmask_size = 0;

    sessionMock->handleEvent(&attention.base);

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
}
using DebugApiLinuxAsyncThreadTest = Test<DebugApiLinuxFixture>;

TEST_F(DebugApiLinuxAsyncThreadTest, GivenPollReturnsErrorAndEinvalWhenReadingInternalEventsAsyncThenDetachEventIsGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    handler->pollRetVal = -1;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    errno = EINVAL;
    session->readInternalEventsAsync();

    EXPECT_EQ(1u, session->apiEvents.size());
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_DETACHED, session->apiEvents.front().type);
    session->apiEvents.pop();
    errno = 0;

    session->readInternalEventsAsync();
    EXPECT_EQ(0u, session->apiEvents.size());
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenPollReturnsErrorAndEBusyWhenReadingInternalEventsAsyncThenDetachEventIsNotGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    handler->pollRetVal = -1;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    errno = EBUSY;
    session->readInternalEventsAsync();

    EXPECT_EQ(0u, session->apiEvents.size());
    errno = 0;
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenPollReturnsZeroWhenReadingInternalEventsAsyncThenNoEventIsReadAndGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    handler->pollRetVal = 0;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    session->readInternalEventsAsync();
    EXPECT_EQ(0, handler->ioctlCalled);
    EXPECT_EQ(0u, handler->debugEventInput.type);

    EXPECT_EQ(0u, session->apiEvents.size());
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenPollReturnsNonZeroWhenReadingInternalEventsAsyncThenEventReadIsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    handler->pollRetVal = 1;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;
    session->synchronousInternalEventRead = true;

    uint64_t clientHandle = 2;
    prelim_drm_i915_debug_event_client client = {};
    client.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    client.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    client.base.size = sizeof(prelim_drm_i915_debug_event_client);
    client.handle = clientHandle;

    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});

    session->readInternalEventsAsync();

    constexpr int clientEventCount = 1;
    constexpr int dummyReadEventCount = 1;

    EXPECT_EQ(clientEventCount + dummyReadEventCount, handler->ioctlCalled);
    EXPECT_EQ(DebugSessionLinux::maxEventSize, handler->debugEventInput.size);
    EXPECT_EQ(static_cast<decltype(prelim_drm_i915_debug_event::type)>(PRELIM_DRM_I915_DEBUG_EVENT_READ), handler->debugEventInput.type);
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenEventsAvailableWhenHandlingEventsAsyncThenEventIsProcessed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    handler->pollRetVal = 1;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;
    session->synchronousInternalEventRead = true;

    uint64_t clientHandle = 2;
    prelim_drm_i915_debug_event_client client = {};
    client.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    client.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    client.base.size = sizeof(prelim_drm_i915_debug_event_client);
    client.handle = clientHandle;

    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});

    EXPECT_EQ(nullptr, session->clientHandleToConnection[clientHandle]);

    session->handleEventsAsync();

    EXPECT_EQ(1u, session->handleEventCalledCount);
    EXPECT_NE(nullptr, session->clientHandleToConnection[clientHandle]);
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenPollReturnsNonZeroWhenReadingEventsAsyncThenEventReadIsCalledForAtMost3Times) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    handler->pollRetVal = 1;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    uint64_t clientHandle = 2;
    prelim_drm_i915_debug_event_client client = {};
    client.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    client.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    client.base.size = sizeof(prelim_drm_i915_debug_event_client);
    client.handle = clientHandle;

    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.size)});

    session->readInternalEventsAsync();

    EXPECT_EQ(3, handler->ioctlCalled);
    EXPECT_EQ(DebugSessionLinux::maxEventSize, handler->debugEventInput.size);
    EXPECT_EQ(static_cast<decltype(prelim_drm_i915_debug_event::type)>(PRELIM_DRM_I915_DEBUG_EVENT_READ), handler->debugEventInput.type);
    EXPECT_EQ(3u, session->internalEventQueue.size());
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenDebugSessionWhenStartingAndClosingAsyncThreadThenThreadIsStartedAndFinishes) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    session->startAsyncThread();

    while (session->getInternalEventCounter == 0)
        ;

    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThread.threadFinished);

    session->closeAsyncThread();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThread.threadFinished);
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenDebugSessionWithAsyncThreadWhenClosingConnectionThenAsyncThreadIsTerminated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    session->startAsyncThread();

    while (session->getInternalEventCounter == 0)
        ;

    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThread.threadFinished);

    session->closeConnection();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThread.threadFinished);
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenNoEventsAvailableWithinTimeoutWhenReadingEventThenNotReadyReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    zet_debug_event_t outputEvent = {};
    outputEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY;

    auto result = session->readEvent(10, &outputEvent);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_INVALID, outputEvent.type);
    EXPECT_EQ(0u, outputEvent.flags);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenInterruptedThreadsWhenNoAttentionEventIsReadThenThreadUnavailableEventIsGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->returnTimeDiff = DebugSessionLinux::interruptTimeout * 10;

    session->startAsyncThread();

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};
    auto result = session->interrupt(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    while (session->getInternalEventCounter == 0)
        ;

    session->closeAsyncThread();

    if (session->apiEvents.size() > 0) {
        auto event = session->apiEvents.front();
        EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, event.type);
        EXPECT_EQ(0u, event.info.thread.thread.slice);
        EXPECT_EQ(0u, event.info.thread.thread.subslice);
        EXPECT_EQ(0u, event.info.thread.thread.eu);
        EXPECT_EQ(UINT32_MAX, event.info.thread.thread.thread);
    }
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenInterruptedThreadsWhenNoAttentionEventIsReadWithinTimeoutThenNoEventGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinux::mockClientHandle;

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);
    session->returnTimeDiff = 0;

    session->startAsyncThread();

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};
    auto result = session->interrupt(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    while (session->getInternalEventCounter == 0)
        ;

    session->closeAsyncThread();

    EXPECT_EQ(0u, session->apiEvents.size());
}

struct DebugApiRegistersAccessFixture : public DebugApiLinuxFixture {
    void setUp() {
        hwInfo = *NEO::defaultHwInfo.get();
        hwInfo.gtSystemInfo.SubSliceCount = 6 * hwInfo.gtSystemInfo.SliceCount;

        DebugApiLinuxFixture::setUp(&hwInfo);

        mockBuiltins = new MockBuiltins();
        mockBuiltins->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltins);
        session = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{}, device, 0);
        session->clientHandle = MockDebugSessionLinux::mockClientHandle;
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        maxDbgSurfaceSize = hwHelper.getSipKernelMaxDbgSurfaceSize(hwInfo);
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle] = {stateSaveAreaGpuVa, maxDbgSurfaceSize};
        session->allThreadsStopped = true;
        session->allThreads[stoppedThreadId]->stopThread(1u);
    }

    void tearDown() {
        DebugApiLinuxFixture::tearDown();
    }
    NEO::HardwareInfo hwInfo;
    std::unique_ptr<MockDebugSessionLinux> session;
    MockBuiltins *mockBuiltins = nullptr;
    MockIoctlHandler *ioctlHandler = nullptr;
    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t stateSaveAreaGpuVa = 0x123400000000;
    size_t maxDbgSurfaceSize = 0;
    ze_device_thread_t stoppedThread = {0, 0, 0, 0};
    EuThread::ThreadId stoppedThreadId{0, stoppedThread};
};

using DebugApiRegistersAccessTest = Test<DebugApiRegistersAccessFixture>;

TEST_F(DebugApiRegistersAccessTest, givenInvalidClientHandleWhenReadRegistersCalledThenErrorIsReturned) {
    session->clientHandle = MockDebugSessionLinux::invalidClientHandle;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugApiRegistersAccessTest, givenCorruptedTssAreaWhenReadRegistersCalledThenErrorUnknownIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);
    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;

    ioctlHandler->setPreadMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);
    ioctlHandler->setPwriteMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);

    session->ioctlHandler.reset(ioctlHandler);
    session->vmHandle = 7;
    memcpy(session->stateSaveAreaHeader.data(), "garbage", 8);
    session->ensureThreadStopped({0, 0, 0, 0});

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugApiRegistersAccessTest, givenInvalidRegistersIndicesWhenReadRegistersCalledThenErrorInvalidArgumentIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 128, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 127, 2, nullptr));
}

TEST_F(DebugApiRegistersAccessTest, givenStateSaveAreaNotCapturedWhenReadRegistersIsCalledThenErrorUnknownIsReturned) {
    auto bindInfoSave = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle];
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo.clear();

    char r0[32];
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, r0));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle] = bindInfoSave;
}

TEST_F(DebugApiRegistersAccessTest, givenReadGpuMemoryFailedWhenReadRegistersCalledThenErrorUnknownIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);
    auto ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapFail = true;
    session->ioctlHandler.reset(ioctlHandler);

    char r0[32];
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, r0));
}

TEST_F(DebugApiRegistersAccessTest, givenCorruptedSrMagicWhenReadRegistersCalledThenErrorUnknownIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;
    session->ioctlHandler.reset(ioctlHandler);

    auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    strcpy(session->stateSaveAreaHeader.data() + threadSlotOffset(pStateSaveAreaHeader, 0, 0, 0, 0) + pStateSaveAreaHeader->regHeader.sr_magic_offset, "garbage"); // NOLINT(clang-analyzer-security.insecureAPI.strcpy)

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugApiRegistersAccessTest, givenReadRegistersCalledCorrectValuesRead) {
    SIP::version version = {1, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);
    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;

    ioctlHandler->setPreadMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);
    ioctlHandler->setPwriteMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);

    session->ioctlHandler.reset(ioctlHandler);
    session->vmHandle = 7;

    char grf[32] = {0};
    char grfRef[32] = {0};

    session->ensureThreadStopped({0, 0, 0, 0});
    for (uint32_t reg = 0; reg < 20; ++reg) {
        memset(grfRef, 'a' + reg, 32);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, reg, 1, grf));
        EXPECT_EQ(0, memcmp(grf, grfRef, 32));
    }

    session->ensureThreadStopped({0, 3, 7, 3});
    for (uint32_t reg = 0; reg < 20; ++reg) {
        memset(grfRef, 'a' + reg, 32);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), {0, 3, 7, 3}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, reg, 1, grf));
        EXPECT_EQ(0, memcmp(grf, grfRef, 32));
    }

    session->ensureThreadStopped({0, 5, 15, 6});
    for (uint32_t reg = 0; reg < 20; ++reg) {
        memset(grfRef, 'a' + reg, 32);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), {0, 5, 15, 6}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, reg, 1, grf));
        EXPECT_EQ(0, memcmp(grf, grfRef, 32));
    }
}

TEST_F(DebugApiRegistersAccessTest, givenInvalidClientHandleWhenWriteRegistersCalledThenErrorIsReturned) {
    session->clientHandle = MockDebugSessionLinux::invalidClientHandle;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugWriteRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugApiRegistersAccessTest, givenCorruptedTssAreaWhenWriteRegistersCalledThenErrorUnknownIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;
    session->ioctlHandler.reset(ioctlHandler);

    memcpy(session->stateSaveAreaHeader.data(), "garbage", 8);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugWriteRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugApiRegistersAccessTest, givenInvalidRegistersIndicesWhenWriteRegistersCalledThenErrorInvalidArgumentIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 128, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 127, 2, nullptr));
}

TEST_F(DebugApiRegistersAccessTest, givenStateSaveAreaNotCapturedWhenWriteRegistersIsCalledThenErrorUnknownIsReturned) {
    auto bindInfoSave = session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle];
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo.clear();

    char r0[32];
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, r0));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle] = bindInfoSave;
}

TEST_F(DebugApiRegistersAccessTest, givenWriteGpuMemoryFailedWhenWriteRegistersCalledThenErrorUnknownIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;
    session->ioctlHandler.reset(ioctlHandler);
    ioctlHandler->mmapFail = true;

    char r0[32];
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, r0));
}

TEST_F(DebugApiRegistersAccessTest, givenCorruptedSrMagicWhenWriteRegistersCalledThenErrorUnknownIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;
    session->ioctlHandler.reset(ioctlHandler);

    auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    strcpy(session->stateSaveAreaHeader.data() + threadSlotOffset(pStateSaveAreaHeader, 0, 0, 0, 0) + pStateSaveAreaHeader->regHeader.sr_magic_offset, "garbage"); // NOLINT(clang-analyzer-security.insecureAPI.strcpy)

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugApiRegistersAccessTest, givenWriteRegistersCalledCorrectValuesRead) {
    SIP::version version = {1, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);
    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;

    ioctlHandler->setPreadMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);
    ioctlHandler->setPwriteMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);

    session->ioctlHandler.reset(ioctlHandler);
    session->vmHandle = 7;

    char grf[32] = {0};
    char grfRef[32] = {0};

    // grfs for 0/0/0/0 - very first eu thread
    memset(grfRef, 'k', 32); // 'a' + 10, r10
    session->ensureThreadStopped(stoppedThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 10, 1, grf));
    EXPECT_EQ(0, memcmp(grf, grfRef, 32));
    memset(grfRef, 'x', 32);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 10, 1, grfRef));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 10, 1, grf));
    EXPECT_EQ(0, memcmp(grf, grfRef, 32));

    // grfs for 0/3/7/3 - somewhere in the middle
    memset(grfRef, 'k', 32);
    session->ensureThreadStopped({0, 3, 7, 3});
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), {0, 3, 7, 3}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 10, 1, grf));
    EXPECT_EQ(0, memcmp(grf, grfRef, 32));
    memset(grfRef, 'y', 32);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugWriteRegisters(session->toHandle(), {0, 3, 7, 3}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 10, 1, grfRef));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), {0, 3, 7, 3}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 10, 1, grf));
    EXPECT_EQ(0, memcmp(grf, grfRef, 32));

    // grfs for 0/5/15/6 - very last eu thread
    memset(grfRef, 'k', 32);
    session->ensureThreadStopped({0, 5, 15, 6});
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), {0, 5, 15, 6}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 10, 1, grf));
    EXPECT_EQ(0, memcmp(grf, grfRef, 32));
    memset(grfRef, 'z', 32);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugWriteRegisters(session->toHandle(), {0, 5, 15, 6}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 10, 1, grfRef));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), {0, 5, 15, 6}, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 10, 1, grf));
    EXPECT_EQ(0, memcmp(grf, grfRef, 32));
}

TEST_F(DebugApiRegistersAccessTest, givenNoneThreadsStoppedWhenWriteRegistersCalledThenErrorNotAvailableReturned) {
    session->allThreadsStopped = false;
    session->allThreads[stoppedThreadId]->resumeThread();
    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugApiRegistersAccessTest, GivenThreadWhenReadingSystemRoutineIdentThenCorrectStateSaveAreaLocationIsRead) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;

    ioctlHandler->setPreadMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);
    ioctlHandler->setPwriteMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);

    session->ioctlHandler.reset(ioctlHandler);
    EuThread thread({0, 0, 0, 0, 0});

    SIP::sr_ident srIdent = {};
    auto result = session->readSystemRoutineIdent(&thread, vmHandle, srIdent);

    EXPECT_TRUE(result);

    EXPECT_EQ(2u, srIdent.count);
    EXPECT_EQ(2u, srIdent.version.major);
    EXPECT_EQ(0u, srIdent.version.minor);
    EXPECT_EQ(0u, srIdent.version.patch);
    EXPECT_STREQ("srmagic", srIdent.magic);

    EuThread thread2({0, 0, 0, 7, 3});

    result = session->readSystemRoutineIdent(&thread, vmHandle, srIdent);

    EXPECT_TRUE(result);

    EXPECT_EQ(2u, srIdent.count);
    EXPECT_EQ(2u, srIdent.version.major);
    EXPECT_EQ(0u, srIdent.version.minor);
    EXPECT_EQ(0u, srIdent.version.patch);
    EXPECT_STREQ("srmagic", srIdent.magic);
}

TEST_F(DebugApiRegistersAccessTest, GivenNoVmHandleWhenReadingSystemRoutineIdentThenFalseIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);
    ioctlHandler = new MockIoctlHandler;

    session->ioctlHandler.reset(ioctlHandler);

    EuThread thread({0, 0, 0, 0, 0});

    SIP::sr_ident srIdent = {};
    auto result = session->readSystemRoutineIdent(&thread, DebugSessionLinux::invalidHandle, srIdent);

    EXPECT_FALSE(result);
}

TEST_F(DebugApiRegistersAccessTest, GivenNoStatSaveAreaWhenReadingSystemRoutineIdentThenErrorReturned) {
    ioctlHandler = new MockIoctlHandler;
    session->ioctlHandler.reset(ioctlHandler);
    EuThread thread({0, 0, 0, 0, 0});

    SIP::sr_ident srIdent = {};
    auto result = session->readSystemRoutineIdent(&thread, 0x1234u, srIdent);

    EXPECT_FALSE(result);
}

TEST_F(DebugApiRegistersAccessTest, GivenMemReadFailureWhenReadingSystemRoutineIdentThenFalseIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;
    session->ioctlHandler.reset(ioctlHandler);
    ioctlHandler->mmapFail = true;
    ioctlHandler->preadRetVal = -1;

    EuThread thread({0, 0, 0, 0, 0});

    SIP::sr_ident srIdent = {};
    auto result = session->readSystemRoutineIdent(&thread, vmHandle, srIdent);

    EXPECT_FALSE(result);
}

TEST_F(DebugApiRegistersAccessTest, GivenCSSANotBoundWhenReadingSystemRoutineIdentThenFalseIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;
    session->ioctlHandler.reset(ioctlHandler);

    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToContextStateSaveAreaBindInfo.clear();

    EuThread thread({0, 0, 0, 0, 0});

    SIP::sr_ident srIdent = {};
    auto result = session->readSystemRoutineIdent(&thread, 0x1234u, srIdent);

    EXPECT_FALSE(result);
}

TEST_F(DebugApiRegistersAccessTest, GivenThreadWithInvalidVmIdWhenReadSbaBufferCalledThenErrorNotAvailableIsReturned) {
    session->vmHandle = L0::DebugSessionLinux::invalidHandle;
    ze_device_thread_t thread{0, 0, 0, 0};
    session->ensureThreadStopped(thread);
    SbaTrackedAddresses sba;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, session->readSbaBuffer(session->convertToThreadId(thread), sba));
}

TEST_F(DebugApiRegistersAccessTest, GivenVmHandleNotFoundWhenReadSbaBufferCalledThenErrorUnknownIsReturned) {
    session->vmHandle = 7;
    ze_device_thread_t thread{0, 0, 0, 0};
    session->ensureThreadStopped(thread);
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToStateBaseAreaBindInfo.clear();
    SbaTrackedAddresses sba;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readSbaBuffer(session->convertToThreadId(thread), sba));
}

struct MockRenderSurfaceState {
    uint32_t unused0[8];
    uint64_t SurfaceBaseAddress;
    uint32_t unused1[6];
};
static_assert(64 == sizeof(MockRenderSurfaceState));

void sbaInit(std::vector<char> &stateSaveArea, uint64_t stateSaveAreaGpuVa, SbaTrackedAddresses &sba, uint32_t r0[8]) {
    auto &hwInfo = *NEO::defaultHwInfo.get();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto maxDbgSurfaceSize = hwHelper.getSipKernelMaxDbgSurfaceSize(hwInfo);
    uint64_t surfaceStateBaseAddress = stateSaveAreaGpuVa + maxDbgSurfaceSize + sizeof(SbaTrackedAddresses);
    uint32_t renderSurfaceStateOffset = 256;
    r0[4] = 0xAAAAAAAA;
    r0[5] = ((renderSurfaceStateOffset) >> 6) << 10;

    sba.GeneralStateBaseAddress = 0x11111111;
    sba.SurfaceStateBaseAddress = surfaceStateBaseAddress;
    sba.DynamicStateBaseAddress = 0x33333333;
    sba.IndirectObjectBaseAddress = 0x44444444;
    sba.InstructionBaseAddress = 0x55555555;
    sba.BindlessSurfaceStateBaseAddress = 0x66666666;
    sba.BindlessSamplerStateBaseAddress = 0x77777777;

    char *sbaCpuPtr = stateSaveArea.data() + maxDbgSurfaceSize;
    char *rssCpuPtr = sbaCpuPtr + sizeof(SbaTrackedAddresses) + renderSurfaceStateOffset;
    memcpy(sbaCpuPtr, &sba, sizeof(sba));
    reinterpret_cast<MockRenderSurfaceState *>(rssCpuPtr)->SurfaceBaseAddress = 0xBA5EBA5E;
}

TEST_F(DebugApiRegistersAccessTest, GivenReadSbaBufferCalledThenSbaBufferIsRead) {

    SIP::version version = {1, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;

    ioctlHandler->setPreadMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);
    ioctlHandler->setPwriteMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);

    session->ioctlHandler.reset(ioctlHandler);
    session->vmHandle = 7;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToStateBaseAreaBindInfo[vmHandle] = {stateSaveAreaGpuVa + maxDbgSurfaceSize, sizeof(SbaTrackedAddresses)};
    ze_device_thread_t thread{0, 0, 0, 0};
    session->ensureThreadStopped(thread);

    SbaTrackedAddresses sba, sbaExpected;
    uint32_t r0[8];
    sbaInit(session->stateSaveAreaHeader, stateSaveAreaGpuVa, sbaExpected, r0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readSbaBuffer(session->convertToThreadId(thread), sba));
    EXPECT_EQ(sbaExpected.GeneralStateBaseAddress, sba.GeneralStateBaseAddress);
    EXPECT_EQ(sbaExpected.SurfaceStateBaseAddress, sba.SurfaceStateBaseAddress);
    EXPECT_EQ(sbaExpected.DynamicStateBaseAddress, sba.DynamicStateBaseAddress);
    EXPECT_EQ(sbaExpected.IndirectObjectBaseAddress, sba.IndirectObjectBaseAddress);
    EXPECT_EQ(sbaExpected.InstructionBaseAddress, sba.InstructionBaseAddress);
    EXPECT_EQ(sbaExpected.BindlessSurfaceStateBaseAddress, sba.BindlessSurfaceStateBaseAddress);
    EXPECT_EQ(sbaExpected.BindlessSamplerStateBaseAddress, sba.BindlessSamplerStateBaseAddress);
}

TEST_F(DebugApiRegistersAccessTest, givenInvalidSbaRegistersIndicesWhenReadSbaRegistersCalledThenErrorInvalidArgumentIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 9, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 8, 2, nullptr));
}

TEST_F(DebugApiRegistersAccessTest, GivenReadSbaRegistersCalledThenSbaRegistersAreRead) {
    SIP::version version = {1, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;

    ioctlHandler->setPreadMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);
    ioctlHandler->setPwriteMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);

    session->ioctlHandler.reset(ioctlHandler);
    session->vmHandle = 7;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToStateBaseAreaBindInfo[vmHandle] = {stateSaveAreaGpuVa + maxDbgSurfaceSize, sizeof(SbaTrackedAddresses)};
    ze_device_thread_t thread{0, 0, 0, 0};
    session->ensureThreadStopped(thread);

    SbaTrackedAddresses sbaExpected;
    uint32_t r0[8];
    sbaInit(session->stateSaveAreaHeader, stateSaveAreaGpuVa, sbaExpected, r0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugWriteRegisters(session->toHandle(), thread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, r0));

    uint64_t sba[9];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), thread, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 0, 9, sba));

    EXPECT_EQ(sbaExpected.GeneralStateBaseAddress, sba[0]);
    EXPECT_EQ(sbaExpected.SurfaceStateBaseAddress, sba[1]);
    EXPECT_EQ(sbaExpected.DynamicStateBaseAddress, sba[2]);
    EXPECT_EQ(sbaExpected.IndirectObjectBaseAddress, sba[3]);
    EXPECT_EQ(sbaExpected.InstructionBaseAddress, sba[4]);
    EXPECT_EQ(sbaExpected.BindlessSurfaceStateBaseAddress, sba[5]);
    EXPECT_EQ(sbaExpected.BindlessSamplerStateBaseAddress, sba[6]);

    uint64_t expectedBindingTableBaseAddress = ((r0[4] >> 5) << 5) + sbaExpected.SurfaceStateBaseAddress;
    uint64_t expectedScratchSpaceBaseAddress = 0;

    auto &hwHelper = HwHelper::get(neoDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (hwHelper.isScratchSpaceSurfaceStateAccessible()) {
        expectedScratchSpaceBaseAddress = 0xBA5EBA5E;
    } else {
        expectedScratchSpaceBaseAddress = ((r0[5] >> 10) << 10) + sbaExpected.GeneralStateBaseAddress;
    }

    EXPECT_EQ(expectedBindingTableBaseAddress, sba[7]);
    EXPECT_EQ(expectedScratchSpaceBaseAddress, sba[8]);
}

TEST_F(DebugApiRegistersAccessTest, GivenScarcthPointerAndZeroAddressInSurfaceStateWhenGettingScratchBaseRegThenValueIsZero) {
    SIP::version version = {1, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);

    ioctlHandler = new MockIoctlHandler;
    ioctlHandler->mmapRet = session->stateSaveAreaHeader.data();
    ioctlHandler->mmapBase = stateSaveAreaGpuVa;

    ioctlHandler->setPreadMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);
    ioctlHandler->setPwriteMemory(session->stateSaveAreaHeader.data(), session->stateSaveAreaHeader.size(), stateSaveAreaGpuVa);

    session->ioctlHandler.reset(ioctlHandler);
    session->vmHandle = 7;
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToStateBaseAreaBindInfo[vmHandle] = {stateSaveAreaGpuVa + maxDbgSurfaceSize, sizeof(SbaTrackedAddresses)};
    ze_device_thread_t thread{0, 3, 7, 3};
    session->ensureThreadStopped(thread);

    SbaTrackedAddresses sbaExpected{};
    uint32_t r0[8];

    uint64_t surfaceStateBaseAddress = stateSaveAreaGpuVa + maxDbgSurfaceSize + sizeof(SbaTrackedAddresses);
    uint32_t renderSurfaceStateOffset = 256;
    r0[4] = 0xAAAAAAAA;
    r0[5] = ((renderSurfaceStateOffset) >> 6) << 10;

    auto &hwHelper = HwHelper::get(neoDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (!hwHelper.isScratchSpaceSurfaceStateAccessible()) {
        r0[5] = 0;
    }
    sbaExpected.SurfaceStateBaseAddress = surfaceStateBaseAddress;

    char *sbaCpuPtr = session->stateSaveAreaHeader.data() + maxDbgSurfaceSize;
    char *rssCpuPtr = sbaCpuPtr + sizeof(SbaTrackedAddresses) + renderSurfaceStateOffset;
    memcpy(sbaCpuPtr, &sbaExpected, sizeof(sbaExpected));
    reinterpret_cast<MockRenderSurfaceState *>(rssCpuPtr)->SurfaceBaseAddress = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugWriteRegisters(session->toHandle(), thread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, r0));

    uint64_t sba[9];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), thread, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 0, 9, sba));

    EXPECT_EQ(sbaExpected.SurfaceStateBaseAddress, sba[1]);

    const uint64_t expectedScratchSpaceBaseAddress = 0;
    EXPECT_EQ(expectedScratchSpaceBaseAddress, sba[8]);
}

TEST_F(DebugApiRegistersAccessTest, givenWriteSbaRegistersCalledThenErrorInvalidArgumentIsReturned) {
    SIP::version version = {2, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 0, 1, nullptr));
}

using DebugApiLinuxMultitileTest = Test<DebugApiLinuxMultiDeviceFixture>;

TEST_F(DebugApiLinuxMultitileTest, GivenMultitileDeviceWhenCallingResumeThenThreadsFromBothTilesAreResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinux>(config, deviceImp, 10);
    ASSERT_NE(nullptr, sessionMock);
    SIP::version version = {2, 0, 0};
    initStateSaveArea(sessionMock->stateSaveAreaHeader, version);

    auto handler = new MockIoctlHandler;
    sessionMock->ioctlHandler.reset(handler);
    sessionMock->clientHandle = MockDebugSessionLinux::mockClientHandle;
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->vmToContextStateSaveAreaBindInfo[1u] = {0x1000, 0x1000};

    zet_debug_session_handle_t session = sessionMock->toHandle();
    ze_device_thread_t thread = {0, 0, 0, 0};

    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->stopThread(1u);
    sessionMock->allThreads[EuThread::ThreadId(1, thread)]->stopThread(1u);

    ze_device_thread_t allSlices = {UINT32_MAX, 0, 0, 0};
    auto result = L0::DebugApiHandlers::debugResume(session, allSlices);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(2, handler->ioctlCalled);
    EXPECT_EQ(uint32_t(PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME), handler->euControl.cmd);

    ASSERT_EQ(2u, sessionMock->resumedDevices.size());
    ASSERT_EQ(2u, sessionMock->resumedThreads.size());
    EXPECT_EQ(0u, sessionMock->resumedDevices[0]);
    EXPECT_EQ(1u, sessionMock->resumedDevices[1]);

    EXPECT_EQ(thread.slice, sessionMock->resumedThreads[0][0].slice);
    EXPECT_EQ(thread.subslice, sessionMock->resumedThreads[0][0].subslice);
    EXPECT_EQ(thread.eu, sessionMock->resumedThreads[0][0].eu);
    EXPECT_EQ(thread.thread, sessionMock->resumedThreads[0][0].thread);

    EXPECT_EQ(thread.slice, sessionMock->resumedThreads[1][0].slice);
    EXPECT_EQ(thread.subslice, sessionMock->resumedThreads[1][0].subslice);
    EXPECT_EQ(thread.eu, sessionMock->resumedThreads[1][0].eu);
    EXPECT_EQ(thread.thread, sessionMock->resumedThreads[1][0].thread);
}

TEST_F(DebugApiLinuxMultitileTest, givenApiThreadAndMultipleTilesWhenGettingDeviceIndexThenCorrectValueReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto debugSession = std::make_unique<MockDebugSessionLinux>(config, deviceImp, 10);
    ASSERT_NE(nullptr, debugSession);

    ze_device_thread_t thread = {sliceCount * 2 - 1, 0, 0, 0};

    uint32_t deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(1u, deviceIndex);

    thread = {sliceCount - 1, 0, 0, 0};
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(0u, deviceIndex);

    thread.slice = UINT32_MAX;
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(UINT32_MAX, deviceIndex);

    debugSession = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, deviceImp->subDevices[0], 10);

    thread = {sliceCount - 1, 0, 0, 0};
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(0u, deviceIndex);

    debugSession = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, deviceImp->subDevices[1], 10);

    thread = {sliceCount - 1, 0, 0, 0};
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(1u, deviceIndex);
}

struct DebugApiLinuxMultiDeviceVmBindFixture : public DebugApiLinuxMultiDeviceFixture, public MockDebugSessionLinuxHelper {
    void setUp() {
        DebugApiLinuxMultiDeviceFixture::setUp();

        zet_debug_config_t config = {};
        config.pid = 0x1234;

        session = std::make_unique<MockDebugSessionLinux>(config, deviceImp, 10);
        ASSERT_NE(nullptr, session);
        session->clientHandle = MockDebugSessionLinux::mockClientHandle;

        handler = new MockIoctlHandler;
        session->ioctlHandler.reset(handler);

        setupSessionClassHandlesAndUuidMap(session.get());
        setupVmToTile(session.get());
    }

    void tearDown() {
        DebugApiLinuxMultiDeviceFixture::tearDown();
    }

    MockIoctlHandler *handler = nullptr;
    std::unique_ptr<MockDebugSessionLinux> session;
};

using DebugApiLinuxMultiDeviceVmBindTest = Test<DebugApiLinuxMultiDeviceVmBindFixture>;

TEST_F(DebugApiLinuxMultiDeviceVmBindTest, givenTileInstancedIsaWhenHandlingVmBindCreateEventsThenModuleLoadIsTriggeredAfterAllInstancesEventsReceived) {

    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint32_t devices = static_cast<uint32_t>(deviceImp->getNEODevice()->getDeviceBitfield().to_ulong());

    DebugSessionLinux::UuidData isaUuidData = {
        .handle = isaUUID,
        .classHandle = isaClassHandle,
        .classIndex = NEO::DrmResourceClass::Isa,
        .data = std::make_unique<char[]>(sizeof(devices)),
        .dataSize = sizeof(devices)};

    memcpy_s(isaUuidData.data.get(), sizeof(devices), &devices, sizeof(devices));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[isaUUID] = std::move(isaUuidData);

    addIsaVmBindEvent(session.get(), vm0, true, true);

    EXPECT_EQ(1u, session->clientHandleToConnection[session->clientHandle]->isaMap[0].size());
    EXPECT_EQ(0u, session->clientHandleToConnection[session->clientHandle]->isaMap[1].size());
    EXPECT_EQ(20u, handler->debugEventAcked.seqno);

    EXPECT_EQ(0u, session->apiEvents.size());

    addIsaVmBindEvent(session.get(), vm1, true, true);

    EXPECT_EQ(1u, session->apiEvents.size());

    zet_debug_event_t event;
    auto result = session->readEvent(0, &event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(isaGpuVa, event.info.module.load);
}

TEST_F(DebugApiLinuxMultiDeviceVmBindTest, givenTileInstancedIsaWhenHandlingVmBindDestroyEventsThenModuleUnloadIsTriggeredAfterAllInstancesEventsReceived) {
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint32_t devices = static_cast<uint32_t>(deviceImp->getNEODevice()->getDeviceBitfield().to_ulong());

    DebugSessionLinux::UuidData isaUuidData = {
        .handle = isaUUID,
        .classHandle = isaClassHandle,
        .classIndex = NEO::DrmResourceClass::Isa,
        .data = std::make_unique<char[]>(sizeof(devices)),
        .dataSize = sizeof(devices)};

    memcpy_s(isaUuidData.data.get(), sizeof(devices), &devices, sizeof(devices));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[isaUUID] = std::move(isaUuidData);

    addIsaVmBindEvent(session.get(), vm0, false, true);
    addIsaVmBindEvent(session.get(), vm1, false, true);

    EXPECT_EQ(1u, session->clientHandleToConnection[session->clientHandle]->isaMap[0].size());
    EXPECT_EQ(1u, session->clientHandleToConnection[session->clientHandle]->isaMap[1].size());

    auto numberOfEvents = session->apiEvents.size();

    addIsaVmBindEvent(session.get(), vm0, false, false);
    EXPECT_EQ(numberOfEvents, session->apiEvents.size());

    addIsaVmBindEvent(session.get(), vm1, false, false);

    auto numberOfAllEvents = session->apiEvents.size();
    EXPECT_EQ(numberOfEvents + 1, numberOfAllEvents);

    zet_debug_event_t event;
    ze_result_t result = ZE_RESULT_SUCCESS;
    while (numberOfAllEvents--) {
        result = session->readEvent(0, &event);
        if (result != ZE_RESULT_SUCCESS) {
            break;
        }
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD, event.type);
    EXPECT_EQ(isaGpuVa, event.info.module.load);
}

TEST_F(DebugApiLinuxMultiDeviceVmBindTest, givenTileInstancedIsaWhenWritingAndReadingIsaMemoryThenOnlyWritesAreMirrored) {
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint32_t devices = static_cast<uint32_t>(deviceImp->getNEODevice()->getDeviceBitfield().to_ulong());

    DebugSessionLinux::UuidData isaUuidData = {
        .handle = isaUUID,
        .classHandle = isaClassHandle,
        .classIndex = NEO::DrmResourceClass::Isa,
        .data = std::make_unique<char[]>(sizeof(devices)),
        .dataSize = sizeof(devices)};

    memcpy_s(isaUuidData.data.get(), sizeof(devices), &devices, sizeof(devices));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[isaUUID] = std::move(isaUuidData);

    addIsaVmBindEvent(session.get(), vm0, false, true);
    addIsaVmBindEvent(session.get(), vm1, false, true);

    EXPECT_EQ(1u, session->clientHandleToConnection[session->clientHandle]->isaMap[0].size());
    EXPECT_EQ(1u, session->clientHandleToConnection[session->clientHandle]->isaMap[1].size());

    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    uint8_t buffer[16];
    handler->pwriteRetVal = 16;
    auto result = session->writeMemory(thread, &desc, 16, buffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(2, handler->pwriteCalled);

    handler->preadRetVal = 16;
    result = session->readMemory(thread, &desc, 16, buffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1, handler->preadCalled);
}

TEST_F(DebugApiLinuxMultiDeviceVmBindTest, givenTileInstancedIsaWhenWritingMemoryFailsThenErrorIsReturned) {
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint32_t devices = static_cast<uint32_t>(deviceImp->getNEODevice()->getDeviceBitfield().to_ulong());

    DebugSessionLinux::UuidData isaUuidData = {
        .handle = isaUUID,
        .classHandle = isaClassHandle,
        .classIndex = NEO::DrmResourceClass::Isa,
        .data = std::make_unique<char[]>(sizeof(devices)),
        .dataSize = sizeof(devices)};

    memcpy_s(isaUuidData.data.get(), sizeof(devices), &devices, sizeof(devices));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[isaUUID] = std::move(isaUuidData);

    addIsaVmBindEvent(session.get(), vm0, false, true);
    addIsaVmBindEvent(session.get(), vm1, false, true);

    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    uint8_t buffer[16];
    handler->pwriteRetVal = -1;
    auto result = session->writeMemory(thread, &desc, 16, buffer);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    EXPECT_EQ(1, handler->pwriteCalled);
}

TEST_F(DebugApiLinuxMultiDeviceVmBindTest, givenSingleIsaWithInvalidVmWhenReadingIsaMemoryThenErrorUninitializedIsReturned) {
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint32_t devices = static_cast<uint32_t>(neoDevice->getDeviceBitfield().to_ulong());

    DebugSessionLinux::UuidData isaUuidData = {
        .handle = isaUUID,
        .classHandle = isaClassHandle,
        .classIndex = NEO::DrmResourceClass::Isa,
        .data = std::make_unique<char[]>(sizeof(devices)),
        .dataSize = sizeof(devices)};

    memcpy_s(isaUuidData.data.get(), sizeof(devices), &devices, sizeof(devices));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[isaUUID] = std::move(isaUuidData);

    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindIsa->base.flags |= PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;

    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 20u;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = MockDebugSessionLinux::invalidHandle;
    vmBindIsa->num_uuids = 2;

    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

    typeOfUUID uuidsTemp[2];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    uint8_t buffer[16];

    handler->preadRetVal = 16;
    auto result = session->readMemory(thread, &desc, 16, buffer);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);

    EXPECT_EQ(0, handler->preadCalled);
}

TEST_F(DebugApiLinuxMultiDeviceVmBindTest, givenTileInstancedIsaAndSingleInstanceWhenGettingIsaInfoThenIsaTrueAndErrorUninitializedIsReturned) {
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint32_t devices = static_cast<uint32_t>(deviceImp->getNEODevice()->getDeviceBitfield().to_ulong());

    DebugSessionLinux::UuidData isaUuidData = {
        .handle = isaUUID,
        .classHandle = isaClassHandle,
        .classIndex = NEO::DrmResourceClass::Isa,
        .data = std::make_unique<char[]>(sizeof(devices)),
        .dataSize = sizeof(devices)};

    memcpy_s(isaUuidData.data.get(), sizeof(devices), &devices, sizeof(devices));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[isaUUID] = std::move(isaUuidData);

    addIsaVmBindEvent(session.get(), vm1, false, true);

    EXPECT_EQ(1u, session->clientHandleToConnection[session->clientHandle]->isaMap[1].size());

    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    uint64_t vm[4];
    ze_result_t status = ZE_RESULT_SUCCESS;
    auto isIsa = session->getIsaInfoForAllInstances(devices, &desc, 16, vm, status);

    EXPECT_TRUE(isIsa);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, status);
}

TEST_F(DebugApiLinuxMultiDeviceVmBindTest, givenNoIsaWhenGettingIsaInfoThenFalseReturned) {
    uint32_t devices = static_cast<uint32_t>(deviceImp->getNEODevice()->getDeviceBitfield().to_ulong());

    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    uint64_t vm[4];
    ze_result_t status = ZE_RESULT_SUCCESS;
    auto isIsa = session->getIsaInfoForAllInstances(devices, &desc, 16, vm, status);

    EXPECT_FALSE(isIsa);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, status);
}

TEST_F(DebugApiLinuxMultiDeviceVmBindTest, givenIsaWhenGettingIsaInfoForWrongAddressThenErrorUninitializedReturned) {
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint32_t devices = static_cast<uint32_t>(deviceImp->getNEODevice()->getDeviceBitfield().to_ulong());

    DebugSessionLinux::UuidData isaUuidData = {
        .handle = isaUUID,
        .classHandle = isaClassHandle,
        .classIndex = NEO::DrmResourceClass::Isa,
        .data = std::make_unique<char[]>(sizeof(devices)),
        .dataSize = sizeof(devices)};

    memcpy_s(isaUuidData.data.get(), sizeof(devices), &devices, sizeof(devices));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[isaUUID] = std::move(isaUuidData);

    addIsaVmBindEvent(session.get(), vm0, false, true);

    EXPECT_EQ(1u, session->clientHandleToConnection[session->clientHandle]->isaMap[0].size());

    uint64_t vm[4];
    ze_result_t status = ZE_RESULT_SUCCESS;
    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa - 0x1000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;
    auto isIsa = session->getIsaInfoForAllInstances(devices, &desc, 16, vm, status);

    EXPECT_FALSE(isIsa);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, status);

    desc.address = isaGpuVa + isaSize + 0x1000;
    isIsa = session->getIsaInfoForAllInstances(devices, &desc, 16, vm, status);

    EXPECT_FALSE(isIsa);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, status);
}

TEST_F(DebugApiLinuxMultiDeviceVmBindTest, givenTileInstancedIsaWhenWritingAndReadingWrongIsaRangeThenErrorInvalidArgReturned) {
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint32_t devices = static_cast<uint32_t>(deviceImp->getNEODevice()->getDeviceBitfield().to_ulong());

    DebugSessionLinux::UuidData isaUuidData = {
        .handle = isaUUID,
        .classHandle = isaClassHandle,
        .classIndex = NEO::DrmResourceClass::Isa,
        .data = std::make_unique<char[]>(sizeof(devices)),
        .dataSize = sizeof(devices)};

    memcpy_s(isaUuidData.data.get(), sizeof(devices), &devices, sizeof(devices));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[isaUUID] = std::move(isaUuidData);

    addIsaVmBindEvent(session.get(), vm0, false, true);

    EXPECT_EQ(1u, session->clientHandleToConnection[session->clientHandle]->isaMap[0].size());

    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    uint8_t buffer[16];
    auto result = session->writeMemory(thread, &desc, isaSize * 2, buffer);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    EXPECT_EQ(0, handler->pwriteCalled);

    result = session->readMemory(thread, &desc, isaSize * 2, buffer);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    EXPECT_EQ(0, handler->preadCalled);
}

TEST_F(DebugApiLinuxMultiDeviceVmBindTest, givenSingleMemoryIsaWhenWritingAndReadingThenOnlyOneInstanceIsWrittenAndRead) {
    auto handler = new MockIoctlHandler;
    session->ioctlHandler.reset(handler);

    uint32_t devices = static_cast<uint32_t>(neoDevice->getSubDevice(1)->getDeviceBitfield().to_ulong());
    EXPECT_EQ(1u, neoDevice->getSubDevice(1)->getDeviceBitfield().count());

    DebugSessionLinux::UuidData isaUuidData = {
        .handle = isaUUID,
        .classHandle = isaClassHandle,
        .classIndex = NEO::DrmResourceClass::Isa,
        .data = std::make_unique<char[]>(sizeof(devices)),
        .dataSize = sizeof(devices)};

    memcpy_s(isaUuidData.data.get(), sizeof(devices), &devices, sizeof(devices));
    session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[isaUUID] = std::move(isaUuidData);

    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    vmBindIsa->base.flags |= PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;

    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 20u;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = vm1;
    vmBindIsa->num_uuids = 2;

    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

    typeOfUUID uuidsTemp[2];
    uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
    uuidsTemp[1] = static_cast<typeOfUUID>(elfUUID);

    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    session->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, session->clientHandleToConnection[session->clientHandle]->isaMap[1].size());

    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    uint8_t buffer[16];
    handler->pwriteRetVal = 16;
    auto result = session->writeMemory(thread, &desc, 16, buffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1, handler->pwriteCalled);

    handler->preadRetVal = 16;
    result = session->readMemory(thread, &desc, 16, buffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1, handler->preadCalled);
}

struct AffinityMaskMultipleSubdevices : DebugApiLinuxMultiDeviceFixture {
    void setUp() {
        DebugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.1,0.3");
        MultipleDevicesWithCustomHwInfo::numSubDevices = 4;
        DebugApiLinuxMultiDeviceFixture::setUp();
    }

    void tearDown() {
        MultipleDevicesWithCustomHwInfo::tearDown();
    }
    DebugManagerStateRestore restorer;
};

using AffinityMaskMultipleSubdevicesTest = Test<AffinityMaskMultipleSubdevices>;

TEST_F(AffinityMaskMultipleSubdevicesTest, givenApiThreadAndMultipleTilesWhenConvertingToPhysicalThenCorrectValuesReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto debugSession = std::make_unique<MockDebugSessionLinux>(config, deviceImp, 10);

    ze_device_thread_t thread = {2 * sliceCount - 1, 0, 0, 0};

    uint32_t deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(1u, deviceIndex);

    auto convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);

    thread = {3 * sliceCount - 1, 0, 0, 0};

    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(3u, deviceIndex);

    convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(3u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);

    thread = {sliceCount - 1, 0, 0, 0};

    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(0u, deviceIndex);

    convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(0u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);

    thread.slice = UINT32_MAX;
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(UINT32_MAX, deviceIndex);
}

TEST_F(AffinityMaskMultipleSubdevicesTest, GivenEventWithAckFlagAndTileNotWithinBitfieldWhenHandlingEventForISAThenIsaIsNotStoredInMapAndEventIsAcked) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    auto debugSession = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{1234}, deviceImp, 10);
    auto handler = new MockIoctlHandler;
    debugSession->ioctlHandler.reset(handler);

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 20u;
    vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
    vmBindIsa->va_start = isaGpuVa;
    vmBindIsa->va_length = isaSize;
    vmBindIsa->vm_handle = 5;
    vmBindIsa->num_uuids = 1;

    auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

    typeOfUUID uuidsTemp[1];
    uuidsTemp[0] = static_cast<typeOfUUID>(6);
    debugSession->clientHandleToConnection[debugSession->clientHandle]->uuidMap[6].classIndex = NEO::DrmResourceClass::Isa;
    debugSession->clientHandleToConnection[debugSession->clientHandle]->vmToTile[vmBindIsa->vm_handle] = 2;
    memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

    debugSession->handleEvent(&vmBindIsa->base);
    EXPECT_EQ(0u, debugSession->clientHandleToConnection[debugSession->clientHandle]->isaMap[0].size());
    EXPECT_EQ(0u, debugSession->clientHandleToConnection[debugSession->clientHandle]->isaMap[2].size());
    EXPECT_EQ(vmBindIsa->base.seqno, handler->debugEventAcked.seqno);
}

TEST_F(AffinityMaskMultipleSubdevicesTest, GivenAttEventForTileNotWithinBitfieldWhenHandlingEventThenEventIsSkipped) {
    auto debugSession = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{1234}, deviceImp, 10);

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    debugSession->clientHandleToConnection[debugSession->clientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    debugSession->clientHandleToConnection[debugSession->clientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;
    debugSession->clientHandleToConnection[debugSession->clientHandle]->vmToTile[vmHandle] = 2;

    prelim_drm_i915_debug_event_eu_attention attEvent = {};
    attEvent.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attEvent.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    attEvent.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention);
    attEvent.bitmask_size = 0;
    attEvent.client_handle = MockDebugSessionLinux::mockClientHandle;
    attEvent.lrc_handle = lrcHandle;
    attEvent.flags = 0;

    auto engineInfo = mockDrm->getEngineInfo();
    auto ci = engineInfo->getEngineInstance(2, hwInfo.capabilityTable.defaultEngineType);
    attEvent.ci.engine_class = ci->engineClass;
    attEvent.ci.engine_instance = ci->engineInstance;

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};
    debugSession->pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread, false));

    debugSession->handleEvent(&attEvent.base);
    EXPECT_FALSE(debugSession->triggerEvents);
}

} // namespace ult
} // namespace L0
