/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/xe_hpg_core/dg2/product_configs_dg2.h"

#include "opencl/test/unit_test/offline_compiler/ocloc_fatbinary_tests.h"
#include "opencl/test/unit_test/offline_compiler/ocloc_product_config_tests.h"

namespace NEO {
INSTANTIATE_TEST_CASE_P(
    OclocProductConfigDg2TestsValues,
    OclocProductConfigTests,
    ::testing::Combine(
        ::testing::ValuesIn(AOT_DG2::productConfigs),
        ::testing::Values(IGFX_DG2)));

INSTANTIATE_TEST_CASE_P(
    OclocFatbinaryDg2Tests,
    OclocFatbinaryPerProductTests,
    ::testing::Combine(
        ::testing::Values("xe-hpg"),
        ::testing::Values(IGFX_DG2)));

} // namespace NEO