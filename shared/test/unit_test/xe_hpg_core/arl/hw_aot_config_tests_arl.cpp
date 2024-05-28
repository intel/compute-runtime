/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/xe_hpg_core/arl/product_configs_arl.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"

using namespace NEO;
INSTANTIATE_TEST_SUITE_P(ProductConfigHwInfoArlTests,
                         ProductConfigHwInfoTests,
                         ::testing::Combine(::testing::ValuesIn(AOT_ARL::productConfigs),
                                            ::testing::Values(IGFX_ARROWLAKE)));
