/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_excludes.h"

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfForceEmuInt32DivRemSPWAIsRequiredThenFalseIsReturned, IGFX_ROCKETLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIf3DPipelineSelectWAIsRequiredThenFalseIsReturned, IGFX_ROCKETLAKE);
HWTEST_EXCLUDE_PRODUCT(AubCenterTests, whenCreatingAubManagerThenCorrectProductFamilyIsPassed, IGFX_ROCKETLAKE);
