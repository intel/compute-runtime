/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/xe2_hpg_core/bmg/product_configs_bmg.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"

using namespace NEO;
INSTANTIATE_TEST_SUITE_P(ProductConfigHwInfoBmgTests,
                         ProductConfigHwInfoTests,
                         ::testing::Combine(::testing::ValuesIn(AOT_BMG::productConfigs),
                                            ::testing::Values(IGFX_BMG)));
