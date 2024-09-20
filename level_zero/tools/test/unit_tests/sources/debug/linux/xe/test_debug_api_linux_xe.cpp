/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/include/zet_intel_gpu_debug.h"
#include "level_zero/tools/source/debug/debug_handlers.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/source/debug/linux/xe/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/xe/debug_session_fixtures_linux_xe.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

#include "common/StateSaveAreaHeader.h"
#include "debug_xe_includes.h"

#include <fcntl.h>
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
extern int fsyncCalled;
extern int fsyncArgPassed;
extern int fsyncRetVal;
} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace ult {

extern CreateDebugSessionHelperFunc createDebugSessionFuncXe;

TEST(IoctlHandlerXe, GivenHandlerWhenEuControlIoctlFailsWithEBUSYThenIoctlIsNotCalledAgain) {
    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl);
    VariableBackup<int> mockErrno(&errno);
    int ioctlCount = 0;

    SysCalls::sysCallsIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        auto ioctlCount = reinterpret_cast<int *>(arg);
        (*ioctlCount)++;
        if (*ioctlCount < 2) {
            errno = EBUSY;
            return -1;
        }
        errno = 0;
        return 0;
    };

    L0::DebugSessionLinuxXe::IoctlHandlerXe handler;

    auto result = handler.ioctl(0, DRM_XE_EUDEBUG_IOCTL_EU_CONTROL, &ioctlCount);
    EXPECT_EQ(-1, result);
    EXPECT_EQ(1, ioctlCount);
}

TEST(IoctlHandlerXe, GivenHandlerWhenEuControlIoctlFailsWithEAGAINOrEINTRThenIoctlIsCalledAgain) {
    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl);
    VariableBackup<int> mockErrno(&errno);
    int ioctlCount = 0;

    SysCalls::sysCallsIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        auto ioctlCount = reinterpret_cast<int *>(arg);
        (*ioctlCount)++;
        if (*ioctlCount == 1) {
            errno = EAGAIN;
            return -1;
        }
        if (*ioctlCount == 2) {
            errno = EINTR;
            return -1;
        }
        errno = 0;
        return 0;
    };

    L0::DebugSessionLinuxXe::IoctlHandlerXe handler;

    auto result = handler.ioctl(0, DRM_XE_EUDEBUG_IOCTL_EU_CONTROL, &ioctlCount);
    EXPECT_EQ(0, result);
    EXPECT_EQ(3, ioctlCount);
}

TEST(IoctlHandler, GivenHandlerWhenIoctlFailsWithEBUSYThenIoctlIsAgain) {
    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl);
    VariableBackup<int> mockErrno(&errno);
    int ioctlCount = 0;

    SysCalls::sysCallsIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        auto ioctlCount = reinterpret_cast<int *>(arg);
        (*ioctlCount)++;
        if (*ioctlCount < 2) {
            errno = EBUSY;
            return -1;
        }
        errno = 0;
        return 0;
    };

    L0::DebugSessionLinuxXe::IoctlHandlerXe handler;

    auto result = handler.ioctl(0, DRM_XE_EUDEBUG_IOCTL_READ_EVENT, &ioctlCount);
    EXPECT_EQ(0, result);
    EXPECT_EQ(2, ioctlCount);
}

using DebugApiLinuxTestXe = Test<DebugApiLinuxXeFixture>;

TEST_F(DebugApiLinuxTestXe, GivenDebugSessionWhenCallingPollThenDefaultHandlerRedirectsToSysCall) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);
    EXPECT_EQ(0, session->ioctlHandler->poll(nullptr, 0, 0));
}

TEST_F(DebugApiLinuxTestXe, givenDeviceWhenCallingDebugAttachThenSuccessAndValidSessionHandleAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession = nullptr;

    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession);
}

TEST_F(DebugApiLinuxTestXe, givenDebugSessionWhenCallingDebugDetachThenSessionIsClosedAndSuccessReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession = nullptr;

    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession);

    result = zetDebugDetach(debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(DebugApiLinuxTestXe, GivenDebuggerCreatedWithParamsThenVersionIsSaved) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    ze_result_t result = ZE_RESULT_SUCCESS;

    VariableBackup<CreateDebugSessionHelperFunc> mockCreateDebugSessionBackup(&L0::ult::createDebugSessionFuncXe, [](const zet_debug_config_t &config, L0::Device *device, int debugFd, void *params) -> DebugSession * {
        auto session = new MockDebugSessionLinuxXe(config, device, debugFd, params);
        session->initializeRetVal = ZE_RESULT_SUCCESS;
        return session;
    });

    mockDrm->debuggerOpenRetval = 10;
    mockDrm->debuggerOpenVersion = 1;
    auto session = std::unique_ptr<DebugSession>(DebugSession::create(config, device, result, !device->getNEODevice()->isSubDevice()));

    EXPECT_NE(nullptr, session);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    MockDebugSessionLinuxXe *linuxSession = static_cast<MockDebugSessionLinuxXe *>(session.get());
    EXPECT_EQ(linuxSession->xeDebuggerVersion, 1u);
}

TEST_F(DebugApiLinuxTestXe, GivenDebugSessionWhenClosingConnectionThenSysCallCloseOnFdIsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);

    EXPECT_NE(nullptr, session);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;

    auto ret = session->closeConnection();
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);
    EXPECT_EQ(10, NEO::SysCalls::closeFuncArgPassed);
    EXPECT_FALSE(session->internalEventThread.threadActive);
    EXPECT_FALSE(session->asyncThread.threadActive);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;
}

TEST_F(DebugApiLinuxTestXe, GivenEventWithInvalidFlagsWhenReadingEventThenUnknownErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    drm_xe_eudebug_event_client clientInvalidFlag = {};
    clientInvalidFlag.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    clientInvalidFlag.base.flags = 0x8000;
    clientInvalidFlag.base.len = sizeof(drm_xe_eudebug_event_client);
    clientInvalidFlag.client_handle = 1;

    auto handler = new MockIoctlHandlerXe;
    handler->eventQueue.push({reinterpret_cast<char *>(&clientInvalidFlag), static_cast<uint64_t>(clientInvalidFlag.base.len)});
    handler->pollRetVal = 1;
    auto eventsCount = handler->eventQueue.size();

    session->ioctlHandler.reset(handler);

    auto memory = std::make_unique<uint64_t[]>(MockDebugSessionLinuxXe::maxEventSize / sizeof(uint64_t));
    drm_xe_eudebug_event *event = reinterpret_cast<drm_xe_eudebug_event *>(memory.get());
    event->type = DRM_XE_EUDEBUG_EVENT_READ;
    event->flags = 0x8000;
    event->len = MockDebugSessionLinuxXe::maxEventSize;

    ze_result_t result = session->readEventImp(event);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    EXPECT_EQ(eventsCount, static_cast<size_t>(handler->ioctlCalled));
}

TEST_F(DebugApiLinuxTestXe, WhenOpenDebuggerFailsThenCorrectErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    ze_result_t result = ZE_RESULT_SUCCESS;

    mockDrm->debuggerOpenRetval = -1;
    mockDrm->baseErrno = false;
    mockDrm->errnoRetVal = EBUSY;
    auto session = DebugSession::create(config, device, result, !device->getNEODevice()->isSubDevice());

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    mockDrm->errnoRetVal = ENODEV;
    session = DebugSession::create(config, device, result, !device->getNEODevice()->isSubDevice());

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    mockDrm->errnoRetVal = EACCES;
    session = DebugSession::create(config, device, result, !device->getNEODevice()->isSubDevice());

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);

    mockDrm->errnoRetVal = ESRCH;
    session = DebugSession::create(config, device, result, !device->getNEODevice()->isSubDevice());

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(DebugApiLinuxTestXe, GivenDebugSessionWhenPollReturnsZeroThenNotReadyIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    ze_result_t result = ZE_RESULT_SUCCESS;

    mockDrm->debuggerOpenRetval = 10;
    mockDrm->baseErrno = false;
    mockDrm->errnoRetVal = 0;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    result = session->initialize();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(DebugApiLinuxTestXe, GivenDebugSessionInitializationWhenNoValidEventsAreReadThenResultNotReadyIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    drm_xe_eudebug_event_client client = {};
    client.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client.base.flags = 0;
    client.base.len = sizeof(drm_xe_eudebug_event_client);
    client.client_handle = MockDebugSessionLinuxXe::mockClientHandle;

    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});

    handler->pollRetVal = 1;

    ze_result_t result = session->initialize();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(DebugApiLinuxTestXe, GivenPollReturnsErrorAndEinvalWhenReadingInternalEventsAsyncThenDetachEventIsGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    handler->pollRetVal = -1;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    errno = EINVAL;
    session->readInternalEventsAsync();

    EXPECT_EQ(1u, session->apiEvents.size());
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_DETACHED, session->apiEvents.front().type);
    session->apiEvents.pop();
    errno = 0;

    session->readInternalEventsAsync();
    EXPECT_EQ(0u, session->apiEvents.size());
}

TEST_F(DebugApiLinuxTestXe, GivenPollReturnsNonZeroWhenReadingInternalEventsAsyncThenEventReadIsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    handler->pollRetVal = 1;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    uint64_t clientHandle = 2;
    drm_xe_eudebug_event_client client = {};
    client.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client.base.flags = 0;
    client.base.len = sizeof(drm_xe_eudebug_event_client);
    client.client_handle = clientHandle;

    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});

    session->readInternalEventsAsync();
    constexpr int dummyReadEventCount = 1;

    EXPECT_EQ(dummyReadEventCount, handler->ioctlCalled);
    EXPECT_EQ(DebugSessionLinuxXe::maxEventSize, handler->debugEventInput.len);
    EXPECT_EQ(static_cast<decltype(drm_xe_eudebug_event::type)>(DRM_XE_EUDEBUG_EVENT_READ), handler->debugEventInput.type);
}

TEST_F(DebugApiLinuxTestXe, GivenMoreThan1EventsInQueueThenInternalEventsOnlyReads1) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    handler->pollRetVal = 1;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    //    session->synchronousInternalEventRead = true;

    uint64_t clientHandle = 2;
    drm_xe_eudebug_event_client client = {};
    client.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client.base.flags = 0;
    client.base.len = sizeof(drm_xe_eudebug_event_client);
    client.client_handle = clientHandle;

    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});

    session->readInternalEventsAsync();

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(DebugSessionLinuxXe::maxEventSize, handler->debugEventInput.len);
    EXPECT_EQ(static_cast<decltype(drm_xe_eudebug_event::type)>(DRM_XE_EUDEBUG_EVENT_READ), handler->debugEventInput.type);
}

