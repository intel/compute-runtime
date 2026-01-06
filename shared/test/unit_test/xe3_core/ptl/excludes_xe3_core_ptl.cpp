/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned, IGFX_PTL);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenGetL1CachePolicyThenReturnWriteByPass_IsSbaRequiredAndAtLeastXeCore, IGFX_PTL);
HWTEST_EXCLUDE_PRODUCT(CompilerProductHelperFixture, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteByPassPolicyOption_IsAtLeastXeCore, IGFX_PTL);
HWTEST_EXCLUDE_PRODUCT(SbaTest, givenStateBaseAddressAndDebugFlagSetWhenAppendExtraCacheSettingsThenProgramCorrectL1CachePolicy_IsSbaRequiredAndAtLeastXeCore, IGFX_PTL);
