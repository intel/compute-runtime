/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_excludes.h"
using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyImageToLocalBufferStatelessIsUsedThenParamsAreCorrect, IGFX_CRI);
HWTEST_EXCLUDE_PRODUCT(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyImageToSystemBufferStatelessIsUsedThenParamsAreCorrect, IGFX_CRI);
HWTEST_EXCLUDE_PRODUCT(CommandQueuePvcAndLaterTests, givenSplitBcsMaskWhenConstructBcsEnginesForSplitThenContainsGivenBcsEngines_IsAtLeastXeHpcCore, IGFX_CRI);