TEST_F(DebugApiLinuxTestXe, GivenEventInInternalEventQueueWhenAsyncThreadFunctionIsExecutedThenEventIsHandled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockAsyncThreadDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();

    uint8_t eventClientData[sizeof(drm_xe_eudebug_event_client)];
    auto client = reinterpret_cast<drm_xe_eudebug_event_client *>(&eventClientData);
    client->base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client->base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client->base.len = sizeof(drm_xe_eudebug_event_client);
    client->client_handle = 0x123456789;

    auto memory = std::make_unique<uint64_t[]>(sizeof(drm_xe_eudebug_event_client) / sizeof(uint64_t));
    memcpy(memory.get(), client, sizeof(drm_xe_eudebug_event_client));

    // Clear the event queue before using it
    while (!session->internalEventQueue.empty()) {
        session->internalEventQueue.pop();
    }
    session->internalEventQueue.push(std::move(memory));

    session->startAsyncThread();

    while (session->getInternalEventCounter == 0)
        ;
    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThreadFinished);

    session->closeAsyncThread();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThreadFinished);
    EXPECT_EQ(session->clientHandleToConnection.size(), 1ul);
    EXPECT_NE(session->clientHandleToConnection.find(client->client_handle), session->clientHandleToConnection.end());

    uint64_t wrongClientHandle = 34;
    EXPECT_EQ(session->clientHandleToConnection.find(wrongClientHandle), session->clientHandleToConnection.end());
}

TEST_F(DebugApiLinuxTestXe, GivenNoEventInInternalEventQueueWhenAsyncThreadFunctionIsExecutedThenEventsAreCheckedForAvailability) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockAsyncThreadDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    // Clear the event queue before using it
    while (!session->internalEventQueue.empty()) {
        session->internalEventQueue.pop();
    }

    session->startAsyncThread();

    while (session->getInternalEventCounter == 0)
        ;
    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThreadFinished);

    session->closeAsyncThread();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThreadFinished);
}

TEST_F(DebugApiLinuxTestXe, GivenOneEuDebugOpenEventAndOneIncorrectEventWhenHandleEventThenEventsAreHandled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = 0x123456789;

    drm_xe_eudebug_event client2;
    client2.type = DRM_XE_EUDEBUG_EVENT_NONE;
    client2.flags = 0;

    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client2));
    EXPECT_EQ(session->clientHandleToConnection.size(), 1ul);
    EXPECT_NE(session->clientHandleToConnection.find(client1.client_handle), session->clientHandleToConnection.end());

    uint64_t wrongClientHandle = 34;
    EXPECT_EQ(session->clientHandleToConnection.find(wrongClientHandle), session->clientHandleToConnection.end());
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugOpenEventWithEventCreateFlagWhenHandleEventThenNewClientConnectionIsCreated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = 0x123456789;

    drm_xe_eudebug_event_client client2;
    client2.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client2.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client2.client_handle = 0x123456788;

    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client2));
    EXPECT_EQ(session->clientHandleToConnection.size(), 2ul);
    EXPECT_NE(session->clientHandleToConnection.find(client1.client_handle), session->clientHandleToConnection.end());
    EXPECT_NE(session->clientHandleToConnection.find(client2.client_handle), session->clientHandleToConnection.end());

    EXPECT_EQ(session->clientHandle, 0x123456788u);
    uint64_t wrongClientHandle = 34;
    EXPECT_EQ(session->clientHandleToConnection.find(wrongClientHandle), session->clientHandleToConnection.end());
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugOpenEventWithEventDestroyFlagWhenHandleEventThenClientConnectionIsDestroyed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = 0x123456789;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));
    EXPECT_EQ(session->clientHandleToConnection.size(), 1ul);
    EXPECT_NE(session->clientHandleToConnection.find(client1.client_handle), session->clientHandleToConnection.end());

    drm_xe_eudebug_event_client client2;
    client2.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client2.base.flags = DRM_XE_EUDEBUG_EVENT_DESTROY;
    client2.client_handle = client1.client_handle;
    EXPECT_EQ(session->clientHandleClosed, session->invalidClientHandle);
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client2));
    EXPECT_EQ(session->clientHandleClosed, client2.client_handle);
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugVmEventWithEventCreateFlagWhenHandleEventThenNewVmIdsIsCreated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = 0x123456789;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    drm_xe_eudebug_event_vm vm1;
    drm_xe_eudebug_event_vm vm2;
    vm1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vm1.base.type = DRM_XE_EUDEBUG_EVENT_VM;
    vm1.client_handle = 0x123456789;
    vm1.vm_handle = 0x123;
    vm2.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vm2.base.type = DRM_XE_EUDEBUG_EVENT_VM;
    vm2.client_handle = 0x123456789;
    vm2.vm_handle = 0x124;

    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vm1));
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vm2));
    ASSERT_EQ(session->clientHandleToConnection[client1.client_handle]->vmIds.size(), 2u);
    auto it1 = session->clientHandleToConnection[client1.client_handle]->vmIds.find(vm1.vm_handle);
    EXPECT_NE(it1, session->clientHandleToConnection[client1.client_handle]->vmIds.end());
    auto it2 = session->clientHandleToConnection[client1.client_handle]->vmIds.find(vm2.vm_handle);
    EXPECT_NE(it2, session->clientHandleToConnection[client1.client_handle]->vmIds.end());

    // Check for wrong vmId
    auto it3 = session->clientHandleToConnection[client1.client_handle]->vmIds.find(0x123456);
    EXPECT_EQ(it3, session->clientHandleToConnection[client1.client_handle]->vmIds.end());
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugVmEventWithEventDestroyFlagWhenHandleEventThenVmIdsIsRemoved) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = 0x123456789;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    drm_xe_eudebug_event_vm vm1;
    vm1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vm1.base.type = DRM_XE_EUDEBUG_EVENT_VM;
    vm1.client_handle = 0x123456789;
    vm1.vm_handle = 0x123;

    drm_xe_eudebug_event_vm vm2;
    vm2.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vm2.base.type = DRM_XE_EUDEBUG_EVENT_VM;
    vm2.client_handle = 0x123456789;
    vm2.vm_handle = 0x124;

    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vm1));
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vm2));
    ASSERT_EQ(session->clientHandleToConnection[client1.client_handle]->vmIds.size(), 2u);
    auto it1 = session->clientHandleToConnection[client1.client_handle]->vmIds.find(vm1.vm_handle);
    EXPECT_NE(it1, session->clientHandleToConnection[client1.client_handle]->vmIds.end());
    auto it2 = session->clientHandleToConnection[client1.client_handle]->vmIds.find(vm2.vm_handle);
    EXPECT_NE(it2, session->clientHandleToConnection[client1.client_handle]->vmIds.end());

    drm_xe_eudebug_event_vm vm3;
    vm3.base.flags = DRM_XE_EUDEBUG_EVENT_DESTROY;
    vm3.base.type = DRM_XE_EUDEBUG_EVENT_VM;
    vm3.client_handle = 0x123456789;
    vm3.vm_handle = 0x124;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vm3));
    auto it3 = session->clientHandleToConnection[client1.client_handle]->vmIds.find(vm2.vm_handle);
    EXPECT_EQ(it3, session->clientHandleToConnection[client1.client_handle]->vmIds.end());
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugExecQueueEventWithEventCreateFlagWhenHandleEventThenExecQueueIsCreated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = 0x123456789;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    uint64_t execQueueData[sizeof(drm_xe_eudebug_event_exec_queue) / sizeof(uint64_t) + 3 * sizeof(typeOfLrcHandle)];
    auto *lrcHandle = reinterpret_cast<typeOfLrcHandle *>(ptrOffset(execQueueData, sizeof(drm_xe_eudebug_event_exec_queue)));
    typeOfLrcHandle lrcHandleTemp[3];
    const uint64_t lrcHandle0 = 2;
    const uint64_t lrcHandle1 = 3;
    const uint64_t lrcHandle2 = 5;
    lrcHandleTemp[0] = static_cast<typeOfLrcHandle>(lrcHandle0);
    lrcHandleTemp[1] = static_cast<typeOfLrcHandle>(lrcHandle1);
    lrcHandleTemp[2] = static_cast<typeOfLrcHandle>(lrcHandle2);

    drm_xe_eudebug_event_exec_queue *execQueue = reinterpret_cast<drm_xe_eudebug_event_exec_queue *>(&execQueueData);
    execQueue->base.type = DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE;
    execQueue->base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    execQueue->client_handle = client1.client_handle;
    execQueue->vm_handle = 0x1234;
    execQueue->exec_queue_handle = 0x100;
    execQueue->engine_class = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    memcpy(lrcHandle, lrcHandleTemp, sizeof(lrcHandleTemp));
    session->handleEvent(&execQueue->base);
    EXPECT_NE(session->clientHandleToConnection.find(execQueue->client_handle), session->clientHandleToConnection.end());
    EXPECT_EQ(session->clientHandleToConnection[execQueue->client_handle]->execQueues[execQueue->exec_queue_handle].vmHandle,
              execQueue->vm_handle);
    EXPECT_EQ(session->clientHandleToConnection[execQueue->client_handle]->execQueues[execQueue->exec_queue_handle].engineClass,
              DRM_XE_ENGINE_CLASS_COMPUTE);
    for (uint16_t idx = 0; idx < execQueue->width; idx++) {
        EXPECT_EQ(session->clientHandleToConnection[execQueue->client_handle]->lrcHandleToVmHandle[execQueue->lrc_handle[idx]],
                  execQueue->vm_handle);
    }
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugExecQueueEventWithEventDestroyFlagWhenHandleEventThenExecQueueIsDestroyed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = 0x123456789;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    uint64_t execQueueData[sizeof(drm_xe_eudebug_event_exec_queue) / sizeof(uint64_t) + 3 * sizeof(typeOfLrcHandle)];
    auto *lrcHandle = reinterpret_cast<typeOfLrcHandle *>(ptrOffset(execQueueData, sizeof(drm_xe_eudebug_event_exec_queue)));
    typeOfLrcHandle lrcHandleTemp[3];
    const uint64_t lrcHandle0 = 2;
    const uint64_t lrcHandle1 = 3;
    const uint64_t lrcHandle2 = 5;
    lrcHandleTemp[0] = static_cast<typeOfLrcHandle>(lrcHandle0);
    lrcHandleTemp[1] = static_cast<typeOfLrcHandle>(lrcHandle1);
    lrcHandleTemp[2] = static_cast<typeOfLrcHandle>(lrcHandle2);

    // ExecQueue create event handle
    drm_xe_eudebug_event_exec_queue *execQueue = reinterpret_cast<drm_xe_eudebug_event_exec_queue *>(&execQueueData);
    execQueue->base.type = DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE;
    execQueue->base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    execQueue->client_handle = client1.client_handle;
    execQueue->vm_handle = 0x1234;
    execQueue->exec_queue_handle = 0x100;
    execQueue->engine_class = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    memcpy(lrcHandle, lrcHandleTemp, sizeof(lrcHandleTemp));
    session->handleEvent(&execQueue->base);
    EXPECT_NE(session->clientHandleToConnection.find(execQueue->client_handle), session->clientHandleToConnection.end());
    EXPECT_EQ(session->clientHandleToConnection[execQueue->client_handle]->execQueues[execQueue->exec_queue_handle].vmHandle,
              execQueue->vm_handle);
    EXPECT_EQ(session->clientHandleToConnection[execQueue->client_handle]->execQueues[execQueue->exec_queue_handle].engineClass,
              DRM_XE_ENGINE_CLASS_COMPUTE);
    for (uint16_t idx = 0; idx < execQueue->width; idx++) {
        EXPECT_EQ(session->clientHandleToConnection[execQueue->client_handle]->lrcHandleToVmHandle[execQueue->lrc_handle[idx]],
                  execQueue->vm_handle);
    }

    // ExecQueue Destroy event handle
    execQueue->base.type = DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE;
    execQueue->base.flags = DRM_XE_EUDEBUG_EVENT_DESTROY;
    execQueue->client_handle = client1.client_handle;
    execQueue->vm_handle = 0x1234;
    execQueue->exec_queue_handle = 0x100;
    execQueue->engine_class = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    session->handleEvent(&execQueue->base);
    EXPECT_TRUE(session->clientHandleToConnection[execQueue->client_handle]->execQueues.empty());
    EXPECT_TRUE(session->clientHandleToConnection[execQueue->client_handle]->lrcHandleToVmHandle.empty());
}

