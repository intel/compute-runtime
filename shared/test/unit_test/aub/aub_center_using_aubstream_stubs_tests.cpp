/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/options.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "aubstream/aubstream.h"
#include "gtest/gtest.h"

using namespace NEO;

namespace aub_stream_stubs {
extern uint16_t tbxServerPort;
extern std::string tbxServerIp;
extern bool tbxFrontdoorMode;
} // namespace aub_stream_stubs

TEST(AubCenter, GivenUseAubStreamAndTbxServerIpDebugVariableSetWhenAubCenterIsCreatedThenServerIpIsModified) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseAubStream.set(true);
    debugManager.flags.TbxServer.set("10.10.10.10");
    VariableBackup<std::string> backup(&aub_stream_stubs::tbxServerIp);

    ASSERT_STRNE("10.10.10.10", aub_stream_stubs::tbxServerIp.c_str());

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);

    EXPECT_STREQ("10.10.10.10", aub_stream_stubs::tbxServerIp.c_str());
}

TEST(AubCenter, GivenUseAubStreamAndTbxServerPortDebugVariableSetWhenAubCenterIsCreatedThenServerPortIsModified) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseAubStream.set(true);
    debugManager.flags.TbxPort.set(1234);

    VariableBackup<uint16_t> backup(&aub_stream_stubs::tbxServerPort);

    aub_stream_stubs::tbxServerPort = 4321u;
    uint16_t port = 1234u;
    ASSERT_NE(port, aub_stream_stubs::tbxServerPort);

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(port, aub_stream_stubs::tbxServerPort);
}

TEST(AubCenter, GivenUseAubStreamAndTbxFrontdoorModeDebugVariableSetWhenAubCenterIsCreatedThenFrontdoorModeIsModified) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseAubStream.set(true);
    debugManager.flags.TbxFrontdoorMode.set(true);

    VariableBackup<bool> backup(&aub_stream_stubs::tbxFrontdoorMode);

    aub_stream_stubs::tbxFrontdoorMode = false;
    EXPECT_FALSE(aub_stream_stubs::tbxFrontdoorMode);

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::tbx);
    EXPECT_TRUE(aub_stream_stubs::tbxFrontdoorMode);
}
