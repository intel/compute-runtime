/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/os_interface/linux/xe/eudebug/mock_eudebug_interface.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/tools/source/debug/debug_handlers.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/source/debug/linux/xe/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/xe/debug_session_fixtures_linux_xe.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"
#include "level_zero/zet_intel_gpu_debug.h"

#include "common/StateSaveAreaHeader.h"

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

    NEO::MockEuDebugInterface euDebugInterface{};
    L0::DebugSessionLinuxXe::IoctlHandlerXe handler(euDebugInterface);

    auto result = handler.ioctl(0, static_cast<uint64_t>(NEO::EuDebugParam::ioctlEuControl), &ioctlCount);
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

    NEO::MockEuDebugInterface euDebugInterface{};
    L0::DebugSessionLinuxXe::IoctlHandlerXe handler(euDebugInterface);

    auto result = handler.ioctl(0, static_cast<uint64_t>(NEO::EuDebugParam::ioctlEuControl), &ioctlCount);
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

    NEO::MockEuDebugInterface euDebugInterface{};
    L0::DebugSessionLinuxXe::IoctlHandlerXe handler(euDebugInterface);

    auto result = handler.ioctl(0, static_cast<uint64_t>(NEO::EuDebugParam::ioctlReadEvent), &ioctlCount);
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

    NEO::EuDebugEventClient clientInvalidFlag = {};
    clientInvalidFlag.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    clientInvalidFlag.base.flags = 0x8000;
    clientInvalidFlag.base.len = sizeof(NEO::EuDebugEventClient);
    clientInvalidFlag.clientHandle = 1;

    auto handler = new MockIoctlHandlerXe;
    handler->eventQueue.push({reinterpret_cast<char *>(&clientInvalidFlag), static_cast<uint64_t>(clientInvalidFlag.base.len)});
    handler->pollRetVal = 1;
    auto eventsCount = handler->eventQueue.size();

    session->ioctlHandler.reset(handler);

    auto memory = std::make_unique<uint64_t[]>(MockDebugSessionLinuxXe::maxEventSize / sizeof(uint64_t));
    NEO::EuDebugEvent *event = reinterpret_cast<NEO::EuDebugEvent *>(memory.get());
    event->type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeRead);
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

    NEO::EuDebugEventClient client = {};
    client.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client.base.flags = 0;
    client.base.len = sizeof(NEO::EuDebugEventClient);
    client.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

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
    NEO::EuDebugEventClient client = {};
    client.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client.base.flags = 0;
    client.base.len = sizeof(NEO::EuDebugEventClient);
    client.clientHandle = clientHandle;

    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});

    session->readInternalEventsAsync();
    constexpr int dummyReadEventCount = 1;

    EXPECT_EQ(dummyReadEventCount, handler->ioctlCalled);
    EXPECT_EQ(DebugSessionLinuxXe::maxEventSize, handler->debugEventInput.len);
    EXPECT_EQ(static_cast<decltype(NEO::EuDebugEvent::type)>(static_cast<uint16_t>(NEO::EuDebugParam::eventTypeRead)), handler->debugEventInput.type);
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

    uint64_t clientHandle = 2;
    NEO::EuDebugEventClient client = {};
    client.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client.base.flags = 0;
    client.base.len = sizeof(NEO::EuDebugEventClient);
    client.clientHandle = clientHandle;

    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});
    handler->eventQueue.push({reinterpret_cast<char *>(&client), static_cast<uint64_t>(client.base.len)});

    session->readInternalEventsAsync();

    EXPECT_EQ(1, handler->ioctlCalled);
    EXPECT_EQ(DebugSessionLinuxXe::maxEventSize, handler->debugEventInput.len);
    EXPECT_EQ(static_cast<decltype(NEO::EuDebugEvent::type)>(static_cast<uint16_t>(NEO::EuDebugParam::eventTypeRead)), handler->debugEventInput.type);
}

TEST_F(DebugApiLinuxTestXe, GivenEventInInternalEventQueueWhenAsyncThreadFunctionIsExecutedThenEventIsHandled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockAsyncThreadDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();

    uint8_t eventClientData[sizeof(NEO::EuDebugEventClient)];
    auto client = reinterpret_cast<NEO::EuDebugEventClient *>(&eventClientData);
    client->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client->base.len = sizeof(NEO::EuDebugEventClient);
    client->clientHandle = 0x123456789;

    auto memory = std::make_unique<uint64_t[]>(sizeof(NEO::EuDebugEventClient) / sizeof(uint64_t));
    memcpy(memory.get(), client, sizeof(NEO::EuDebugEventClient));

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
    EXPECT_NE(session->clientHandleToConnection.find(client->clientHandle), session->clientHandleToConnection.end());

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
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;

    NEO::EuDebugEvent client2{};
    client2.flags = 0;

    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client2));
    EXPECT_EQ(session->clientHandleToConnection.size(), 1ul);
    EXPECT_NE(session->clientHandleToConnection.find(client1.clientHandle), session->clientHandleToConnection.end());

    uint64_t wrongClientHandle = 34;
    EXPECT_EQ(session->clientHandleToConnection.find(wrongClientHandle), session->clientHandleToConnection.end());
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugOpenEventWithEventCreateFlagWhenHandleEventThenNewClientConnectionIsCreated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;

    NEO::EuDebugEventClient client2;
    client2.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client2.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client2.clientHandle = 0x123456788;

    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client2));
    EXPECT_EQ(session->clientHandleToConnection.size(), 2ul);
    EXPECT_NE(session->clientHandleToConnection.find(client1.clientHandle), session->clientHandleToConnection.end());
    EXPECT_NE(session->clientHandleToConnection.find(client2.clientHandle), session->clientHandleToConnection.end());

    uint64_t wrongClientHandle = 34;
    EXPECT_EQ(session->clientHandleToConnection.find(wrongClientHandle), session->clientHandleToConnection.end());
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugOpenEventWithEventDestroyFlagWhenHandleEventThenClientConnectionIsDestroyed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));
    EXPECT_EQ(session->clientHandleToConnection.size(), 1ul);
    EXPECT_NE(session->clientHandleToConnection.find(client1.clientHandle), session->clientHandleToConnection.end());

    NEO::EuDebugEventClient client2;
    client2.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client2.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy)));
    client2.clientHandle = client1.clientHandle;
    EXPECT_EQ(session->clientHandleClosed, session->invalidClientHandle);
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client2));
    EXPECT_EQ(session->clientHandleClosed, client2.clientHandle);
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugVmEventWithEventCreateFlagWhenHandleEventThenNewVmIdsIsCreated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    NEO::EuDebugEventVm vm1;
    NEO::EuDebugEventVm vm2;
    vm1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vm1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVm);
    vm1.clientHandle = 0x123456789;
    vm1.vmHandle = 0x123;
    vm2.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vm2.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVm);
    vm2.clientHandle = 0x123456789;
    vm2.vmHandle = 0x124;

    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vm1));
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vm2));
    ASSERT_EQ(session->clientHandleToConnection[client1.clientHandle]->vmIds.size(), 2u);
    auto it1 = session->clientHandleToConnection[client1.clientHandle]->vmIds.find(vm1.vmHandle);
    EXPECT_NE(it1, session->clientHandleToConnection[client1.clientHandle]->vmIds.end());
    auto it2 = session->clientHandleToConnection[client1.clientHandle]->vmIds.find(vm2.vmHandle);
    EXPECT_NE(it2, session->clientHandleToConnection[client1.clientHandle]->vmIds.end());

    // Check for wrong vmId
    auto it3 = session->clientHandleToConnection[client1.clientHandle]->vmIds.find(0x123456);
    EXPECT_EQ(it3, session->clientHandleToConnection[client1.clientHandle]->vmIds.end());
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugVmEventWithEventDestroyFlagWhenHandleEventThenVmIdsIsRemoved) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    NEO::EuDebugEventVm vm1;
    vm1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vm1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVm);
    vm1.clientHandle = 0x123456789;
    vm1.vmHandle = 0x123;

    NEO::EuDebugEventVm vm2;
    vm2.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vm2.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVm);
    vm2.clientHandle = 0x123456789;
    vm2.vmHandle = 0x124;

    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vm1));
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vm2));
    ASSERT_EQ(session->clientHandleToConnection[client1.clientHandle]->vmIds.size(), 2u);
    auto it1 = session->clientHandleToConnection[client1.clientHandle]->vmIds.find(vm1.vmHandle);
    EXPECT_NE(it1, session->clientHandleToConnection[client1.clientHandle]->vmIds.end());
    auto it2 = session->clientHandleToConnection[client1.clientHandle]->vmIds.find(vm2.vmHandle);
    EXPECT_NE(it2, session->clientHandleToConnection[client1.clientHandle]->vmIds.end());

    NEO::EuDebugEventVm vm3;
    vm3.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy)));
    vm3.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVm);
    vm3.clientHandle = 0x123456789;
    vm3.vmHandle = 0x124;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vm3));
    auto it3 = session->clientHandleToConnection[client1.clientHandle]->vmIds.find(vm2.vmHandle);
    EXPECT_EQ(it3, session->clientHandleToConnection[client1.clientHandle]->vmIds.end());
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugExecQueueEventWithEventCreateFlagWhenHandleEventThenExecQueueIsCreated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    uint64_t execQueueData[sizeof(NEO::EuDebugEventExecQueue) / sizeof(uint64_t) + 3 * sizeof(typeOfLrcHandle)];
    auto *lrcHandle = reinterpret_cast<typeOfLrcHandle *>(ptrOffset(execQueueData, sizeof(NEO::EuDebugEventExecQueue)));
    typeOfLrcHandle lrcHandleTemp[3];
    const uint64_t lrcHandle0 = 2;
    const uint64_t lrcHandle1 = 3;
    const uint64_t lrcHandle2 = 5;
    lrcHandleTemp[0] = static_cast<typeOfLrcHandle>(lrcHandle0);
    lrcHandleTemp[1] = static_cast<typeOfLrcHandle>(lrcHandle1);
    lrcHandleTemp[2] = static_cast<typeOfLrcHandle>(lrcHandle2);

    NEO::EuDebugEventExecQueue *execQueue = reinterpret_cast<NEO::EuDebugEventExecQueue *>(&execQueueData);
    execQueue->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeExecQueue);
    execQueue->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    execQueue->clientHandle = client1.clientHandle;
    execQueue->vmHandle = 0x1234;
    execQueue->execQueueHandle = 0x100;
    execQueue->engineClass = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    memcpy(lrcHandle, lrcHandleTemp, sizeof(lrcHandleTemp));
    session->handleEvent(&execQueue->base);
    EXPECT_NE(session->clientHandleToConnection.find(execQueue->clientHandle), session->clientHandleToConnection.end());
    EXPECT_EQ(session->clientHandleToConnection[execQueue->clientHandle]->execQueues[execQueue->execQueueHandle].vmHandle,
              execQueue->vmHandle);
    EXPECT_EQ(session->clientHandleToConnection[execQueue->clientHandle]->execQueues[execQueue->execQueueHandle].engineClass,
              DRM_XE_ENGINE_CLASS_COMPUTE);
    for (uint16_t idx = 0; idx < execQueue->width; idx++) {
        EXPECT_EQ(session->clientHandleToConnection[execQueue->clientHandle]->lrcHandleToVmHandle[execQueue->lrcHandle[idx]],
                  execQueue->vmHandle);
    }
}

