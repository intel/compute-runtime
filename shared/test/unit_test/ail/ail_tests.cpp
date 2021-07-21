/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include "test.h"

namespace NEO {
using IsSKL = IsProduct<IGFX_SKYLAKE>;

using AILTests = ::testing::Test;

HWTEST2_F(AILTests, givenUninitializedTemplateWhenGetAILConfigurationThenNullptrIsReturned, IsSKL) {
    auto ailConfiguration = AILConfiguration::get(productFamily);

    ASSERT_EQ(nullptr, ailConfiguration);
}
} // namespace NEO