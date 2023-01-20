/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gen9/hw_cmds_skl.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"
#include "opencl/test/unit_test/offline_compiler/offline_compiler_tests.h"

namespace NEO {

using MockOfflineCompilerSklTests = ::testing::Test;

SKLTEST_F(MockOfflineCompilerSklTests, GivenSklWhenParseDebugSettingsThenStatelessToStatefulOptimizationIsEnabled) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.deviceName = "skl";
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    mockOfflineCompiler.parseDebugSettings();
    std::string internalOptions = mockOfflineCompiler.internalOptions;
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
    EXPECT_NE(std::string::npos, found);
}

SKLTEST_F(MockOfflineCompilerSklTests, GivenSklAndDisabledViaDebugThenStatelessToStatefulOptimizationDisabled) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.deviceName = "skl";
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(0);
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    mockOfflineCompiler.setStatelessToStatefulBufferOffsetFlag();
    std::string internalOptions = mockOfflineCompiler.internalOptions;
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
    EXPECT_EQ(std::string::npos, found);
}

SKLTEST_F(MockOfflineCompilerSklTests, givenSklWhenAppendExtraInternalOptionsThenForceEmuInt32DivRemSPIsNotApplied) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.deviceName = "skl";
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    std::string internalOptions = mockOfflineCompiler.internalOptions;
    mockOfflineCompiler.appendExtraInternalOptions(internalOptions);
    size_t found = internalOptions.find(NEO::CompilerOptions::forceEmuInt32DivRemSP.data());
    EXPECT_EQ(std::string::npos, found);
}

SKLTEST_F(MockOfflineCompilerSklTests, givenSklWhenAppendExtraInternalOptionsThenGreaterThan4gbBuffersRequiredIsNotSet) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.deviceName = "skl";
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    std::string internalOptions = mockOfflineCompiler.internalOptions;
    mockOfflineCompiler.forceStatelessToStatefulOptimization = false;
    mockOfflineCompiler.appendExtraInternalOptions(internalOptions);
    size_t found = internalOptions.find(NEO::CompilerOptions::greaterThan4gbBuffersRequired.data());
    EXPECT_EQ(std::string::npos, found);
}

} // namespace NEO