TEST_F(DebugApiLinuxTestXe, GivenEuDebugExecQueueEventWithEventDestroyFlagWhenHandleEventThenExecQueueIsDestroyed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    uint64_t execQueueData[sizeof(NEO::EuDebugEventExecQueue) / sizeof(uint64_t) + 3 * sizeof(typeOfLrcHandle)];
    auto *lrcHandle = reinterpret_cast<typeOfLrcHandle *>(ptrOffset(execQueueData, sizeof(NEO::EuDebugEventExecQueue)));
    typeOfLrcHandle lrcHandleTemp[3];
    const uint64_t lrcHandle0 = 2;
    const uint64_t lrcHandle1 = 3;
    const uint64_t lrcHandle2 = 5;
    lrcHandleTemp[0] = static_cast<typeOfLrcHandle>(lrcHandle0);
    lrcHandleTemp[1] = static_cast<typeOfLrcHandle>(lrcHandle1);
    lrcHandleTemp[2] = static_cast<typeOfLrcHandle>(lrcHandle2);

    // ExecQueue create event handle
    NEO::EuDebugEventExecQueue *execQueue = reinterpret_cast<NEO::EuDebugEventExecQueue *>(&execQueueData);
    execQueue->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeExecQueue);
    execQueue->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    execQueue->clientHandle = client1.clientHandle;
    execQueue->vmHandle = 0x1234;
    execQueue->execQueueHandle = 0x100;
    execQueue->engineClass = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    memcpy(lrcHandle, lrcHandleTemp, sizeof(lrcHandleTemp));
    session->handleEvent(&execQueue->base);
    EXPECT_NE(session->clientHandleToConnection.find(execQueue->clientHandle), session->clientHandleToConnection.end());
    EXPECT_EQ(session->clientHandleToConnection[execQueue->clientHandle]->execQueues[execQueue->execQueueHandle].vmHandle,
              execQueue->vmHandle);
    EXPECT_EQ(session->clientHandleToConnection[execQueue->clientHandle]->execQueues[execQueue->execQueueHandle].engineClass,
              DRM_XE_ENGINE_CLASS_COMPUTE);
    for (uint16_t idx = 0; idx < execQueue->width; idx++) {
        EXPECT_EQ(session->clientHandleToConnection[execQueue->clientHandle]->lrcHandleToVmHandle[execQueue->lrcHandle[idx]],
                  execQueue->vmHandle);
    }

    // ExecQueue Destroy event handle
    execQueue->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeExecQueue);
    execQueue->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy)));
    execQueue->clientHandle = client1.clientHandle;
    execQueue->vmHandle = 0x1234;
    execQueue->execQueueHandle = 0x100;
    execQueue->engineClass = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    session->handleEvent(&execQueue->base);
    EXPECT_TRUE(session->clientHandleToConnection[execQueue->clientHandle]->execQueues.empty());
    EXPECT_TRUE(session->clientHandleToConnection[execQueue->clientHandle]->lrcHandleToVmHandle.empty());
}

TEST_F(DebugApiLinuxTestXe, GivenExecQueueWhenGetVmHandleFromClientAndlrcHandleThenProperVmHandleReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    uint64_t execQueueData[sizeof(NEO::EuDebugEventExecQueue) / sizeof(uint64_t) + 3 * sizeof(typeOfLrcHandle)];
    auto *lrcHandle = reinterpret_cast<typeOfLrcHandle *>(ptrOffset(execQueueData, sizeof(NEO::EuDebugEventExecQueue)));
    typeOfLrcHandle lrcHandleTemp[3];
    const uint64_t lrcHandle0 = 2;
    const uint64_t lrcHandle1 = 3;
    const uint64_t lrcHandle2 = 5;
    lrcHandleTemp[0] = static_cast<typeOfLrcHandle>(lrcHandle0);
    lrcHandleTemp[1] = static_cast<typeOfLrcHandle>(lrcHandle1);
    lrcHandleTemp[2] = static_cast<typeOfLrcHandle>(lrcHandle2);

    NEO::EuDebugEventExecQueue *execQueue = reinterpret_cast<NEO::EuDebugEventExecQueue *>(&execQueueData);
    execQueue->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeExecQueue);
    execQueue->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    execQueue->clientHandle = client1.clientHandle;
    execQueue->vmHandle = 0x1234;
    execQueue->execQueueHandle = 0x100;
    execQueue->engineClass = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    memcpy(lrcHandle, lrcHandleTemp, sizeof(lrcHandleTemp));
    session->handleEvent(&execQueue->base);

    EXPECT_EQ(execQueue->vmHandle, session->getVmHandleFromClientAndlrcHandle(execQueue->clientHandle, lrcHandleTemp[0]));
    EXPECT_EQ(DebugSessionLinux::invalidHandle, session->getVmHandleFromClientAndlrcHandle(0x1234567, lrcHandleTemp[0]));
    EXPECT_EQ(DebugSessionLinux::invalidHandle, session->getVmHandleFromClientAndlrcHandle(execQueue->clientHandle, 7));
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
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    uint64_t execQueueData[sizeof(NEO::EuDebugEventExecQueue) / sizeof(uint64_t) + 3 * sizeof(typeOfLrcHandle)];
    auto *lrcHandle = reinterpret_cast<typeOfLrcHandle *>(ptrOffset(execQueueData, sizeof(NEO::EuDebugEventExecQueue)));
    typeOfLrcHandle lrcHandleTemp[3];
    const uint64_t lrcHandle0 = 2;
    const uint64_t lrcHandle1 = 3;
    const uint64_t lrcHandle2 = 5;
    lrcHandleTemp[0] = static_cast<typeOfLrcHandle>(lrcHandle0);
    lrcHandleTemp[1] = static_cast<typeOfLrcHandle>(lrcHandle1);
    lrcHandleTemp[2] = static_cast<typeOfLrcHandle>(lrcHandle2);

    NEO::EuDebugEventExecQueue *execQueue = reinterpret_cast<NEO::EuDebugEventExecQueue *>(&execQueueData);
    execQueue->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeExecQueue);
    execQueue->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    execQueue->clientHandle = client1.clientHandle;
    execQueue->vmHandle = 0x1234;
    execQueue->execQueueHandle = 0x100;
    execQueue->engineClass = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    memcpy(lrcHandle, lrcHandleTemp, sizeof(lrcHandleTemp));
    session->handleEvent(&execQueue->base); // ExecQueue create event handle for first client

    zet_debug_event_t event1 = {};
    ze_result_t result = zetDebugReadEvent(session->toHandle(), 0, &event1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY, event1.type);

    // second client connection
    NEO::EuDebugEventClient client2;
    client2.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client2.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client2.clientHandle = 0x123456788;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client2));

    execQueue->clientHandle = client2.clientHandle;
    execQueue->vmHandle = 0x1235;
    execQueue->execQueueHandle = 0x101;
    session->handleEvent(&execQueue->base); // ExecQueue1 create event handle for second client

    zet_debug_event_t event2 = {};
    result = zetDebugReadEvent(session->toHandle(), 0, &event2);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    execQueue->clientHandle = client2.clientHandle;
    execQueue->vmHandle = 0x1236;
    execQueue->execQueueHandle = 0x102;
    session->handleEvent(&execQueue->base); // ExecQueue2 create event handle for second client
    result = zetDebugReadEvent(session->toHandle(), 0, &event2);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    // ExecQueue Destroy event handle
    execQueue->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeExecQueue);
    execQueue->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy)));
    execQueue->clientHandle = client1.clientHandle;
    execQueue->vmHandle = 0x1234;
    execQueue->execQueueHandle = 0x100;
    execQueue->engineClass = DRM_XE_ENGINE_CLASS_COMPUTE;
    execQueue->width = 3;
    session->handleEvent(&execQueue->base);
    result = zetDebugReadEvent(session->toHandle(), 0, &event2);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    execQueue->clientHandle = client2.clientHandle;
    execQueue->vmHandle = 0x1235;
    execQueue->execQueueHandle = 0x101;
    session->handleEvent(&execQueue->base);
    result = zetDebugReadEvent(session->toHandle(), 0, &event2);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    execQueue->clientHandle = client2.clientHandle;
    execQueue->vmHandle = 0x1236;
    execQueue->execQueueHandle = 0x102;
    session->handleEvent(&execQueue->base);
    result = zetDebugReadEvent(session->toHandle(), 0, &event2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT, event2.type);
}

TEST_F(DebugApiLinuxTestXe, GivenMetadataEventWhenClientHandleIsInvalidThenClientHandleUpdated) {

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = session->invalidClientHandle;
    NEO::EuDebugEventMetadata metadata = {};
    metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadataHandle = 2;
    metadata.len = 0;

    auto handler = new MockIoctlHandlerXe;
    handler->eventQueue.push({reinterpret_cast<char *>(&metadata), static_cast<uint64_t>(metadata.base.len)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);
    session->handleEvent(&metadata.base);
    EXPECT_EQ_VAL(session->clientHandle, metadata.clientHandle);
}

TEST_F(DebugApiLinuxTestXe, GivenMetadataEventWhenHandlingAndMetadataLengthIsZeroThenMetadataIsInsertedToMap) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    NEO::EuDebugEventMetadata metadata = {};
    metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadataHandle = 2;
    metadata.len = 0;

    auto handler = new MockIoctlHandlerXe;
    handler->eventQueue.push({reinterpret_cast<char *>(&metadata), static_cast<uint64_t>(metadata.base.len)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);
    session->handleEvent(&metadata.base);

    EXPECT_EQ(1u, session->clientHandleToConnection[metadata.clientHandle]->metaDataMap.size());
    EXPECT_NE(session->clientHandleToConnection[metadata.clientHandle]->metaDataMap.end(), session->clientHandleToConnection[metadata.clientHandle]->metaDataMap.find(metadata.metadataHandle));

    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy)));
    session->handleEvent(&metadata.base);
    EXPECT_NE(session->clientHandleToConnection[metadata.clientHandle]->metaDataMap.end(), session->clientHandleToConnection[metadata.clientHandle]->metaDataMap.find(metadata.metadataHandle));
}

