/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options/compiler_options.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"
#include "opencl/test/unit_test/offline_compiler/offline_compiler_tests.h"

#include "gmock/gmock.h"

using namespace NEO;

using MockOfflineCompilerBdwTests = ::testing::Test;

BDWTEST_F(MockOfflineCompilerBdwTests, givenDebugOptionAndBdwThenInternalOptionShouldNotContainKernelDebugEnable) {
    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-options",
        "-g",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        "bdw"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->initialize(argv.size(), argv);

    std::string internalOptions = mockOfflineCompiler->internalOptions;

    EXPECT_THAT(internalOptions, Not(::testing::HasSubstr("-cl-kernel-debug-enable")));
}

BDWTEST_F(MockOfflineCompilerBdwTests, GivenBdwWhenParseDebugSettingsThenContainsHasBufferOffsetArg) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.deviceName = "bdw";
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.internalOptions;
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
    EXPECT_EQ(std::string::npos, found);
}
