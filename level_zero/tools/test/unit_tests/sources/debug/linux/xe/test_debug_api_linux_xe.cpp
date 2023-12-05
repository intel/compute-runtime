/*
 * Copyright (C) 2023 Intel Corporation
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
#include "uapi-eudebug/drm/xe_drm.h"
#include "uapi-eudebug/drm/xe_drm_tmp.h"

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

TEST_F(DebugApiLinuxTestXe, GivenDebugSessionInitializedThenInternalEventsThreadStarted) {
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
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(session->internalEventThread.threadActive);
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
    //    session->synchronousInternalEventRead = true;

    uint64_t clientHandle = 2;
    drm_xe_eudebug_event_client client = {};
    client.base.type = DRM_XE_EUDEBUG_EVENT_OPEN;
    client.base.flags = 0;
    client.base.len = sizeof(drm_xe_eudebug_event_client);
    client.client_handle = clientHandle;

    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});

    session->readInternalEventsAsync();

    constexpr int clientEventCount = 1;
    constexpr int dummyReadEventCount = 1;

    EXPECT_EQ(clientEventCount + dummyReadEventCount, handler->ioctlCalled);
    EXPECT_EQ(DebugSessionLinuxXe::maxEventSize, handler->debugEventInput.len);
    EXPECT_EQ(static_cast<decltype(drm_xe_eudebug_event::type)>(DRM_XE_EUDEBUG_EVENT_READ), handler->debugEventInput.type);
}

TEST_F(DebugApiLinuxTestXe, GivenMoreThan3EventsInQueueThenInternalEventsOnlyReads3) {
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

    EXPECT_EQ(3, handler->ioctlCalled);
    EXPECT_EQ(DebugSessionLinuxXe::maxEventSize, handler->debugEventInput.len);
    EXPECT_EQ(static_cast<decltype(drm_xe_eudebug_event::type)>(DRM_XE_EUDEBUG_EVENT_READ), handler->debugEventInput.type);
}

} // namespace ult
} // namespace L0