TEST_F(DebugApiLinuxTestXe, GivenExecQueueWhenGetVmHandleFromClientAndlrcHandleThenProperVmHandleReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = 0x123456789;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    uint64_t execQueueData[sizeof(drm_xe_eudebug_event_exec_queue) / sizeof(uint64_t) + 3 * sizeof(typeOfLrcHandle)];
    auto *lrcHandle = reinterpret_cast<typeOfLrcHandle *>(ptrOffset(execQueueData, sizeof(drm_xe_eudebug_event_exec_queue)));
    typeOfLrcHandle lrcHandleTemp[3];
    const uint64_t lrcHandle0 = 2;
    const uint64_t lrcHandle1 = 3;
    const uint64_t lrcHandle2 = 5;
    lrcHandleTemp[0] = static_cast<typeOfLrcHandle>(lrcHandle0);
    lrcHandleTemp[1] = static_cast<typeOfLrcHandle>(lrcHandle1);
    lrcHandleTemp[2] = static_cast<typeOfLrcHandle>(lrcHandle2);

    drm_xe_eudebug_event_exec_queue *execQueue = reinterpret_cast<drm_xe_eudebug_event_exec_queue *>(&execQueueData);
    execQueue->base.type = DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE;
    execQueue->base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    execQueue->client_handle = client1.client_handle;
    execQueue->vm_handle = 0x1234;
    execQueue->exec_queue_handle = 0x100;
    execQueue->engine_class = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    memcpy(lrcHandle, lrcHandleTemp, sizeof(lrcHandleTemp));
    session->handleEvent(&execQueue->base);

    EXPECT_EQ(execQueue->vm_handle, session->getVmHandleFromClientAndlrcHandle(execQueue->client_handle, lrcHandleTemp[0]));
    EXPECT_EQ(DebugSessionLinux::invalidHandle, session->getVmHandleFromClientAndlrcHandle(0x1234567, lrcHandleTemp[0]));
    EXPECT_EQ(DebugSessionLinux::invalidHandle, session->getVmHandleFromClientAndlrcHandle(execQueue->client_handle, 7));
}

TEST_F(DebugApiLinuxTestXe, whenGetThreadStateMutexForTileSessionThenNullLockIsReceived) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);
    std::unique_lock<std::mutex> lock;
    EXPECT_FALSE(session->getThreadStateMutexForTileSession(0).owns_lock());
}

TEST_F(DebugApiLinuxTestXe, GivenNewlyStoppedThreadsAndNoExpectedAttentionEventsWhenCheckTriggerEventsForTileSessionThenTriggerEventsDontChange) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    EuThread::ThreadId threadId{0, 0, 0, 0, 0};
    session->addThreadToNewlyStoppedFromRaisedAttentionForTileSession(threadId, 0x12345678, nullptr, 0);
    EXPECT_EQ(session->countToAddThreadToNewlyStoppedFromRaisedAttentionForTileSession, 1u);
    session->newlyStoppedThreads.push_back(threadId);
    session->expectedAttentionEvents = 0;
    session->triggerEvents = false;
    session->checkTriggerEventsForAttentionForTileSession(0);
    EXPECT_FALSE(session->triggerEvents);
}

TEST_F(DebugApiLinuxTestXe, whenHandleExecQueueEventThenProcessEnterAndProcessExitEventTriggeredUponFirstExecQueueCreateAndLastExecQueueDestroyRespectively) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();

    // First client connection
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = 0x123456789;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    uint64_t execQueueData[sizeof(drm_xe_eudebug_event_exec_queue) / sizeof(uint64_t) + 3 * sizeof(typeOfLrcHandle)];
    auto *lrcHandle = reinterpret_cast<typeOfLrcHandle *>(ptrOffset(execQueueData, sizeof(drm_xe_eudebug_event_exec_queue)));
    typeOfLrcHandle lrcHandleTemp[3];
    const uint64_t lrcHandle0 = 2;
    const uint64_t lrcHandle1 = 3;
    const uint64_t lrcHandle2 = 5;
    lrcHandleTemp[0] = static_cast<typeOfLrcHandle>(lrcHandle0);
    lrcHandleTemp[1] = static_cast<typeOfLrcHandle>(lrcHandle1);
    lrcHandleTemp[2] = static_cast<typeOfLrcHandle>(lrcHandle2);

    drm_xe_eudebug_event_exec_queue *execQueue = reinterpret_cast<drm_xe_eudebug_event_exec_queue *>(&execQueueData);
    execQueue->base.type = DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE;
    execQueue->base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    execQueue->client_handle = client1.client_handle;
    execQueue->vm_handle = 0x1234;
    execQueue->exec_queue_handle = 0x100;
    execQueue->engine_class = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    memcpy(lrcHandle, lrcHandleTemp, sizeof(lrcHandleTemp));
    session->handleEvent(&execQueue->base); // ExecQueue create event handle for first client

    zet_debug_event_t event1 = {};
    ze_result_t result = zetDebugReadEvent(session->toHandle(), 0, &event1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY, event1.type);

    // second client connection
    drm_xe_eudebug_event_client client2;
    client2.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client2.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client2.client_handle = 0x123456788;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client2));

    execQueue->client_handle = client2.client_handle;
    execQueue->vm_handle = 0x1235;
    execQueue->exec_queue_handle = 0x101;
    session->handleEvent(&execQueue->base); // ExecQueue1 create event handle for second client

    zet_debug_event_t event2 = {};
    result = zetDebugReadEvent(session->toHandle(), 0, &event2);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    execQueue->client_handle = client2.client_handle;
    execQueue->vm_handle = 0x1236;
    execQueue->exec_queue_handle = 0x102;
    session->handleEvent(&execQueue->base); // ExecQueue2 create event handle for second client
    result = zetDebugReadEvent(session->toHandle(), 0, &event2);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    // ExecQueue Destroy event handle
    execQueue->base.type = DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE;
    execQueue->base.flags = DRM_XE_EUDEBUG_EVENT_DESTROY;
    execQueue->client_handle = client1.client_handle;
    execQueue->vm_handle = 0x1234;
    execQueue->exec_queue_handle = 0x100;
    execQueue->engine_class = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    session->handleEvent(&execQueue->base);
    result = zetDebugReadEvent(session->toHandle(), 0, &event2);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    execQueue->client_handle = client2.client_handle;
    execQueue->vm_handle = 0x1235;
    execQueue->exec_queue_handle = 0x101;
    session->handleEvent(&execQueue->base);
    result = zetDebugReadEvent(session->toHandle(), 0, &event2);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    execQueue->client_handle = client2.client_handle;
    execQueue->vm_handle = 0x1236;
    execQueue->exec_queue_handle = 0x102;
    session->handleEvent(&execQueue->base);
    result = zetDebugReadEvent(session->toHandle(), 0, &event2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT, event2.type);
}

TEST_F(DebugApiLinuxTestXe, GivenMetadataEventWhenHandlingAndMetadataLengthIsZeroThenMetadataIsInsertedToMap) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    drm_xe_eudebug_event_metadata metadata = {};
    metadata.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadata.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadata.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadata_handle = 2;
    metadata.len = 0;

    auto handler = new MockIoctlHandlerXe;
    handler->eventQueue.push({reinterpret_cast<char *>(&metadata), static_cast<uint64_t>(metadata.base.len)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);
    session->handleEvent(&metadata.base);

    EXPECT_EQ(1u, session->clientHandleToConnection[metadata.client_handle]->metaDataMap.size());
    EXPECT_NE(session->clientHandleToConnection[metadata.client_handle]->metaDataMap.end(), session->clientHandleToConnection[metadata.client_handle]->metaDataMap.find(metadata.metadata_handle));

    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_DESTROY;
    session->handleEvent(&metadata.base);
    EXPECT_NE(session->clientHandleToConnection[metadata.client_handle]->metaDataMap.end(), session->clientHandleToConnection[metadata.client_handle]->metaDataMap.find(metadata.metadata_handle));
}

