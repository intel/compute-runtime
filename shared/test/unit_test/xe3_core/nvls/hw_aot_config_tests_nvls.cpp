/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/xe3_core/nvls/product_configs_nvls.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"

using namespace NEO;
INSTANTIATE_TEST_SUITE_P(ProductConfigHwInfoNvlsTests,
                         ProductConfigHwInfoTests,
                         ::testing::Combine(::testing::ValuesIn(AOT_NVLS::productConfigs),
                                            ::testing::Values(IGFX_NVL_XE3G)));