TEST_F(DebugApiLinuxTestXe, GivenMetadataCreateEventWhenHandlingAndIoctlFailsThenEventHandlingCallImmediatelyReturns) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    NEO::EuDebugEventMetadata metadata = {};
    metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadataHandle = 2;
    metadata.len = 5;

    auto handler = new MockIoctlHandlerXe;
    handler->eventQueue.push({reinterpret_cast<char *>(&metadata), static_cast<uint64_t>(metadata.base.len)});
    handler->pollRetVal = 1;

    session->ioctlHandler.reset(handler);
    session->handleEvent(&metadata.base);

    EXPECT_EQ(0u, session->clientHandleToConnection[metadata.clientHandle]->metaDataMap.size());
    EXPECT_EQ(session->clientHandleToConnection[metadata.clientHandle]->metaDataMap.end(), session->clientHandleToConnection[metadata.clientHandle]->metaDataMap.find(metadata.metadataHandle));

    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy)));
    session->handleEvent(&metadata.base);
    EXPECT_NE(session->clientHandleToConnection[metadata.clientHandle]->metaDataMap.end(), session->clientHandleToConnection[metadata.clientHandle]->metaDataMap.find(metadata.metadataHandle));
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
    NEO::EuDebugEventMetadata metadata;
    metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadataHandle = 3;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataProgramModule);
    metadata.len = sizeof(kernelCount);

    NEO::EuDebugReadMetadata readMetadata{};
    readMetadata.ptr = reinterpret_cast<uint64_t>(&kernelCount);
    readMetadata.size = sizeof(kernelCount);
    readMetadata.metadataHandle = 3;
    handler->returnMetadata = &readMetadata;
    session->handleEvent(&metadata.base);

    EXPECT_NE(session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule.end(),
              session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule.find(metadata.metadataHandle));

    EXPECT_EQ(kernelCount, session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule[metadata.metadataHandle].segmentCount);

    EXPECT_EQ_VAL(metadata.metadataHandle, session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataMap[metadata.metadataHandle].metadata.metadataHandle);
    EXPECT_NE(nullptr, session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataMap[metadata.metadataHandle].data);
    EXPECT_EQ(sizeof(kernelCount), session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataMap[metadata.metadataHandle].metadata.len);

    EXPECT_EQ(1, handler->ioctlCalled);

    // inject module segment
    auto &module = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule[metadata.metadataHandle];
    module.loadAddresses[0].insert(0x12340000);

    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy)));
    metadata.len = 0;
    session->handleEvent(&metadata.base);

    EXPECT_EQ(session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule.end(),
              session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->metaDataToModule.find(metadata.metadataHandle));
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

    NEO::EuDebugEventMetadata metadataSba = {};
    metadataSba.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadataSba.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadataSba.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadataSba.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadataSba.metadataHandle = 4;
    metadataSba.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataSbaArea);
    metadataSba.len = sizeof(sbaAddress);

    NEO::EuDebugReadMetadata readMetadata = {};
    readMetadata.ptr = reinterpret_cast<uint64_t>(&sbaAddress);
    readMetadata.size = sizeof(sbaAddress);
    readMetadata.metadataHandle = 4;

    handler->returnMetadata = &readMetadata;
    session->handleEvent(&metadataSba.base);

    NEO::EuDebugEventMetadata metadataModule = {};
    metadataModule.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadataModule.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadataModule.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadataModule.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadataModule.metadataHandle = 5;
    metadataModule.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataModuleArea);
    metadataModule.len = sizeof(moduleDebugAddress);

    readMetadata = {};
    readMetadata.ptr = reinterpret_cast<uint64_t>(&moduleDebugAddress);
    readMetadata.size = sizeof(moduleDebugAddress);
    readMetadata.metadataHandle = 5;

    handler->returnMetadata = &readMetadata;
    session->handleEvent(&metadataModule.base);

    NEO::EuDebugEventMetadata metadataContextSave = {};

    metadataContextSave.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadataContextSave.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadataContextSave.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadataContextSave.clientHandle = MockDebugSessionLinuxXe::mockClientHandle,
    metadataContextSave.metadataHandle = 6,
    metadataContextSave.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataSipArea);
    metadataContextSave.len = sizeof(contextSaveAddress);

    readMetadata = {};
    readMetadata.ptr = reinterpret_cast<uint64_t>(&contextSaveAddress);
    readMetadata.size = sizeof(contextSaveAddress);
    readMetadata.metadataHandle = 6;

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
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    NEO::EuDebugEventVmBind vmBind1{};
    vmBind1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBind);
    vmBind1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
    vmBind1.base.len = sizeof(NEO::EuDebugEventVmBind);
    vmBind1.base.seqno = 1;
    vmBind1.clientHandle = client1.clientHandle;
    vmBind1.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
    vmBind1.numBinds = 2;

    EXPECT_EQ(session->clientHandleToConnection[client1.clientHandle]->vmBindMap.size(), 0u);
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind1.base));

    vmBind1.base.seqno = 2;
    vmBind1.clientHandle = client1.clientHandle;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind1.base));

    EXPECT_EQ(session->clientHandleToConnection[client1.clientHandle]->vmBindMap.size(), 2u);
}

TEST_F(DebugApiLinuxTestXe, GivenVmBindOpEventWhenHandlingEventThenVmBindMapIsCorrectlyUpdated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    NEO::EuDebugEventVmBind vmBind{};
    vmBind.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBind);
    vmBind.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
    vmBind.base.len = sizeof(NEO::EuDebugEventVmBind);
    vmBind.base.seqno = 1;
    vmBind.clientHandle = client1.clientHandle;
    vmBind.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
    vmBind.numBinds = 2;

    auto &vmBindMap = session->clientHandleToConnection[client1.clientHandle]->vmBindMap;
    EXPECT_EQ(vmBindMap.size(), 0u);
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind.base));
    EXPECT_EQ(vmBindMap.size(), 1u);
    EXPECT_EQ(vmBindMap[vmBind.base.seqno].pendingNumBinds, 2ull);

    NEO::EuDebugEventVmBindOp vmBindOp{};
    vmBindOp.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOp);
    vmBindOp.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vmBindOp.base.len = sizeof(NEO::EuDebugEventVmBindOp);
    vmBindOp.base.seqno = 2;
    vmBindOp.numExtensions = 2;
    vmBindOp.addr = 0xffff1234;
    vmBindOp.range = 65536;
    vmBindOp.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));
    EXPECT_EQ(vmBindMap[vmBind.base.seqno].pendingNumBinds, 1ull);
    EXPECT_EQ(session->clientHandleToConnection[client1.clientHandle]->vmBindIdentifierMap[vmBindOp.base.seqno], vmBindOp.vmBindRefSeqno);

    auto &vmBindOpData = vmBindMap[vmBindOp.vmBindRefSeqno].vmBindOpMap[vmBindOp.base.seqno];
    EXPECT_EQ(vmBindOpData.pendingNumExtensions, vmBindOp.numExtensions);
    EXPECT_TRUE(0 == std::memcmp(&vmBindOp, &vmBindOpData.vmBindOp, sizeof(NEO::EuDebugEventVmBindOp)));
}

class DebugApiLinuxTestXeMetadataOpEventTest : public DebugApiLinuxTestXe,
                                               public ::testing::WithParamInterface<uint64_t> {
  public:
    uint64_t metadataType;
    DebugApiLinuxTestXeMetadataOpEventTest() {
        metadataType = GetParam();
    }
    ~DebugApiLinuxTestXeMetadataOpEventTest() override {}

    void createMetadata(NEO::EuDebugEventMetadata &metadata, uint64_t metadataHandle) {
        metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
        metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
        metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
        metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
        metadata.metadataHandle = metadataHandle;
        metadata.type = metadataType;
        metadata.len = sizeof(metadata);
    }

    void createVmBindEvent(NEO::EuDebugEventVmBind &vmBind, uint64_t vmHandle, uint32_t &seqNo) {
        vmBind.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBind);
        vmBind.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
        vmBind.base.len = sizeof(NEO::EuDebugEventVmBind);
        vmBind.base.seqno = seqNo++;
        vmBind.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
        vmBind.vmHandle = vmHandle;
        vmBind.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
        vmBind.numBinds = 1;
    }

    void createVmBindOpEvent(NEO::EuDebugEventVmBindOp &vmBindOp, uint32_t &seqNo, uint64_t vmBindRefSeqNo,
                             uint64_t vmBindOpAddr, uint64_t numExtensions, uint16_t flags) {
        vmBindOp.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOp);
        vmBindOp.base.flags = flags;
        vmBindOp.base.len = sizeof(NEO::EuDebugEventVmBindOp);
        vmBindOp.base.seqno = seqNo++;
        vmBindOp.numExtensions = numExtensions;
        vmBindOp.addr = vmBindOpAddr;
        vmBindOp.range = 1000;
        vmBindOp.vmBindRefSeqno = vmBindRefSeqNo;
    }

    void createUfenceEvent(NEO::EuDebugEventVmBindUfence &vmBindUfence, uint32_t &seqNo, uint64_t vmBindRefSeqno) {
        vmBindUfence.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindUfence);
        vmBindUfence.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate))) | static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitNeedAck)));
        vmBindUfence.base.len = sizeof(NEO::EuDebugEventVmBindUfence);
        vmBindUfence.base.seqno = seqNo++;
        vmBindUfence.vmBindRefSeqno = vmBindRefSeqno;
    }

    void createVmBindOpMetadata(NEO::EuDebugEventVmBindOpMetadata &vmBindOpMetadata, uint32_t &seqNo, uint64_t vmBindOpRefSeqno, uint64_t metadataHandle) {
        vmBindOpMetadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOpMetadata);
        vmBindOpMetadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
        vmBindOpMetadata.base.len = sizeof(NEO::EuDebugEventVmBindOpMetadata);
        vmBindOpMetadata.base.seqno = seqNo++;
        vmBindOpMetadata.vmBindOpRefSeqno = vmBindOpRefSeqno;
        vmBindOpMetadata.metadataHandle = metadataHandle;
        vmBindOpMetadata.metadataCookie = 0x3;
    }

    uint32_t seqNo = 1;
    const uint64_t metadataHandle1 = 10;
};
TEST_P(DebugApiLinuxTestXeMetadataOpEventTest, GivenVmBindOpMetadataEventForMetadataAreaWhenHandlingEventThenMapIsCorrectlyUpdated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    auto &connection = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    NEO::EuDebugEventMetadata metadata{};
    createMetadata(metadata, metadataHandle1);
    connection->metaDataMap[metadataHandle1].metadata = metadata;

    constexpr uint64_t vmHandle = 0x1234;
    connection->vmToTile[vmHandle] = 0u;
    NEO::EuDebugEventVmBind vmBind{};
    createVmBindEvent(vmBind, vmHandle, seqNo);

    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind.base));
    auto &vmBindMap = session->clientHandleToConnection[client1.clientHandle]->vmBindMap;

    NEO::EuDebugEventVmBindOp vmBindOp{};
    createVmBindOpEvent(vmBindOp, seqNo, vmBind.base.seqno, 0xffff1234, 1, static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate))));
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));

    auto &vmBindOpData = vmBindMap[vmBindOp.vmBindRefSeqno].vmBindOpMap[vmBindOp.base.seqno];
    NEO::EuDebugEventVmBindOpMetadata vmBindOpMetadata{};
    createVmBindOpMetadata(vmBindOpMetadata, seqNo, vmBindOp.base.seqno, metadataHandle1);
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));

    EXPECT_EQ(vmBindOpData.pendingNumExtensions, 0ull);
    EXPECT_EQ(vmBindOpData.vmBindOpMetadataVec.size(), 1ull);

    NEO::EuDebugEventVmBindUfence vmBindUfence{};
    createUfenceEvent(vmBindUfence, seqNo, vmBind.base.seqno);
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence.base));
    session->processPendingVmBindEvents();

    if (metadataType == static_cast<uint64_t>(NEO::EuDebugParam::metadataSbaArea)) {
        EXPECT_EQ(connection->vmToStateBaseAreaBindInfo[vmHandle].gpuVa, vmBindOp.addr);
        EXPECT_EQ(connection->vmToStateBaseAreaBindInfo[vmHandle].size, vmBindOp.range);
    }

    if (metadataType == static_cast<uint64_t>(NEO::EuDebugParam::metadataModuleArea)) {
        EXPECT_EQ(connection->vmToModuleDebugAreaBindInfo[vmHandle].gpuVa, vmBindOp.addr);
        EXPECT_EQ(connection->vmToModuleDebugAreaBindInfo[vmHandle].size, vmBindOp.range);
    }

    if (metadataType == static_cast<uint64_t>(NEO::EuDebugParam::metadataSipArea)) {
        EXPECT_EQ(connection->vmToContextStateSaveAreaBindInfo[vmHandle].gpuVa, vmBindOp.addr);
        EXPECT_EQ(connection->vmToContextStateSaveAreaBindInfo[vmHandle].size, vmBindOp.range);
    }

    if (metadataType == static_cast<uint64_t>(NEO::EuDebugParam::metadataElfBinary)) {
        EXPECT_EQ(connection->isaMap[0][vmBindOp.addr]->bindInfo.gpuVa, vmBindOp.addr);
        EXPECT_EQ(connection->isaMap[0][vmBindOp.addr]->bindInfo.size, vmBindOp.range);
        EXPECT_TRUE(connection->isaMap[0][vmBindOp.addr]->tileInstanced);
        EXPECT_FALSE(connection->isaMap[0][vmBindOp.addr]->perKernelModule);
    }
}