TEST_F(DebugApiLinuxTestXe, GivenMetadataCreateEventWhenHandlingAndIoctlFailsThenEventHandlingCallImmediatelyReturns) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    drm_xe_eudebug_event_metadata metadata = {};
    metadata.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadata.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadata.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadata_handle = 2;
    metadata.len = 5;

    auto handler = new MockIoctlHandlerXe;
    handler->eventQueue.push({reinterpret_cast<char *>(&metadata), static_cast<uint64_t>(metadata.base.len)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);
    session->handleEvent(&metadata.base);

    EXPECT_EQ(0u, session->clientHandleToConnection[metadata.client_handle]->metaDataMap.size());
    EXPECT_EQ(session->clientHandleToConnection[metadata.client_handle]->metaDataMap.end(), session->clientHandleToConnection[metadata.client_handle]->metaDataMap.find(metadata.metadata_handle));

    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_DESTROY;
    session->handleEvent(&metadata.base);
    EXPECT_NE(session->clientHandleToConnection[metadata.client_handle]->metaDataMap.end(), session->clientHandleToConnection[metadata.client_handle]->metaDataMap.find(metadata.metadata_handle));
}

TEST_F(DebugApiLinuxTestXe, GivenMetadataCreateEventForL0ZebinModuleWhenHandlingEventThenKernelCountFromReadMetadataIsRead) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    uint32_t kernelCount = 5;
    drm_xe_eudebug_event_metadata metadata;
    metadata.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadata.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadata.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadata_handle = 3;
    metadata.type = DRM_XE_DEBUG_METADATA_PROGRAM_MODULE;
    metadata.len = sizeof(kernelCount);

    drm_xe_eudebug_read_metadata readMetadata{};
    readMetadata.ptr = reinterpret_cast<uint64_t>(&kernelCount);
    readMetadata.size = sizeof(kernelCount);
    readMetadata.metadata_handle = 3;
    handler->returnMetadata = &readMetadata;
    session->handleEvent(&metadata.base);

    EXPECT_NE(session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule.end(),
              session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule.find(metadata.metadata_handle));

    EXPECT_EQ(kernelCount, session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule[metadata.metadata_handle].segmentCount);

    EXPECT_EQ(metadata.metadata_handle, session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataMap[metadata.metadata_handle].metadata.metadata_handle);
    EXPECT_NE(nullptr, session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataMap[metadata.metadata_handle].data);
    EXPECT_EQ(sizeof(kernelCount), session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataMap[metadata.metadata_handle].metadata.len);

    EXPECT_EQ(1, handler->ioctlCalled);

    // inject module segment
    auto &module = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule[metadata.metadata_handle];
    module.loadAddresses[0].insert(0x12340000);

    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_DESTROY;
    metadata.len = 0;
    session->handleEvent(&metadata.base);

    EXPECT_EQ(session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule.end(),
              session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule.find(metadata.metadata_handle));
}

TEST_F(DebugApiLinuxTestXe, GivenMetadataEventWhenHandlingEventThenGpuAddressIsSavedFromReadMetadata) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinuxXe::invalidClientHandle;

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    uint64_t sbaAddress = 0x1234000;
    uint64_t moduleDebugAddress = 0x34567000;
    uint64_t contextSaveAddress = 0x56789000;

    drm_xe_eudebug_event_metadata metadataSba = {};
    metadataSba.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadataSba.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadataSba.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadataSba.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    metadataSba.metadata_handle = 4;
    metadataSba.type = WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA;
    metadataSba.len = sizeof(sbaAddress);

    drm_xe_eudebug_read_metadata readMetadata = {};
    readMetadata.ptr = reinterpret_cast<uint64_t>(&sbaAddress);
    readMetadata.size = sizeof(sbaAddress);
    readMetadata.metadata_handle = 4;

    handler->returnMetadata = &readMetadata;
    session->handleEvent(&metadataSba.base);

    drm_xe_eudebug_event_metadata metadataModule = {};
    metadataModule.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadataModule.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadataModule.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadataModule.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    metadataModule.metadata_handle = 5;
    metadataModule.type = WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA;
    metadataModule.len = sizeof(moduleDebugAddress);

    readMetadata = {};
    readMetadata.ptr = reinterpret_cast<uint64_t>(&moduleDebugAddress);
    readMetadata.size = sizeof(moduleDebugAddress);
    readMetadata.metadata_handle = 5;

    handler->returnMetadata = &readMetadata;
    session->handleEvent(&metadataModule.base);

    drm_xe_eudebug_event_metadata metadataContextSave = {};

    metadataContextSave.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadataContextSave.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadataContextSave.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadataContextSave.client_handle = MockDebugSessionLinuxXe::mockClientHandle,
    metadataContextSave.metadata_handle = 6,
    metadataContextSave.type = WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA;
    metadataContextSave.len = sizeof(contextSaveAddress);

    readMetadata = {};
    readMetadata.ptr = reinterpret_cast<uint64_t>(&contextSaveAddress);
    readMetadata.size = sizeof(contextSaveAddress);
    readMetadata.metadata_handle = 6;

    handler->returnMetadata = &readMetadata;
    session->handleEvent(&metadataContextSave.base);

    EXPECT_EQ(moduleDebugAddress, session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->moduleDebugAreaGpuVa);
    EXPECT_EQ(sbaAddress, session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->stateBaseAreaGpuVa);
    EXPECT_EQ(contextSaveAddress, session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->contextStateSaveAreaGpuVa);
}

TEST_F(DebugApiLinuxTestXe, GivenVmBindEventWhenHandlingEventThenVmBindMapIsCorrectlyUpdated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);
    session->clientHandle = DebugSessionLinuxXe::invalidClientHandle;

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = 0x123456789;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    drm_xe_eudebug_event_vm_bind vmBind1{};
    vmBind1.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND;
    vmBind1.base.flags = DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    vmBind1.base.len = sizeof(drm_xe_eudebug_event_vm_bind);
    vmBind1.base.seqno = 1;
    vmBind1.client_handle = client1.client_handle;
    vmBind1.flags = DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    vmBind1.num_binds = 2;

    EXPECT_EQ(session->clientHandleToConnection[client1.client_handle]->vmBindMap.size(), 0u);
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind1.base));

    vmBind1.base.seqno = 2;
    vmBind1.client_handle = client1.client_handle;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind1.base));

    EXPECT_EQ(session->clientHandleToConnection[client1.client_handle]->vmBindMap.size(), 2u);
}

TEST_F(DebugApiLinuxTestXe, GivenVmBindOpEventWhenHandlingEventThenVmBindMapIsCorrectlyUpdated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    drm_xe_eudebug_event_vm_bind vmBind{};
    vmBind.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND;
    vmBind.base.flags = DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    vmBind.base.len = sizeof(drm_xe_eudebug_event_vm_bind);
    vmBind.base.seqno = 1;
    vmBind.client_handle = client1.client_handle;
    vmBind.flags = DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    vmBind.num_binds = 2;

    auto &vmBindMap = session->clientHandleToConnection[client1.client_handle]->vmBindMap;
    EXPECT_EQ(vmBindMap.size(), 0u);
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind.base));
    EXPECT_EQ(vmBindMap.size(), 1u);
    EXPECT_EQ(vmBindMap[vmBind.base.seqno].pendingNumBinds, 2ull);

    drm_xe_eudebug_event_vm_bind_op vmBindOp{};
    vmBindOp.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_OP;
    vmBindOp.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vmBindOp.base.len = sizeof(drm_xe_eudebug_event_vm_bind_op);
    vmBindOp.base.seqno = 2;
    vmBindOp.num_extensions = 2;
    vmBindOp.addr = 0xffff1234;
    vmBindOp.range = 65536;
    vmBindOp.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOp.base));
    EXPECT_EQ(vmBindMap[vmBind.base.seqno].pendingNumBinds, 1ull);
    EXPECT_EQ(session->clientHandleToConnection[client1.client_handle]->vmBindIdentifierMap[vmBindOp.base.seqno], vmBindOp.vm_bind_ref_seqno);

    auto &vmBindOpData = vmBindMap[vmBindOp.vm_bind_ref_seqno].vmBindOpMap[vmBindOp.base.seqno];
    EXPECT_EQ(vmBindOpData.pendingNumExtensions, vmBindOp.num_extensions);
    EXPECT_TRUE(0 == std::memcmp(&vmBindOp, &vmBindOpData.vmBindOp, sizeof(drm_xe_eudebug_event_vm_bind_op)));
}

class DebugApiLinuxTestXeMetadataOpEventTest : public DebugApiLinuxTestXe,
                                               public ::testing::WithParamInterface<uint64_t> {
  public:
    uint64_t metadataType;
    DebugApiLinuxTestXeMetadataOpEventTest() {
        metadataType = GetParam();
    }
    ~DebugApiLinuxTestXeMetadataOpEventTest() override {}
};
TEST_P(DebugApiLinuxTestXeMetadataOpEventTest, GivenVmBindOpMetadataEventForMetadataAreaWhenHandlingEventThenMapIsCorrectlyUpdated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    auto &connection = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    drm_xe_eudebug_event_metadata metadata{};
    metadata.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadata.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadata.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadata_handle = 10;
    metadata.type = metadataType;
    metadata.len = sizeof(metadata);
    connection->metaDataMap[10].metadata = metadata;

    drm_xe_eudebug_event_vm_bind vmBind{};
    vmBind.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND;
    vmBind.base.flags = DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    vmBind.base.len = sizeof(drm_xe_eudebug_event_vm_bind);
    vmBind.base.seqno = 1;
    vmBind.client_handle = client1.client_handle;
    vmBind.vm_handle = 0x1234;
    vmBind.flags = DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    vmBind.num_binds = 1;

    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind.base));
    auto &vmBindMap = session->clientHandleToConnection[client1.client_handle]->vmBindMap;

    drm_xe_eudebug_event_vm_bind_op vmBindOp{};
    vmBindOp.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_OP;
    vmBindOp.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vmBindOp.base.len = sizeof(drm_xe_eudebug_event_vm_bind_op);
    vmBindOp.base.seqno = 2;
    vmBindOp.num_extensions = 1;
    vmBindOp.addr = 0xffff1234;
    vmBindOp.range = 1000;
    vmBindOp.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOp.base));

    auto &vmBindOpData = vmBindMap[vmBindOp.vm_bind_ref_seqno].vmBindOpMap[vmBindOp.base.seqno];
    drm_xe_eudebug_event_vm_bind_op_metadata vmBindOpMetadata{};
    vmBindOpMetadata.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA;
    vmBindOpMetadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vmBindOpMetadata.base.len = sizeof(drm_xe_eudebug_event_vm_bind_op_metadata);
    vmBindOpMetadata.base.seqno = 3;
    vmBindOpMetadata.vm_bind_op_ref_seqno = vmBindOp.base.seqno;
    vmBindOpMetadata.metadata_handle = 10;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOpMetadata.base));

    EXPECT_EQ(vmBindOpData.pendingNumExtensions, 0ull);
    EXPECT_EQ(vmBindOpData.vmBindOpMetadataVec.size(), 1ull);

    drm_xe_eudebug_event_vm_bind_ufence vmBindUfence{};
    vmBindUfence.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE;
    vmBindUfence.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE | DRM_XE_EUDEBUG_EVENT_NEED_ACK;
    vmBindUfence.base.len = sizeof(drm_xe_eudebug_event_vm_bind_ufence);
    vmBindUfence.base.seqno = 4;
    vmBindUfence.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindUfence.base));

    if (metadataType == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA) {
        EXPECT_EQ(connection->vmToStateBaseAreaBindInfo[0x1234].gpuVa, vmBindOp.addr);
        EXPECT_EQ(connection->vmToStateBaseAreaBindInfo[0x1234].size, vmBindOp.range);
    }

    if (metadataType == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA) {
        EXPECT_EQ(connection->vmToModuleDebugAreaBindInfo[0x1234].gpuVa, vmBindOp.addr);
        EXPECT_EQ(connection->vmToModuleDebugAreaBindInfo[0x1234].size, vmBindOp.range);
    }

    if (metadataType == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA) {
        EXPECT_EQ(connection->vmToContextStateSaveAreaBindInfo[0x1234].gpuVa, vmBindOp.addr);
        EXPECT_EQ(connection->vmToContextStateSaveAreaBindInfo[0x1234].size, vmBindOp.range);
    }

    if (metadataType == DRM_XE_DEBUG_METADATA_ELF_BINARY) {
        EXPECT_EQ(connection->isaMap[0][vmBindOp.addr]->bindInfo.gpuVa, vmBindOp.addr);
        EXPECT_EQ(connection->isaMap[0][vmBindOp.addr]->bindInfo.size, vmBindOp.range);
        EXPECT_FALSE(connection->isaMap[0][vmBindOp.addr]->tileInstanced);
        EXPECT_FALSE(connection->isaMap[0][vmBindOp.addr]->perKernelModule);
    }
}

