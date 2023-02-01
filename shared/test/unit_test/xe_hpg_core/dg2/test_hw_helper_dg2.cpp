/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
using GfxCoreHelperTestDg2 = ::testing::Test;

DG2TEST_F(GfxCoreHelperTestDg2, GivenDifferentSteppingWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned) {
    using SHARED_LOCAL_MEMORY_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    __REVID revisions[] = {REVISION_A0, REVISION_B};
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    auto hwInfo = *mockExecutionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    for (auto revision : revisions) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        for (auto &testInput : computeSlmValuesXeHPAndLaterTestsInput) {
            EXPECT_EQ(testInput.expected, gfxCoreHelper.computeSlmValues(hwInfo, testInput.slmSize));
        }
    }
}
