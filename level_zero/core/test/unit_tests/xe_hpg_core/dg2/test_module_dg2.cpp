/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using ModuleTranslationUnitTest = Test<DeviceFixture>;

HWTEST_EXCLUDE_PRODUCT(ModuleTranslationUnitTest, givenAtLeastXeHpgWhenGetInternalOptionsThenCorrectBuildOptionIsSet_IsAtLeastXeHpgCore, IGFX_XE_HPG_CORE);
DG2TEST_F(ModuleTranslationUnitTest, givenDG2WhenGetInternalOptionsThenCorrectBuildOptionIsSet) {
    auto pMockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(pMockCompilerInterface);
    MockModuleTranslationUnit moduleTu(this->device);
    auto ret = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_TRUE(ret);
    EXPECT_NE(pMockCompilerInterface->inputInternalOptions.find("-cl-store-cache-default=7 -cl-load-cache-default=4"), std::string::npos);
}

} // namespace ult
} // namespace L0