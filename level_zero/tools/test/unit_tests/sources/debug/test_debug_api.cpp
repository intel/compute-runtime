/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/test_debug_api.inl"

#include "test.h"

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
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::DebugSession *session = L0::DebugSession::create(config, nullptr);
    EXPECT_EQ(nullptr, session);
}

} // namespace ult
} // namespace L0
