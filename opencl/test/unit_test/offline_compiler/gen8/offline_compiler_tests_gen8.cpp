/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"
#include "opencl/test/unit_test/offline_compiler/offline_compiler_tests.h"

#include "gmock/gmock.h"

namespace NEO {

TEST_F(OfflineCompilerTests, givenDebugOptionWhenPlatformIsBelowGen9ThenInternalOptionShouldNotContainKernelDebugEnable) {
    std::vector<std::string> argv = {
        "ocloc",
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
} // namespace NEO