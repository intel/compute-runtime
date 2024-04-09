/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "aubstream/aubstream.h"
#include "gtest/gtest.h"

using namespace NEO;

struct AubCenterTests : public ::testing::Test {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment{};
    RootDeviceEnvironment &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
};

TEST_F(AubCenterTests, GivenUseAubStreamDebugVariableNotSetWhenAubCenterIsCreatedThenAubCenterDoesNotCreateAubManager) {
    debugManager.flags.UseAubStream.set(false);

    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::aub);
    EXPECT_EQ(nullptr, aubCenter.aubManager.get());
}

TEST_F(AubCenterTests, GivenDefaultSetCommandStreamReceiverFlagAndAubFileNameWhenGettingAubStreamModeThenModeAubFileIsReturned) {
    debugManager.flags.UseAubStream.set(true);

    std::string aubFile("test.aub");
    auto mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::aub);

    EXPECT_EQ(aub_stream::mode::aubFile, mode);
}

TEST_F(AubCenterTests, GivenCsrHwAndEmptyAubFileNameWhenGettingAubStreamModeThenModeAubFileIsReturned) {
    debugManager.flags.UseAubStream.set(true);

    std::string aubFile("");
    auto mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::hardware);

    EXPECT_EQ(aub_stream::mode::aubFile, mode);
}

TEST_F(AubCenterTests, GivenCsrHwAndNotEmptyAubFileNameWhenGettingAubStreamModeThenModeAubFileIsReturned) {
    debugManager.flags.UseAubStream.set(true);

    std::string aubFile("test.aub");
    auto mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::hardware);

    EXPECT_EQ(aub_stream::mode::aubFile, mode);
}

TEST_F(AubCenterTests, WhenAubManagerIsCreatedThenCorrectSteppingIsSet) {
    struct {
        __REVID stepping;
        uint32_t expectedAubStreamStepping;
    } steppingPairsToTest[] = {
        {REVISION_A0, AubMemDump::SteppingValues::A},
        {REVISION_A1, AubMemDump::SteppingValues::A},
        {REVISION_A3, AubMemDump::SteppingValues::A},
        {REVISION_B, AubMemDump::SteppingValues::B},
        {REVISION_C, AubMemDump::SteppingValues::C},
        {REVISION_D, AubMemDump::SteppingValues::D},
        {REVISION_K, AubMemDump::SteppingValues::K}};

    debugManager.flags.UseAubStream.set(true);

    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    for (auto steppingPair : steppingPairsToTest) {
        auto hwRevId = productHelper.getHwRevIdFromStepping(steppingPair.stepping, hwInfo);
        if (hwRevId == CommonConstants::invalidStepping) {
            continue;
        }

        hwInfo.platform.usRevId = hwRevId;
        MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::aub);
        EXPECT_EQ(steppingPair.expectedAubStreamStepping, aubCenter.stepping);
    }
}

HWTEST_F(AubCenterTests, whenCreatingAubManagerThenCorrectProductFamilyIsPassed) {
    debugManager.flags.UseAubStream.set(true);

    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));

    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

    auto aubStreamProductFamily = productHelper.getAubStreamProductFamily();

    ASSERT_TRUE(aubStreamProductFamily.has_value());

    MockAubCenter aubCenter(rootDeviceEnvironment, true, "", CommandStreamReceiverType::aub);

    auto aubManager = static_cast<MockAubManager *>(aubCenter.aubManager.get());
    ASSERT_NE(nullptr, aubManager);

    EXPECT_EQ(static_cast<uint32_t>(*aubStreamProductFamily), aubManager->options.productFamily);
}

TEST_F(AubCenterTests, GivenCsrTypeWhenGettingAubStreamModeThenCorrectModeIsReturned) {
    debugManager.flags.UseAubStream.set(true);

    std::string aubFile("test.aub");

    auto mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::aub);
    EXPECT_EQ(aub_stream::mode::aubFile, mode);

    mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::tbx);
    EXPECT_EQ(aub_stream::mode::tbx, mode);

    mode = AubCenter::getAubStreamMode(aubFile, CommandStreamReceiverType::tbxWithAub);
    EXPECT_EQ(aub_stream::mode::aubFileAndTbx, mode);
}

