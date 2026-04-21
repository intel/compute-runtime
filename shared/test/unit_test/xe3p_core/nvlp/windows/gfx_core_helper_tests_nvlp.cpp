/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_mapper.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using GfxCoreHelperTestsNvlp = GfxCoreHelperTest;

NVLPTEST_F(GfxCoreHelperTestsNvlp, whenCallingAreSecondaryContextsSupportedThenTrueIsReturnedAndContextGroupCountIs8) {
    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto hwInfo = *NEO::defaultHwInfo.get();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);

    EXPECT_TRUE(executionEnvironment->rootDeviceEnvironments[0]->gfxCoreHelper->areSecondaryContextsSupported());
    EXPECT_EQ(8u, executionEnvironment->rootDeviceEnvironments[0]->gfxCoreHelper->getContextGroupContextsCount());
}
