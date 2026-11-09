/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_excludes.h"

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfIsBlitSplitEnqueueWARequiredThenReturnFalse, IGFX_PVC);
HWTEST_EXCLUDE_PRODUCT(BlitTests, GivenCpuAccessToLocalMemoryWhenGettingMaxBlitSizeThenValuesAreOverridden_BlitPlatforms, IGFX_PVC);
