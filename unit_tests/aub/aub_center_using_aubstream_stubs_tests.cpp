/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "core/helpers/hw_info.h"
#include "core/helpers/options.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/helpers/default_hw_info.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_aub_center.h"

#include "gtest/gtest.h"
#include "third_party/aub_stream/headers/aubstream.h"

using namespace NEO;

namespace aub_stream_stubs {
extern uint16_t tbxServerPort;
extern std::string tbxServerIp;
} // namespace aub_stream_stubs

TEST(AubCenter, GivenUseAubStreamDebugVariableSetWhenAubCenterIsCreatedThenAubManagerIsNotCreated) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    MockAubCenter aubCenter(platformDevices[0], false, "test", CommandStreamReceiverType::CSR_AUB);

    EXPECT_EQ(nullptr, aubCenter.aubManager.get());
}

TEST(AubCenter, GivenUseAubStreamAndTbxServerIpDebugVariableSetWhenAubCenterIsCreatedThenServerIpIsModified) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);
    DebugManager.flags.TbxServer.set("10.10.10.10");
    VariableBackup<std::string> backup(&aub_stream_stubs::tbxServerIp);

    MockAubCenter aubCenter(platformDevices[0], false, "", CommandStreamReceiverType::CSR_TBX);

    EXPECT_STREQ("10.10.10.10", aub_stream_stubs::tbxServerIp.c_str());
}

TEST(AubCenter, GivenUseAubStreamAndTbxServerPortDebugVariableSetWhenAubCenterIsCreatedThenServerIpIsModified) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);
    DebugManager.flags.TbxPort.set(1234);

    VariableBackup<uint16_t> backup(&aub_stream_stubs::tbxServerPort);

    uint16_t port = 1234u;
    EXPECT_NE(port, aub_stream_stubs::tbxServerPort);

    MockAubCenter aubCenter(platformDevices[0], false, "", CommandStreamReceiverType::CSR_TBX);
    EXPECT_EQ(port, aub_stream_stubs::tbxServerPort);
}
