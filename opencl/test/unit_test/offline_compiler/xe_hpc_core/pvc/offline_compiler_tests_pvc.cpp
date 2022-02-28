/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/ocloc_product_config_tests.h"

namespace NEO {
static PRODUCT_CONFIG pvcProductConfig[] = {
    PVC_XL_A0,
    PVC_XL_B0,
    PVC_XT_A0,
    PVC_XT_B0};

INSTANTIATE_TEST_CASE_P(
    OclocProductConfigPvcTestsValues,
    OclocProductConfigTests,
    ::testing::Combine(
        ::testing::ValuesIn(pvcProductConfig),
        ::testing::Values(IGFX_PVC)));

} // namespace NEO