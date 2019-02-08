/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_aub_center.h"

#include "third_party/aub_stream/headers/options.h"

#include "gtest/gtest.h"
using namespace OCLRT;

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
    VariableBackup<std::string> backup(&aub_stream::tbxServerIp);

    MockAubCenter aubCenter(platformDevices[0], false, "", CommandStreamReceiverType::CSR_TBX);

    EXPECT_STREQ("10.10.10.10", aub_stream::tbxServerIp.c_str());
}

TEST(AubCenter, GivenUseAubStreamAndTbxServerPortDebugVariableSetWhenAubCenterIsCreatedThenServerIpIsModified) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);
    DebugManager.flags.TbxPort.set(1234);

    VariableBackup<uint16_t> backup(&aub_stream::tbxServerPort);

    uint16_t port = 1234u;
    EXPECT_NE(port, aub_stream::tbxServerPort);

    MockAubCenter aubCenter(platformDevices[0], false, "", CommandStreamReceiverType::CSR_TBX);
    EXPECT_EQ(port, aub_stream::tbxServerPort);
}
