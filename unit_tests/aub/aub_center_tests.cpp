/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_aub_center.h"
#include "unit_tests/mocks/mock_aub_manager.h"

#include "third_party/aub_stream/headers/modes.h"

#include "gtest/gtest.h"
using namespace OCLRT;

TEST(AubCenter, GivenUseAubStreamDebugVariableNotSetWhenAubCenterIsCreatedThenAubCenterDoesNotCreateAubManager) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(false);

    MockAubCenter aubCenter(platformDevices[0], false, "", CommandStreamReceiverType::CSR_AUB);
    EXPECT_EQ(nullptr, aubCenter.aubManager.get());
}

TEST(AubCenter, GivenUseAubStreamDebugVariableSetWhenAubCenterIsCreatedThenCreateAubManagerWithCorrectParameters) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(false);

    MockAubManager *mockAubManager = new MockAubManager(platformDevices[0]->pPlatform->eProductFamily, 4, 8 * MB, true, "aub_file.aub", aub_stream::mode::aubFile);
    MockAubCenter mockAubCenter(platformDevices[0], false, "", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter.aubManager = std::unique_ptr<MockAubManager>(mockAubManager);

    EXPECT_EQ(platformDevices[0]->pPlatform->eProductFamily, mockAubManager->mockAubManagerParams.productFamily);
    EXPECT_EQ(4, mockAubManager->mockAubManagerParams.devicesCount);
    EXPECT_EQ(8 * MB, mockAubManager->mockAubManagerParams.memoryBankSize);
    EXPECT_EQ(true, mockAubManager->mockAubManagerParams.localMemorySupported);
    EXPECT_STREQ("aub_file.aub", mockAubManager->mockAubManagerParams.aubFileName.c_str());
    EXPECT_EQ(aub_stream::mode::aubFile, mockAubManager->mockAubManagerParams.streamMode);
}

TEST(AubCenter, GivenDefaultSetCommandStreamReceiverFlagAndAubFileNameWhenGettingAubStreamModeThenModeAubFileIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    std::string aubFile("test.aub");
    auto mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::CSR_AUB);

    EXPECT_EQ(aub_stream::mode::aubFile, mode);
}

TEST(AubCenter, GivenCsrHwAndEmptyAubFileNameWhenGettingAubStreamModeThenModeAubFileIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    std::string aubFile("");
    auto mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::CSR_HW);

    EXPECT_EQ(aub_stream::mode::aubFile, mode);
}

TEST(AubCenter, GivenCsrHwAndNotEmptyAubFileNameWhenGettingAubStreamModeThenModeAubFileIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    std::string aubFile("test.aub");
    auto mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::CSR_HW);

    EXPECT_EQ(aub_stream::mode::aubFile, mode);
}

TEST(AubCenter, GivenCsrTypeWhenGettingAubStreamModeThenCorrectModeIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    std::string aubFile("test.aub");

    auto mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::CSR_AUB);
    EXPECT_EQ(aub_stream::mode::aubFile, mode);

    mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::CSR_TBX);
    EXPECT_EQ(aub_stream::mode::tbx, mode);

    mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::CSR_TBX_WITH_AUB);
    EXPECT_EQ(aub_stream::mode::aubFileAndTbx, mode);
}

TEST(AubCenter, GivenSetCommandStreamReceiverFlagEqualDefaultHwWhenAubManagerIsCreatedThenCsrTypeDefinesAubStreamMode) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    std::vector<CommandStreamReceiverType> aubTypes = {CommandStreamReceiverType::CSR_HW,
                                                       CommandStreamReceiverType::CSR_HW_WITH_AUB,
                                                       CommandStreamReceiverType::CSR_AUB};

    for (auto type : aubTypes) {
        MockAubCenter aubCenter(platformDevices[0], true, "test", type);
        EXPECT_EQ(aub_stream::mode::aubFile, aubCenter.aubStreamMode);
    }

    MockAubCenter aubCenter2(platformDevices[0], true, "", CommandStreamReceiverType::CSR_TBX);
    EXPECT_EQ(aub_stream::mode::tbx, aubCenter2.aubStreamMode);

    MockAubCenter aubCenter3(platformDevices[0], true, "", CommandStreamReceiverType::CSR_TBX_WITH_AUB);
    EXPECT_EQ(aub_stream::mode::aubFileAndTbx, aubCenter3.aubStreamMode);
}

TEST(AubCenter, GivenSetCommandStreamReceiverFlagSetWhenAubManagerIsCreatedThenDebugFlagDefinesAubStreamMode) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_TBX);

    MockAubCenter aubCenter(platformDevices[0], true, "", CommandStreamReceiverType::CSR_AUB);
    EXPECT_EQ(aub_stream::mode::tbx, aubCenter.aubStreamMode);

    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_TBX_WITH_AUB);

    MockAubCenter aubCenter2(platformDevices[0], true, "", CommandStreamReceiverType::CSR_AUB);
    EXPECT_EQ(aub_stream::mode::aubFileAndTbx, aubCenter2.aubStreamMode);
}
