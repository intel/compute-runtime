/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/test_debug_api.inl"

#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/source/debug/debug_handlers.h"

namespace L0 {
namespace ult {

TEST_F(DebugApiTest, givenDeviceWhenDebugAttachIsCalledThenNullptrSessionHandleAndErrorAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    zet_debug_session_handle_t debugSession = nullptr;
    auto result = zetDebugAttach(deviceImp.toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(nullptr, debugSession);
}

TEST_F(DebugApiTest, givenDebugSessionSetWhenGettingDebugSessionThenCorrectObjectIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    EXPECT_NE(nullptr, deviceImp.getDebugSession(config));
}

TEST_F(DebugApiTest, givenNoDebugSessionWhenGettingDebugSessionThenNullptrIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.debugSession.release();
    EXPECT_EQ(nullptr, deviceImp.getDebugSession(config));
}

TEST_F(DebugApiTest, givenSubdeviceWhenGettingDebugSessionThenNullptrIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.isSubdevice = true;
    EXPECT_EQ(nullptr, deviceImp.getDebugSession(config));
}

TEST(DebugSessionTest, WhenDebugSessionCreateIsCalledThenNullptrReturned) {
    ze_result_t result;
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::DebugSession *session = L0::DebugSession::create(config, nullptr, result);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(nullptr, session);
}

TEST(DebugSessionTest, WhenUnsupportedFunctionCalledThenErrorIsReturned) {
    zet_debug_session_handle_t session = {};

    auto result = L0::DebugApiHandlers::debugDetach(session);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugReadEvent(session, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    ze_device_thread_t thread = {};
    result = L0::DebugApiHandlers::debugInterrupt(session, thread);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugResume(session, thread);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugReadMemory(session, thread, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugWriteMemory(session, thread, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugAcknowledgeEvent(session, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    zet_device_handle_t hDevice = {};
    result = L0::DebugApiHandlers::debugGetRegisterSetProperties(hDevice, nullptr, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    uint32_t type = {0};
    result = L0::DebugApiHandlers::debugReadRegisters(session, thread, type, 0, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugWriteRegisters(session, thread, type, 0, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

} // namespace ult
} // namespace L0
