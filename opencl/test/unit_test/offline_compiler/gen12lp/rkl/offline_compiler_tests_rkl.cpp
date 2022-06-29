/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options/compiler_options.h"
#include "shared/source/gen12lp/hw_cmds_rkl.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"
#include "opencl/test/unit_test/offline_compiler/offline_compiler_tests.h"

using namespace NEO;

using MockOfflineCompilerRklTests = ::testing::Test;
RKLTEST_F(MockOfflineCompilerRklTests, givenRklWhenAppendExtraInternalOptionsThenForceEmuInt32DivRemSPIsApplied) {

    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.deviceName = "rkl";
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    std::string internalOptions = mockOfflineCompiler.internalOptions;
    mockOfflineCompiler.appendExtraInternalOptions(internalOptions);
    size_t found = internalOptions.find(NEO::CompilerOptions::forceEmuInt32DivRemSP.data());
    EXPECT_NE(std::string::npos, found);
}
