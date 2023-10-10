/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/xe_hpg_core/mtl/product_configs_mtl.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"

namespace NEO {
INSTANTIATE_TEST_CASE_P(ProductConfigHwInfoMtlTests,
                        ProductConfigHwInfoTests,
                        ::testing::Combine(::testing::ValuesIn(AOT_MTL::productConfigs),
                                           ::testing::Values(IGFX_METEORLAKE)));

} // namespace NEO