static uint64_t metadataTypes[] =
    {
        static_cast<uint64_t>(NEO::EuDebugParam::metadataModuleArea),
        static_cast<uint64_t>(NEO::EuDebugParam::metadataSbaArea),
        static_cast<uint64_t>(NEO::EuDebugParam::metadataSipArea),
        static_cast<uint64_t>(NEO::EuDebugParam::metadataElfBinary)};

INSTANTIATE_TEST_SUITE_P(
    MetadataOpEventUfenceTest,
    DebugApiLinuxTestXeMetadataOpEventTest,
    testing::ValuesIn(metadataTypes));

TEST_F(DebugApiLinuxTestXe, GivenPendingNumBindsWhenCanHandleVmBindCalledThenFalseIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);
    auto &vmBindData = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmBindMap[1];
    vmBindData.pendingNumBinds = 1;
    EXPECT_FALSE(session->canHandleVmBind(vmBindData));
}

TEST_F(DebugApiLinuxTestXe, GivenPendingNumExtensionsForVmBindOpWhenCanHandleVmBindCalledThenFalseIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);
    auto &vmBindData = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmBindMap[1];
    vmBindData.pendingNumBinds = 1;

    auto &vmBindOpData1 = vmBindData.vmBindOpMap[10];
    vmBindOpData1.pendingNumExtensions = 1;
    vmBindData.pendingNumBinds--;

    EXPECT_FALSE(session->canHandleVmBind(vmBindData));
}

TEST_F(DebugApiLinuxTestXe, GivenUfenceNotRequiredAndNoPendingVmBindWhenCanHandleVmBindCalledThenTrueIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);
    auto &vmBindData = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmBindMap[1];
    vmBindData.pendingNumBinds = 0;
    vmBindData.vmBind.flags = 0;
    EXPECT_TRUE(session->canHandleVmBind(vmBindData));
}

TEST_F(DebugApiLinuxTestXe, GivenUfenceRequiredButUfenceNotReceiveddWhenCanHandleVmBindCalledThenFalseIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);
    auto &vmBindData = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmBindMap[1];
    vmBindData.pendingNumBinds = 0;
    vmBindData.vmBind.flags = static_cast<uint16_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
    vmBindData.uFenceReceived = false;
    EXPECT_FALSE(session->canHandleVmBind(vmBindData));
}

TEST_F(DebugApiLinuxTestXe, GivenCanHandleVmBindReturnFalsedWhenHandleVmBindCalledThenFalseIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);
    auto &vmBindData = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmBindMap[1];
    vmBindData.pendingNumBinds = 1;
    EXPECT_FALSE(session->canHandleVmBind(vmBindData));
    EXPECT_FALSE(session->handleVmBind(vmBindData));
}

TEST_F(DebugApiLinuxTestXe, GivenNoEventRelatedToVmBindHandlingWhenHandleInternalEventCalledThenTrueIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();

    uint8_t eventClientData[sizeof(NEO::EuDebugEventClient)];
    auto client = reinterpret_cast<NEO::EuDebugEventClient *>(&eventClientData);
    client->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client->base.len = sizeof(NEO::EuDebugEventClient);
    client->clientHandle = 0x123456789;
    auto memory = std::make_unique<uint64_t[]>(sizeof(NEO::EuDebugEventClient) / sizeof(uint64_t));
    memcpy(memory.get(), client, sizeof(NEO::EuDebugEventClient));

    // Clear the event queue before using it
    while (!session->internalEventQueue.empty()) {
        session->internalEventQueue.pop();
    }
    session->internalEventQueue.push(std::move(memory));
    EXPECT_TRUE(session->handleInternalEvent());
}

TEST_F(DebugApiLinuxTestXe, GivenEventTypeExecQueuePlacementsAndClientHandleIsInvalidWhenHandleInternalEventCalledThenHandleVmBindNotCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto &vmBindData = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmBindMap[1];
    vmBindData.pendingNumBinds = 1;

    constexpr uint64_t vmHandle = 10;
    // Allocate memory for the structure including the flexible array member
    constexpr auto size = sizeof(NEO::EuDebugEventExecQueuePlacements) + sizeof(drm_xe_engine_class_instance) * 1;

    uint8_t memoryExecQueuePlacements[size];
    auto execQueuePlacements = reinterpret_cast<NEO::EuDebugEventExecQueuePlacements *>(memoryExecQueuePlacements);
    memset(execQueuePlacements, 0, size);
    execQueuePlacements->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeExecQueuePlacements);
    execQueuePlacements->base.len = sizeof(NEO::EuDebugEventExecQueuePlacements);
    execQueuePlacements->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    execQueuePlacements->vmHandle = vmHandle;
    execQueuePlacements->execQueueHandle = 200;
    execQueuePlacements->lrcHandle = 10;
    execQueuePlacements->numPlacements = 1;

    auto engineClassInstance = reinterpret_cast<drm_xe_engine_class_instance *>(&(execQueuePlacements->instances[0]));
    engineClassInstance[0].engine_class = 0;
    engineClassInstance[0].engine_instance = 1;
    engineClassInstance[0].gt_id = 2;

    auto memory = std::make_unique<uint64_t[]>(size / sizeof(uint64_t));
    memcpy(memory.get(), memoryExecQueuePlacements, size);

    // Clear the event queue before using it
    while (!session->internalEventQueue.empty()) {
        session->internalEventQueue.pop();
    }
    session->internalEventQueue.push(std::move(memory));

    session->clientHandle = DebugSessionLinuxXe::invalidClientHandle;
    EXPECT_EQ(session->handleVmBindCallCount, 0u);

    EXPECT_TRUE(session->handleInternalEvent());

    EXPECT_EQ(session->handleVmBindCallCount, 0u);
}

TEST_F(DebugApiLinuxTestXe, GivenEventTypeExecQueuePlacementsWhenHandleInternalEventCalledThenProcessPendingVmBindEventsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto &vmBindData = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmBindMap[1];
    vmBindData.pendingNumBinds = 1;

    constexpr uint64_t vmHandle = 10;
    // Allocate memory for the structure including the flexible array member
    constexpr auto size = sizeof(NEO::EuDebugEventExecQueuePlacements) + sizeof(drm_xe_engine_class_instance) * 1;

    uint8_t memoryExecQueuePlacements[size];
    auto execQueuePlacements = reinterpret_cast<NEO::EuDebugEventExecQueuePlacements *>(memoryExecQueuePlacements);
    memset(execQueuePlacements, 0, size);
    execQueuePlacements->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeExecQueuePlacements);
    execQueuePlacements->base.len = sizeof(NEO::EuDebugEventExecQueuePlacements);
    execQueuePlacements->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    execQueuePlacements->vmHandle = vmHandle;
    execQueuePlacements->execQueueHandle = 200;
    execQueuePlacements->lrcHandle = 10;
    execQueuePlacements->numPlacements = 1;

    auto engineClassInstance = reinterpret_cast<drm_xe_engine_class_instance *>(&(execQueuePlacements->instances[0]));
    engineClassInstance[0].engine_class = 0;
    engineClassInstance[0].engine_instance = 1;
    engineClassInstance[0].gt_id = 2;

    auto memory = std::make_unique<uint64_t[]>(size / sizeof(uint64_t));
    memcpy(memory.get(), memoryExecQueuePlacements, size);

    // Clear the event queue before using it
    while (!session->internalEventQueue.empty()) {
        session->internalEventQueue.pop();
    }
    session->internalEventQueue.push(std::move(memory));
    EXPECT_TRUE(session->handleInternalEvent());
    EXPECT_EQ(session->processPendingVmBindEventsCallCount, 1u);
}

TEST_F(DebugApiLinuxTestXe,
       GivenEventTypeVmBindOpOrEventTypeVmBindOpMetadataOrUfenceWhenHandleInternalEventCalledThenProcessPendingVmBindEventsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    NEO::EuDebugEventVmBind vmBind{};
    vmBind.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBind);
    vmBind.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
    vmBind.base.len = sizeof(NEO::EuDebugEventVmBind);
    vmBind.base.seqno = 1;
    vmBind.clientHandle = client1.clientHandle;
    vmBind.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
    vmBind.numBinds = 2;

    auto &vmBindMap = session->clientHandleToConnection[client1.clientHandle]->vmBindMap;
    EXPECT_EQ(vmBindMap.size(), 0u);
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind.base));
    EXPECT_EQ(vmBindMap.size(), 1u);
    EXPECT_EQ(vmBindMap[vmBind.base.seqno].pendingNumBinds, 2ull);

    constexpr auto sizeEuDebugEventVmBindOp = sizeof(NEO::EuDebugEventVmBindOp);

    uint8_t memoryEuDebugEventVmBindOp[sizeEuDebugEventVmBindOp];
    auto vmBindOp = reinterpret_cast<NEO::EuDebugEventVmBindOp *>(memoryEuDebugEventVmBindOp);
    memset(vmBindOp, 0, sizeEuDebugEventVmBindOp);
    vmBindOp->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOp);
    vmBindOp->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vmBindOp->base.len = sizeof(NEO::EuDebugEventVmBindOp);
    vmBindOp->base.seqno = 2;
    vmBindOp->numExtensions = 2;
    vmBindOp->addr = 0xffff1234;
    vmBindOp->range = 65536;
    vmBindOp->vmBindRefSeqno = vmBind.base.seqno;

    auto memoryEvent1 = std::make_unique<uint64_t[]>(sizeEuDebugEventVmBindOp / sizeof(uint64_t));
    memcpy(memoryEvent1.get(), vmBindOp, sizeEuDebugEventVmBindOp);

    constexpr auto sizeEuDebugEventVmBindOpMetadata = sizeof(NEO::EuDebugEventVmBindOpMetadata);
    uint8_t memoryEuDebugEventVmBindOpMetadata[sizeEuDebugEventVmBindOpMetadata];
    auto vmBindOpMetadata = reinterpret_cast<NEO::EuDebugEventVmBindOpMetadata *>(memoryEuDebugEventVmBindOpMetadata);
    memset(vmBindOpMetadata, 0, sizeEuDebugEventVmBindOpMetadata);
    vmBindOpMetadata->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOpMetadata);
    vmBindOpMetadata->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vmBindOpMetadata->base.len = sizeof(NEO::EuDebugEventVmBindOpMetadata);
    vmBindOpMetadata->base.seqno = 3;
    vmBindOpMetadata->vmBindOpRefSeqno = vmBindOp->base.seqno;
    vmBindOpMetadata->metadataHandle = 10;
    auto memoryEvent2 = std::make_unique<uint64_t[]>(sizeEuDebugEventVmBindOpMetadata / sizeof(uint64_t));
    memcpy(memoryEvent2.get(), vmBindOpMetadata, sizeEuDebugEventVmBindOpMetadata);

    constexpr auto sizeEuDebugEventVmBindUfence = sizeof(NEO::EuDebugEventVmBindUfence);
    uint8_t memoryEuDebugEventVmBindUfence[sizeEuDebugEventVmBindUfence];
    auto vmBindUfence = reinterpret_cast<NEO::EuDebugEventVmBindUfence *>(memoryEuDebugEventVmBindUfence);
    memset(vmBindUfence, 0, sizeEuDebugEventVmBindUfence);
    vmBindUfence->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindUfence);
    vmBindUfence->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate))) | static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitNeedAck)));
    vmBindUfence->base.len = sizeof(NEO::EuDebugEventVmBindUfence);
    vmBindUfence->base.seqno = 4;
    vmBindUfence->vmBindRefSeqno = vmBind.base.seqno;
    auto memoryEvent3 = std::make_unique<uint64_t[]>(sizeEuDebugEventVmBindUfence / sizeof(uint64_t));
    memcpy(memoryEvent3.get(), vmBindUfence, sizeEuDebugEventVmBindUfence);

    // Clear the event queue before using it
    while (!session->internalEventQueue.empty()) {
        session->internalEventQueue.pop();
    }
    session->internalEventQueue.push(std::move(memoryEvent1));
    session->internalEventQueue.push(std::move(memoryEvent2));
    session->internalEventQueue.push(std::move(memoryEvent3));

    EXPECT_TRUE(session->handleInternalEvent());
    EXPECT_EQ(session->processPendingVmBindEventsCallCount, 1u);
    EXPECT_TRUE(session->handleInternalEvent());
    EXPECT_EQ(session->processPendingVmBindEventsCallCount, 2u);
    EXPECT_TRUE(session->handleInternalEvent());
    EXPECT_EQ(session->processPendingVmBindEventsCallCount, 3u);
}

