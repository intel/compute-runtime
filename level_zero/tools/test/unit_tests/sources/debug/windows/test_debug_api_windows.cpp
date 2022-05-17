/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

struct DebugApiWindowsFixture : public DeviceFixture {
    void SetUp() {
        DeviceFixture::SetUp();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }
};

using DebugApiWinTest = Test<DebugApiWindowsFixture>;

TEST_F(DebugApiWinTest, givenDeviceWhenGettingDebugPropertiesThanNoFlagIsSet) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, debugProperties.flags);
}

TEST_F(DebugApiWinTest, givenDeviceWhenCallingDebugAttachThenErrorIsReturned) {

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession = nullptr;

    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(nullptr, debugSession);
}

} // namespace ult
} // namespace L0
