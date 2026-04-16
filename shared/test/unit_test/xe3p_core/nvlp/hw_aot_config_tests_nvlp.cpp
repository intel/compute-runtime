/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/xe3p_core/nvlp/product_configs_nvlp.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"

using namespace NEO;
INSTANTIATE_TEST_SUITE_P(ProductConfigHwInfoNvlTests,
                         ProductConfigHwInfoTests,
                         ::testing::Combine(::testing::ValuesIn(AOT_NVL::productConfigs),
                                            ::testing::Values(IGFX_NVL)));