TEST_F(DebugApiLinuxTestXe, GivenVmBindOpMetadataCreateEventAndUfenceForProgramModuleWhenHandlingEventThenEventIsAcked) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    session->clientHandleToConnection.clear();
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    session->pushApiEventValidateAckEvents = true;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    constexpr uint64_t vmHandle1 = 0x1234;
    constexpr uint64_t metadataHandle1 = 10;
    constexpr uint64_t metadataHandle2 = 11;

    auto &connection = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    connection->vmToTile[vmHandle1] = 0u;

    NEO::EuDebugEventMetadata metadata{};
    metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadataHandle = metadataHandle1;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataProgramModule);
    metadata.len = sizeof(metadata);
    connection->metaDataMap[metadataHandle1].metadata = metadata;
    connection->metaDataToModule[metadataHandle1].segmentCount = 1;

    metadata.metadataHandle = metadataHandle2;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataElfBinary);
    metadata.len = sizeof(metadata);
    connection->metaDataMap[metadataHandle2].metadata = metadata;
    auto ptr = std::make_unique<char[]>(metadata.len);
    connection->metaDataMap[metadataHandle2].data = std::move(ptr);
    connection->metaDataMap[metadataHandle2].metadata.len = 10000;

    NEO::EuDebugEventVmBind vmBind{};
    vmBind.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBind);
    vmBind.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
    vmBind.base.len = sizeof(NEO::EuDebugEventVmBind);
    vmBind.base.seqno = 1;
    vmBind.clientHandle = client1.clientHandle;
    vmBind.vmHandle = vmHandle1;
    vmBind.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
    vmBind.numBinds = 1;

    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind.base));
    auto &vmBindMap = session->clientHandleToConnection[client1.clientHandle]->vmBindMap;

    NEO::EuDebugEventVmBindOp vmBindOp{};
    vmBindOp.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOp);
    vmBindOp.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vmBindOp.base.len = sizeof(NEO::EuDebugEventVmBindOp);
    vmBindOp.base.seqno = 2;
    vmBindOp.numExtensions = 2;
    vmBindOp.addr = 0xffff1234;
    vmBindOp.range = 1000;
    vmBindOp.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));

    NEO::EuDebugEventVmBindUfence vmBindUfence{};
    vmBindUfence.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindUfence);
    vmBindUfence.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate))) | static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitNeedAck)));
    vmBindUfence.base.len = sizeof(NEO::EuDebugEventVmBindUfence);
    vmBindUfence.base.seqno = 3;
    vmBindUfence.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence.base));

    auto &vmBindOpData = vmBindMap[vmBindOp.vmBindRefSeqno].vmBindOpMap[vmBindOp.base.seqno];
    NEO::EuDebugEventVmBindOpMetadata vmBindOpMetadata{};
    vmBindOpMetadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOpMetadata);
    vmBindOpMetadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vmBindOpMetadata.base.len = sizeof(NEO::EuDebugEventVmBindOpMetadata);
    vmBindOpMetadata.base.seqno = 4;
    vmBindOpMetadata.vmBindOpRefSeqno = vmBindOp.base.seqno;
    vmBindOpMetadata.metadataHandle = metadataHandle1;
    vmBindOpMetadata.metadataCookie = 0x3;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));

    vmBindOpMetadata.base.seqno = 5;
    vmBindOpMetadata.metadataHandle = metadataHandle2;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));

    EXPECT_EQ(vmBindOpData.pendingNumExtensions, 0ull);
    EXPECT_EQ(vmBindOpData.vmBindOpMetadataVec.size(), 2ull);
    session->processPendingVmBindEvents();

    EXPECT_TRUE(session->pushApiEventAckEventsFound);

    EXPECT_EQ(connection->metaDataToModule[metadataHandle1].ackEvents->size(), 1ull);
    EXPECT_EQ(connection->metaDataToModule[metadataHandle1].ackEvents[0][0].seqno, vmBindUfence.base.seqno);
    EXPECT_EQ(connection->metaDataToModule[metadataHandle1].ackEvents[0][0].type, vmBindUfence.base.type);

    auto event = session->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(reinterpret_cast<uint64_t>(connection->metaDataMap[metadataHandle2].data.get()), event.info.module.moduleBegin);
    EXPECT_EQ(reinterpret_cast<uint64_t>(connection->metaDataMap[metadataHandle2].data.get()) + connection->metaDataMap[metadataHandle2].metadata.len, event.info.module.moduleEnd);
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
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    auto &connection = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    NEO::EuDebugEventMetadata metadata{};
    metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadataHandle = 10;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataProgramModule);
    metadata.len = sizeof(metadata);
    connection->metaDataMap[10].metadata = metadata;
    connection->metaDataToModule[10].segmentCount = 2;

    metadata.metadataHandle = 11;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataElfBinary);
    metadata.len = sizeof(metadata);
    connection->metaDataMap[11].metadata = metadata;
    auto ptr = std::make_unique<char[]>(metadata.len);
    connection->metaDataMap[11].data = std::move(ptr);
    connection->metaDataMap[11].metadata.len = 10000;

    const uint64_t vmHandle = 0x1234;
    connection->vmToTile[vmHandle] = 0u;
    NEO::EuDebugEventVmBind vmBind{};
    vmBind.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBind);
    vmBind.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
    vmBind.base.len = sizeof(NEO::EuDebugEventVmBind);
    vmBind.base.seqno = seqno++;
    vmBind.clientHandle = client1.clientHandle;
    vmBind.vmHandle = vmHandle;
    vmBind.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
    vmBind.numBinds = 1;

    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind.base));
    auto &vmBindMap = session->clientHandleToConnection[client1.clientHandle]->vmBindMap;

    NEO::EuDebugEventVmBindOp vmBindOp{};
    vmBindOp.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOp);
    vmBindOp.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vmBindOp.base.len = sizeof(NEO::EuDebugEventVmBindOp);
    vmBindOp.base.seqno = seqno++;
    vmBindOp.numExtensions = 2;
    vmBindOp.addr = seg1Va;
    vmBindOp.range = 1000;
    vmBindOp.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));

    NEO::EuDebugEventVmBindUfence vmBindUfence{};
    vmBindUfence.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindUfence);
    vmBindUfence.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate))) | static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitNeedAck)));
    vmBindUfence.base.len = sizeof(NEO::EuDebugEventVmBindUfence);
    vmBindUfence.base.seqno = seqno++;
    vmBindUfence.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence.base));

    auto &vmBindOpData = vmBindMap[vmBindOp.vmBindRefSeqno].vmBindOpMap[vmBindOp.base.seqno];
    NEO::EuDebugEventVmBindOpMetadata vmBindOpMetadata{};
    vmBindOpMetadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOpMetadata);
    vmBindOpMetadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vmBindOpMetadata.base.len = sizeof(NEO::EuDebugEventVmBindOpMetadata);
    vmBindOpMetadata.base.seqno = seqno++;
    vmBindOpMetadata.vmBindOpRefSeqno = vmBindOp.base.seqno;
    vmBindOpMetadata.metadataHandle = 10;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));

    vmBindOpMetadata.base.seqno = seqno++;
    vmBindOpMetadata.metadataHandle = 11;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));
    ASSERT_EQ(vmBindOpData.pendingNumExtensions, 0ull);
    ASSERT_EQ(vmBindOpData.vmBindOpMetadataVec.size(), 2ull);
    session->processPendingVmBindEvents();
    EXPECT_EQ(session->apiEvents.size(), 0u);

    // Now bind 2nd segment
    vmBind.base.seqno = seqno++;
    vmBind.vmHandle = 0x1234;
    vmBind.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
    vmBind.numBinds = 1;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind.base));
    vmBindOp.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vmBindOp.base.seqno = seqno++;
    vmBindOp.numExtensions = 2;
    vmBindOp.vmBindRefSeqno = vmBind.base.seqno;
    vmBindOp.addr = seg2Va;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));
    vmBindOpMetadata.vmBindOpRefSeqno = vmBindOp.base.seqno;
    vmBindOpMetadata.base.seqno = seqno++;
    vmBindOpMetadata.metadataHandle = 10;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));
    vmBindOpMetadata.base.seqno = seqno++;
    vmBindOpMetadata.metadataHandle = 11;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));
    vmBindUfence.base.seqno = seqno++;
    vmBindUfence.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence.base));
    session->processPendingVmBindEvents();

    auto event = session->apiEvents.front();
    ASSERT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    session->apiEvents.pop();

    // now do unbinds
    vmBind.base.seqno = seqno++;
    vmBind.vmHandle = 0x1234;
    vmBind.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
    vmBind.numBinds = 1;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind.base));
    vmBindOp.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy)));
    vmBindOp.base.seqno = seqno++;
    vmBindOp.numExtensions = 0;
    vmBindOp.vmBindRefSeqno = vmBind.base.seqno;
    vmBindOp.addr = seg2Va;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));
    vmBindUfence.base.seqno = seqno++;
    vmBindUfence.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence.base));
    session->processPendingVmBindEvents();
    EXPECT_EQ(session->apiEvents.size(), 0u);

    vmBind.base.seqno = seqno++;
    vmBind.vmHandle = 0x555; // Unbind unmatched VM
    vmBind.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
    vmBind.numBinds = 1;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind.base));
    vmBindOp.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy)));
    vmBindOp.base.seqno = seqno++;
    vmBindOp.numExtensions = 0;
    vmBindOp.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));
    vmBindUfence.base.seqno = seqno++;
    vmBindUfence.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence.base));
    session->processPendingVmBindEvents();
    // Unmatched vmHandle should not trigger any events
    EXPECT_EQ(session->apiEvents.size(), 0u);

    // now unbind final segment
    vmBind.base.seqno = seqno++;
    vmBind.vmHandle = 0x1234;
    vmBind.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
    vmBind.numBinds = 1;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind.base));
    vmBindOp.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy)));
    vmBindOp.base.seqno = seqno++;
    vmBindOp.numExtensions = 0;
    vmBindOp.vmBindRefSeqno = vmBind.base.seqno;
    vmBindOp.addr = seg1Va;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));
    vmBindUfence.base.seqno = seqno++;
    vmBindUfence.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence.base));
    session->processPendingVmBindEvents();

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
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    auto &connection = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    NEO::EuDebugEventMetadata metadata{};
    metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadataHandle = 10;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataProgramModule);
    metadata.len = sizeof(metadata);
    connection->metaDataMap[10].metadata = metadata;

    metadata.metadataHandle = 11;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataSbaArea);
    connection->metaDataMap[11].metadata = metadata;
    metadata.metadataHandle = 12;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataSipArea);
    connection->metaDataMap[12].metadata = metadata;
    connection->metaDataToModule[10].segmentCount = 1;

    NEO::EuDebugEventVmBind vmBind{};
    vmBind.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBind);
    vmBind.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
    vmBind.base.len = sizeof(NEO::EuDebugEventVmBind);
    vmBind.base.seqno = 1;
    vmBind.clientHandle = client1.clientHandle;
    vmBind.vmHandle = 0x1234;
    vmBind.numBinds = 2;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind.base));
    auto &vmBindMap = session->clientHandleToConnection[client1.clientHandle]->vmBindMap;

    NEO::EuDebugEventVmBindOp vmBindOp{};
    // first vm_bind_op
    vmBindOp.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOp);
    vmBindOp.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vmBindOp.base.len = sizeof(NEO::EuDebugEventVmBindOp);
    vmBindOp.base.seqno = 2;
    vmBindOp.numExtensions = 2;
    vmBindOp.addr = 0xffff1234;
    vmBindOp.range = 1000;
    vmBindOp.vmBindRefSeqno = vmBind.base.seqno;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));

    auto &vmBindOpData = vmBindMap[vmBindOp.vmBindRefSeqno].vmBindOpMap[vmBindOp.base.seqno];
    NEO::EuDebugEventVmBindOpMetadata vmBindOpMetadata{};
    vmBindOpMetadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOpMetadata);
    vmBindOpMetadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vmBindOpMetadata.base.len = sizeof(NEO::EuDebugEventVmBindOpMetadata);
    vmBindOpMetadata.base.seqno = 3;
    vmBindOpMetadata.vmBindOpRefSeqno = vmBindOp.base.seqno;
    vmBindOpMetadata.metadataHandle = 10;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));

    vmBindOpMetadata.base.seqno = 4;
    vmBindOpMetadata.metadataHandle = 11;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));

    // second vm_bind_op
    vmBindOp.base.seqno = 5;
    vmBindOp.numExtensions = 1;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));

    vmBindOpMetadata.vmBindOpRefSeqno = 5;
    vmBindOpMetadata.base.seqno = 6;
    vmBindOpMetadata.metadataHandle = 12;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));

    EXPECT_EQ(vmBindOpData.pendingNumExtensions, 0ull);
    EXPECT_EQ(vmBindOpData.vmBindOpMetadataVec.size(), 2ull);
    EXPECT_EQ(vmBindMap[vmBindOp.vmBindRefSeqno].vmBindOpMap[5].pendingNumExtensions, 0ull);
    EXPECT_EQ(vmBindMap[vmBindOp.vmBindRefSeqno].vmBindOpMap[5].vmBindOpMetadataVec.size(), 1ull);

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
        EXPECT_EQ(static_cast<uint32_t>(sessionMock->getEuControlCmdUnlock()), handler->euControlArgs[0].euControl.cmd);
        EXPECT_EQ(static_cast<uint32_t>(NEO::EuDebugParam::euControlCmdResume), handler->euControlArgs[1].euControl.cmd);
    } else {
        EXPECT_EQ(1, handler->ioctlCalled);
        EXPECT_EQ(static_cast<uint32_t>(NEO::EuDebugParam::euControlCmdResume), handler->euControlArgs[0].euControl.cmd);
    }
    EXPECT_NE(0u, handler->euControlArgs[0].euControl.bitmaskSize);
    auto bitMaskPtrToCheck = handler->euControlArgs[0].euControl.bitmaskPtr;
    EXPECT_NE(0u, bitMaskPtrToCheck);

    EXPECT_EQ(0u, bitmaskSizeOut);
    EXPECT_EQ(nullptr, bitmaskOut.get());
}