TEST_F(AubCenterTests, GivenSetCommandStreamReceiverFlagEqualDefaultHwWhenAubManagerIsCreatedThenCsrTypeDefinesAubStreamMode) {
    debugManager.flags.UseAubStream.set(true);
    debugManager.flags.SetCommandStreamReceiver.set(-1);

    std::vector<CommandStreamReceiverType> aubTypes = {CommandStreamReceiverType::hardware,
                                                       CommandStreamReceiverType::hardwareWithAub,
                                                       CommandStreamReceiverType::aub};

    for (auto &type : aubTypes) {
        MockAubCenter aubCenter(rootDeviceEnvironment, true, "test", type);
        EXPECT_EQ(aub_stream::mode::aubFile, aubCenter.aubStreamMode);
    }

    MockAubCenter aubCenter2(rootDeviceEnvironment, true, "", CommandStreamReceiverType::tbx);
    EXPECT_EQ(aub_stream::mode::tbx, aubCenter2.aubStreamMode);

    MockAubCenter aubCenter3(rootDeviceEnvironment, true, "", CommandStreamReceiverType::tbxWithAub);
    EXPECT_EQ(aub_stream::mode::aubFileAndTbx, aubCenter3.aubStreamMode);
}

TEST_F(AubCenterTests, GivenSetCommandStreamReceiverFlagSetWhenAubManagerIsCreatedThenDebugFlagDefinesAubStreamMode) {
    debugManager.flags.UseAubStream.set(true);

    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));

    MockAubCenter aubCenter(rootDeviceEnvironment, true, "", CommandStreamReceiverType::aub);
    EXPECT_EQ(aub_stream::mode::tbx, aubCenter.aubStreamMode);

    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbxWithAub));

    MockAubCenter aubCenter2(rootDeviceEnvironment, true, "", CommandStreamReceiverType::aub);
    EXPECT_EQ(aub_stream::mode::aubFileAndTbx, aubCenter2.aubStreamMode);
}

TEST_F(AubCenterTests, GivenAubCenterInSubCaptureModeWhenItIsCreatedWithoutDebugFilterSettingsThenItInitializesSubCaptureFiltersWithDefaults) {
    debugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::filter));

    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::aub);
    auto subCaptureCommon = aubCenter.getSubCaptureCommon();
    EXPECT_NE(nullptr, subCaptureCommon);

    EXPECT_EQ(0u, subCaptureCommon->subCaptureFilter.dumpKernelStartIdx);
    EXPECT_EQ(static_cast<uint32_t>(-1), subCaptureCommon->subCaptureFilter.dumpKernelEndIdx);
    EXPECT_STREQ("", subCaptureCommon->subCaptureFilter.dumpKernelName.c_str());
}

TEST_F(AubCenterTests, GivenAubCenterInSubCaptureModeWhenItIsCreatedWithDebugFilterSettingsThenItInitializesSubCaptureFiltersWithDebugFilterSettings) {
    debugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::filter));
    debugManager.flags.AUBDumpFilterKernelStartIdx.set(10);
    debugManager.flags.AUBDumpFilterKernelEndIdx.set(100);
    debugManager.flags.AUBDumpFilterKernelName.set("kernel_name");

    MockAubCenter aubCenter(rootDeviceEnvironment, false, "", CommandStreamReceiverType::aub);
    auto subCaptureCommon = aubCenter.getSubCaptureCommon();
    EXPECT_NE(nullptr, subCaptureCommon);

    EXPECT_EQ(static_cast<uint32_t>(debugManager.flags.AUBDumpFilterKernelStartIdx.get()), subCaptureCommon->subCaptureFilter.dumpKernelStartIdx);
    EXPECT_EQ(static_cast<uint32_t>(debugManager.flags.AUBDumpFilterKernelEndIdx.get()), subCaptureCommon->subCaptureFilter.dumpKernelEndIdx);
    EXPECT_STREQ(debugManager.flags.AUBDumpFilterKernelName.get().c_str(), subCaptureCommon->subCaptureFilter.dumpKernelName.c_str());
}
