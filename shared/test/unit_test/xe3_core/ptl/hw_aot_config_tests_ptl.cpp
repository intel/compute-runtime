/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/xe3_core/ptl/product_configs_ptl.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"

using namespace NEO;
INSTANTIATE_TEST_SUITE_P(ProductConfigHwInfoPtlTests,
                         ProductConfigHwInfoTests,
                         ::testing::Combine(::testing::ValuesIn(AOT_PTL::productConfigs),
                                            ::testing::Values(IGFX_PTL)));
