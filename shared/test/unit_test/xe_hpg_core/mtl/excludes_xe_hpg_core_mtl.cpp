/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(CommandEncoderTests, givenRequiredWorkGroupOrderWhenCallAdjustWalkOrderThenWalkerIsNotChanged_IsAtMostXeHpcCore, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenCoherencyWithoutSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded_ForceNonCoherentSupportedMatcher, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenCoherencyWithSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded_ForceNonCoherentSupportedMatcher, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeChangeIsRequiredThenCorrectCommandsAreAdded_ForceNonCoherentSupportedMatcher, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsLowerThan128ThenSmallGRFModeIsProgrammed_ForceNonCoherentSupportedMatcher, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsGreaterThan128ThenLargeGRFModeIsProgrammed_ForceNonCoherentSupportedMatcher, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnFalse, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenIsAdjustWalkOrderAvailableCallThenFalseReturn, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenCheckBlitEnqueueAllowedThenReturnTrue, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenBooleanUncachedWhenCallOverridePatIndexThenProperPatIndexIsReturned, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenCheckDummyBlitWaRequiredThenReturnFalse, IGFX_METEORLAKE);