HWTEST_F(DebugApiLinuxTestXe, GivenNoAttentionBitsWhenMultipleThreadsPassedToCheckStoppedThreadsAndGenerateEventsThenThreadsStateNotCheckedAndEventsNotGenerated) {
    MockL0GfxCoreHelperSupportsThreadControlStopped<FamilyType> mockL0GfxCoreHelper;
    std::unique_ptr<ApiGfxCoreHelper> l0GfxCoreHelperBackup(static_cast<ApiGfxCoreHelper *>(&mockL0GfxCoreHelper));
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);

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
    EXPECT_EQ(static_cast<uint32_t>(NEO::EuDebugParam::euControlCmdStopped), handler->euControlArgs[0].euControl.cmd);

    EXPECT_TRUE(sessionMock->allThreads[thread.packed]->isStopped());
    EXPECT_FALSE(sessionMock->allThreads[thread1.packed]->isStopped());
    EXPECT_FALSE(sessionMock->allThreads[thread2.packed]->isStopped());

    EXPECT_EQ(0u, sessionMock->apiEvents.size());
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);
    l0GfxCoreHelperBackup.release();
}

TEST_F(DebugApiLinuxTestXe, GivenEventSeqnoLowerEqualThanSentInterruptWhenHandlingAttentionEventThenEventIsNotProcessed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    uint8_t data[sizeof(NEO::EuDebugEventEuAttention) + 128];
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
    NEO::EuDebugEventEuAttention attention = {};
    attention.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeEuAttention);
    attention.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
    attention.base.seqno = 3;
    attention.base.len = sizeof(NEO::EuDebugEventEuAttention) + std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));
    attention.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    attention.flags = 0;
    attention.execQueueHandle = contextHandle0;
    attention.lrcHandle = lrcHandle0;
    attention.bitmaskSize = std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));

    memcpy(data, &attention, sizeof(NEO::EuDebugEventEuAttention));
    memcpy(ptrOffset(data, offsetof(NEO::EuDebugEventEuAttention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    sessionMock->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(data));

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_EQ(1u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->pendingInterrupts[0].second);
    EXPECT_FALSE(sessionMock->triggerEvents);

    // Validate for case when event seq number is less than euControlInterruptSeqno
    attention.base.seqno = 2;
    memcpy(data, &attention, sizeof(NEO::EuDebugEventEuAttention));
    memcpy(ptrOffset(data, offsetof(NEO::EuDebugEventEuAttention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    sessionMock->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(data));

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

    NEO::EuDebugEventEuAttention attention = {};
    attention.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeEuAttention);
    attention.base.len = sizeof(NEO::EuDebugEventEuAttention);
    attention.base.seqno = 2;
    attention.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    attention.lrcHandle = lrcHandle;
    attention.flags = 0;
    attention.lrcHandle = lrcHandle;
    attention.execQueueHandle = execQueueHandle;
    attention.bitmaskSize = 0;

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

    uint8_t data[sizeof(NEO::EuDebugEventEuAttention) + 128];
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

    NEO::EuDebugEventEuAttention attention = {};
    attention.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeEuAttention);
    attention.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
    attention.base.len = sizeof(NEO::EuDebugEventEuAttention) + std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));
    attention.base.seqno = 2;
    attention.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    attention.lrcHandle = lrcHandle;
    attention.flags = 0;
    attention.execQueueHandle = execQueueHandle;

    attention.bitmaskSize = std::min(uint32_t(128), static_cast<uint32_t>(bitmaskSize));

    memcpy(data, &attention, sizeof(NEO::EuDebugEventEuAttention));
    memcpy(ptrOffset(data, offsetof(NEO::EuDebugEventEuAttention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    sessionMock->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(data));

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
    NEO::EuDebugEventEuAttention attention = {};
    attention.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeEuAttention);
    attention.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
    attention.base.len = sizeof(NEO::EuDebugEventEuAttention);
    attention.base.seqno = 2;
    attention.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    attention.lrcHandle = 0;
    attention.flags = 0;
    attention.execQueueHandle = 0;
    attention.bitmaskSize = 0;
    uint8_t data[sizeof(NEO::EuDebugEventEuAttention) + 128];
    memcpy(data, &attention, sizeof(NEO::EuDebugEventEuAttention));

    sessionMock->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(data));

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

    uint8_t data[sizeof(NEO::EuDebugEventEuAttention) + 128];
    ze_device_thread_t thread{0, 0, 0, 0};

    sessionMock->stoppedThreads[EuThread::ThreadId(0, thread).packed] = 1;
    sessionMock->pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread, false));

    sessionMock->interruptSent = true;
    sessionMock->euControlInterruptSeqno = 1;

    NEO::EuDebugEventEuAttention attention = {};
    attention.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeEuAttention);
    attention.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
    attention.base.len = sizeof(NEO::EuDebugEventEuAttention);
    attention.base.seqno = 2;
    attention.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    attention.lrcHandle = lrcHandle;
    attention.flags = 0;
    attention.execQueueHandle = execQueueHandle;
    attention.bitmaskSize = 0;

    memcpy(data, &attention, sizeof(NEO::EuDebugEventEuAttention));

    sessionMock->expectedAttentionEvents = 2;

    sessionMock->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(data));

    EXPECT_FALSE(sessionMock->triggerEvents);

    attention.base.seqno = 10;
    memcpy(data, &attention, sizeof(NEO::EuDebugEventEuAttention));
    sessionMock->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(data));

    EXPECT_TRUE(sessionMock->triggerEvents);
}

TEST_F(DebugApiLinuxTestXe, GivenNoElfDataImplementationThenGetElfDataReturnsNullptr) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);

    auto clientConnection = sessionMock->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    NEO::EuDebugEventMetadata metadata{};
    metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadataHandle = 10;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataProgramModule);
    metadata.len = 1000;
    clientConnection->metaDataMap[10].metadata = metadata;
    clientConnection->metaDataMap[10].metadata.len = 1000;
    auto ptr = std::make_unique<char[]>(metadata.len);
    clientConnection->metaDataMap[10].data = std::move(ptr);

    metadata.metadataHandle = 11;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataElfBinary);
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
    session->returnTimeDiff = session->interruptTimeout * 10;
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
    EXPECT_EQ(MockDebugSessionLinuxXe::mockClientHandle, handler->vmOpen.clientHandle);
    EXPECT_EQ(vmHandle, handler->vmOpen.vmHandle);
    EXPECT_EQ(static_cast<uint64_t>(0), handler->vmOpen.flags);

    EXPECT_EQ(1u, session->debugArea.reserved1);
    EXPECT_EQ(1u, session->debugArea.version);
    EXPECT_EQ(4u, session->debugArea.pgsize);
}

