/*
 * Copyright (C) 2022 Intel Corporation
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

INSTANTIATE_TEST_CASE_P(ProductConfigHwInfoMtlTests,
                        ProductConfigHwInfoBadRevisionTests,
                        ::testing::Combine(::testing::Values(AOT::XE_LPG_MD_A0, AOT::XE_LPG_LG_A0),
                                           ::testing::Values(IGFX_METEORLAKE)));

} // namespace NEO
