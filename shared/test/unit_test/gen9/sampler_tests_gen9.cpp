/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>

using namespace NEO;

using Gen9SamplerTest = ::testing::Test;

GEN9TEST_F(Gen9SamplerTest, WhenAppendingSamplerStateParamsThenStateIsNotChanged) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto stateWithoutAppendedParams = FamilyType::cmdInitSamplerState;
    auto stateWithAppendedParams = FamilyType::cmdInitSamplerState;
    EXPECT_TRUE(memcmp(&stateWithoutAppendedParams, &stateWithAppendedParams, sizeof(SAMPLER_STATE)) == 0);
    ProductHelper::get(defaultHwInfo->platform.eProductFamily)->adjustSamplerState(&stateWithAppendedParams, *defaultHwInfo);
    EXPECT_TRUE(memcmp(&stateWithoutAppendedParams, &stateWithAppendedParams, sizeof(SAMPLER_STATE)) == 0);
}