TEST_F(DebugApiLinuxTestXe, GivenSharedModuleDebugAreaWhenVmBindDebugAreaThenIsaMapUpdatedAndDebugIsaNotTileInstanced) {
    constexpr uint64_t debugAreaVA = 0x1234000;

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->debugArea.isShared = true;

    constexpr uint64_t metadataHandle = 10;
    NEO::EuDebugEventMetadata metadata = {};
    metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadataHandle = metadataHandle;
    metadata.len = 0x1000;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataModuleArea);

    auto &clientConnection = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    clientConnection->metaDataMap[metadataHandle].metadata = std::move(metadata);

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);

    constexpr uint64_t vmHandle1 = 1;
    constexpr uint64_t vmHandle2 = 2;
    NEO::EuDebugEventVm vm1 = {};
    NEO::EuDebugEventVm vm2 = {};
    vm1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVm);
    vm1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vm1.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    vm1.vmHandle = vmHandle1;
    vm2.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVm);
    vm2.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    vm2.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    vm2.vmHandle = vmHandle2;

    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vm1));
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vm2));

    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmToModuleDebugAreaBindInfo[vmHandle1] = {debugAreaVA, 0x1000};
    session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmToTile[vmHandle1] = 0u;

    {
        NEO::EuDebugEventVmBind vmBindEvent = {};
        vmBindEvent.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBind);
        vmBindEvent.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
        vmBindEvent.base.len = sizeof(NEO::EuDebugEventVmBind);
        vmBindEvent.base.seqno = 1;
        vmBindEvent.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
        vmBindEvent.vmHandle = vmHandle1;
        vmBindEvent.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
        vmBindEvent.numBinds = 1;
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindEvent.base));

        auto &vmBindMap = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->vmBindMap;
        EXPECT_EQ(vmBindMap.size(), 1u);

        NEO::EuDebugEventVmBindOp vmBindOp{};
        vmBindOp.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOp);
        vmBindOp.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
        vmBindOp.base.len = sizeof(NEO::EuDebugEventVmBindOp);
        vmBindOp.base.seqno = 2;
        vmBindOp.numExtensions = 1;
        vmBindOp.addr = debugAreaVA;
        vmBindOp.range = 0x1000;
        vmBindOp.vmBindRefSeqno = vmBindEvent.base.seqno;
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp.base));

        NEO::EuDebugEventVmBindOpMetadata vmBindOpMetadata{};
        vmBindOpMetadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOpMetadata);
        vmBindOpMetadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
        vmBindOpMetadata.base.len = sizeof(NEO::EuDebugEventVmBindOpMetadata);
        vmBindOpMetadata.base.seqno = 3;
        vmBindOpMetadata.vmBindOpRefSeqno = vmBindOp.base.seqno;
        vmBindOpMetadata.metadataHandle = metadataHandle;
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata.base));

        NEO::EuDebugEventVmBindUfence vmBindUfence{};
        vmBindUfence.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindUfence);
        vmBindUfence.base.flags = (NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate))) | (NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitNeedAck)));
        vmBindUfence.base.len = sizeof(NEO::EuDebugEventVmBindUfence);
        vmBindUfence.base.seqno = 4;
        vmBindUfence.vmBindRefSeqno = vmBindEvent.base.seqno;
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence.base));
        session->processPendingVmBindEvents();
    }

    DebugAreaHeader debugArea;
    debugArea.reserved1 = 1;
    debugArea.pgsize = uint8_t(4);
    debugArea.version = 1;
    debugArea.isShared = 1;

    constexpr size_t bufSize = sizeof(session->debugArea);
    handler->mmapRet = reinterpret_cast<char *>(&debugArea);
    handler->pwriteRetVal = bufSize;
    handler->preadRetVal = bufSize;

    handler->setPreadMemory(reinterpret_cast<char *>(&debugArea), sizeof(debugArea), debugAreaVA);
    handler->setPwriteMemory(reinterpret_cast<char *>(&debugArea), sizeof(debugArea), debugAreaVA);

    session->readModuleDebugArea();
    EXPECT_EQ(1u, session->debugArea.version);

    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    zet_debug_memory_space_desc_t desc;
    desc.address = debugAreaVA;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    uint8_t buffer[16];
    memset(buffer, 1, sizeof(buffer));
    handler->pwriteRetVal = 16;

    auto result = session->writeMemory(thread, &desc, 16, buffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &isaMap = clientConnection->isaMap[0];
    EXPECT_EQ(isaMap.size(), 1u);
    EXPECT_FALSE(isaMap[debugAreaVA]->tileInstanced);
}

TEST_F(DebugApiLinuxTestXe, GivenFifoPollEnvironmentVariableWhenAsyncThreadLaunchedThenDebugUmdFifoPollIntervalUpdated) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.DebugUmdFifoPollInterval.set(100);
    auto session = std::make_unique<MockDebugSessionLinuxXe>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    EXPECT_EQ(150, session->fifoPollInterval);
    session->asyncThread.threadActive = false;
    session->asyncThreadFunction(session.get());

    EXPECT_EQ(100, session->fifoPollInterval);

    session->closeAsyncThread();
}

TEST_F(DebugApiLinuxTestXe, GivenInterruptTimeoutProvidedByDebugVariablesWhenAsyncThreadLaunchedThenInterruptTimeoutCorrectlyRead) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.DebugUmdInterruptTimeout.set(5000);
    auto session = std::make_unique<MockDebugSessionLinuxXe>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    EXPECT_EQ(2000, session->interruptTimeout);
    session->asyncThread.threadActive = false;
    session->asyncThreadFunction(session.get());

    EXPECT_EQ(5000, session->interruptTimeout);

    session->closeAsyncThread();
}

TEST_F(DebugApiLinuxTestXe, GivenMaxRetriesProvidedByDebugVariablesWhenAsyncThreadLaunchedThenMaxRetriesCorrectlyRead) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.DebugUmdMaxReadWriteRetry.set(10);
    auto session = std::make_unique<MockDebugSessionLinuxXe>(zet_debug_config_t{0x1234}, device, 10);
    ASSERT_NE(nullptr, session);

    EXPECT_EQ(3, session->maxRetries);
    session->asyncThread.threadActive = false;
    session->asyncThreadFunction(session.get());

    EXPECT_EQ(10, session->maxRetries);

    session->closeAsyncThread();
}

TEST(DebugSessionLinuxXeTest, GivenRootDebugSessionWhenCreateTileSessionCalledThenSessionIsNotCreated) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto mockDrm = new DrmMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    MockDeviceImp deviceImp(neoDevice);
    struct DebugSession : public DebugSessionLinuxXe {
        using DebugSessionLinuxXe::createTileSession;
        using DebugSessionLinuxXe::DebugSessionLinuxXe;
    };
    auto session = std::make_unique<DebugSession>(zet_debug_config_t{0x1234}, &deviceImp, 10, nullptr);
    ASSERT_NE(nullptr, session);

    std::unique_ptr<DebugSessionImp> tileSession = std::unique_ptr<DebugSessionImp>{session->createTileSession(zet_debug_config_t{0x1234}, &deviceImp, nullptr)};
    EXPECT_EQ(nullptr, tileSession);
}

TEST_F(DebugApiLinuxTestXe, GivenExecQueuePlacementEventWhenHandlingThenVmToTileMapUpdates) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();

    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    constexpr uint64_t vmHandle = 10;
    // Allocate memory for the structure including the flexible array member
    constexpr auto size = sizeof(NEO::EuDebugEventExecQueuePlacements) + sizeof(drm_xe_engine_class_instance) * 1;
    auto memory = alignedMalloc(size, sizeof(NEO::EuDebugEventExecQueuePlacements));
    auto execQueuePlacements = static_cast<NEO::EuDebugEventExecQueuePlacements *>(memory);
    memset(execQueuePlacements, 0, size);
    execQueuePlacements->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeExecQueuePlacements);
    execQueuePlacements->base.len = sizeof(NEO::EuDebugEventExecQueuePlacements);
    execQueuePlacements->clientHandle = client1.clientHandle;
    execQueuePlacements->vmHandle = vmHandle;
    execQueuePlacements->execQueueHandle = 200;
    execQueuePlacements->lrcHandle = 10;
    execQueuePlacements->numPlacements = 1;

    auto engineClassInstance = reinterpret_cast<drm_xe_engine_class_instance *>(&(execQueuePlacements->instances[0]));
    engineClassInstance[0].engine_class = 0;
    engineClassInstance[0].engine_instance = 1;
    engineClassInstance[0].gt_id = 2;

    session->handleEvent(&execQueuePlacements->base);
    alignedFree(memory);

    EXPECT_NE(session->clientHandleToConnection[client1.clientHandle]->vmToTile.end(),
              session->clientHandleToConnection[client1.clientHandle]->vmToTile.find(vmHandle));
    EXPECT_EQ(1u, session->clientHandleToConnection[client1.clientHandle]->vmToTile[vmHandle]);
}

TEST_F(DebugApiLinuxTestXe, GivenMultipleExecQueuePlacementEventForSameVmHandleWithDifferentTileIndexWhenHandlingThenErrorReported) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_ERROR);
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();

    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = 0x123456789;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    constexpr uint64_t vmHandle = 10;
    session->clientHandleToConnection[client1.clientHandle]->vmToTile[vmHandle] = 1;
    // Allocate memory for the structure including the flexible array member
    constexpr auto size = sizeof(NEO::EuDebugEventExecQueuePlacements) + sizeof(NEO::XeEngineClassInstance) * 1;
    auto memory = alignedMalloc(size, sizeof(NEO::EuDebugEventExecQueuePlacements));
    auto execQueuePlacements = static_cast<NEO::EuDebugEventExecQueuePlacements *>(memory);
    memset(execQueuePlacements, 0, size);
    execQueuePlacements->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeExecQueuePlacements);
    execQueuePlacements->base.len = sizeof(NEO::EuDebugEventExecQueuePlacements);
    execQueuePlacements->clientHandle = client1.clientHandle;
    execQueuePlacements->vmHandle = vmHandle;
    execQueuePlacements->execQueueHandle = 200;
    execQueuePlacements->lrcHandle = 10;
    execQueuePlacements->numPlacements = 1;

    auto engineClassInstance = reinterpret_cast<NEO::XeEngineClassInstance *>(&(execQueuePlacements->instances[0]));
    engineClassInstance[0].engineClass = 0;
    engineClassInstance[0].engineInstance = 1;
    engineClassInstance[0].gtId = 0;

    ::testing::internal::CaptureStderr();
    session->handleEvent(&execQueuePlacements->base);
    alignedFree(memory);

    auto infoMessage = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(1u, session->clientHandleToConnection[client1.clientHandle]->vmToTile[vmHandle]);
    EXPECT_TRUE(hasSubstr(infoMessage, std::string("tileIndex = 1 already present. Attempt to overwrite with tileIndex = 0")));
}

struct DebugApiLinuxMultiDeviceVmBindFixtureXe : public DebugApiLinuxMultiDeviceFixtureXe {
    void setUp() {
        DebugApiLinuxMultiDeviceFixtureXe::setUp();

        zet_debug_config_t config = {};
        config.pid = 0x1234;

        session = std::make_unique<MockDebugSessionLinuxXe>(config, deviceImp, 10);
        ASSERT_NE(nullptr, session);
        session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

        handler = new MockIoctlHandlerXe;
        session->ioctlHandler.reset(handler);
    }

    void tearDown() {
        DebugApiLinuxMultiDeviceFixtureXe::tearDown();
    }

