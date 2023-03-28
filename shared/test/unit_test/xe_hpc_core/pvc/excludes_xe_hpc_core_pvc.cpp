/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfIsBlitSplitEnqueueWARequiredThenReturnFalse, IGFX_PVC);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenNotLockableAllocationWhenGettingIsBlitCopyRequiredForLocalMemoryThenCorrectValuesAreReturned, IGFX_PVC);
HWTEST_EXCLUDE_PRODUCT(BlitTests, GivenCpuAccessToLocalMemoryWhenGettingMaxBlitSizeThenValuesAreOverriden_BlitPlatforms, IGFX_PVC);
HWTEST_EXCLUDE_PRODUCT(GfxCoreHelperTest, GivenCooperativeEngineSupportedAndNotUsedWhenAdjustMaxWorkGroupCountIsCalledThenSmallerValueIsReturned, IGFX_PVC);