static uint64_t metadataTypes[] =
    {
        WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA,
        WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA,
        WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA,
        DRM_XE_DEBUG_METADATA_ELF_BINARY};

INSTANTIATE_TEST_SUITE_P(
    MetadataOpEventUfenceTest,
    DebugApiLinuxTestXeMetadataOpEventTest,
    testing::ValuesIn(metadataTypes));

TEST_F(DebugApiLinuxTestXe, GivenVmBindOpMetadataCreateEventAndUfenceForProgramModuleWhenHandlingEventThenEventIsAcked) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    auto &connection = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    drm_xe_eudebug_event_metadata metadata{};
    metadata.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadata.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadata.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadata_handle = 10;
    metadata.type = DRM_XE_DEBUG_METADATA_PROGRAM_MODULE;
    metadata.len = sizeof(metadata);
    connection->metaDataMap[10].metadata = metadata;
    connection->metaDataToModule[10].segmentCount = 1;

    metadata.metadata_handle = 11;
    metadata.type = DRM_XE_DEBUG_METADATA_ELF_BINARY;
    metadata.len = sizeof(metadata);
    connection->metaDataMap[11].metadata = metadata;
    auto ptr = std::make_unique<char[]>(metadata.len);
    connection->metaDataMap[11].data = std::move(ptr);
    connection->metaDataMap[11].metadata.len = 10000;

    drm_xe_eudebug_event_vm_bind vmBind{};
    vmBind.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND;
    vmBind.base.flags = DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    vmBind.base.len = sizeof(drm_xe_eudebug_event_vm_bind);
    vmBind.base.seqno = 1;
    vmBind.client_handle = client1.client_handle;
    vmBind.vm_handle = 0x1234;
    vmBind.flags = DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    vmBind.num_binds = 1;

    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind.base));
    auto &vmBindMap = session->clientHandleToConnection[client1.client_handle]->vmBindMap;

    drm_xe_eudebug_event_vm_bind_op vmBindOp{};
    vmBindOp.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_OP;
    vmBindOp.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vmBindOp.base.len = sizeof(drm_xe_eudebug_event_vm_bind_op);
    vmBindOp.base.seqno = 2;
    vmBindOp.num_extensions = 2;
    vmBindOp.addr = 0xffff1234;
    vmBindOp.range = 1000;
    vmBindOp.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOp.base));

    drm_xe_eudebug_event_vm_bind_ufence vmBindUfence{};
    vmBindUfence.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE;
    vmBindUfence.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE | DRM_XE_EUDEBUG_EVENT_NEED_ACK;
    vmBindUfence.base.len = sizeof(drm_xe_eudebug_event_vm_bind_ufence);
    vmBindUfence.base.seqno = 3;
    vmBindUfence.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindUfence.base));

    auto &vmBindOpData = vmBindMap[vmBindOp.vm_bind_ref_seqno].vmBindOpMap[vmBindOp.base.seqno];
    drm_xe_eudebug_event_vm_bind_op_metadata vmBindOpMetadata{};
    vmBindOpMetadata.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA;
    vmBindOpMetadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vmBindOpMetadata.base.len = sizeof(drm_xe_eudebug_event_vm_bind_op_metadata);
    vmBindOpMetadata.base.seqno = 4;
    vmBindOpMetadata.vm_bind_op_ref_seqno = vmBindOp.base.seqno;
    vmBindOpMetadata.metadata_handle = 10;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOpMetadata.base));

    vmBindOpMetadata.base.seqno = 5;
    vmBindOpMetadata.metadata_handle = 11;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOpMetadata.base));

    EXPECT_EQ(vmBindOpData.pendingNumExtensions, 0ull);
    EXPECT_EQ(vmBindOpData.vmBindOpMetadataVec.size(), 2ull);

    EXPECT_EQ(connection->metaDataToModule[10].ackEvents->size(), 1ull);
    EXPECT_EQ(connection->metaDataToModule[10].ackEvents[0][0].seqno, vmBindUfence.base.seqno);
    EXPECT_EQ(connection->metaDataToModule[10].ackEvents[0][0].type, vmBindUfence.base.type);

    auto event = session->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(reinterpret_cast<uint64_t>(connection->metaDataMap[11].data.get()), event.info.module.moduleBegin);
    EXPECT_EQ(reinterpret_cast<uint64_t>(connection->metaDataMap[11].data.get()) + connection->metaDataMap[11].metadata.len, event.info.module.moduleEnd);
}

TEST_F(DebugApiLinuxTestXe, GivenVmUnbindForLastIsaSegmentThenL0ModuleUnloadEventSent) {

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    uint64_t seg1Va = 0xffff1234;
    uint64_t seg2Va = 0xffff5678;
    uint64_t seqno = 0;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    auto &connection = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    drm_xe_eudebug_event_metadata metadata{};
    metadata.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadata.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadata.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadata_handle = 10;
    metadata.type = DRM_XE_DEBUG_METADATA_PROGRAM_MODULE;
    metadata.len = sizeof(metadata);
    connection->metaDataMap[10].metadata = metadata;
    connection->metaDataToModule[10].segmentCount = 2;

    metadata.metadata_handle = 11;
    metadata.type = DRM_XE_DEBUG_METADATA_ELF_BINARY;
    metadata.len = sizeof(metadata);
    connection->metaDataMap[11].metadata = metadata;
    auto ptr = std::make_unique<char[]>(metadata.len);
    connection->metaDataMap[11].data = std::move(ptr);
    connection->metaDataMap[11].metadata.len = 10000;

    drm_xe_eudebug_event_vm_bind vmBind{};
    vmBind.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND;
    vmBind.base.flags = DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    vmBind.base.len = sizeof(drm_xe_eudebug_event_vm_bind);
    vmBind.base.seqno = seqno++;
    vmBind.client_handle = client1.client_handle;
    vmBind.vm_handle = 0x1234;
    vmBind.flags = DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    vmBind.num_binds = 1;

    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind.base));
    auto &vmBindMap = session->clientHandleToConnection[client1.client_handle]->vmBindMap;

    drm_xe_eudebug_event_vm_bind_op vmBindOp{};
    vmBindOp.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_OP;
    vmBindOp.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vmBindOp.base.len = sizeof(drm_xe_eudebug_event_vm_bind_op);
    vmBindOp.base.seqno = seqno++;
    vmBindOp.num_extensions = 2;
    vmBindOp.addr = seg1Va;
    vmBindOp.range = 1000;
    vmBindOp.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOp.base));

    drm_xe_eudebug_event_vm_bind_ufence vmBindUfence{};
    vmBindUfence.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE;
    vmBindUfence.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE | DRM_XE_EUDEBUG_EVENT_NEED_ACK;
    vmBindUfence.base.len = sizeof(drm_xe_eudebug_event_vm_bind_ufence);
    vmBindUfence.base.seqno = seqno++;
    vmBindUfence.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindUfence.base));

    auto &vmBindOpData = vmBindMap[vmBindOp.vm_bind_ref_seqno].vmBindOpMap[vmBindOp.base.seqno];
    drm_xe_eudebug_event_vm_bind_op_metadata vmBindOpMetadata{};
    vmBindOpMetadata.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA;
    vmBindOpMetadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vmBindOpMetadata.base.len = sizeof(drm_xe_eudebug_event_vm_bind_op_metadata);
    vmBindOpMetadata.base.seqno = seqno++;
    vmBindOpMetadata.vm_bind_op_ref_seqno = vmBindOp.base.seqno;
    vmBindOpMetadata.metadata_handle = 10;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOpMetadata.base));

    vmBindOpMetadata.base.seqno = seqno++;
    vmBindOpMetadata.metadata_handle = 11;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOpMetadata.base));

    ASSERT_EQ(vmBindOpData.pendingNumExtensions, 0ull);
    ASSERT_EQ(vmBindOpData.vmBindOpMetadataVec.size(), 2ull);
    EXPECT_EQ(session->apiEvents.size(), 0u);

    // Now bind 2nd segment
    vmBind.base.seqno = seqno++;
    vmBind.vm_handle = 0x1234;
    vmBind.flags = DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    vmBind.num_binds = 1;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind.base));
    vmBindOp.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vmBindOp.base.seqno = seqno++;
    vmBindOp.num_extensions = 2;
    vmBindOp.vm_bind_ref_seqno = vmBind.base.seqno;
    vmBindOp.addr = seg2Va;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOp.base));
    vmBindOpMetadata.vm_bind_op_ref_seqno = vmBindOp.base.seqno;
    vmBindOpMetadata.base.seqno = seqno++;
    vmBindOpMetadata.metadata_handle = 10;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOpMetadata.base));
    vmBindOpMetadata.base.seqno = seqno++;
    vmBindOpMetadata.metadata_handle = 11;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOpMetadata.base));
    vmBindUfence.base.seqno = seqno++;
    vmBindUfence.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindUfence.base));

    auto event = session->apiEvents.front();
    ASSERT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    session->apiEvents.pop();

    // now do unbinds
    vmBind.base.seqno = seqno++;
    vmBind.vm_handle = 0x1234;
    vmBind.flags = DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    vmBind.num_binds = 1;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind.base));
    vmBindOp.base.flags = DRM_XE_EUDEBUG_EVENT_DESTROY;
    vmBindOp.base.seqno = seqno++;
    vmBindOp.num_extensions = 0;
    vmBindOp.vm_bind_ref_seqno = vmBind.base.seqno;
    vmBindOp.addr = seg2Va;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOp.base));
    vmBindUfence.base.seqno = seqno++;
    vmBindUfence.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindUfence.base));
    EXPECT_EQ(session->apiEvents.size(), 0u);

    vmBind.base.seqno = seqno++;
    vmBind.vm_handle = 0x555; // Unbind unmatched VM
    vmBind.flags = DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    vmBind.num_binds = 1;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind.base));
    vmBindOp.base.flags = DRM_XE_EUDEBUG_EVENT_DESTROY;
    vmBindOp.base.seqno = seqno++;
    vmBindOp.num_extensions = 0;
    vmBindOp.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOp.base));
    vmBindUfence.base.seqno = seqno++;
    vmBindUfence.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindUfence.base));
    // Unmatched vmHandle should not trigger any events
    EXPECT_EQ(session->apiEvents.size(), 0u);

    // now unbind final segment
    vmBind.base.seqno = seqno++;
    vmBind.vm_handle = 0x1234;
    vmBind.flags = DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    vmBind.num_binds = 1;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind.base));
    vmBindOp.base.flags = DRM_XE_EUDEBUG_EVENT_DESTROY;
    vmBindOp.base.seqno = seqno++;
    vmBindOp.num_extensions = 0;
    vmBindOp.vm_bind_ref_seqno = vmBind.base.seqno;
    vmBindOp.addr = seg1Va;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOp.base));
    vmBindUfence.base.seqno = seqno++;
    vmBindUfence.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindUfence.base));

    event = session->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD, event.type);
}

