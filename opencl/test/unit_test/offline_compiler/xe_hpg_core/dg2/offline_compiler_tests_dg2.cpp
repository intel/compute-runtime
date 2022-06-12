/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/ocloc_product_config_tests.h"

namespace NEO {
static PRODUCT_CONFIG dg2ProductConfig[] = {
    DG2_G10_A0,
    DG2_G11,
    DG2_G10_B0};

INSTANTIATE_TEST_CASE_P(
    OclocProductConfigDg2TestsValues,
    OclocProductConfigTests,
    ::testing::Combine(
        ::testing::ValuesIn(dg2ProductConfig),
        ::testing::Values(IGFX_DG2)));

} // namespace NEO