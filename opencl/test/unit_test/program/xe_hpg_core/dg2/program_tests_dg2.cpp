/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_tests.h"
using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(ProgramTests, givenAtLeastXeHpgWhenGetInternalOptionsThenCorrectBuildOptionIsSet_IsAtLeastXeHpgCore, IGFX_XE_HPG_CORE);
DG2TEST_F(ProgramTests, givenDG2WhenGetInternalOptionsThenCorrectBuildOptionIsSet) {
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, "-cl-store-cache-default=7 -cl-load-cache-default=4"));
}