TEST_F(DebugApiLinuxTestXe, GivenVmBindOpMetadataEventAndUfenceNotProvidedForProgramModuleWhenHandlingEventThenEventIsNotAckedButHandled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    session->clientHandleToConnection.clear();
    drm_xe_eudebug_event_client client1;
    client1.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client1.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    client1.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&client1));

    auto &connection = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    drm_xe_eudebug_event_metadata metadata{};
    metadata.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadata.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadata.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadata_handle = 10;
    metadata.type = DRM_XE_DEBUG_METADATA_PROGRAM_MODULE;
    metadata.len = sizeof(metadata);
    connection->metaDataMap[10].metadata = metadata;

    metadata.metadata_handle = 11;
    metadata.type = WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA;
    connection->metaDataMap[11].metadata = metadata;
    metadata.metadata_handle = 12;
    metadata.type = WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA;
    connection->metaDataMap[12].metadata = metadata;
    connection->metaDataToModule[10].segmentCount = 1;

    drm_xe_eudebug_event_vm_bind vmBind{};
    vmBind.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND;
    vmBind.base.flags = DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    vmBind.base.len = sizeof(drm_xe_eudebug_event_vm_bind);
    vmBind.base.seqno = 1;
    vmBind.client_handle = client1.client_handle;
    vmBind.vm_handle = 0x1234;
    vmBind.num_binds = 2;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBind.base));
    auto &vmBindMap = session->clientHandleToConnection[client1.client_handle]->vmBindMap;

    drm_xe_eudebug_event_vm_bind_op vmBindOp{};
    // first vm_bind_op
    vmBindOp.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_OP;
    vmBindOp.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vmBindOp.base.len = sizeof(drm_xe_eudebug_event_vm_bind_op);
    vmBindOp.base.seqno = 2;
    vmBindOp.num_extensions = 2;
    vmBindOp.addr = 0xffff1234;
    vmBindOp.range = 1000;
    vmBindOp.vm_bind_ref_seqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOp.base));

    auto &vmBindOpData = vmBindMap[vmBindOp.vm_bind_ref_seqno].vmBindOpMap[vmBindOp.base.seqno];
    drm_xe_eudebug_event_vm_bind_op_metadata vmBindOpMetadata{};
    vmBindOpMetadata.base.type = DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA;
    vmBindOpMetadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    vmBindOpMetadata.base.len = sizeof(drm_xe_eudebug_event_vm_bind_op_metadata);
    vmBindOpMetadata.base.seqno = 3;
    vmBindOpMetadata.vm_bind_op_ref_seqno = vmBindOp.base.seqno;
    vmBindOpMetadata.metadata_handle = 10;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOpMetadata.base));

    vmBindOpMetadata.base.seqno = 4;
    vmBindOpMetadata.metadata_handle = 11;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOpMetadata.base));

    // second vm_bind_op
    vmBindOp.base.seqno = 5;
    vmBindOp.num_extensions = 1;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOp.base));

    vmBindOpMetadata.vm_bind_op_ref_seqno = 5;
    vmBindOpMetadata.base.seqno = 6;
    vmBindOpMetadata.metadata_handle = 12;
    session->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(&vmBindOpMetadata.base));

    EXPECT_EQ(vmBindOpData.pendingNumExtensions, 0ull);
    EXPECT_EQ(vmBindOpData.vmBindOpMetadataVec.size(), 2ull);
    EXPECT_EQ(vmBindMap[vmBindOp.vm_bind_ref_seqno].vmBindOpMap[5].pendingNumExtensions, 0ull);
    EXPECT_EQ(vmBindMap[vmBindOp.vm_bind_ref_seqno].vmBindOpMap[5].vmBindOpMetadataVec.size(), 1ull);

    EXPECT_EQ(connection->metaDataToModule[10].ackEvents->size(), 0ull);
}

TEST_F(DebugApiLinuxTestXe, WhenCallingReadAndWriteGpuMemoryThenFsyncIsCalledTwice) {
    auto session = std::make_unique<MockDebugSessionLinuxXe>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    char output[bufferSize];
    handler->preadRetVal = bufferSize;
    auto retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(0, retVal);
    EXPECT_EQ(handler->fsyncCalled, 2);

    handler->pwriteRetVal = bufferSize;
    retVal = session->writeGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(0, retVal);
    EXPECT_EQ(handler->fsyncCalled, 4);
}

TEST_F(DebugApiLinuxTestXe, WhenCallingReadOrWriteGpuMemoryAndFsyncFailsThenErrorIsReturned) {
    auto session = std::make_unique<MockDebugSessionLinuxXe>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    handler->fsyncRetVal = -1;
    char output[bufferSize];
    handler->preadRetVal = bufferSize;
    handler->numFsyncToSucceed = 0;
    auto retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    EXPECT_EQ(handler->fsyncCalled, 1);

    handler->pwriteRetVal = bufferSize;
    handler->numFsyncToSucceed = 0;
    retVal = session->writeGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    EXPECT_EQ(handler->fsyncCalled, 2);

    handler->numFsyncToSucceed = 1;
    retVal = session->readGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    EXPECT_EQ(handler->fsyncCalled, 4);

    handler->numFsyncToSucceed = 1;
    retVal = session->writeGpuMemory(7, output, bufferSize, 0x23000);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    EXPECT_EQ(handler->fsyncCalled, 6);
}

TEST_F(DebugApiLinuxTestXe, WhenCallingThreadControlForInterruptOrAnyInvalidThreadControlCmdThenErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandlerXe;
    sessionMock->ioctlHandler.reset(handler);
    std::vector<EuThread::ThreadId> threads({});

    std::unique_ptr<uint8_t[]> bitmaskOut;
    size_t bitmaskSizeOut = 0;

    auto result = sessionMock->threadControl(threads, 0, MockDebugSessionLinuxXe::ThreadControlCmd::interrupt, bitmaskOut, bitmaskSizeOut);
    EXPECT_EQ(result, -1);
}

TEST_F(DebugApiLinuxTestXe, GivenThreadControlStoppedCalledWhenExecQueueAndLRCIsValidThenIoctlSucceeds) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandlerXe;
    sessionMock->ioctlHandler.reset(handler);

    std::unique_ptr<uint8_t[]> bitmaskOut;
    size_t bitmaskSizeOut = 0;

    sessionMock->clientHandleToConnection[sessionMock->clientHandle].reset(new MockDebugSessionLinuxXe::ClientConnectionXe);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[100].lrcHandles.push_back(10000);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[100].lrcHandles.push_back(10001);

    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[101].lrcHandles.push_back(11000);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[101].lrcHandles.push_back(11001);

    auto result = sessionMock->threadControl({}, 0, MockDebugSessionLinuxXe::ThreadControlCmd::stopped, bitmaskOut, bitmaskSizeOut);
    EXPECT_EQ(0, result);
    EXPECT_EQ(1, handler->ioctlCalled);
}

TEST_F(DebugApiLinuxTestXe, GivenErrorFromIoctlWhenCallingThreadControlThenThreadControlCallFails) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandlerXe;
    sessionMock->ioctlHandler.reset(handler);

    EuThread::ThreadId thread = {0, 0, 0, 0, 0};
    EuThread::ThreadId thread1 = {0, 0, 0, 0, 1};

    const auto memoryHandle = 1u;
    sessionMock->allThreads[thread.packed]->stopThread(memoryHandle);
    sessionMock->allThreads[thread.packed]->reportAsStopped();

    std::vector<EuThread::ThreadId> threads;
    threads.push_back(thread);
    threads.push_back(thread1);

    std::unique_ptr<uint8_t[]> bitmaskOut;
    size_t bitmaskSizeOut = 0;

    handler->ioctlRetVal = -1;

    sessionMock->clientHandleToConnection[sessionMock->clientHandle].reset(new MockDebugSessionLinuxXe::ClientConnectionXe);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[100].lrcHandles.push_back(10000);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[100].lrcHandles.push_back(10001);

    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[101].lrcHandles.push_back(11000);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[101].lrcHandles.push_back(11001);
    auto result = sessionMock->threadControl(threads, 0, MockDebugSessionLinuxXe::ThreadControlCmd::interruptAll, bitmaskOut, bitmaskSizeOut);
    EXPECT_NE(0, result);

    EXPECT_EQ(4, handler->ioctlCalled);

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    result = sessionMock->threadControl(threads, 0, MockDebugSessionLinuxXe::ThreadControlCmd::resume, bitmaskOut, bitmaskSizeOut);
    EXPECT_NE(0, result);
    if (l0GfxCoreHelper.threadResumeRequiresUnlock()) {
        EXPECT_EQ(6, handler->ioctlCalled);
    } else {
        EXPECT_EQ(5, handler->ioctlCalled);
    }

    result = sessionMock->threadControl(threads, 0, MockDebugSessionLinuxXe::ThreadControlCmd::stopped, bitmaskOut, bitmaskSizeOut);
    EXPECT_NE(0, result);
    if (l0GfxCoreHelper.threadResumeRequiresUnlock()) {
        EXPECT_EQ(10, handler->ioctlCalled);
    } else {
        EXPECT_EQ(9, handler->ioctlCalled);
    }
}

