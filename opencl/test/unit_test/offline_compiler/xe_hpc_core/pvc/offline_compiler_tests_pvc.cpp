/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/xe_hpc_core/pvc/product_configs_pvc.h"

#include "opencl/test/unit_test/offline_compiler/ocloc_product_config_tests.h"

namespace NEO {
INSTANTIATE_TEST_CASE_P(
    OclocProductConfigPvcTestsValues,
    OclocProductConfigTests,
    ::testing::Combine(
        ::testing::ValuesIn(AOT_PVC::productConfigs),
        ::testing::Values(IGFX_PVC)));

} // namespace NEO