    void createVmBindEvent(NEO::EuDebugEventVmBind &vmBind, uint64_t vmHandle, uint32_t &seqNo) {
        vmBind.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBind);
        vmBind.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange)));
        vmBind.base.len = sizeof(NEO::EuDebugEventVmBind);
        vmBind.base.seqno = seqNo++;
        vmBind.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
        vmBind.vmHandle = vmHandle;
        vmBind.flags = static_cast<uint64_t>(NEO::EuDebugParam::eventVmBindFlagUfence);
        vmBind.numBinds = 1;
    }

    void createVmBindOpEvent(NEO::EuDebugEventVmBindOp &vmBindOp, uint32_t &seqNo, uint64_t vmBindRefSeqNo,
                             uint64_t vmBindOpAddr, uint64_t numExtensions, uint16_t flags) {
        vmBindOp.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOp);
        vmBindOp.base.flags = flags;
        vmBindOp.base.len = sizeof(NEO::EuDebugEventVmBindOp);
        vmBindOp.base.seqno = seqNo++;
        vmBindOp.numExtensions = numExtensions;
        vmBindOp.addr = vmBindOpAddr;
        vmBindOp.range = 1000;
        vmBindOp.vmBindRefSeqno = vmBindRefSeqNo;
    }

    void createUfenceEvent(NEO::EuDebugEventVmBindUfence &vmBindUfence, uint32_t &seqNo, uint64_t vmBindRefSeqno) {
        vmBindUfence.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindUfence);
        vmBindUfence.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate))) | static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitNeedAck)));
        vmBindUfence.base.len = sizeof(NEO::EuDebugEventVmBindUfence);
        vmBindUfence.base.seqno = seqNo++;
        vmBindUfence.vmBindRefSeqno = vmBindRefSeqno;
    }

    void createVmBindOpMetadata(NEO::EuDebugEventVmBindOpMetadata &vmBindOpMetadata, uint32_t &seqNo, uint64_t vmBindOpRefSeqno, uint64_t metadataHandle) {
        vmBindOpMetadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeVmBindOpMetadata);
        vmBindOpMetadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
        vmBindOpMetadata.base.len = sizeof(NEO::EuDebugEventVmBindOpMetadata);
        vmBindOpMetadata.base.seqno = seqNo++;
        vmBindOpMetadata.vmBindOpRefSeqno = vmBindOpRefSeqno;
        vmBindOpMetadata.metadataHandle = metadataHandle;
        vmBindOpMetadata.metadataCookie = 0x3;
    }

    MockIoctlHandlerXe *handler = nullptr;
    std::unique_ptr<MockDebugSessionLinuxXe> session;
    const uint64_t vmHandle1 = 0x1234;
    const uint64_t vmHandle2 = 0x1235;
    const uint64_t metadataHandle1 = 10;
    const uint64_t metadataHandle2 = 11;
    uint32_t seqNo = 1;
};

using DebugApiLinuxMultiDeviceVmBindTestXe = Test<DebugApiLinuxMultiDeviceVmBindFixtureXe>;
TEST_F(DebugApiLinuxMultiDeviceVmBindTestXe, GivenVmBindForProgramModuleWhenHandlingEventThenModuleLoadAndUnloadIsTriggeredAfterAllInstancesEventsReceived) {
    NEO::EuDebugEventClient client1;
    client1.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client1.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client1.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    session->pushApiEventValidateAckEvents = true;
    session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&client1));

    auto &connection = session->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle];
    connection->vmToTile[vmHandle1] = 1u;
    connection->vmToTile[vmHandle2] = 0u;

    NEO::EuDebugEventMetadata metadata{};
    metadata.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeMetadata);
    metadata.base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    metadata.base.len = sizeof(NEO::EuDebugEventMetadata);
    metadata.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    metadata.metadataHandle = metadataHandle1;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataProgramModule);
    metadata.len = sizeof(metadata);
    connection->metaDataMap[metadataHandle1].metadata = metadata;
    connection->metaDataToModule[metadataHandle1].segmentCount = 1;

    metadata.metadataHandle = metadataHandle2;
    metadata.type = static_cast<uint64_t>(NEO::EuDebugParam::metadataElfBinary);
    metadata.len = sizeof(metadata);
    connection->metaDataMap[metadataHandle2].metadata = metadata;
    auto ptr = std::make_unique<char[]>(metadata.len);
    connection->metaDataMap[metadataHandle2].data = std::move(ptr);
    connection->metaDataMap[metadataHandle2].metadata.len = 10000;

    {
        NEO::EuDebugEventVmBind vmBind1{};
        createVmBindEvent(vmBind1, vmHandle1, seqNo);
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind1.base));

        NEO::EuDebugEventVmBindOp vmBindOp1{};
        createVmBindOpEvent(vmBindOp1, seqNo, vmBind1.base.seqno, 0xffff1234, 2, static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate))));
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp1.base));

        NEO::EuDebugEventVmBindUfence vmBindUfence1{};
        createUfenceEvent(vmBindUfence1, seqNo, vmBind1.base.seqno);
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence1.base));

        NEO::EuDebugEventVmBindOpMetadata vmBindOpMetadata1{};
        createVmBindOpMetadata(vmBindOpMetadata1, seqNo, vmBindOp1.base.seqno, metadataHandle1);
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata1.base));

        vmBindOpMetadata1.base.seqno = seqNo++;
        vmBindOpMetadata1.metadataHandle = metadataHandle2;
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata1.base));

        auto &vmBindMap = session->clientHandleToConnection[client1.clientHandle]->vmBindMap;
        auto &vmBindOpData = vmBindMap[vmBindOp1.vmBindRefSeqno].vmBindOpMap[vmBindOp1.base.seqno];
        EXPECT_EQ(vmBindOpData.pendingNumExtensions, 0ull);
        EXPECT_EQ(vmBindOpData.vmBindOpMetadataVec.size(), 2ull);
        session->processPendingVmBindEvents();
    }
    EXPECT_FALSE(session->pushApiEventAckEventsFound);
    EXPECT_TRUE(connection->metaDataToModule[metadataHandle1].ackEvents->empty());
    EXPECT_TRUE(session->apiEvents.empty());

    NEO::EuDebugEventVmBindUfence vmBindUfence2{};
    {
        NEO::EuDebugEventVmBind vmBind2{};
        createVmBindEvent(vmBind2, vmHandle2, seqNo);
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind2.base));

        NEO::EuDebugEventVmBindOp vmBindOp2{};
        createVmBindOpEvent(vmBindOp2, seqNo, vmBind2.base.seqno, 0xffff1235, 2, static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate))));
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp2.base));

        createUfenceEvent(vmBindUfence2, seqNo, vmBind2.base.seqno);
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence2.base));

        NEO::EuDebugEventVmBindOpMetadata vmBindOpMetadata2{};
        createVmBindOpMetadata(vmBindOpMetadata2, seqNo, vmBindOp2.base.seqno, metadataHandle1);
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata2.base));

        vmBindOpMetadata2.base.seqno = seqNo++;
        vmBindOpMetadata2.metadataHandle = metadataHandle2;
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOpMetadata2.base));

        auto &vmBindMap = session->clientHandleToConnection[client1.clientHandle]->vmBindMap;
        auto &vmBindOpData = vmBindMap[vmBindOp2.vmBindRefSeqno].vmBindOpMap[vmBindOp2.base.seqno];
        EXPECT_EQ(vmBindOpData.pendingNumExtensions, 0ull);
        EXPECT_EQ(vmBindOpData.vmBindOpMetadataVec.size(), 2ull);
        session->processPendingVmBindEvents();
    }
    EXPECT_TRUE(session->pushApiEventAckEventsFound);

    EXPECT_EQ(connection->metaDataToModule[metadataHandle1].ackEvents->size(), 1ull);
    EXPECT_EQ(connection->metaDataToModule[metadataHandle1].ackEvents[0][0].seqno, vmBindUfence2.base.seqno);
    EXPECT_EQ(connection->metaDataToModule[metadataHandle1].ackEvents[0][0].type, vmBindUfence2.base.type);

    auto event = session->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(reinterpret_cast<uint64_t>(connection->metaDataMap[metadataHandle2].data.get()), event.info.module.moduleBegin);
    EXPECT_EQ(reinterpret_cast<uint64_t>(connection->metaDataMap[metadataHandle2].data.get()) + connection->metaDataMap[metadataHandle2].metadata.len, event.info.module.moduleEnd);

    session->apiEvents.pop();

    // now do unbinds
    NEO::EuDebugEventVmBindUfence vmBindUfence3{};
    {
        NEO::EuDebugEventVmBind vmBind3{};
        createVmBindEvent(vmBind3, vmHandle2, seqNo);
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind3.base));

        NEO::EuDebugEventVmBindOp vmBindOp3{};
        createVmBindOpEvent(vmBindOp3, seqNo, vmBind3.base.seqno, 0xffff1235, 0, static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy))));
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp3.base));

        createUfenceEvent(vmBindUfence3, seqNo, vmBind3.base.seqno);
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence3.base));
        session->processPendingVmBindEvents();
        EXPECT_TRUE(session->apiEvents.empty());
    }

    NEO::EuDebugEventVmBindUfence vmBindUfence4{};
    {
        NEO::EuDebugEventVmBind vmBind4{};
        createVmBindEvent(vmBind4, vmHandle1, seqNo);
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBind4.base));

        NEO::EuDebugEventVmBindOp vmBindOp4{};
        createVmBindOpEvent(vmBindOp4, seqNo, vmBind4.base.seqno, 0xffff1234, 0, static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitDestroy))));
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindOp4.base));

        createUfenceEvent(vmBindUfence4, seqNo, vmBind4.base.seqno);
        session->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(&vmBindUfence4.base));
        session->processPendingVmBindEvents();
    }
    event = session->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD, event.type);
}

TEST_F(DebugApiLinuxTestXe, GivenPageFaultEventThenPageFaultHandledCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    uint64_t execQueueHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    sessionMock->clientHandleToConnection[MockDebugSessionLinuxXe::mockClientHandle]->lrcHandleToVmHandle[lrcHandle] = vmHandle;

    uint8_t data[sizeof(NEO::EuDebugEventPageFault) + 128];
    ze_device_thread_t thread{0, 0, 0, 0};

    sessionMock->stoppedThreads[EuThread::ThreadId(0, thread).packed] = 1;
    sessionMock->pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread, false));

    sessionMock->interruptSent = true;
    sessionMock->euControlInterruptSeqno = 1;

    NEO::EuDebugEventPageFault pf = {};
    pf.base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypePagefault);
    pf.base.flags = static_cast<uint16_t>(NEO::EuDebugParam::eventBitStateChange);
    pf.base.len = sizeof(data);
    pf.base.seqno = 2;
    pf.clientHandle = MockDebugSessionLinuxXe::mockClientHandle;
    pf.lrcHandle = lrcHandle;
    pf.flags = 0;
    pf.execQueueHandle = execQueueHandle;
    pf.bitmaskSize = 0;
    memcpy(data, &pf, sizeof(NEO::EuDebugEventPageFault));
    sessionMock->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(data));
    EXPECT_EQ(sessionMock->handlePageFaultEventCalled, 1u);

    pf.lrcHandle = 0x1234;
    memcpy(data, &pf, sizeof(NEO::EuDebugEventPageFault));
    sessionMock->handleEvent(reinterpret_cast<NEO::EuDebugEvent *>(data));
    EXPECT_EQ(sessionMock->handlePageFaultEventCalled, 1u);
}

} // namespace ult
} // namespace L0
