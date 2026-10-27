/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(CommandEncoderTests, givenRequiredWorkGroupOrderWhenCallAdjustWalkOrderThenWalkerIsNotChanged_IsAtMostXeCore, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnFalse, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenCheckBlitEnqueuePreferredThenReturnTrue, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenBooleanUncachedWhenCallOverridePatIndexThenProperPatIndexIsReturned, IGFX_METEORLAKE);