TEST_F(DebugApiLinuxTestXe, GivenSuccessFromIoctlWhenCallingThreadControlForInterruptAllThenSequenceNumbersProperlyUpdates) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandlerXe;
    sessionMock->ioctlHandler.reset(handler);
    std::vector<EuThread::ThreadId> threads({});

    std::unique_ptr<uint8_t[]> bitmaskOut;
    size_t bitmaskSizeOut = 0;

    sessionMock->clientHandleToConnection[sessionMock->clientHandle].reset(new MockDebugSessionLinuxXe::ClientConnectionXe);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[100].lrcHandles.push_back(10000);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[100].lrcHandles.push_back(10001);

    // Invoke first threadControl IOCTL for above two lrc Handles created above.
    auto result = sessionMock->threadControl(threads, 0, MockDebugSessionLinuxXe::ThreadControlCmd::interruptAll, bitmaskOut, bitmaskSizeOut);
    EXPECT_EQ(0, result);
    EXPECT_EQ(2, handler->ioctlCalled);
    EXPECT_EQ(2ul, sessionMock->euControlInterruptSeqno);

    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[101].lrcHandles.push_back(11000);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[101].lrcHandles.push_back(11001);

    // Invoke second threadControl IOCTL for total 4 lrc Handles created above.
    // Total called IOCTL count would be 4 for second threadControl invoked below + 2 for above first threadControl method.
    result = sessionMock->threadControl(threads, 0, MockDebugSessionLinuxXe::ThreadControlCmd::interruptAll, bitmaskOut, bitmaskSizeOut);
    EXPECT_EQ(0, result);

    EXPECT_EQ(6, handler->ioctlCalled);
    EXPECT_EQ(handler->euControlOutputSeqno, 6u);
}

TEST_F(DebugApiLinuxTestXe, WhenCallingThreadControlForResumeThenProperIoctlsIsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandlerXe;
    sessionMock->ioctlHandler.reset(handler);

    EuThread::ThreadId thread = {0, 0, 0, 0, 0};
    EuThread::ThreadId thread1 = {0, 0, 0, 0, 1};

    const auto memoryHandle = 1u;
    sessionMock->allThreads[thread.packed]->stopThread(memoryHandle);
    sessionMock->allThreads[thread.packed]->reportAsStopped();

    std::vector<EuThread::ThreadId> threads;
    threads.push_back(thread);
    threads.push_back(thread1);

    for (auto thread : threads) {
        sessionMock->stoppedThreads[thread.packed] = 3;
    }

    std::unique_ptr<uint8_t[]> bitmaskOut;
    size_t bitmaskSizeOut = 0;

    auto result = sessionMock->threadControl(threads, 0, MockDebugSessionLinuxXe::ThreadControlCmd::resume, bitmaskOut, bitmaskSizeOut);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    if (l0GfxCoreHelper.threadResumeRequiresUnlock()) {
        EXPECT_EQ(2, handler->ioctlCalled);
        EXPECT_EQ(uint32_t(sessionMock->getEuControlCmdUnlock()), handler->euControlArgs[0].euControl.cmd);
        EXPECT_EQ(uint32_t(DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME), handler->euControlArgs[1].euControl.cmd);
    } else {
        EXPECT_EQ(1, handler->ioctlCalled);
        EXPECT_EQ(uint32_t(DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME), handler->euControlArgs[0].euControl.cmd);
    }
    EXPECT_NE(0u, handler->euControlArgs[0].euControl.bitmask_size);
    auto bitMaskPtrToCheck = handler->euControlArgs[0].euControl.bitmask_ptr;
    EXPECT_NE(0u, bitMaskPtrToCheck);

    EXPECT_EQ(0u, bitmaskSizeOut);
    EXPECT_EQ(nullptr, bitmaskOut.get());
}

TEST_F(DebugApiLinuxTestXe, GivenNoAttentionBitsWhenMultipleThreadsPassedToCheckStoppedThreadsAndGenerateEventsThenThreadsStateNotCheckedAndEventsNotGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto handler = new MockIoctlHandlerXe;
    sessionMock->ioctlHandler.reset(handler);
    EuThread::ThreadId thread = {0, 0, 0, 0, 0};
    EuThread::ThreadId thread1 = {0, 0, 0, 0, 1};
    EuThread::ThreadId thread2 = {0, 0, 0, 0, 2};
    const auto memoryHandle = 1u;

    std::vector<EuThread::ThreadId> threads;
    threads.push_back(thread);
    threads.push_back(thread1);

    sessionMock->clientHandleToConnection[sessionMock->clientHandle].reset(new MockDebugSessionLinuxXe::ClientConnectionXe);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[100].lrcHandles.push_back(10000);
    sessionMock->clientHandleToConnection[sessionMock->clientHandle]->execQueues[100].lrcHandles.push_back(10001);

    sessionMock->allThreads[thread.packed]->verifyStopped(1);
    sessionMock->allThreads[thread.packed]->stopThread(memoryHandle);
    sessionMock->allThreads[thread.packed]->reportAsStopped();
    sessionMock->stoppedThreads[thread.packed] = 1;  // previously stopped
    sessionMock->stoppedThreads[thread1.packed] = 3; // newly stopped
    sessionMock->stoppedThreads[thread2.packed] = 3; // newly stopped

    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);
    memset(bitmask.get(), 0, bitmaskSize);

    handler->outputBitmaskSize = bitmaskSize;
    handler->outputBitmask = std::move(bitmask);

    sessionMock->checkStoppedThreadsAndGenerateEvents(threads, memoryHandle, 0);

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(1u, handler->euControlArgs.size());
    EXPECT_EQ(2u, sessionMock->numThreadsPassedToThreadControl);
    EXPECT_EQ(uint32_t(DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED), handler->euControlArgs[0].euControl.cmd);

    EXPECT_TRUE(sessionMock->allThreads[thread.packed]->isStopped());
    EXPECT_FALSE(sessionMock->allThreads[thread1.packed]->isStopped());
    EXPECT_FALSE(sessionMock->allThreads[thread2.packed]->isStopped());

    EXPECT_EQ(0u, sessionMock->apiEvents.size());
}

