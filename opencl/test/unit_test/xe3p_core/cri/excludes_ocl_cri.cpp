/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"
using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyImageToLocalBufferStatelessIsUsedThenParamsAreCorrect, IGFX_CRI);
HWTEST_EXCLUDE_PRODUCT(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyImageToSystemBufferStatelessIsUsedThenParamsAreCorrect, IGFX_CRI);
HWTEST_EXCLUDE_PRODUCT(CommandQueuePvcAndLaterTests, givenSplitBcsMaskWhenConstructBcsEnginesForSplitThenContainsGivenBcsEngines_IsAtLeastXeHpcCore, IGFX_CRI);
