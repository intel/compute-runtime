/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/offline_compiler_ext.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"

#include <gtest/gtest.h>

namespace NEO {

TEST(OclocExt, GivenCallToGetOfflineCompilerOptionsExtThenEmptyStringIsReturned) {
    EXPECT_TRUE(getOfflineCompilerOptionsExt().empty()) << getOfflineCompilerOptionsExt();
}

TEST(OclocExt, GivenCallToParseCommandLineExtThenErrorIsReturned) {
    MockOfflineCompiler mockOfflineCompiler;
    const std::vector<std::string> argv = {"--arg"};
    uint32_t argIdx = 0;
    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, mockOfflineCompiler.parseCommandLineExt(1, argv, argIdx));
}

} // namespace NEO