TEST_F(DebugApiLinuxTestXe, GivenEventSeqnoLowerEqualThanSentInterruptWhenHandlingAttentionEventThenEventIsNotProcessed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    uint8_t data[sizeof(drm_xe_eudebug_event_eu_attention) + 128];
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    std::vector<EuThread::ThreadId> threads{
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 1},
    };

    constexpr uint64_t contextHandle0 = 100ul;
    constexpr uint64_t lrcHandle0 = 1000ul;
    sessionMock->allThreads[threads[0]]->setContextHandle(contextHandle0);
    sessionMock->allThreads[threads[0]]->setLrcHandle(lrcHandle0);

    constexpr uint64_t contextHandle1 = 100ul;
    constexpr uint64_t lrcHandle1 = 1001ul;
    sessionMock->allThreads[threads[0]]->setContextHandle(contextHandle1);
    sessionMock->allThreads[threads[0]]->setLrcHandle(lrcHandle1);

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};
    sessionMock->pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread, false));
    sessionMock->interruptSent = true;
    sessionMock->euControlInterruptSeqno = 3;

    // Validate for case when event seq number is equal than euControlInterruptSeqno
    drm_xe_eudebug_event_eu_attention attention = {};
    attention.base.type = DRM_XE_EUDEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    attention.base.seqno = 3;
    attention.base.len = sizeof(drm_xe_eudebug_event_eu_attention) + std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));
    attention.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    attention.flags = 0;
    attention.exec_queue_handle = contextHandle0;
    attention.lrc_handle = lrcHandle0;
    attention.bitmask_size = std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));

    memcpy(data, &attention, sizeof(drm_xe_eudebug_event_eu_attention));
    memcpy(ptrOffset(data, offsetof(drm_xe_eudebug_event_eu_attention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    sessionMock->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(data));

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_EQ(1u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->pendingInterrupts[0].second);
    EXPECT_FALSE(sessionMock->triggerEvents);

    // Validate for case when event seq number is less than euControlInterruptSeqno
    attention.base.seqno = 2;
    memcpy(data, &attention, sizeof(drm_xe_eudebug_event_eu_attention));
    memcpy(ptrOffset(data, offsetof(drm_xe_eudebug_event_eu_attention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    sessionMock->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(data));

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_EQ(1u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->pendingInterrupts[0].second);
    EXPECT_FALSE(sessionMock->triggerEvents);
}

TEST_F(DebugApiLinuxTestXe, GivenLrcHandleEntryNotFoundInLrcToVmHandleMapWhenHandlingAttentionEventThenEventIsNotProcessed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    uint64_t execQueueHandle = 2;
    uint64_t lrcHandle = 8;

    sessionMock->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->lrcHandleToVmHandle[10] = 20ul;

    drm_xe_eudebug_event_eu_attention attention = {};
    attention.base.type = DRM_XE_EUDEBUG_EVENT_EU_ATTENTION;
    attention.base.len = sizeof(drm_xe_eudebug_event_eu_attention);
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.lrc_handle = lrcHandle;
    attention.exec_queue_handle = execQueueHandle;
    attention.bitmask_size = 0;

    sessionMock->handleEvent(&attention.base);

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
}

TEST_F(DebugApiLinuxTestXe, GivenSentInterruptWhenHandlingAttEventThenAttBitsAreSynchronouslyScannedAgainAndAllNewThreadsChecked) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    auto handler = new MockIoctlHandlerXe;
    sessionMock->ioctlHandler.reset(handler);
    SIP::version version = {2, 0, 0};
    initStateSaveArea(sessionMock->stateSaveAreaHeader, version, device);
    handler->setPreadMemory(sessionMock->stateSaveAreaHeader.data(), sessionMock->stateSaveAreaHeader.size(), 0x1000);

    uint64_t execQueueHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;
    sessionMock->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->lrcHandleToVmHandle[lrcHandle] = vmHandle;

    uint8_t data[sizeof(drm_xe_eudebug_event_eu_attention) + 128];
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;

    std::vector<EuThread::ThreadId> threads{
        {0, 0, 0, 0, 0}, {0, 0, 0, 0, 2}};

    sessionMock->allThreads[threads[0]]->setContextHandle(execQueueHandle);
    sessionMock->allThreads[threads[0]]->setLrcHandle(lrcHandle);
    sessionMock->allThreads[threads[1]]->setContextHandle(execQueueHandle);
    sessionMock->allThreads[threads[1]]->setLrcHandle(lrcHandle);

    sessionMock->stoppedThreads[threads[0].packed] = 1;
    sessionMock->stoppedThreads[threads[1].packed] = 1;

    // bitmask returned from ATT scan - both threads
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, handler->outputBitmask, handler->outputBitmaskSize);

    // bitmask returned in ATT event - only one thread
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads({threads[0]}, hwInfo, bitmask, bitmaskSize);

    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    sessionMock->pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread, false));
    sessionMock->interruptSent = true;
    sessionMock->euControlInterruptSeqno = 1;

    drm_xe_eudebug_event_eu_attention attention = {};
    attention.base.type = DRM_XE_EUDEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    attention.base.len = sizeof(drm_xe_eudebug_event_eu_attention) + std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.exec_queue_handle = execQueueHandle;

    attention.bitmask_size = std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));

    memcpy(data, &attention, sizeof(drm_xe_eudebug_event_eu_attention));
    memcpy(ptrOffset(data, offsetof(drm_xe_eudebug_event_eu_attention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    sessionMock->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(data));

    EXPECT_EQ(1u, sessionMock->newlyStoppedThreads.size());
    auto expectedThreadsToCheck = hwInfo.capabilityTable.fusedEuEnabled ? 2u : 1u;
    EXPECT_EQ(expectedThreadsToCheck, sessionMock->addThreadToNewlyStoppedFromRaisedAttentionCallCount);
    EXPECT_EQ(expectedThreadsToCheck, sessionMock->readSystemRoutineIdentFromMemoryCallCount);
    EXPECT_EQ(0u, sessionMock->readSystemRoutineIdentCallCount);
}

TEST_F(DebugApiLinuxTestXe, GivenStaleAttentionEventThenEventNotHandled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    sessionMock->newestAttSeqNo.store(10);
    drm_xe_eudebug_event_eu_attention attention = {};
    attention.base.type = DRM_XE_EUDEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    attention.base.len = sizeof(drm_xe_eudebug_event_eu_attention);
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    attention.lrc_handle = 0;
    attention.flags = 0;
    attention.exec_queue_handle = 0;
    attention.bitmask_size = 0;
    uint8_t data[sizeof(drm_xe_eudebug_event_eu_attention) + 128];
    memcpy(data, &attention, sizeof(drm_xe_eudebug_event_eu_attention));

    sessionMock->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(data));

    EXPECT_EQ(sessionMock->handleAttentionEventCalled, 0u);
}

TEST_F(DebugApiLinuxTestXe, GivenInterruptedThreadsWhenAttentionEventReceivedThenEventsTriggeredAfterExpectedAttentionEventCount) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    uint64_t execQueueHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    sessionMock->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->lrcHandleToVmHandle[lrcHandle] = vmHandle;

    uint8_t data[sizeof(drm_xe_eudebug_event_eu_attention) + 128];
    ze_device_thread_t thread{0, 0, 0, 0};

    sessionMock->stoppedThreads[EuThread::ThreadId(0, thread).packed] = 1;
    sessionMock->pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread, false));

    sessionMock->interruptSent = true;
    sessionMock->euControlInterruptSeqno = 1;

    drm_xe_eudebug_event_eu_attention attention = {};
    attention.base.type = DRM_XE_EUDEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    attention.base.len = sizeof(drm_xe_eudebug_event_eu_attention);
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.exec_queue_handle = execQueueHandle;
    attention.bitmask_size = 0;

    memcpy(data, &attention, sizeof(drm_xe_eudebug_event_eu_attention));

    sessionMock->expectedAttentionEvents = 2;

    sessionMock->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(data));

    EXPECT_FALSE(sessionMock->triggerEvents);

    attention.base.seqno = 10;
    memcpy(data, &attention, sizeof(drm_xe_eudebug_event_eu_attention));
    sessionMock->handleEvent(reinterpret_cast<drm_xe_eudebug_event *>(data));

    EXPECT_TRUE(sessionMock->triggerEvents);
}

TEST_F(DebugApiLinuxTestXe, GivenNoElfDataImplementationThenGetElfDataReturnsNullptr) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto clientConnection = sessionMock->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    drm_xe_eudebug_event_metadata metadata{};
    metadata.base.type = DRM_XE_EUDEBUG_EVENT_METADATA;
    metadata.base.flags = DRM_XE_EUDEBUG_EVENT_CREATE;
    metadata.base.len = sizeof(drm_xe_eudebug_event_metadata);
    metadata.client_handle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadata_handle = 10;
    metadata.type = DRM_XE_DEBUG_METADATA_PROGRAM_MODULE;
    metadata.len = 1000;
    clientConnection->metaDataMap[10].metadata = metadata;
    clientConnection->metaDataMap[10].metadata.len = 1000;
    auto ptr = std::make_unique<char[]>(metadata.len);
    clientConnection->metaDataMap[10].data = std::move(ptr);

    metadata.metadata_handle = 11;
    metadata.type = DRM_XE_DEBUG_METADATA_ELF_BINARY;
    metadata.len = 2000;
    clientConnection->metaDataMap[11].metadata = metadata;
    clientConnection->metaDataMap[11].metadata.len = 2000;

    ASSERT_NE(nullptr, clientConnection);
    ASSERT_EQ(reinterpret_cast<char *>(clientConnection->metaDataMap[10].data.get()), sessionMock->getClientConnection(MockDebugSessionLinuxXe::mockClientHandle)->getElfData(10));
    ASSERT_EQ(clientConnection->metaDataMap[10].metadata.len, sessionMock->getClientConnection(MockDebugSessionLinuxXe::mockClientHandle)->getElfSize(10));

    ASSERT_EQ(reinterpret_cast<char *>(clientConnection->metaDataMap[11].data.get()), sessionMock->getClientConnection(MockDebugSessionLinuxXe::mockClientHandle)->getElfData(11));
    ASSERT_EQ(clientConnection->metaDataMap[11].metadata.len, sessionMock->getClientConnection(MockDebugSessionLinuxXe::mockClientHandle)->getElfSize(11));
}

TEST_F(DebugApiLinuxTestXe, GivenInterruptedThreadsWhenNoAttentionEventIsReadThenThreadUnavailableEventIsGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);
    session->returnTimeDiff = DebugSessionLinuxXe::interruptTimeout * 10;
    session->synchronousInternalEventRead = true;

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};

    auto result = session->interrupt(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    session->startAsyncThread();

    while (session->getInternalEventCounter < 2)
        ;

    session->closeAsyncThread();

    EXPECT_EQ(session->apiEvents.size(), 1u);
    if (session->apiEvents.size() > 0) {
        auto event = session->apiEvents.front();
        EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, event.type);
        EXPECT_EQ(0u, event.info.thread.thread.slice);
        EXPECT_EQ(0u, event.info.thread.thread.subslice);
        EXPECT_EQ(0u, event.info.thread.thread.eu);
        EXPECT_EQ(UINT32_MAX, event.info.thread.thread.thread);
    }
}

TEST_F(DebugApiLinuxTestXe, GivenBindInfoForVmHandleWhenReadingModuleDebugAreaThenGpuMemoryIsRead) {
    auto session = std::make_unique<MockDebugSessionLinuxXe>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    uint64_t vmHandle = 6;
    session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmToModuleDebugAreaBindInfo[vmHandle] = {0x1234000, 0x1000};

    DebugAreaHeader debugArea;
    debugArea.reserved1 = 1;
    debugArea.pgsize = uint8_t(4);
    debugArea.version = 1;

    handler->mmapRet = reinterpret_cast<char *>(&debugArea);
    handler->setPreadMemory(reinterpret_cast<char *>(&debugArea), sizeof(session->debugArea), 0x1234000);
    handler->preadRetVal = sizeof(session->debugArea);

    auto retVal = session->readModuleDebugArea();

    EXPECT_TRUE(retVal);

    if (debugManager.flags.EnableDebuggerMmapMemoryAccess.get()) {
        EXPECT_EQ(1, handler->mmapCalled);
        EXPECT_EQ(1, handler->munmapCalled);
    } else {
        EXPECT_EQ(1, handler->preadCalled);
    }
    EXPECT_EQ(MockDebugSessionLinuxXe::mockClientHandle, handler->vmOpen.client_handle);
    EXPECT_EQ(vmHandle, handler->vmOpen.vm_handle);
    EXPECT_EQ(static_cast<uint64_t>(0), handler->vmOpen.flags);

    EXPECT_EQ(1u, session->debugArea.reserved1);
    EXPECT_EQ(1u, session->debugArea.version);
    EXPECT_EQ(4u, session->debugArea.pgsize);
}

TEST(DebugSessionLinuxXeTest, GivenRootDebugSessionWhenCreateTileSessionCalledThenSessionIsNotCreated) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto mockDrm = new DrmQueryMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    MockDeviceImp deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    struct DebugSession : public DebugSessionLinuxXe {
        using DebugSessionLinuxXe::createTileSession;
        using DebugSessionLinuxXe::DebugSessionLinuxXe;
    };
    auto session = std::make_unique<DebugSession>(zet_debug_config_t{0x1234}, &deviceImp, 10, nullptr);
    ASSERT_NE(nullptr, session);

    std::unique_ptr<DebugSessionImp> tileSession = std::unique_ptr<DebugSessionImp>{session->createTileSession(zet_debug_config_t{0x1234}, &deviceImp, nullptr)};
    EXPECT_EQ(nullptr, tileSession);
}

} // namespace ult
} // namespace L0
